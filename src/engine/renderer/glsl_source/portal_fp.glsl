/*
===========================================================================
Copyright (C) 2008 Robert Beckebans <trebor_7@users.sourceforge.net>

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

/* portal_fp.glsl */

#define CURRENTMAP_GLSL

uniform sampler2D	u_CurrentMap;
uniform float		u_InversePortalRange;

IN(smooth) vec3		var_Position;
IN(smooth) vec4		var_Color;
IN(smooth) vec2	var_TexCoords;

DECLARE_OUTPUT(vec4)

void	main()
{
	#insert material_fp

	vec4 color = texture2D(u_CurrentMap, var_TexCoords);
	color *= var_Color;

	float len = length(var_Position);

	len *= u_InversePortalRange;
	color.a = clamp(len, 0.0, 1.0);

	outputColor = color;
}
