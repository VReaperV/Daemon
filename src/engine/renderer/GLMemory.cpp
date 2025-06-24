/*
===========================================================================

Daemon BSD Source Code
Copyright (c) 2025 Daemon Developers
All rights reserved.

This file is part of the Daemon BSD Source Code (Daemon Source Code).

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.
	* Neither the name of the Daemon developers nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL DAEMON DEVELOPERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

===========================================================================
*/
// GLMemory.cpp

#include "common/Common.h"

#include "GLMemory.h"

#include "gl_shader.h"

// 128 MB, should be enough to fit anything in BAR without going overboard
const GLsizeiptr GLStagingBuffer::SIZE = 128 * 1024 * 1024 / sizeof( uint32_t );

GLStagingBuffer stagingBuffer;
PushBuffer pushBuffer;

void GLBufferCopy( GLBuffer* src, GLBuffer* dst, GLintptr srcOffset, GLintptr dstOffset, GLsizeiptr size ) {
	glCopyNamedBufferSubData( src->id, dst->id,
		srcOffset * sizeof( uint32_t ), dstOffset * sizeof( uint32_t ), size * sizeof( uint32_t ) );
}

uint32_t* GLStagingBuffer::MapBuffer( const GLsizeiptr size ) {
	if ( size > SIZE ) {
		Sys::Drop( "Couldn't map GL staging buffer: size too large (%u/%u)", size, SIZE );
	}

	if ( pointer + size > SIZE ) {
		FlushAll();

		GLsync sync = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );

		constexpr GLuint64 SYNC_TIMEOUT = 10000000000; // 10 seconds
		if ( glClientWaitSync( sync, GL_SYNC_FLUSH_COMMANDS_BIT, SYNC_TIMEOUT ) == GL_TIMEOUT_EXPIRED ) {
			Sys::Drop( "Failed GL staging buffer copy sync" );
		}
		glDeleteSync( sync );

		pointer = 0;
		current = 0;
		last = 0;
	}

	uint32_t* ret = buffer.GetData() + pointer;
	last = pointer;
	pointer += size;

	return ret;
}

void GLStagingBuffer::FlushBuffer() {
	buffer.FlushRange( current, pointer - current );

	GL_CheckErrors();

	current = pointer;
}

void GLStagingBuffer::QueueStagingCopy( GLBuffer* dst, const GLsizeiptr dstOffset ) {
	copyQueue[currentCopy].dst = dst;
	copyQueue[currentCopy].dstOffset = dstOffset;
	copyQueue[currentCopy].stagingOffset = last;
	copyQueue[currentCopy].size = pointer - last;

	currentCopy++;

	if ( currentCopy == MAX_COPIES ) {
		FlushStagingCopyQueue();
	}
}

void GLStagingBuffer::FlushStagingCopyQueue() {
	for ( GLStagingCopy& copy : copyQueue ) {
		if ( copy.dst ) {
			GLBufferCopy( &buffer, copy.dst, copy.stagingOffset, copy.dstOffset, copy.size );
			copy.dst = nullptr;
		}
	}

	currentCopy = 0;

	GL_CheckErrors();
}

void GLStagingBuffer::FlushAll() {
	FlushBuffer();
	FlushStagingCopyQueue();
}

bool GLStagingBuffer::Active() const {
	return buffer.id;
}

void GLStagingBuffer::InitGLBuffer() {
	buffer.GenBuffer();

	buffer.BufferStorage( SIZE, 1, nullptr );
	buffer.MapAll();

	GL_CheckErrors();
}

void GLStagingBuffer::FreeGLBuffer() {
	buffer.DelBuffer();

	pointer = 0;
	current = 0;
	last = 0;
}

void PushBuffer::InitGLBuffers() {
	globalUBO.GenBuffer();

	globalUBO.BufferStorage( pushBuffer.constUniformsSize + pushBuffer.frameUniformsSize, 1, nullptr );

	globalUBO.BindBufferBase();

	pushUBO.GenBuffer();

	pushUBO.BufferStorage( PUSH_SIZE, 1, nullptr );
	pushUBO.MapAll();

	pushUBO.BindBufferBase();
}

void PushBuffer::FreeGLBuffers() {
	globalUBO.DelBuffer();
	pushUBO.DelBuffer();
}

uint32_t* PushBuffer::MapGlobalUniformData( const int updateType ) {
	switch ( updateType ) {
		case GLUniform::CONST:
			globalUBOData = stagingBuffer.MapBuffer( constUniformsSize );
			stagingBuffer.QueueStagingCopy( &globalUBO, 0 );
			break;
		case GLUniform::FRAME:
			globalUBOData = stagingBuffer.MapBuffer( frameUniformsSize );
			stagingBuffer.QueueStagingCopy( &globalUBO, constUniformsSize );
			break;
		default:
			ASSERT_UNREACHABLE();
	}

	return globalUBOData;
}

void PushBuffer::PushGlobalUniforms() {
	stagingBuffer.FlushAll();
}

uint32_t* PushBuffer::MapPushUniformData( const GLuint size ) {
	if ( !size ) {
		return nullptr;
	}

	if ( size > PUSH_SIZE ) {
		Sys::Drop( "Couldn't map GL PushBuffer: size too large (%u/%u)", size, PUSH_SIZE );
	}

	uint32_t padding = pushEnd;
	pushEnd = ( pushEnd + size - 1 ) / size * size;
	padding = pushEnd - padding;
	pushStart += padding;
	sector = pushEnd / size;

	if ( pushEnd + size > PUSH_SIZE ) {
		PushUniforms();

		GLsync pushSync = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );

		GLenum syncValue = glClientWaitSync( pushSync, GL_SYNC_FLUSH_COMMANDS_BIT, SYNC_TIMEOUT );
		while ( syncValue != GL_ALREADY_SIGNALED && syncValue != GL_CONDITION_SATISFIED ) {
			syncValue = glClientWaitSync( pushSync, 0, SYNC_TIMEOUT );
		}

		GL_CheckErrors();
		glDeleteSync( pushSync );

		pushStart = 0;
		pushEnd = 0;
		sector = 0;

		GL_CheckErrors();
	}
	
	uint32_t* ret = pushUBO.GetData() + pushEnd;
	pushEnd += size;

	GL_CheckErrors();

	return ret;
}

void PushBuffer::PushUniforms() {
	if ( pushEnd - pushStart ) {
		pushUBO.FlushRange( pushStart, pushEnd - pushStart );
	}

	pushStart = pushEnd;

	GL_CheckErrors();
}

void PushBuffer::WriteCurrentShaderToPushUBO() {
	if ( glState.currentShader ) {
		glState.currentShader->WriteUniformsToBuffer(
			MapPushUniformData( glState.currentShader->pushUniformsSize + glState.currentShader->pushUniformsPadding ),
			GLShader::PUSH
		);
	}

	if( glState.currentMaterialShader ) {
		glState.currentMaterialShader->SetUniform_PushBufferSector( sector );
	}

	PushUniforms();
}
