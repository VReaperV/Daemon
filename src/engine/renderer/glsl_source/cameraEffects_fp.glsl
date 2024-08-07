/*
===========================================================================
Copyright (C) 2009-2010 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/* cameraEffects_fp.glsl */

uniform sampler2D u_CurrentMap;
uniform sampler3D u_ColorMap3D;
layout(r32f, binding = 4) uniform readonly image2D luminanceImageIn;

uniform float u_LightFactor;

uniform vec4      u_ColorModulate;
uniform float     u_InverseGamma;

IN(smooth) vec2		var_TexCoords;

DECLARE_OUTPUT(vec4)

float Tonemap_Lottes(float x) {
    // Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
    const float a = 1.6;
    const float d = 0.977;
    const float hdrMax = 8.0;
    const float midIn = 0.18;
    const float midOut = 0.267;

    // Can be precomputed
    const float b =
        (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
        ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    const float c =
        (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
        ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    return pow(x, a) / (pow(x, a * d) * b + c);
}

void	main()
{
	// calculate the screen texcoord in the 0.0 to 1.0 range
	vec2 st = gl_FragCoord.st / r_FBufSize;

	vec4 color = texture2D(u_CurrentMap, st);

	color.rgb *= u_LightFactor;

	color = clamp(color, 0.0, 1.0);

	float luminance = imageLoad( luminanceImageIn, ivec2( 0, 0 ) ).r;
	luminance = clamp( luminance, 0.0004, 0.1 ) * 10.0;
	float l = luminance;
	/* float exposure = 0.5; // 1.0 / pow( 2.0, 2 );
	float brightness = exposure * luminance;
	float targetBrightness = 0.5; */
	float exposure = 2.0;
	luminance = 1 - exp( -luminance * exposure );
	// luminance *= 1 / 8.0;
	// luminance = 1 - ( log( luminance + 0.01 ) - log( 0.1 ) ) / ( log( 1.01 ) - log( 0.01 ) );
	luminance = Tonemap_Lottes( 1 - luminance );// * 1.0 * exp( -l * 1.0 );

	// apply color grading
	vec3 colCoord = color.rgb * 15.0 / 16.0 + 0.5 / 16.0;
	colCoord.z *= 0.25;
	color.rgb = u_ColorModulate.x * texture3D(u_ColorMap3D, colCoord).rgb;
	color.rgb += u_ColorModulate.y * texture3D(u_ColorMap3D, colCoord + vec3(0.0, 0.0, 0.25)).rgb;
	color.rgb += u_ColorModulate.z * texture3D(u_ColorMap3D, colCoord + vec3(0.0, 0.0, 0.50)).rgb;
	color.rgb += u_ColorModulate.w * texture3D(u_ColorMap3D, colCoord + vec3(0.0, 0.0, 0.75)).rgb;

	color.rgb *= luminance;

	color.xyz = pow(color.xyz, vec3(u_InverseGamma));

	outputColor = color;
	// outputColor = vec4( luminance, color.yzw );
}
