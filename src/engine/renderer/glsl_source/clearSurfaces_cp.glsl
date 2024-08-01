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

/* clearSurfaces_cp.glsl */

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 4) writeonly restrict buffer atomicCommandCountersBuffer {
    uint atomicCommandCounters[MAX_COMMAND_COUNTERS * MAX_VIEWS * MAX_FRAMES];
};

#define MAX_MATERIALS 128

layout(std430, binding = 0) restrict buffer atomicMaterialCountersBuffer {
    uint atomicMaterialCounters[MAX_MATERIALS * MAX_VIEWS * MAX_FRAMES];
};

layout(std430, binding = 1) writeonly restrict buffer atomicMaterialCounters2Buffer {
    uint atomicMaterialCounters2[MAX_MATERIALS * MAX_VIEWS * MAX_FRAMES];
};

layout(std430, binding = 2) writeonly restrict buffer atomicClusterCountersBuffer {
    uint atomicClusterCounters[MAX_VIEWS * MAX_FRAMES + MAX_FRAMES];
};

layout(std430, binding = 3) writeonly restrict buffer clusterWorkgroupCountersBuffer {
    uint clusterWorkgroupCounters[MAX_VIEWS * MAX_FRAMES * 3];
};

struct GLIndirectCommand {
	uint count;
	uint instanceCount;
	uint firstIndex;
	int baseVertex;
	uint baseInstance;
};

layout(std430, binding = 6) writeonly restrict buffer drawCommandBuffer {
    GLIndirectCommand drawCommands[MAX_MATERIALS * MAX_VIEWS * MAX_FRAMES];
};

uniform uint u_Frame;
uniform uint u_MaxViewFrameTriangles;

void main() {
    const uint globalInvocationID = gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_WorkGroupSize.x * gl_NumWorkGroups.y * gl_WorkGroupSize.y
                             + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                             + gl_GlobalInvocationID.x;

    if( globalInvocationID >= MAX_MATERIALS * MAX_VIEWS ) {
        return;
    }

    const uint materialID = globalInvocationID % MAX_MATERIALS;

    const uint currentFrame = u_Frame > 0 ? u_Frame - 1 : MAX_FRAMES - 1;
    uint globalIndexOffset = 0;
    for( uint j = 0; j < materialID; j++ ) {
        globalIndexOffset += atomicMaterialCounters[j + ( globalInvocationID / MAX_MATERIALS ) * MAX_MATERIALS
                                                    + currentFrame * MAX_MATERIALS * MAX_VIEWS];
    }

    GLIndirectCommand cmd;
    cmd.count = atomicMaterialCounters[globalInvocationID + currentFrame * MAX_MATERIALS * MAX_VIEWS] * 3;
    cmd.instanceCount = 1;
    cmd.firstIndex = ( globalIndexOffset + u_MaxViewFrameTriangles * ( globalInvocationID / MAX_MATERIALS
                                                                   + currentFrame * MAX_VIEWS ) ) * 3;
    cmd.baseVertex = 0;
    cmd.baseInstance = 0;
    drawCommands[globalInvocationID + currentFrame * MAX_MATERIALS * MAX_VIEWS] = cmd;
    
    atomicMaterialCounters[globalInvocationID + currentFrame * MAX_MATERIALS * MAX_VIEWS] = 0;
    atomicMaterialCounters2[globalInvocationID + currentFrame * MAX_MATERIALS * MAX_VIEWS] = 0;

    if( globalInvocationID >= MAX_COMMAND_COUNTERS * MAX_VIEWS ) {
        return;
    }
    atomicCommandCounters[globalInvocationID + MAX_COMMAND_COUNTERS * MAX_VIEWS * u_Frame] = 0;
    
    if( globalInvocationID >= MAX_VIEWS + 1 ) {
        return;
    }
    atomicClusterCounters[globalInvocationID + u_Frame * ( MAX_VIEWS + 1 )] = 0;
    
    if( globalInvocationID >= MAX_VIEWS ) {
        return;
    }
    clusterWorkgroupCounters[globalInvocationID * 3 + u_Frame * MAX_VIEWS * 3] = 0;
    clusterWorkgroupCounters[globalInvocationID * 3 + 1 + u_Frame * MAX_VIEWS * 3] = 1;
    clusterWorkgroupCounters[globalInvocationID * 3 + 2 + u_Frame * MAX_VIEWS * 3] = 1;
}
