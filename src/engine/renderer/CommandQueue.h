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
// CommandQueue.h

#ifndef COMMANDQUEUE_H
#define COMMANDQUEUE_H

#include "common/Common.h"
#include "GL/glew.h"

#include "tr_local.h"
#include "BufferBind.h"
#include "GLMemory.h"
#include "Material.h"

struct BindInfo {
	VBO_t* VBO;
	IBO_t* IBO;
	ShaderProgramDescriptor* program;

	uint32_t GLState;
	uint32_t vertexAttribsState;
	bool vertexSkinning;
	cullType_t cullType;
	vec2_t depthRange;

	uint32_t drawInfoOffset = 0;
	uint32_t drawInfoCount = 0;
};

struct BindInfoHasher {
	size_t operator()( const BindInfo& bindInfo ) const {
		size_t hash = ( ( size_t ) bindInfo.VBO ) >> 8;
		hash += ( ( size_t ) bindInfo.IBO ) >> 16;
		hash += ( ( size_t ) bindInfo.program ) >> 32;
		hash ^= ( ( size_t ) bindInfo.GLState ) >> 32;
		return hash;
	}
};

struct BindInfoEqual {
	bool operator()( const BindInfo& lhs, const BindInfo& rhs ) const {
		return lhs.VBO == rhs.VBO && lhs.IBO == rhs.IBO
			&& lhs.program == rhs.program
			&& lhs.GLState == rhs.GLState
			&& lhs.vertexAttribsState == rhs.vertexAttribsState && lhs.vertexSkinning == rhs.vertexSkinning
			&& lhs.cullType == rhs.cullType
			&& lhs.depthRange[0] == rhs.depthRange[0] && lhs.depthRange[1] == rhs.depthRange[1];
	}
};

struct DrawInfo {
	GLenum mode;
	GLsizei offset;
	GLsizei count;
};

class CommandQueue {
	public:
	CommandQueue();
	~CommandQueue();

	void InitGLBuffers();
	void FreeGLBuffers();

	void AddDrawCommand( const GLenum mode, const GLuint count, const GLuint firstIndex, const GLint baseVertex );

	void Flush();

	private:
	std::unordered_map<BindInfo, uint32_t, BindInfoHasher, BindInfoEqual> bindInfosMap;

	/* pushUBO is up to 64kb, with ~204 bytes on average for a shader this is enough for 321 draws,
	with a little extra here */
	static const uint32_t MAX_DRAWINFOS_PER_BINDINFO = 512;
	static const uint32_t MAX_BINDINFOS = 1024; // More than enough
	static const uint32_t MAX_DRAWINFOS = MAX_BINDINFOS * MAX_DRAWINFOS_PER_BINDINFO;

	BindInfo bindInfos[MAX_BINDINFOS];
	DrawInfo drawInfos[MAX_DRAWINFOS];

	BindInfo* currentBindInfo;
	DrawInfo* currentDrawInfo = drawInfos;
	uint32_t currentBindInfoOffset = 0;
	uint32_t currentDrawInfoOffset = 0;

	uint32_t indirectBufferOffset = 0;
	static const uint32_t MAX_RENDER_CMDS = 65536;
	GLBuffer indirectRenderBuffer = GLBuffer( "commandQueueIndirectRender", 0, 0, 0 );

	GLIndirectCommand cmds[MAX_RENDER_CMDS];

	void AddDrawInfo( BindInfo* bindInfo, const GLenum mode );
	void UpdateCurrentBindInfo();
};

extern CommandQueue commandQueue;

#endif // COMMANDQUEUE_H