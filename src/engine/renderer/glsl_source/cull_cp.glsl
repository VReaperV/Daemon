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

/* cull_cp.glsl */

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D depthImage;

struct BoundingSphere {
    vec3 center;
    float radius;
};

struct SurfaceDescriptor {
    BoundingSphere boundingSphere;
    uint surfaceCommandIDs[MAX_SURFACE_COMMANDS];
};

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

layout(std430, binding = 1) readonly restrict buffer surfaceDescriptorsSSBO {
    SurfaceDescriptor surfaces[];
};

layout(std430, binding = 2) writeonly restrict buffer surfaceCommandsSSBO {
    SurfaceCommand surfaceCommands[];
};

layout(std430, binding = 5) writeonly restrict buffer debugSSBO {
    uvec4 debugSurfaces[];
};


layout(std430, binding = 7) readonly restrict buffer clusterIndexesSSBO {
    int clusterIndexes[];
};

layout(std430, binding = 8) writeonly restrict buffer globalIndexesSSBO {
    int globalIndexes[];
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

layout(std430, binding = 11) coherent buffer culledClustersBuffer {
    uint culledClusters[];
};

layout(std430, binding = 12) writeonly restrict buffer clustersVisibilitySSBO {
    uint clustersVisibility[];
};

#define MAX_CLUSTERS 65536/24

layout(std140, binding = 1) uniform ub_Clusters {
    uvec4 clusters[MAX_CLUSTERS];
};

layout(std140, binding = 2) uniform ub_ClusterSurfaceTypes {
    uvec4 clusterSurfaceTypes[MAX_CLUSTERS];
};

#define MAX_MATERIALS 128

layout (binding = 1) uniform atomic_uint atomicMaterialCounters[MAX_MATERIALS * MAX_VIEWS * MAX_FRAMES];
layout (binding = 3) uniform atomic_uint clusterCounters[MAX_VIEWS * MAX_FRAMES + MAX_FRAMES];
layout (binding = 5) uniform atomic_uint clusterWorkgroupCounters[MAX_VIEWS * MAX_FRAMES * 3];

uniform uint u_Frame;
uniform uint u_ViewID;
uniform uint u_MaxViewFrameTriangles;

uniform uint u_TotalDrawSurfs;
uniform uint u_SurfaceCommandsOffset;
uniform vec4 u_Frustum[6]; // xyz - normal, w - distance
uniform bool u_UseFrustumCulling;
uniform vec3 u_CameraPosition;
uniform mat4 u_ModelViewMatrix;
uniform mat4 u_ModelViewProjectionMatrix;
uniform uint u_ViewWidth;
uniform uint u_ViewHeight;
uniform float u_P00;
uniform float u_P11;

bool ProjectSphere( in vec3 center, in float radius, in float zNear, in float P00, in float P11, inout vec4 boundingBox, in uint debugID ) {
    if ( -center.z - radius < zNear ) {
		return false;
    }

	vec3 cr = center * radius;
	float czr2 = center.z * center.z - radius * radius;

	float vx = sqrt( center.x * center.x + czr2 );
	float minx = center.x * center.x + czr2 >= 0.0 ? ( vx * center.x - cr.z ) / ( vx * center.z + cr.x ) : -1.0;
	float maxx = center.x * center.x + czr2 >= 0.0 ? ( vx * center.x + cr.z ) / ( vx * center.z - cr.x ) : 1.0;

	float vy = sqrt( center.y * center.y + czr2 );
	float miny = center.y * center.y + czr2 >= 0.0 ? ( vy * center.y - cr.z ) / ( vy * center.z + cr.y ) : -1.0;
	float maxy = center.y * center.y + czr2 >= 0.0 ? ( vy * center.y + cr.z ) / ( vy * center.z - cr.y ) : 1.0;

	boundingBox = vec4( minx * P00, miny * P11, maxx * P00, maxy * P11 );
	boundingBox = boundingBox.xwzy * vec4( 0.5f, -0.5f, 0.5f, -0.5f ) + vec4( 0.5, 0.5, 0.5, 0.5 ); // clip space -> uv space

	return true;
}

bool CullSurface( in BoundingSphere boundingSphere ) {
    bool culled = false;

    for( int i = 0; i < 5; i++ ) { // Skip far plane for now because we always have it set to { 0, 0, 0, 0 } for some reason
        const float distance = dot( u_Frustum[i].xyz, boundingSphere.center ) - u_Frustum[i].w;

        if( distance < -boundingSphere.radius ) {
            culled = u_UseFrustumCulling;
        }
    }
    
    const uint debugID = gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_WorkGroupSize.x * gl_NumWorkGroups.y * gl_WorkGroupSize.y
                            + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                            + gl_GlobalInvocationID.x;
    vec4 boundingBox = vec4( -1.0, 1.0, -1.0, 1.0 );
    const vec3 viewSpaceCenter = vec3( u_ModelViewMatrix * vec4( boundingSphere.center, 1.0 ) );
    
    // Only do occlusion culling on the main view because portals use stencil buffer,
    // so doing depth reduction there would be complicated
    if( u_ViewID == 0 && !culled && ProjectSphere( viewSpaceCenter, boundingSphere.radius, 3.0, u_P00, u_P11, boundingBox, debugID ) ) {
        boundingBox.xz = vec2( min( 1 - boundingBox.x, 1 - boundingBox.z ), max( 1 - boundingBox.x, 1 - boundingBox.z ) );
        const float width = clamp( ( boundingBox.z - boundingBox.x ) * float( u_ViewWidth ), 0.0, float( u_ViewWidth ) );
		const float height = clamp( ( boundingBox.w - boundingBox.y ) * float( u_ViewHeight ), 0.0, float( u_ViewHeight ) );

        const float level = floor( log2( max( width, height ) ) );
        const int levelInt = int( level );

        vec2 surfaceCoords = vec2( u_ViewWidth >> levelInt, u_ViewHeight >> levelInt );
        surfaceCoords *= ( boundingBox.xy + boundingBox.zw ) * 0.5;
        const ivec2 surfaceCoordsFloor = ivec2( clamp( surfaceCoords.x, 0.0, float( u_ViewWidth >> levelInt ) ), clamp( surfaceCoords.y, 0.0, float( u_ViewHeight >> levelInt ) ) );
        ivec4 depthCoords = ivec4( surfaceCoordsFloor.xy,
                                   surfaceCoordsFloor.x + ( surfaceCoords.x - surfaceCoordsFloor.x >= 0.5 ? 1 : -1 ),
                                   surfaceCoordsFloor.y + ( surfaceCoords.y - surfaceCoordsFloor.y >= 0.5 ? 1 : -1 ) );

        depthCoords.x = clamp( depthCoords.x, 0, int( ( u_ViewWidth >> levelInt ) - 1 ) );
        depthCoords.y = clamp( depthCoords.y, 0, int( ( u_ViewHeight >> levelInt ) - 1 ) );
        depthCoords.z = clamp( depthCoords.z, 0, int( ( u_ViewWidth >> levelInt ) - 1 ) );
        depthCoords.w = clamp( depthCoords.w, 0, int( ( u_ViewHeight >> levelInt ) - 1 ) );

        vec4 depthValues;
        depthValues.x = texelFetch( depthImage, depthCoords.xy, levelInt ).r;
        depthValues.y = texelFetch( depthImage, depthCoords.zy, levelInt ).r;
        depthValues.z = texelFetch( depthImage, depthCoords.xw, levelInt ).r;
        depthValues.w = texelFetch( depthImage, depthCoords.zw, levelInt ).r;

        const float surfaceDepth = max( max( max( depthValues.x, depthValues.y ), depthValues.z ), depthValues.w );

        culled = ( 1 + 3.0 / ( viewSpaceCenter.z + boundingSphere.radius ) ) > surfaceDepth;
    }

    return culled;
}

void ProcessSurfaceCommands( const in SurfaceDescriptor surface, const in bool enabled ) {
    for( uint i = 0; i < MAX_SURFACE_COMMANDS; i++ ) {
        const uint commandID = surface.surfaceCommandIDs[i];
        if( commandID == 0 ) {
            return;
        }
        surfaceCommands[commandID + u_SurfaceCommandsOffset].enabled = enabled;
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

void ProcessCluster( const in Cluster cluster ) {
    uvec2 clusterIDs = uintIDToUvec4( cluster.surfaceTypeID );
    const uint materialCount = clusterSurfaceTypes[clusterIDs.x][clusterIDs.y];
    const uint debugID = gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_WorkGroupSize.x * gl_NumWorkGroups.y * gl_WorkGroupSize.y
                            + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                            + gl_GlobalInvocationID.x;
    for( uint i = 1; i <= materialCount; i++ ) {
        clusterIDs = uintIDToUvec4( cluster.surfaceTypeID + i );
        const uint materialID = clusterSurfaceTypes[clusterIDs.x][clusterIDs.y];
        atomicCounterAddARB( atomicMaterialCounters[( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_MATERIALS + materialID], cluster.triangleCount );
        /* if( i <= 4) {
            debugSurfaces[debugID * 5 + 1][i - 1] = materialID;
        } */
    }
}

#define MAX_CLUSTERS_NEW 65536 / 4

shared uint localClusters[64];
shared bool visibleClusters[64];

void PackClusters( const in uint globalID ) {
    uint count = 0;
    uint firstClusterID;
    for( uint i = 0; i < 64; i++ ) {
        if( visibleClusters[i] ) {
            if( count == 0 ) {
                firstClusterID = i;
            }
            count++;
        }
    }
    
    const uint localInvocationCount = min( 64, u_TotalDrawSurfs - globalID );
    /* const uint localGroupCount = ( u_TotalDrawSurfs - globalID ) % 4 == 0 ? ( u_TotalDrawSurfs - globalID ) / 4
                                                                         : ( u_TotalDrawSurfs - globalID ) / 4 + 1;
    for( uint i = 0; i < min( 16, localGroupCount ); i++ ) {
        const uint clusterData = ( localClusters[i * 4] + ( uint( visibleClusters[i * 4] ) << 7 ) ) << 24
                               + ( i * 4 + 1 < localInvocationCount ) ?
                                 ( localClusters[i * 4 + 1] + ( uint( visibleClusters[i * 4 + 1] ) << 7 ) ) << 16 : 0
                               + ( i * 4 + 1 < localInvocationCount ) ?
                                 ( localClusters[i * 4 + 2] + ( uint( visibleClusters[i * 4 + 2] ) << 7 ) ) << 8 : 0
                               + ( i * 4 + 1 < localInvocationCount ) ?
                                 ( localClusters[i * 4 + 3] + ( uint( visibleClusters[i * 4 + 3] ) << 7 ) ) : 0;
        clustersVisibility[globalID / 4 + i] = clusterData;
    } */

    // 
    int shift = 24;
    uint j = 0;
    uint data = 0;
    while( j < localInvocationCount ) {
        data += ( localClusters[j] + ( uint( visibleClusters[j] ) << 7 ) ) << shift;
        shift -= 8;
        if( shift < 0 ) {
            clustersVisibility[( globalID + j ) / 4] = data;
            data = 0;
            shift = 24;
        }
        j++;
    }

    // debugSurfaces[globalID * 5 + u_Frame * 5 + 2].xy = uvec2( u_Frame, count );
    if( count == 0 ) {
        return;
    }

    const uint culledClusterID = atomicCounterAddARB( clusterCounters[( MAX_VIEWS + 1 ) * u_Frame + u_ViewID], count );
    const uint workgroupCount = atomicCounter( clusterWorkgroupCounters[( MAX_VIEWS * u_Frame + u_ViewID ) * 3] );
    if( culledClusterID + count > workgroupCount * 64 ) {
        atomicCounterIncrement( clusterWorkgroupCounters[( MAX_VIEWS * u_Frame + u_ViewID ) * 3] );
    }

    uint id = 0;
    for( uint i = 0; i < 64 && id < count; i++ ) {
        if( visibleClusters[i] ) {
            culledClusters[culledClusterID + id + ( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_CLUSTERS_NEW] = globalID + i;
            id++;
        }
    }

    /* uint offset = 0;
    if( ( culledClusterID & 1 ) == 1 ) {
        const uint data = culledClusters[culledClusterID / 2 + ( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_CLUSTERS_NEW] & 0xFFFF0000;
        culledClusters[culledClusterID / 2 + ( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_CLUSTERS_NEW] = data + globalID + firstClusterID;
        visibleClusters[firstClusterID] = false;
        count--;
        offset = 1;
    }

    uint id = 0;
    uint clusterData = 0;
    bool bitShift = true;
    for( uint i = 0; i < 64 && id < count; i++ ) {
        if( visibleClusters[i] ) {
            clusterData += bitShift ? ( globalID + i ) << 16 : globalID + i;
            if( !bitShift ) {
                culledClusters[culledClusterID + id / 2 + offset + ( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_CLUSTERS_NEW] = clusterData;
                clusterData = 0;
            }
            bitShift = !bitShift;
            id++;
        }
    }
    if( count - id == 1 ) {
        clusterData += culledClusters[culledClusterID + id / 2 + offset + 1 + ( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_CLUSTERS_NEW] & 0xFFFF;
        culledClusters[culledClusterID + id / 2 + 1 + ( MAX_VIEWS * u_Frame + u_ViewID ) * MAX_CLUSTERS_NEW] = clusterData;
    } */
}

void main() {
    const uint globalInvocationID = gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_WorkGroupSize.x * gl_NumWorkGroups.y * gl_WorkGroupSize.y
                                  + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                                  + gl_GlobalInvocationID.x;
    if( globalInvocationID >= u_TotalDrawSurfs ) {
        visibleClusters[gl_LocalInvocationIndex] = false;
        return;
    }
    // SurfaceDescriptor surface = surfaces[globalInvocationID];
    const ClusterData data = clusterData[globalInvocationID];
    BoundingSphere surface;
    surface.center = vec3( data.x, data.y, data.z );
    surface.radius = data.radius;
    const bool culled = CullSurface( surface );

    Cluster cluster = UnpackCluster( globalInvocationID );
    // debugSurfaces[globalInvocationID * 5] = uvec4( globalInvocationID, cluster.triangleCount, cluster.indexOffset, cluster.surfaceTypeID );

    localClusters[gl_LocalInvocationIndex] = cluster.surfaceTypeID;
    if( !culled ) {
        ProcessCluster( cluster );
        visibleClusters[gl_LocalInvocationIndex] = true;
    } else {
        visibleClusters[gl_LocalInvocationIndex] = false;
    }

    // ProcessSurfaceCommands( surface, !culled );

    barrier();
    if( gl_LocalInvocationIndex == 0 ) {
        PackClusters( globalInvocationID );
    }
    memoryBarrier();
}
