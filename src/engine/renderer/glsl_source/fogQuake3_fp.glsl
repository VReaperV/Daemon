/*
===========================================================================
Copyright (C) 2011 Robert Beckebans <trebor_7@users.sourceforge.net>

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

/* fogQuake3_fp.glsl */

#define FOGQUAKE3_GLSL

#if defined(r_texturePacks)
	uniform sampler2DArray	u_ColorMap;
	uniform vec3 u_ColorMapModifier;
#else
	uniform sampler2D	u_ColorMap;
#endif

uniform float u_InverseLightFactor;
IN(smooth) vec3		var_Position;
IN(smooth) vec2		var_TexCoords;
IN(smooth) vec4		var_Color;

DECLARE_OUTPUT(vec4)

void	main()
{
	#insert material_fp

#if defined(r_texturePacks)
	vec4 color = texture2D(u_ColorMap, TEXTURE_WRAP( var_TexCoords, u_ColorMapModifier ));
#else
	vec4 color = texture2D(u_ColorMap, var_TexCoords);
#endif

	color *= var_Color;

	color.rgb *= u_InverseLightFactor;

	outputColor = color;

#if 0
	outputColor = vec4(vec3(1.0, 0.0, 0.0), color.a);
#endif
}
