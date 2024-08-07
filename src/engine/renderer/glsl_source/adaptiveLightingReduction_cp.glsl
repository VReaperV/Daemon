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

/* adaptiveLightingReduction_cp.glsl */

// Keep this to 8x8 because we don't want extra shared mem etc. to be allocated, and to minimize wasted lanes
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D initialRenderImage;
layout(r32f, binding = 1) uniform readonly image2D luminanceImageIn;
layout(r32f, binding = 2) uniform writeonly image2D luminanceImageOut;

uniform uint u_ViewWidth;
uniform uint u_ViewHeight;
uniform bool u_InitialDepthLevel;

float ColorToLuminance( const in vec3 color ) {
    return color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722;
}

void main() {
    const uint globalInvocationID = gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_WorkGroupSize.x * gl_NumWorkGroups.y * gl_WorkGroupSize.y
                             + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
                             + gl_GlobalInvocationID.x;

    const ivec2 position = ivec2( gl_GlobalInvocationID.xy );
    if( position.x >= u_ViewWidth || position.y >= u_ViewHeight ) {
        return;
    };

    // Depth buffer uses a packed D24S8 format, so we have to copy it over to an r32f image first
    if( u_InitialDepthLevel ) {
        float luminance = 0.0;
        for( int x = 0; x < 2; x++ ) {
            for( int y = 0; y < 2; y++ ) {
                luminance += ColorToLuminance( texelFetch( initialRenderImage, position * 2 + ivec2( x, y ), 0 ).rgb );
            }
        }
        luminance /= 4;
        imageStore( luminanceImageOut, position, vec4( luminance, 0.0, 0.0, 0.0 ) );
    } else {
        float luminance = 0.0;
        
        for( int x = 0; x < 2; x++ ) {
            for( int y = 0; y < 2; y++ ) {
                luminance += imageLoad( luminanceImageIn, position * 2 + ivec2( x, y ) ).r;
            }
        }

        uint count = 4;
        // Mipmaps round the dimensions down for each level, so we might need to sample up to 5 extra texels along the edges
        if( ( u_ViewWidth & 1 ) == 1 ) {
            luminance += imageLoad( luminanceImageIn, position * 2 + ivec2( 2, 0 ) ).r;
            luminance += imageLoad( luminanceImageIn, position * 2 + ivec2( 2, 1 ) ).r;
            count += 2;
        }
        if( ( u_ViewHeight & 1 ) == 1 ) {
            luminance += imageLoad( luminanceImageIn, position * 2 + ivec2( 0, 2 ) ).r;
            luminance += imageLoad( luminanceImageIn, position * 2 + ivec2( 1, 2 ) ).r;
            count += 2;
        }
        if( ( u_ViewWidth & 1 ) == 1 && ( u_ViewHeight & 1 ) == 1 ) {
            luminance += imageLoad( luminanceImageIn, position * 2 + ivec2( 2, 2 ) ).r;
            count++;
        }
        luminance /= count;

        imageStore( luminanceImageOut, position, vec4( luminance, 0.0, 0.0, 0.0 ) );
    }
}
