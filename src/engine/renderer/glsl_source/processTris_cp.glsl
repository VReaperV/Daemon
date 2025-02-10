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

/* processTris_cp.glsl */

#insert common_cp

// Keep this to 64 because we don't want extra shared mem etc. to be allocated, and to minimize wasted lanes
layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = GEOMETRY_CACHE_TRIS) readonly buffer atomicTrisCountersBuffer {
    uint atomicTrisCounters;
};

struct sVertex {
	vec3 position;
	float padding0;
	vec4 padding1;
};

layout(std430, binding = GEOMETRY_CACHE_INPUT_VBO) readonly restrict buffer geometryCacheVertexInputSSBO {
	sVertex inputVertices[];
};

layout(std430, binding = GEOMETRY_CACHE_TRIS_I) readonly restrict buffer geometryCacheTrisBuffer {
	SurfaceDescriptor culledTris[];
};

#if defined(HAVE_KHR_shader_subgroup_basic) && defined(HAVE_KHR_shader_subgroup_arithmetic)\
	&& defined(HAVE_KHR_shader_subgroup_ballot) && defined(HAVE_ARB_shader_atomic_counter_ops)
	#define HAVE_processSurfaces_subgroup
#endif

void ProcessSurfaceCommands( const in SurfaceDescriptor surface, const in bool enabled, const in uint count ) {
	for( uint i = 2; i < MAX_SURFACE_COMMANDS; i++ ) {
		const uint commandID = surface.surfaceCommandIDs[i];
		if( commandID == 0 ) { // Reserved for no-command
			return;
		}
		// Subtract 1 because of no-command
		surfaceCommands[commandID + u_SurfaceCommandsOffset - 1].enabled = enabled;
		// surfaceCommands[commandID + u_SurfaceCommandsOffset - 1].drawCommand.count = count;
		
		#if defined( r_materialDebug )
			// debug[DEBUG_ID( GLOBAL_INVOCATION_ID ) + 1][i] = commandID;
		#endif
	}
}

void main() {
	const uint globalGroupID = GLOBAL_GROUP_ID;
	const uint globalInvocationID = GLOBAL_INVOCATION_ID;

	if( globalInvocationID >= atomicTrisCounters ) {
		return;
	}

	atomicTrisCounters

	uvec3 triangle = uvec3( culledTris[globalInvocationID * 3], culledTris[globalInvocationID * 3 + 1],
		culledTris[globalInvocationID * 3 + 2] );
	vec3 verts[3] = vec3[3](
		vec3( inputVertices[triangle.x].position ),
		vec3( inputVertices[triangle.y].position ),
		vec3( inputVertices[triangle.z].position )
	);

	vec3 normal = cross( verts[1] - verts[0], verts[2] - verts[0] );

    if( dot( normalize( u_CameraPosition - verts[0] ), normal ) >= 0 ) {
        return;
    }

	outputIndices[firstIndex + indices] = triangle.x;
		outputIndices[firstIndex + indices + 1] = triangle.y;
		outputIndices[firstIndex + indices + 2] = triangle.z;
		indices += 3;

	ProcessSurfaceCommands( surface, !culled, count );
}
