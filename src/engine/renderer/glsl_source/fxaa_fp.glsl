/*
===========================================================================

Daemon BSD Source Code
Copyright (c) 2013 Daemon Developers
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

===========================================================================
*/

/* fxaa_fp.glsl */

// The FXAA parameters are put directly in fxaa3_11_fp.glsl
// because we cannot #include in the middle of a shader
// ^This is no longer true, but I'm not touching that mess

#insert fxaa3_11_fp

#define COLORMAP_GLSL

uniform sampler2D	u_ColorMap;

#if __VERSION__ > 120
out vec4 outputColor;
#else
#define outputColor gl_FragColor
#endif

void	main()
{
	#insert material_fp

	outputColor = FxaaPixelShader(
		gl_FragCoord.xy / r_FBufSize, //pos
		vec4(0.0), //not used
		u_ColorMap, //tex
		u_ColorMap, //not used
		u_ColorMap, //not used
		1 / r_FBufSize, //fxaaQualityRcpFrame
		vec4(0.0), //not used
		vec4(0.0), //not used
		vec4(0.0), //not used
		0.75, //fxaaQualitySubpix
		0.166, //fxaaQualityEdgeThreshold
		0.0625, //fxaaQualityEdgeThresholdMin
		0.0, //not used
		0.0, //not used
		0.0, //not used
		vec4(0.0) //not used
	);
}
