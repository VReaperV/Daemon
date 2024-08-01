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

/* processSurfaces_cp.glsl */

// Keep this to 64 because we don't want extra shared mem etc. to be allocated, and to minimize wasted lanes
layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct GLIndirectCommand {
	uint count;
	uint instanceCount;
	uint firstIndex;
	int baseVertex;
	uint baseInstance;
};

struct SurfaceCommand {
	bool enabled;
	GLIndirectCommand drawCommand;
};

struct SurfaceCommandBatch {
	uvec2 materialIDs[2];
};

layout(std430, binding = 2) readonly restrict buffer surfaceCommandsSSBO {
	SurfaceCommand surfaceCommands[];
};

layout(std430, binding = 3) writeonly restrict buffer culledCommandsSSBO {
	GLIndirectCommand culledCommands[];
};

layout(std140, binding = 0) uniform ub_SurfaceBatches {
	SurfaceCommandBatch surfaceBatches[MAX_SURFACE_COMMAND_BATCHES];
};

#define MAX_CLUSTERS 65536/24

layout(std430, binding = 7) readonly restrict buffer clusterIndexesSSBO {
    int clusterIndexes[];
};

layout(std430, binding = 8) writeonly restrict buffer globalIndexesSSBO {
    int globalIndexes[];
};

struct sVertex{
    vec3 position;
    vec4 pad;
    float pad2;
};

layout(std430, binding = 12) readonly restrict buffer clusterVertexesSSBO {
    sVertex clusterVertexes[];
};

layout(std430, binding = 9) writeonly restrict buffer materialIDsSSBO {
    uint materialIDs[];
};

struct ClusterData {
    uint baseIndexOffset;
    uint indexOffset;
    uint entityID;
    uint padding;
    float x;
    float y;
    float z;
    float radius;
    uint materials[MAX_SURFACE_COMMANDS];
};

layout(std430, binding = 10) readonly restrict buffer clusterDataSSBO {
    ClusterData clusterData[];
};

layout(std140, binding = 1) uniform ub_Clusters {
    uvec4 clusters[MAX_CLUSTERS];
};

layout(std140, binding = 2) uniform ub_ClusterSurfaceTypes {
    uvec4 clusterSurfaceTypes[MAX_CLUSTERS];
};

layout(std140, binding = 3) uniform ub_CulledClusters {
    uvec4 culledClusters[MAX_CLUSTERS];
};

layout(std140, binding = 4) uniform ub_ClusterCounters {
    uvec4 clusterCounters[( MAX_VIEWS * MAX_FRAMES + MAX_FRAMES ) / 4 + 1];
};

#define MAX_MATERIALS 128

layout(std140, binding = 5) uniform ub_MaterialCounters {
    uvec4 materialCounters[( MAX_MATERIALS * MAX_VIEWS * MAX_FRAMES ) / 4 + 1];
};

layout (binding = 2) uniform atomic_uint atomicMaterialCounters2[MAX_MATERIALS * MAX_VIEWS * MAX_FRAMES];
layout (binding = 4) uniform atomic_uint atomicCommandCounters[MAX_COMMAND_COUNTERS * MAX_VIEWS * MAX_FRAMES];

layout(std430, binding = 13) writeonly restrict buffer debugSSBO {
    uvec4 debugSurfaces[];
};

uniform uint u_Frame;
uniform uint u_ViewID;
uniform vec3 u_CameraPosition;
uniform uint u_MaxViewFrameTriangles;
uniform uint u_SurfaceCommandsOffset;
uniform uint u_CulledCommandsOffset;

void AddDrawCommand( in uint commandID, in uvec2 materialID ) {
	SurfaceCommand command = surfaceCommands[commandID + u_SurfaceCommandsOffset];
	if( command.enabled ) {
		// materialID.x is the global ID of the material
		// materialID.y is the offset for the memory allocated to the material's culled commands
		const uint atomicCmdID = atomicCounterIncrement( atomicCommandCounters[materialID.x
		                                                 + MAX_COMMAND_COUNTERS * ( MAX_VIEWS * u_Frame + u_ViewID )] );
		culledCommands[atomicCmdID + materialID.y * MAX_COMMAND_COUNTERS + u_CulledCommandsOffset] = command.drawCommand;
	}
}


uvec2 uintIDToUvec4( const in uint id ) {
    return uvec2( id / 4, id % 4 );
}

struct Cluster {
    uint triangleCount;
    // uint materialCount;
    uint indexOffset;
    uint surfaceTypeID;
};

/*                  sfTypeID|sfTypeID|sfTypeID|sfTypeID
   clusters - uint [________|________|________|________] */
Cluster UnpackCluster( const in uint clusterID ) {
    Cluster cluster;

    uvec2 clusterIDs = uintIDToUvec4( clusterID / 4 );
    uint surfaceTypeID = clusters[clusterIDs.x][clusterIDs.y];

    cluster.surfaceTypeID = ( surfaceTypeID >> (
                                                 ( 3 - ( clusterID % 4 ) ) * 8
                                               ) ) & 0x7F;
    // cluster.indexOffset = clusterData[clusterID * 3];
    ClusterData data = clusterData[clusterID];
    // cluster.indexOffset = cluster.indexOffset & 0xFFFFFF;
    cluster.indexOffset = data.baseIndexOffset;
    // cluster.triangleCount = ( cluster.indexOffset >> 24 ) + 1; // Clusters have at least 1 triangle, so we can save 1 bit
    cluster.triangleCount = ( ( data.entityID >> 22 ) & 0xFF ) + 1; // Clusters have at least 1 triangle, so we can save 1 bit

    // cluster.indexOffset = cluster.indexOffset & 0xFFFFFF;
    return cluster;
}

void CopyClusterTriangles( const in uint clusterBase, const in uint clusterOffset, const in uint globalID, const in uint triangleCount,
                           const in uint triangleMaterialID ) {
    const uint globalInvocationID = gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                             * gl_NumWorkGroups.y * gl_WorkGroupSize.y
                             + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                             + gl_GlobalInvocationID.x
                             + 1;
    for( uint i = 0; i < triangleCount; i++ ) {
        // globalIndexes[globalID + ( u_Frame * MAX_VIEWS + u_ViewID ) * u_MaxViewFrameTriangles + i] = clusterIndexes[clusterBase + i]
        //                                                                            + ivec3( clusterOffset, clusterOffset, clusterOffset );
        const ivec3 triangle = ivec3( clusterIndexes[clusterBase + i * 3],
                                      clusterIndexes[clusterBase + i * 3 + 1],
                                      clusterIndexes[clusterBase + i * 3 + 2] ) + ivec3( clusterOffset, clusterOffset, clusterOffset );
        /* sVertex vert0 = clusterVertexes[triangle.x];
        sVertex vert1 = clusterVertexes[triangle.y];
        sVertex vert2 = clusterVertexes[triangle.z];
        vec3 normal = cross( vert2.position - vert0.position, vert1.position - vert0.position );
        if( dot( u_CameraPosition - vert0.position, normal ) >= 0 ) {
            continue;
        } */

        globalIndexes[( globalID + ( u_Frame * MAX_VIEWS + u_ViewID ) * u_MaxViewFrameTriangles ) * 3 + i * 3] = triangle.x;
        globalIndexes[( globalID + ( u_Frame * MAX_VIEWS + u_ViewID ) * u_MaxViewFrameTriangles ) * 3 + i * 3 + 1] = triangle.y;
        globalIndexes[( globalID + ( u_Frame * MAX_VIEWS + u_ViewID ) * u_MaxViewFrameTriangles ) * 3 + i * 3 + 2] = triangle.z;
        /* materialIDs[( globalID + ( u_Frame * MAX_VIEWS + u_ViewID ) * u_MaxViewFrameTriangles ) * 3 + i * 3] = triangleMaterialID;
        materialIDs[( globalID + ( u_Frame * MAX_VIEWS + u_ViewID ) * u_MaxViewFrameTriangles ) * 3 + i * 3 + 1] = triangleMaterialID;
        materialIDs[( globalID + ( u_Frame * MAX_VIEWS + u_ViewID ) * u_MaxViewFrameTriangles ) * 3 + i * 3 + 2] = triangleMaterialID; */
    }
}

void ProcessCluster( const in Cluster cluster, const in uint clusterOffset, const in ClusterData data ) {
    uvec2 clusterIDs = uintIDToUvec4( cluster.surfaceTypeID );
    const uint materialCount = clusterSurfaceTypes[clusterIDs.x][clusterIDs.y];
    const uint globalInvocationID = gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                             * gl_NumWorkGroups.y * gl_WorkGroupSize.y
                             + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                             + gl_GlobalInvocationID.x;
    for( uint i = 1; i <= materialCount; i++ ) {
        clusterIDs = uintIDToUvec4( cluster.surfaceTypeID + i );
        const uint materialID = clusterSurfaceTypes[clusterIDs.x][clusterIDs.y];

        uint globalIndexOffset = 0;
        for( uint j = 0; j < materialID; j++ ) {
            clusterIDs = uintIDToUvec4( j + ( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_MATERIALS );
            globalIndexOffset += materialCounters[clusterIDs.x][clusterIDs.y];
        }

        const uint atomicTriangleID = atomicCounterAddARB( atomicMaterialCounters2[materialID
                                                           + ( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_MATERIALS],
                                                           cluster.triangleCount );

        // const uint atomicTriangleID = atomicCounterAddARB( atomicClusterCounters[MAX_VIEWS * u_Frame + u_ViewID],
        //                                                    cluster.triangleCount );
        // debugSurfaces[globalInvocationID * 5 + 3][i - 1] = data.materials[i - 1];
        CopyClusterTriangles( cluster.indexOffset, clusterOffset + data.materials[i - 1], atomicTriangleID + globalIndexOffset,
                              cluster.triangleCount, data.materials[i - 1] );
    }
}

void main() {
    const uint globalGroupID = gl_WorkGroupID.z * gl_NumWorkGroups.x * gl_NumWorkGroups.y
                             + gl_WorkGroupID.y * gl_NumWorkGroups.x
                             + gl_WorkGroupID.x;
    const uint globalInvocationID = gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                             * gl_NumWorkGroups.y * gl_WorkGroupSize.y
                             + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                             + gl_GlobalInvocationID.x;
    // const uvec2 materialID = surfaceBatches[globalGroupID / 2].materialIDs[globalGroupID % 2];

    // AddDrawCommand( globalInvocationID, materialID );
    
    uvec2 clusterIDs = uintIDToUvec4( ( MAX_VIEWS + 1 ) * u_Frame + u_ViewID );
    uint clusterID = clusterCounters[clusterIDs.x][clusterIDs.y];
    if( globalInvocationID >= clusterID ) {
        return;
    }

    // clusterIDs = uintIDToUvec4( globalInvocationID / 2 );
    clusterIDs = uintIDToUvec4( globalInvocationID );
    // clusterID = ( globalInvocationID & 1 ) == 1 ? culledClusters[clusterIDs.x][clusterIDs.y] & 0xFFFF
    //                                             : culledClusters[clusterIDs.x][clusterIDs.y] >> 16;
    clusterID = culledClusters[clusterIDs.x][clusterIDs.y];
    Cluster cluster = UnpackCluster( clusterID );
    // debugSurfaces[globalInvocationID * 5 + 4] = uvec4( globalInvocationID, cluster.triangleCount, cluster.indexOffset, cluster.surfaceTypeID );
    const ClusterData data = clusterData[clusterID];
    ProcessCluster( cluster, 0, data );
}
