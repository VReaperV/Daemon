/*
===========================================================================
Copyright (C) 2006-2011 Robert Beckebans <trebor_7@users.sourceforge.net>

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

/* reflection_CB_vp.glsl */

#insert vertexSimple_vp
#insert vertexSkinning_vp
#insert vertexAnimation_vp

#define REFLECTION_CB_GLSL

#if !defined(USE_MATERIAL_SYSTEM)
	uniform mat3x2 u_TextureMatrix;
#endif

uniform mat4		u_ModelMatrix;
uniform mat4		u_ModelViewProjectionMatrix;

uniform float		u_Time;

#if defined(r_showCubeProbes)
	uniform vec3 u_CameraPosition;
#endif

OUT(smooth) vec3	var_Position;
OUT(smooth) vec2	var_TexCoords;
OUT(smooth) vec4	var_Tangent;
OUT(smooth) vec4	var_Binormal;
OUT(smooth) vec4	var_Normal;

void DeformVertex( inout vec4 pos,
		   inout vec3 normal,
		   inout vec2 st,
		   inout vec4 color,
		   in    float time);

void	main()
{
	#insert material_vp

	vec4 position;
	localBasis LB;
	vec2 texCoord, lmCoord;
	vec4 color;

	VertexFetch( position, LB, color, texCoord, lmCoord );

	DeformVertex( position,
		      LB.normal,
		      texCoord,
		      color,
		      u_Time);

	// transform vertex position into homogenous clip-space
	gl_Position = u_ModelViewProjectionMatrix * position;

	// transform position into world space
	#if defined(r_showCubeProbes)
		/* Hack: This is used for debug purposes only,
		but it will break ST_REFLECTIONMAP and ST_COLLAPSE_REFLECTIONMAP stages */
		var_Position = (u_ModelMatrix * ( position - vec4( u_CameraPosition, 0.0 ) )).xyz;
	#else
		var_Position = (u_ModelMatrix * position).xyz;
	#endif

	var_Tangent.xyz = (u_ModelMatrix * vec4(LB.tangent, 0.0)).xyz;
	var_Binormal.xyz = (u_ModelMatrix * vec4(LB.binormal, 0.0)).xyz;
	var_Normal.xyz = (u_ModelMatrix * vec4(LB.normal, 0.0)).xyz;

	// transform normalmap texcoords
	var_TexCoords = (u_TextureMatrix * vec3(texCoord, 1.0)).st;
}

