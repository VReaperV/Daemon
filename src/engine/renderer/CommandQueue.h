#pragma once

#include <vector>

#include "gl_shader.h"

/* class DrawcallStateSSBO : GLSSBO {
	private:

}; */

/* struct GLIndirectCommand {
	GLuint count;
	GLuint instanceCount;
	GLuint firstIndex;
	GLint baseVertex;
	GLuint baseInstance;
}; */

class DrawcallState {
	public:
	GLuint std430size;
	uint32_t* uniformValues;
	uint currentTexture = 0;
	uint textureCount = 0;
	Texture** textures;
	GLIndirectBuffer::GLIndirectCommand renderingCommand;
	GLShader* shader;
	
	GLuint FBOhandle = 0; // We only bind to GL_FRAMEBUFFER, so we don't care about the target
	GLuint program = 0;
	// uint32_t vertexBindingState = 0;
	struct VertexFormat {
		bool enabled = false;
		GLuint index = 0;
		GLint size = 0;
		GLenum type = GL_FLOAT;
		GLboolean normalized = false;
		GLsizei stride = 0;
		void* offset = nullptr;
	} vertexFormats[ATTR_INDEX_MAX];
	GLuint VBOhandle = 0;
	GLuint IBOhandle = 0;

	uint id;
	bool syncSequential = false;
	uint previousDrawcall;
	uint syncPoint;

	DrawcallState();
	DrawcallState( const DrawcallState& other );
	DrawcallState& operator=( const DrawcallState& other );
	~DrawcallState();

	GLuint SetShader( GLShader *newShader, uint32_t* uniformValuesHunk ) {
		shader = newShader;
		std430size = shader->GetSTD430Size();
		if( shaderSet ) {
			// delete[] uniformValues;
			delete[] textures;
		}
		shaderSet = true;
		currentTexture = 0;
		textureCount = shader->GetTextureCount();
		// uniformValues = new uint32_t[shader->GetSTD430Size()];
		uniformValues = uniformValuesHunk;
		textures = new Texture*[textureCount];
		return shader->GetSTD430Size();
	}

	void AddTexture( Texture* texture ) {
		if ( currentTexture >= textureCount ) {
			Sys::Drop( "Malformed shader image binds" );
		}

		textures[currentTexture] = texture;
		currentTexture++;
	}

	void SetUniforms() {
		shader->WriteUniformsToBuffer( uniformValues );
	}

	GLuint GetInfo() {
		return shader->GetSTD430Size();
	}

	bool operator() ( DrawcallState& lhs, DrawcallState& rhs ) {
		if ( lhs.syncSequential && lhs.id == rhs.previousDrawcall ) {
			return false;
		}

		if ( lhs.syncPoint < rhs.syncPoint ) {
			return true;
		}

		if ( lhs.IBOhandle < rhs.IBOhandle ) {
			return true;
		}

		if ( lhs.program < rhs.program ) {
			return true;
		}

		// GlobalState

		return false;
	}

	private:
	bool shaderSet = false;
	// std::vector<GLUniformBlock *> uniformBlocks;
	// std::vector<Texture *> textures;
	// std::vector<GLIndirectBuffer::GLIndirectCommand> renderingCommands;
};

bool operator==( const DrawcallState::VertexFormat& lhs, const DrawcallState::VertexFormat& rhs );

struct DrawcallStateCompare {
	bool operator() ( const DrawcallState& lhs, const DrawcallState& rhs ) const {
		// In (presumably) descending order of state change cost
		// (Except for sync points of course)

		if ( rhs.shader == gl_generic2DShader ) {
			// return true;
		}

		if ( rhs.shader == gl_cameraEffectsShader ) { // TEMPORARY
			return true;
		}

		if ( lhs.syncSequential && lhs.id == rhs.previousDrawcall ) {
			return true;
		}

		if ( lhs.syncPoint < rhs.syncPoint ) {
			return true;
		} else if ( lhs.syncPoint > rhs.syncPoint ) {
			return false;
		}

		if ( lhs.FBOhandle < rhs.FBOhandle ) {
			return true;
		} else if ( lhs.FBOhandle > rhs.FBOhandle ) {
			return false;
		}

		if ( lhs.program < rhs.program ) {
			return true;
		} else if ( lhs.program > rhs.program ) {
			return false;
		}

		for ( int i = 0; i < ATTR_INDEX_MAX; i++ ) {
			if ( !( lhs.vertexFormats[i] == rhs.vertexFormats[i] ) ) {
				return lhs.id < rhs.id;
			}
		}

		// We don't use the same IBO with multiple different VBOs so only need to check the IBO handles
		if ( lhs.IBOhandle < rhs.IBOhandle ) {
			return true;
		} else if ( lhs.IBOhandle > rhs.IBOhandle ) {
			return false;
		}

		// Just in case we do at some point
		ASSERT_EQ( lhs.VBOhandle, rhs.VBOhandle );

		// GlobalState

		if ( /* !lhs.syncSequential && */ lhs.syncPoint > rhs.syncPoint || lhs.IBOhandle > rhs.IBOhandle || lhs.program > rhs.program ) {
			return false;
		}

		// Drawcall id doesn't matetr at this point, but comparator has to have strict weak ordering
		return lhs.id < rhs.id;
	}
};

class CommandQueue {
};

class GlobalCommandQueue;

/* class SyncObject {
	public:
	SyncObject() : id( globalCommandQueue.GetSyncCounter() ) {
	}

	~SyncObject() = default;

	uint GetID() const {
		return id;
	}

	private:
	const uint id;
	// public:
	// friend void GlobalCommandQueue::RegisterSyncObject( SyncObject& sync );
}; */

class GlobalCommandQueue {
	public:
	// An estimate for max allowed drawcalls, 256 uniforms per drawcall for skeletal animation, with indexCapacity of
	// 1048576 this results in about 35k allowed drawcalls, ~34Mb of memory
	// Dividing by 3 because most primitives will be triangles, dividing by 10 to bring down memory requirements
	static const uint MAX_UNIFORMS = 256 * ( DYN_BUFFER_SIZE / sizeof( glIndex_t ) / 3 / 10 );

	GlobalCommandQueue() {
		// drawcalls.reserve( 1000 );
	}

	void SetDrawcallShader( GLShader* shader ) {
		currentUniformPtr += currentDrawcall.SetShader( shader, currentUniformPtr );
		if ( currentUniformPtr - uniformValues > MAX_UNIFORMS ) {
			Sys::Drop( "GlobalCommandQueue out of memory" );
		}
	}

	void SetDrawcallUniforms() {
		currentDrawcall.SetUniforms();
	}

	void RegisterDrawcall( const GLuint count, const GLuint firstIndex ) {
		drawcallCount++;
		std430Size += currentDrawcall.std430size;

		if ( syncSequentialStrictStarted ) {
			currentBatchSize++;
			currentDrawcall.renderingCommand.count = count;
			currentDrawcall.renderingCommand.instanceCount = 1;
			currentDrawcall.renderingCommand.firstIndex = firstIndex;
			currentDrawcall.renderingCommand.baseVertex = 0;
			currentDrawcall.renderingCommand.baseInstance = 0;
			sequenceDrawcalls.push_back( currentDrawcall );
			return;
		}

		if ( syncSequentialStarted ) {
			if ( syncSequentialFirst ) {
				currentDrawcall.previousDrawcall = currentDrawcallID - 1;
			} else {
				syncSequentialFirst = true;
			}
			currentDrawcall.syncSequential = true;
		}

		currentDrawcall.syncPoint = currentSync;
		currentDrawcall.id = currentDrawcallID;

		currentDrawcall.renderingCommand.count = count;
		currentDrawcall.renderingCommand.instanceCount = 1;
		currentDrawcall.renderingCommand.firstIndex = firstIndex;
		currentDrawcall.renderingCommand.baseVertex = 0;
		currentDrawcall.renderingCommand.baseInstance = 0;
		drawcalls.emplace( currentDrawcall );

		currentDrawcallID++;
		// Execute();
		// Clear();
	}

	void RegisterFBO( const GLuint handle ) {
		currentDrawcall.FBOhandle = handle;
	}

	void RegisterVertexBindingState( const bool enabled, const GLuint index, const GLint size, const GLenum type,
									 const GLboolean normalized, const GLsizei stride, const void* offset ) {
		currentDrawcall.vertexFormats[index].enabled = enabled;
		currentDrawcall.vertexFormats[index].index = index;
		currentDrawcall.vertexFormats[index].size = size;
		currentDrawcall.vertexFormats[index].type = type;
		currentDrawcall.vertexFormats[index].normalized = normalized;
		currentDrawcall.vertexFormats[index].stride = stride;
		currentDrawcall.vertexFormats[index].offset = const_cast<void*>(offset);
	}

	void RegisterVBO( const GLuint handle ) {
		currentDrawcall.VBOhandle = handle;
	}

	void RegisterIBO( const GLuint handle ) {
		currentDrawcall.IBOhandle = handle;
	}

	void RegisterProgram( const GLuint program ) {
		currentDrawcall.program = program;
	}

	void RegisterTexture( Texture* texture ) {
		currentDrawcall.AddTexture( texture );
	}

	void Render( const uint count, GLuint newProgram, GLuint newIBO );
	uint UpdateDrawcall( DrawcallState& drawcall, GLIndirectBuffer::GLIndirectCommand* commands, uint32_t* ssbo,
						 const uint ssboOffset );
	void UpdateDrawcallBuffers( const GLIndirectBuffer::GLIndirectCommand* commands, const uint32_t* ssbo );
	void Execute();

	void Init() {
		uniformValues = ( uint32_t* ) ri.Hunk_Alloc( MAX_UNIFORMS * sizeof( uint32_t ), ha_pref::h_high );
		currentUniformPtr = uniformValues;
	}

	void Clear() {
		drawcalls.clear();
		sequenceDrawcalls.clear();
		waitPoints.clear();
		currentUniformPtr = uniformValues;

		currentDrawcallID = 0;
		currentSync = 0;
		currentBatchSize = 0;
		drawcallCount = 0;
		std430Size = 0;
		syncSequentialFirst = false;
		syncSequentialStarted = false;
		syncSequentialStrictWaitPoint = 0;
		syncSequentialStrictStarted = false;
	}

	void PrintAll() {
		struct ShaderInfo {
			uint count = 0;
			uint size = 0;
		};
		std::unordered_map<const GLShader*, ShaderInfo> shaders;
		for ( std::set<DrawcallState, DrawcallStateCompare>::iterator dc = drawcalls.begin(); dc != drawcalls.end(); dc++ ) {
			// Log::Warn( "dc: %u", const_cast<DrawcallState*>(&*dc)->GetInfo());
			/* if ( shaders.find(const_cast< DrawcallState* >( &*dc )->shader) == shaders.end() ) {
				shaders[const_cast< DrawcallState* >( &*dc )->shader].count = 1;
				shaders[const_cast< DrawcallState* >( &*dc )->shader].size += const_cast< DrawcallState* >( &*dc )->shader->GetSTD430Size();
			} */
			shaders[const_cast< DrawcallState* >( &*dc )->shader].count++;
			shaders[const_cast< DrawcallState* >( &*dc )->shader].size += const_cast< DrawcallState* >( &*dc )->shader->GetSTD430Size();
		}
		for ( const std::pair<const GLShader*, ShaderInfo>& info : shaders ) {
			Log::Warn( "Shader: %s, count: %u, size: %u", info.first->GetName(), info.second.count, info.second.size );
		}
	}

	void SyncWait() {
		// waitPoints.push_back( currentDrawcallID - lastDrawcallID );
		// lastDrawcallID = currentDrawcallID;

		if ( syncSequentialStarted ) {
			Sys::Drop( "SyncWait in a SyncPointSequential is not allowed" );
		}
		if ( syncSequentialStrictStarted ) {
			Sys::Drop( "SyncWait in a SyncPointSequentialStrict is not allowed" );
		}

		currentSync++;
	}

	void SyncPointSequential() {
		if ( syncSequentialStrictStarted ) {
			Sys::Drop( "SyncPointSequential in a SyncPointSequentialStrict is not allowed" );
		}

		if ( syncSequentialStarted ) {
			syncSequentialStarted = false;
			syncSequentialFirst = false;
			currentDrawcall.syncSequential = false;
			return;
		}

		syncSequentialStarted = true;
		// currentSync = GetSyncCounter();
	}

	void SyncPointSequentialStrict() {
		if ( syncSequentialStarted ) {
			Sys::Drop( "SyncPointSequentialStrict in a SyncPointSequential is not allowed" );
		}

		if ( syncSequentialStrictStarted ) {
			syncSequentialStrictStarted = false;
			waitPoints[currentSync] = currentBatchSize;
			currentBatchSize = 0;
			return;
		}

		syncSequentialStrictStarted = true;
		syncSequentialStrictWaitPoint = currentSync;
		// currentSync = GetSyncCounter();
	}

	private:
	uint32_t *uniformValues;
	uint32_t *currentUniformPtr;

	uint currentDrawcallID = 0;
	uint currentSync = 0;
	uint currentBatchSize = 0;
	uint drawcallCount = 0;
	GLuint std430Size = 0;
	bool syncSequentialFirst = false;
	bool syncSequentialStarted = false;
	uint syncSequentialStrictWaitPoint = 0;
	bool syncSequentialStrictStarted = false;

	bool first = true;
	GLuint FBOhandle = 0;
	GLuint program = 0;
	DrawcallState::VertexFormat vertexFormats[ATTR_INDEX_MAX];
	GLuint VBOhandle = 0;
	GLuint IBOHandle = 0;

	uint currentOffset = 0;
	uint currentCount = 0;

	GLuint currentFBO = 0;
	GLuint currentProgram = 0;
	DrawcallState::VertexFormat currentVertexFormats[ATTR_INDEX_MAX];
	GLuint currentVBO = 0;
	GLuint currentIBO = 0;

	DrawcallState currentDrawcall;
	std::set<DrawcallState, DrawcallStateCompare> drawcalls;
	std::vector<DrawcallState> sequenceDrawcalls;
	std::unordered_map<uint, uint> waitPoints; // Contains the amount of drawcalls before each SyncWait

	uint syncObjectCounter = 0;

	void AddDrawcall( DrawcallState& drawcall );

	uint GetSyncCounter() {
		syncObjectCounter++;
		return syncObjectCounter - 1;
	}

	// void RegisterSyncObject( SyncObject& sync );
};

extern GLIndirectBuffer commandQueueBuffer;
extern GLSSBO drawcallStateSSBO;
extern GlobalCommandQueue globalCommandQueue;
