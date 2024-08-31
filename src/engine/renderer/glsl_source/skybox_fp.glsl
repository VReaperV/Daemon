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

/* skybox_fp.glsl */

#define SKYBOX_GLSL

const float radiusWorld = 4096.0; // Value used by quake 3 skybox code

uniform samplerCube	u_ColorMapCube;

uniform float u_InverseLightFactor;
#if defined(r_texturePacks)
	uniform sampler2DArray u_CloudMap;
	uniform vec3 u_CloudMapModifier;
#else
	uniform sampler2D u_CloudMap;
#endif

uniform bool        u_UseCloudMap;
uniform float       u_CloudHeight;

uniform mat4		u_TextureMatrix;

uniform float		u_AlphaThreshold;

IN(smooth) vec3		var_Position;

DECLARE_OUTPUT(vec4)

// Parametric cloud function used by quake 3 skybox code
float ComputeCloudParametric( vec3 skyVec, float radiusWorld, float cloudHeight ) {
	return (1.0 / 2.0 * dot( skyVec, skyVec ) ) * ( -2.0 * skyVec.z * radiusWorld
			+ 2.0 * sqrt( skyVec.z * skyVec.z * radiusWorld * radiusWorld
			+ 2.0 * skyVec.x * skyVec.x * radiusWorld * cloudHeight
			+ skyVec.x * skyVec.x * cloudHeight * cloudHeight + 2.0 * skyVec.y * skyVec.y * radiusWorld * cloudHeight
			+ skyVec.y * skyVec.y * cloudHeight * cloudHeight
			+ 2.0 * skyVec.z * skyVec.z * radiusWorld * cloudHeight + skyVec.z * skyVec.z * cloudHeight * cloudHeight  ) );
}

void	main()
{
	#insert material_fp

	// compute incident ray
	vec3 incidentRay = normalize(var_Position);
	vec4 color;

	if( !u_UseCloudMap ) {
		color = textureCube(u_ColorMapCube, incidentRay).rgba;
	} else {
		incidentRay *= ComputeCloudParametric( incidentRay, radiusWorld, u_CloudHeight );
		incidentRay.z += radiusWorld;
		incidentRay = normalize( incidentRay );
		vec2 st = vec2( acos(incidentRay.x), acos(incidentRay.y) );
		st = (u_TextureMatrix * vec4(st, 0.0, 1.0)).xy;
	#if defined(r_texturePacks)
		color = texture2D( u_CloudMap, TEXTURE_WRAP( st, u_CloudMapModifier ) );
	#else
		color = texture2D( u_CloudMap, st ).rgba;
	#endif
	}
	
	if( abs(color.a + u_AlphaThreshold) <= 1.0 )
	{
		discard;
		return;
	}

	color.rgb *= u_InverseLightFactor;

	outputColor = color;
}
