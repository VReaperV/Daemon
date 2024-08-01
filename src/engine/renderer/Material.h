/*
===========================================================================

Daemon BSD Source Code
Copyright (c) 2024 Daemon Developers
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
// Material.h

#ifndef MATERIAL_H
#define MATERIAL_H

#include <vector>

#include "gl_shader.h"
#include "tr_local.h"

static constexpr uint32_t MAX_DRAWCOMMAND_TEXTURES = 64;

struct DrawCommand {
	GLIndirectBuffer::GLIndirectCommand cmd;
	uint32_t materialsSSBOOffset = 0;
	uint32_t textureCount = 0;
	Texture* textures[MAX_DRAWCOMMAND_TEXTURES];

	DrawCommand() {
	}

	DrawCommand( const DrawCommand& other ) {
		cmd = other.cmd;
		materialsSSBOOffset = other.materialsSSBOOffset;
		textureCount = other.textureCount;
		memcpy( textures, other.textures, textureCount * sizeof( Texture* ) );
	}
};

struct Material {
	uint32_t materialsSSBOOffset = 0;
	uint32_t staticMaterialsSSBOOffset = 0;
	uint32_t dynamicMaterialsSSBOOffset = 0;
	uint32_t totalDrawSurfCount = 0;
	uint32_t totalStaticDrawSurfCount = 0;
	uint32_t totalDynamicDrawSurfCount = 0;
	uint32_t currentDrawSurfCount = 0;
	uint32_t currentStaticDrawSurfCount = 0;
	uint32_t currentDynamicDrawSurfCount = 0;

	uint32_t globalID = 0;
	uint32_t surfaceCommandBatchOffset = 0;
	uint32_t surfaceCommandBatchCount = 0;
	uint32_t surfaceCommandBatchPadding = 0;

	uint32_t id = 0;
	bool useSync = false;
	uint32_t syncMaterial = 0; // Must not be drawn before the material with this id

	uint32_t stateBits = 0;
	stageType_t stageType;
	GLuint program = 0;
	GLShader* shader;

	int deformIndex;
	bool vertexAnimation;
	bool tcGenEnvironment;
	bool tcGen_Lightmap;
	bool hasDepthFade;
	bool vertexSprite;
	bool alphaTest;

	bool bspSurface;
	bool enableDeluxeMapping;
	bool enableGridLighting;
	bool enableGridDeluxeMapping;
	bool hasHeightMapInNormalMap;
	bool enableReliefMapping;
	bool enableNormalMapping;
	bool enablePhysicalMapping;

	cullType_t cullType;

	bool usePolygonOffset = false;

	VBO_t* vbo;
	IBO_t* ibo;

	std::vector<drawSurf_t*> drawSurfs;
	std::vector<DrawCommand> drawCommands;
	bool texturesResident = false;
	std::vector<Texture*> textures;

	int texturePacks[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

	bool operator==( const Material& other ) {
		for ( uint32_t i = 0; i < 8; i++ ) {
			if ( texturePacks[i] != other.texturePacks[i] ) { // && texturePacks[i] != -1 && other.texturePacks[i] != -1 ) {
				return false;
			}
		}

		return program == other.program && stateBits == other.stateBits && vbo == other.vbo && ibo == other.ibo
			&& cullType == other.cullType && usePolygonOffset == other.usePolygonOffset;
	}

	void AddTexture( Texture* texture ) {
		if ( !texture->hasBindlessHandle ) {
			texture->GenBindlessHandle();
		}

		if ( std::find( textures.begin(), textures.end(), texture ) == textures.end() ) {
			textures.emplace_back( texture );
		}
	}
};

struct PortalSurface {
	vec3_t origin;
	float radius;

	uint32_t drawSurfID;
	float distance;
	vec2_t padding;
};

struct PortalView {
	uint32_t count;
	drawSurf_t* drawSurf;
	uint32_t views[MAX_VIEWS];
};

extern PortalView portalStack[MAX_VIEWS];

#define MAX_SURFACE_COMMANDS 16
#define MAX_COMMAND_COUNTERS 64
#define SURFACE_COMMANDS_PER_BATCH 64

#define MAX_SURFACE_COMMAND_BATCHES 2048

#define BOUNDING_SPHERE_SIZE 4

#define INDIRECT_COMMAND_SIZE 5
#define SURFACE_COMMAND_SIZE 6
#define SURFACE_COMMAND_BATCH_SIZE 4 // Aligned to 4 components
#define PORTAL_SURFACE_SIZE 8

#define MAX_CLUSTERS 65536/24
#define MAX_CLUSTER_TRIANGLES 256
#define MAX_BASE_TRIANGLES MAX_CLUSTERS * MAX_CLUSTER_TRIANGLES
#define MAX_VIEWFRAME_TRIANGLES 524288
#define MAX_FRAME_TRIANGLES 4194304
#define MAX_MATERIALS 128

#define MAX_CLUSTERS_NEW 65536/4

#define MAX_FRAMES 2
#define MAX_VIEWFRAMES MAX_VIEWS * MAX_FRAMES // Buffer 2 frames for each view

struct ViewFrame {
	uint32_t viewID = 0;
	uint32_t portalViews[MAX_VIEWS];
	uint32_t viewCount;
	vec3_t origin;
	frustum_t frustum;
	uint32_t portalSurfaceID;
};

struct Frame {
	uint32_t viewCount = 0;
	ViewFrame viewFrames[MAX_VIEWS];
};

struct BoundingSphere {
	vec3_t origin;
	float radius;
};

struct SurfaceDescriptor {
	BoundingSphere boundingSphere;
	uint32_t surfaceCommandIDs[MAX_SURFACE_COMMANDS] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

struct ClusterData {
	uint baseIndexOffset;
	uint indexOffset;
	uint entityID;
	uint padding;
	BoundingSphere boundingSphere;
	uint materials[MAX_SURFACE_COMMANDS] = {};
};

struct SurfaceCommand {
	uint32_t enabled; // uint because bool in GLSL is always 4 bytes
	GLIndirectBuffer::GLIndirectCommand drawCommand;
};

struct SurfaceCommandBatch {
	uint32_t materialIDs[4] { 0, 0, 0, 0 };
};

struct SurfaceType {
	uint8_t id = 0;
	uint count = 0;
	uint materialIDs[16] = {};

	bool operator==( const SurfaceType& other ) {
		return ( count == other.count ) && !memcmp( materialIDs, other.materialIDs, 16 * sizeof( uint ) );
	}
};

class MaterialSystem {
	public:
	bool generatedWorldCommandBuffer = false;
	bool skipDrawCommands;
	bool generatingWorldCommandBuffer = false;
	vec3_t worldViewBounds[2] = {};

	uint32_t currentView = 0;

	uint8_t maxStages = 0;
	uint32_t descriptorSize;

	std::vector<DrawCommand> drawCommands;

	std::vector<drawSurf_t*> portalSurfacesTmp;
	std::vector<drawSurf_t> portalSurfaces;
	std::vector<PortalSurface> portalBounds;
	uint32_t totalPortals;
	std::vector<shader_t*> skyShaders;

	std::vector<Material*> renderedMaterials;

	/* MaterialPack is an abstraction to match a range of materials with the 3 different calls to RB_RenderDrawSurfaces()
	with 3 different shaderSort_t ranges in RB_RenderView(). The 4th one that uses a different surface filter (DRAWSURFACES_NEAR_ENTITIES)
	is ignored because it's never used for BSP surfaces. */
	struct MaterialPack {
		const shaderSort_t fromSort;
		const shaderSort_t toSort;
		std::vector<Material> materials;
		
		MaterialPack( const shaderSort_t newFromSort, const shaderSort_t newToSort ) :
		fromSort( newFromSort ),
		toSort( newToSort ) {
		}
	};

	MaterialPack materialPacks[3]{
		{ shaderSort_t::SS_DEPTH, shaderSort_t::SS_DEPTH },
		{ shaderSort_t::SS_ENVIRONMENT_FOG, shaderSort_t::SS_OPAQUE },
		{ shaderSort_t::SS_ENVIRONMENT_NOFOG, shaderSort_t::SS_POST_PROCESS }
	};

	bool frameStart = false;

	void AddTexture( Texture* texture );
	void AddDrawCommand( const uint32_t materialID, const uint32_t materialPackID, const uint32_t materialsSSBOOffset,
						 const GLuint count, const GLuint firstIndex );

	void AddPortalSurfaces();
	void RenderMaterials( const shaderSort_t fromSort, const shaderSort_t toSort, const uint32_t viewID );
	void UpdateDynamicSurfaces();

	void QueueSurfaceCull( const uint32_t viewID, const vec3_t origin, const frustum_t* frustum );
	void DepthReduction();
	void CullSurfaces();
	
	void StartFrame();
	void EndFrame();

	void VertexAttribsState( uint32_t stateBits );
	void VertexAttribPointers( uint32_t attribBits );
	uint GetTotalVertexCount();
	void GenerateDrawSurfClusters( drawSurf_t* drawSurf, const uint indexCount, const uint firstIndex,
								   uint8_t* baseClusters, uint32_t* surfaceTypes, uint32_t* clusterData,
								   shaderVertex_t* clusterVertexes, uint32_t* materialIDs, const uint totalVertexCount,
								   uint32_t* clusterIndexes, VBO_t* VBO, IBO_t* IBO );

	void GenerateDepthImages( const int width, const int height, imageParams_t imageParms );

	void AddStageTextures( drawSurf_t* drawSurf, shaderStage_t* pStage, Material* material );
	void ProcessStage( drawSurf_t* drawSurf, shaderStage_t* pStage, shader_t* shader, uint* packIDs, uint& stage,
		uint& previousMaterialID );
	void GenerateWorldMaterials();
	void GenerateWorldMaterialsBuffer();
	void GenerateWorldCommandBuffer();
	void GeneratePortalBoundingSpheres();

	void AddAllWorldSurfaces();

	void Free();

	private:
	bool PVSLocked = false;
	frustum_t lockedFrustum;
	image_t* lockedDepthImage;
	matrix_t lockedViewMatrix;

	uint32_t viewCount;

	image_t* depthImage;
	int depthImageLevels;

	uint surfaceTypeLast = 0;
	std::vector<SurfaceType> clusterSurfaceTypes;
	uint currentBaseClusterIndex = 0;
	uint currentBaseCluster = 0;
	uint currentGlobalCluster = 0;
	uint currentClusterVertex = 0;

	uint clusterCount = 0;
	uint clusterTriangles = 0;
	uint totalTriangles = 0;

	uint totalMaterialCount;
	vboAttributeLayout_t clusterVertexLayout[ATTR_INDEX_MAX];

	DrawCommand cmd;
	uint32_t lastCommandID;
	uint32_t totalDrawSurfs;
	uint32_t totalBatchCount = 0;

	uint32_t surfaceCommandsCount = 0;
	uint32_t culledCommandsCount = 0;
	uint32_t surfaceDescriptorsCount = 0;

	std::vector<drawSurf_t> dynamicDrawSurfs;
	uint32_t dynamicDrawSurfsOffset = 0;
	uint32_t dynamicDrawSurfsSize = 0;

	Frame frames[MAX_FRAMES];
	uint32_t currentFrame = 0;
	uint32_t nextFrame = 1;

	bool AddPortalSurface( uint32_t viewID, PortalSurface* portalSurfs );
	uint32_t* clusterTest;

	void RenderMaterial( Material& material, const uint32_t viewID );
	void UpdateFrameData();
};

extern GLSSBO materialsSSBO;
extern GLSSBO surfaceDescriptorsSSBO; // Global
extern GLSSBO surfaceCommandsSSBO; // Per viewframe, GPU updated
extern GLBuffer culledCommandsBuffer; // Per viewframe
extern GLUBO surfaceBatchesUBO; // Global
extern GLBuffer atomicCommandCountersBuffer; // Per viewframe
extern GLSSBO portalSurfacesSSBO; // Per viewframe

extern GLBuffer drawCommandBuffer; // Per viewframe
extern GLSSBO clusterIndexesBuffer; // Global
extern GLBuffer globalIndexesSSBO; // Per viewframe
extern GLBuffer materialIDsSSBO; // Per viewframe

extern GLBuffer clustersUBO; // Global
extern GLUBO clusterSurfaceTypesUBO; // Global
extern GLSSBO clusterDataSSBO; // Global
extern GLBuffer culledClustersBuffer; // Per viewframe
extern GLBuffer atomicMaterialCountersBuffer; // Per viewframe
extern GLBuffer atomicMaterialCountersBuffer2; // Per viewframe
extern GLBuffer clusterCountersBuffer; // Per viewframe | Per frame (transformed vertices counters)
extern GLBuffer clusterWorkgroupCountersBuffer; // Per viewframe

extern GLBuffer clusterVertexesBuffer; // Per frame

extern GLSSBO debugSSBO; // Global

extern MaterialSystem materialSystem;

#endif // MATERIAL_H
