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

uniform float u_LightFactor;

uniform vec4      u_ColorModulate;
uniform float     u_InverseGamma;

IN(smooth) vec2		var_TexCoords;

DECLARE_OUTPUT(vec4)

void	main()
{
	// calculate the screen texcoord in the 0.0 to 1.0 range
	vec2 st = gl_FragCoord.st / r_FBufSize;

	vec4 color = texture2D(u_CurrentMap, st);

	color.rgb *= u_LightFactor;

	color = clamp(color, 0.0, 1.0);

	// apply color grading
	vec3 colCoord = color.rgb * 15.0 / 16.0 + 0.5 / 16.0;
	colCoord.z *= 0.25;
	/* color.rgb = u_ColorModulate.x * texture3D(u_ColorMap3D, colCoord).rgb;
	color.rgb += u_ColorModulate.y * texture3D(u_ColorMap3D, colCoord + vec3(0.0, 0.0, 0.25)).rgb;
	color.rgb += u_ColorModulate.z * texture3D(u_ColorMap3D, colCoord + vec3(0.0, 0.0, 0.50)).rgb;
	color.rgb += u_ColorModulate.w * texture3D(u_ColorMap3D, colCoord + vec3(0.0, 0.0, 0.75)).rgb; */

	color.xyz = pow(color.xyz, vec3(u_InverseGamma));

	outputColor = color;
}
