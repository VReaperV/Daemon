/*
===========================================================================
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

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

/* screen_vp.glsl */

IN vec3 		attr_Position;
IN vec4		attr_Color;

uniform mat4		u_ModelViewProjectionMatrix;

OUT(smooth) vec4	var_Color;

void	main()
{
	#insert material_vp

	// transform vertex position into homogenous clip-space
	gl_Position = u_ModelViewProjectionMatrix * vec4(attr_Position, 1.0);

	// assign color
	var_Color = attr_Color;
}
