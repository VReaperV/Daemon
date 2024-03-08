#include "CommandQueue.h"

#include "gl_shader.h"

GLIndirectBuffer commandQueueBuffer( "commandQueue" );
GLSSBO drawcallStateSSBO( "drawcallState", 0 );
GlobalCommandQueue globalCommandQueue;

DrawcallState::DrawcallState() {
}

DrawcallState::DrawcallState( const DrawcallState& other ) {
	if ( other.shaderSet ) {
		shader = other.shader;
		program = other.program;
		shaderSet = other.shaderSet;
		currentTexture = other.currentTexture;
		textureCount = other.textureCount;
		textures = new Texture*[textureCount];
		// uniformValues = new uint32_t[shader->GetSTD430Size()];
		uniformValues = other.uniformValues;
		std430size = other.std430size;

		FBOhandle = other.FBOhandle;
		memcpy( vertexFormats, other.vertexFormats, sizeof( vertexFormats ) );
		VBOhandle = other.VBOhandle;
		IBOhandle = other.IBOhandle;

		id = other.id;
		previousDrawcall = other.previousDrawcall;
		syncPoint = other.syncPoint;
		syncSequential = other.syncSequential;
		
		renderingCommand.baseInstance = other.renderingCommand.baseInstance;
		renderingCommand.baseVertex = other.renderingCommand.baseVertex;
		renderingCommand.count = other.renderingCommand.count;
		renderingCommand.firstIndex = other.renderingCommand.firstIndex;
		renderingCommand.instanceCount = other.renderingCommand.instanceCount;
		// memcpy( uniformValues, other.uniformValues, shader->GetSTD430Size() * sizeof( uint32_t ) );
		memcpy( textures, other.textures, textureCount * sizeof( Texture* ) );
	}
}

DrawcallState& DrawcallState::operator=( const DrawcallState& other ) {
	if ( this != &other ) {
		if ( shaderSet ) {
			// delete[] uniformValues;
			delete[] textures;
		}
		if ( other.shaderSet ) {
			shaderSet = other.shaderSet;
			shader = other.shader;
			program = other.program;
			currentTexture = other.currentTexture;
			textureCount = other.textureCount;
			textures = new Texture * [textureCount];
			// uniformValues = new uint32_t[shader->GetSTD430Size()];
			uniformValues = other.uniformValues;
			std430size = other.std430size;

			renderingCommand.baseInstance = other.renderingCommand.baseInstance;
			renderingCommand.baseVertex = other.renderingCommand.baseVertex;
			renderingCommand.count = other.renderingCommand.count;
			renderingCommand.firstIndex = other.renderingCommand.firstIndex;
			renderingCommand.instanceCount = other.renderingCommand.instanceCount;
			// memcpy( uniformValues, other.uniformValues, shader->GetSTD430Size() * sizeof( uint32_t ) );
			memcpy( textures, other.textures, textureCount * sizeof( Texture* ) );
		}

		FBOhandle = other.FBOhandle;
		memcpy( vertexFormats, other.vertexFormats, sizeof( vertexFormats ) );
		VBOhandle = other.VBOhandle;
		IBOhandle = other.IBOhandle;

		id = other.id;
		previousDrawcall = other.previousDrawcall;
		syncPoint = other.syncPoint;
		syncSequential = other.syncSequential;
	}

	return *this;
}

DrawcallState::~DrawcallState() {
	if( shaderSet ) {
		// delete[] uniformValues;
		delete[] textures;
	}
}

bool operator==( const DrawcallState::VertexFormat& lhs, const DrawcallState::VertexFormat& rhs ) {
	if ( !lhs.enabled && !rhs.enabled ) {
		// return true;
	}

	return lhs.enabled == rhs.enabled && lhs.index == rhs.index && lhs.size == rhs.size && lhs.type == rhs.type
		&& lhs.normalized == rhs.normalized && lhs.stride == rhs.stride && lhs.offset == rhs.offset;
}

uint GlobalCommandQueue::UpdateDrawcall( DrawcallState& drawcall, GLIndirectBuffer::GLIndirectCommand* commands, uint32_t* ssbo,
										 const uint ssboOffset ) {
	const uint paddedSize = drawcall.std430size + drawcall.shader->GetPadding();
	// Log::Warn( "Size: %i padding: %i", drawcall.std430size, drawcall.shader->GetPadding() );
	const uint padding = ssboOffset % paddedSize == 0 ? 0 :
														paddedSize - ( ssboOffset % paddedSize );
	ssbo += padding;
	drawcall.renderingCommand.baseInstance = ( ssboOffset + padding ) / paddedSize;

	memcpy( commands, &drawcall.renderingCommand, sizeof( GLIndirectBuffer::GLIndirectCommand ) );
	memcpy( ssbo, drawcall.uniformValues, drawcall.std430size * 4 );

	const uint newOffset = ssboOffset + padding + drawcall.std430size;
	return newOffset;
}

void GlobalCommandQueue::UpdateDrawcallBuffers( const GLIndirectBuffer::GLIndirectCommand* commands, const uint32_t* ssbo ) {
	uint ssboOffset = 0;

	uint sync = 0;
	bool useSync = false;
	uint waitPointsOffset = 0;
	uint count = 0;
	GLIndirectBuffer::GLIndirectCommand* cmds = const_cast<GLIndirectBuffer::GLIndirectCommand*>( commands );
	uint32_t* ssboPtr = const_cast< uint32_t* >( ssbo );
	for ( std::set<DrawcallState, DrawcallStateCompare>::iterator dcIt = drawcalls.begin(); dcIt != drawcalls.end(); dcIt++ ) {
		DrawcallState* dc = const_cast< DrawcallState* >( &*dcIt );

		if ( !useSync || dc->syncPoint != sync ) {
			std::unordered_map<uint, uint>::const_iterator countIt = waitPoints.find( dc->syncPoint );
			if ( countIt != waitPoints.end() ) {
				for ( int i = 0; i < waitPoints[dc->syncPoint]; i++ ) {
					DrawcallState& dcS = sequenceDrawcalls[waitPointsOffset];
					const uint newSSBOOffset = UpdateDrawcall( dcS, cmds, ssboPtr, ssboOffset );
					cmds++;
					ssboPtr += newSSBOOffset - ssboOffset;
					ssboOffset = newSSBOOffset;

					waitPointsOffset++;
				}
			}
		}
		useSync = true;
		sync = dc->syncPoint;

		const uint newSSBOOffset = UpdateDrawcall( *dc, cmds, ssboPtr, ssboOffset );
		cmds++;
		ssboPtr += newSSBOOffset - ssboOffset;
		ssboOffset = newSSBOOffset;
	}
}

void GlobalCommandQueue::Render( const uint count, GLuint newProgram, GLuint newIBO ) {
	if ( FBOhandle != currentFBO || first ) {
		FBOhandle = currentFBO;
		GL_fboShim.glBindFramebuffer( GL_FRAMEBUFFER, FBOhandle );
		if ( FBOhandle == 0 ) {
			GL_fboShim.glBindRenderbuffer( GL_RENDERBUFFER, 0 );
		}
		glClearDepthf( 1.0f );
		glDepthMask( true );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	if ( program != newProgram || first ) {
		program = newProgram;
		glUseProgram( program );
	}

	if ( VBOhandle != currentVBO || first ) {
		VBOhandle = currentVBO;
		glBindBuffer( GL_ARRAY_BUFFER, VBOhandle );
	}

	for ( int i = 0; i < ATTR_INDEX_MAX; i++ ) {
		if ( !( vertexFormats[i] == currentVertexFormats[i] ) || first ) {
			vertexFormats[i] = currentVertexFormats[i];
			DrawcallState::VertexFormat& vf = vertexFormats[i];
			glVertexAttribPointer( vf.index, vf.size, vf.type, vf.normalized, vf.stride, vf.offset );
			if ( vf.enabled ) {
				glEnableVertexAttribArray( vf.index );
			} else {
				glDisableVertexAttribArray( vf.index );
			}
		}
	}

	if ( IBOHandle != newIBO || first ) {
		IBOHandle = newIBO;
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, IBOHandle );
	}

	first = false;
	if ( count == 0 ) {
		return;
	}

	glMultiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_INT,
		BUFFER_OFFSET( currentOffset * sizeof( GLIndirectBuffer::GLIndirectCommand ) ), count, 0 );
	currentOffset += count;
}

void GlobalCommandQueue::AddDrawcall( DrawcallState& drawcall ) {
	bool makeResidentFailed = false;
	uint texIndex = 0;
	do {
		makeResidentFailed = tr.textureManager.MakeTextureResident( drawcall.textures[texIndex], false );
		
		if ( makeResidentFailed ) {
			if ( currentCount == 0 ) {
				Sys::Drop( "No texture space available" );
			}

			Render( currentCount, currentProgram, currentIBO );
			tr.textureManager.ClearTextureQueue();
			texIndex = 0;
			continue;
		}

		texIndex++;
	} while ( texIndex < drawcall.currentTexture );

	if ( currentCount == 0 ) {
		currentFBO = drawcall.FBOhandle;
		currentProgram = drawcall.program;
		for ( int i = 0; i < ATTR_INDEX_MAX; i++ ) {
			currentVertexFormats[i] = drawcall.vertexFormats[i];
		}
		currentVBO = drawcall.VBOhandle;
		currentIBO = drawcall.IBOhandle;
	} 

	bool vF = true;
	for ( int i = 0; i < ATTR_INDEX_MAX; i++ ) {
		if ( !( currentVertexFormats[i] == drawcall.vertexFormats[i] ) ) {
			vF = false;
			break;
		}
	}

	if ( currentFBO == drawcall.FBOhandle && currentProgram == drawcall.program && currentIBO == drawcall.IBOhandle
		 && vF ) {
		currentCount++;
	} else {
		Render( currentCount, currentProgram, currentIBO );
		
		tr.textureManager.ClearTextureQueue();
		currentFBO = drawcall.FBOhandle;
		currentProgram = drawcall.program;
		for ( int i = 0; i < ATTR_INDEX_MAX; i++ ) {
			currentVertexFormats[i] = drawcall.vertexFormats[i];
		}
		currentVBO = drawcall.VBOhandle;
		currentIBO = drawcall.IBOhandle;
		currentCount = 1;
	}
}

void GlobalCommandQueue::Execute() {
	if ( std430Size == 0 ) {
		return;
	}

	// Reserve twice as much space for each drawcall to make sure we have enough for padding
	// without doing another loop on all the drawcalls
	std430Size *= 2;

	commandQueueBuffer.BindBuffer();
	glBufferData( GL_DRAW_INDIRECT_BUFFER, drawcallCount * sizeof( GLIndirectBuffer::GLIndirectCommand ), nullptr, GL_DYNAMIC_DRAW );
	const GLIndirectBuffer::GLIndirectCommand* commands = commandQueueBuffer.MapBufferRange( drawcallCount );

	drawcallStateSSBO.BindBuffer();
	glBufferData( GL_SHADER_STORAGE_BUFFER, std430Size * sizeof( uint32_t ), nullptr, GL_DYNAMIC_DRAW );
	const uint32_t* ssbo = drawcallStateSSBO.MapBufferRange( std430Size );

	UpdateDrawcallBuffers( commands, ssbo );

	commandQueueBuffer.UnmapBuffer();
	drawcallStateSSBO.UnmapBuffer();
	drawcallStateSSBO.BindBufferBase();

	GL_CheckErrors();

	first = true;
	currentFBO = 0;
	currentProgram = 0;
	currentVBO = 0;
	currentIBO = 0;

	currentOffset = 0;
	currentCount = 0;

	glDisable( GL_BLEND );
	glEnable( GL_DEPTH_TEST );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	uint sync = 0;
	bool skipSync = true;
	uint waitPointsOffset = 0;
	for ( std::set<DrawcallState, DrawcallStateCompare>::iterator dcIt = drawcalls.begin(); dcIt != drawcalls.end(); dcIt++ ) {
		DrawcallState* dc = const_cast<DrawcallState*>( &*dcIt );

		// SyncPointSequential START
		if ( skipSync || dc->syncPoint != sync ) {
			std::unordered_map<uint, uint>::const_iterator countIt = waitPoints.find( dc->syncPoint );

			if ( countIt != waitPoints.end() ) {
				for ( int i = 0; i < waitPoints[dc->syncPoint]; i++ ) {
					DrawcallState& dcStrict = sequenceDrawcalls[waitPointsOffset];
					AddDrawcall( dcStrict );

					waitPointsOffset++;
				}
			}
		}
		skipSync = false;
		sync = dc->syncPoint;
		// SyncPointSequential END

		AddDrawcall( *dc );

		GL_CheckErrors();
	}

	if ( currentCount > 0 ) {
		Render( currentCount, currentProgram, currentIBO );
		tr.textureManager.ClearTextureQueue();
		GL_CheckErrors();
	}

	// Make sure we don't mess up non-rendering calls, e. g. buffer updates
	glState.currentFBO = nullptr;
	glState.currentVBO = nullptr;
	glState.currentIBO = nullptr;
	glState.currentProgram = nullptr;
}

/* void GlobalCommandQueue::RegisterSyncObject(SyncObject& sync) {
	sync.id = syncObjectCounter;
} */
