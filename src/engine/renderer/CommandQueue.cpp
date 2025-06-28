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
// CommandQueue.cpp

#include "common/Common.h"

#include "CommandQueue.h"

const uint32_t CommandQueue::MAX_DRAWINFOS_PER_BINDINFO;
const uint32_t CommandQueue::MAX_BINDINFOS;
const uint32_t CommandQueue::MAX_DRAWINFOS;
const uint32_t CommandQueue::MAX_RENDER_CMDS;

CommandQueue commandQueue;

CommandQueue::CommandQueue() {
}

CommandQueue::~CommandQueue() {
}

void CommandQueue::InitGLBuffers() {
	indirectRenderBuffer.GenBuffer();

	indirectRenderBuffer.BufferStorage( MAX_RENDER_CMDS * INDIRECT_COMMAND_SIZE, 1, nullptr );
}

void CommandQueue::FreeGLBuffers() {
	indirectRenderBuffer.DelBuffer();
}

void CommandQueue::AddDrawCommand( const GLenum mode, const GLuint count, const GLuint firstIndex ) {
	UpdateCurrentBindInfo();

	if ( mode != currentDrawInfo->mode ) {
		AddDrawInfo( &currentBindInfo, mode );
	}

	if ( indirectBufferOffset == MAX_RENDER_CMDS ) {
		Flush();
	}

	currentDrawInfo->count++;

	cmds[indirectBufferOffset].count = count;
	cmds[indirectBufferOffset].firstIndex = firstIndex;
	cmds[indirectBufferOffset].instanceCount = 1;
	cmds[indirectBufferOffset].baseInstance = pushBuffer.sector;

	indirectBufferOffset++;
}

void CommandQueue::Flush() {
	uint32_t* indirectCmdsData = stagingBuffer.MapBuffer( indirectBufferOffset * INDIRECT_COMMAND_SIZE );
	memcpy( indirectCmdsData, cmds, indirectBufferOffset * INDIRECT_COMMAND_SIZE );
	stagingBuffer.QueueStagingCopy( &indirectRenderBuffer, 0 );
	stagingBuffer.FlushAll();

	indirectRenderBuffer.BindBuffer( GL_DRAW_INDIRECT_BUFFER );

	for ( uint32_t i = 0; i < currentBindInfoOffset; i++ ) {
		R_BindVBO( bindInfos[i].VBO );
		R_BindIBO( bindInfos[i].IBO );
		GL_BindProgram( bindInfos[i].program );
		GL_State( bindInfos[i].GLState );
		GL_Cull( bindInfos[i].cullType );
		GL_DepthRange( bindInfos[i].depthRange[0], bindInfos[i].depthRange[1] );

		for ( const DrawInfo* drawInfo = &drawInfos[bindInfos[i].drawInfoOffset];
			drawInfo < &drawInfos[bindInfos[i].drawInfoCount]; drawInfo++ ) {
			glMultiDrawElementsIndirect( drawInfo->mode, GL_INDEX_TYPE, BUFFER_OFFSET( drawInfo->offset ), drawInfo->count, 0 );
		}
	}

	memset( bindInfos, 0, currentBindInfoOffset * sizeof( BindInfo ) );
	memset( drawInfos, 0, currentDrawInfoOffset * sizeof( DrawInfo ) );

	currentBindInfoOffset = 0;
	currentDrawInfoOffset = 0;

	memset( &currentBindInfo, 0, sizeof( BindInfo ) );
	currentDrawInfo = drawInfos;

	bindInfosMap.clear();

	if ( glConfig2.usingMaterialSystem ) {
		materialSystem.BindIndirectBuffer();
	}
}

void CommandQueue::AddDrawInfo( BindInfo* bindInfo, const GLenum mode ) {
	if ( bindInfo->drawInfoCount == MAX_DRAWINFOS_PER_BINDINFO ) {
		Flush();
	}

	if ( !bindInfo->drawInfoCount ) {
		if ( currentDrawInfoOffset >= MAX_DRAWINFOS ) {
			Flush();
		}

		bindInfo->drawInfoOffset = currentDrawInfoOffset;
		currentDrawInfoOffset += MAX_DRAWINFOS_PER_BINDINFO;
	}

	if ( bindInfo->drawInfoCount == MAX_DRAWINFOS_PER_BINDINFO ) {
		Flush();
	}

	currentDrawInfo = &drawInfos[bindInfo->drawInfoOffset + bindInfo->drawInfoCount];

	currentDrawInfo->mode = mode;
	currentDrawInfo->offset = indirectBufferOffset;
	currentDrawInfo->count = 0;

	bindInfo->drawInfoCount++;
}

void CommandQueue::UpdateCurrentBindInfo() {
	const bool update = currentBindInfo.VBO != glState.currentVBO || currentBindInfo.IBO != glState.currentIBO
		|| currentBindInfo.program != glState.currentProgram
		|| currentBindInfo.GLState != glState.glStateBits || currentBindInfo.cullType != glState.faceCulling
		|| currentBindInfo.depthRange[0] != glState.depthRange[0] || currentBindInfo.depthRange[1] != glState.depthRange[1];

	if ( update ) {
		/* currentBindInfo.VBO = glState.currentVBO;
		currentBindInfo.IBO = glState.currentIBO;
		currentBindInfo.program = glState.currentProgram;
		currentBindInfo.GLState = glState.glStateBits;
		currentBindInfo.cullType = glState.faceCulling;
		currentBindInfo.depthRange[0] = glState.depthRange[0];
		currentBindInfo.depthRange[1] = glState.depthRange[1]; */

		BindInfo bindInfo { glState.currentVBO, glState.currentIBO,
			glState.currentProgram,
			glState.glStateBits, glState.faceCulling, glState.depthRange[0], glState.depthRange[1] };

		if ( bindInfosMap.find( bindInfo ) == bindInfosMap.end() ) {
			AddDrawInfo( &bindInfo, GL_TRIANGLES );

			bindInfosMap[bindInfo] = currentBindInfoOffset;
			bindInfos[currentBindInfoOffset] = bindInfo;

			currentBindInfoOffset++;
		}

		currentBindInfo = bindInfo;
	}

	/* if ( currentBindInfo.VBO != glState.currentVBO ) {
		currentBindInfo.VBO = glState.currentVBO;
	}

	if ( currentBindInfo.IBO != glState.currentIBO ) {
		currentBindInfo.IBO = glState.currentIBO;
	}

	if ( currentBindInfo.program != glState.currentProgram ) {
		currentBindInfo.program = glState.currentProgram;
	}

	if ( currentBindInfo.GLState != glState.glStateBits ) {
		currentBindInfo.GLState = glState.glStateBits;
	}

	if ( currentBindInfo.cullType != glState.faceCulling ) {
		currentBindInfo.cullType = glState.faceCulling;
	}

	if ( currentBindInfo.depthRange[0] != glState.depthRange[0] || currentBindInfo.depthRange[1] != glState.depthRange[1] ) {
		currentBindInfo.depthRange[0] = glState.depthRange[0];
		currentBindInfo.depthRange[1] = glState.depthRange[1];
	} */
}
