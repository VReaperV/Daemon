/*
===========================================================================
Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>

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

/* refraction_C_fp.glsl */

uniform samplerCube	u_ColorMapCube;
uniform vec3		u_ViewOrigin;
uniform float		u_RefractionIndex;
uniform float		u_FresnelPower;
uniform float		u_FresnelScale;
uniform float		u_FresnelBias;

IN(smooth) vec3		var_Position;
IN(smooth) vec3		var_Normal;

DECLARE_OUTPUT(vec4)

void	main()
{
	// compute incident ray
	vec3 incidentRay = normalize(var_Position - u_ViewOrigin);

	// compute normal
	vec3 normal = normalize(var_Normal);

	// compute reflection ray
	vec3 reflectionRay = reflect(incidentRay, normal);

	// compute refraction ray
	vec3 T = refract(incidentRay, N, u_RefractionIndex);

	// compute fresnel term
	float fresnel = u_FresnelBias + pow(1.0 - dot(incidentRay, normal), u_FresnelPower) * u_FresnelScale;

	vec3 reflectColor = textureCube(u_ColorMapCube, reflectionRay).rgb;
	vec3 refractColor = textureCube(u_ColorMapCube, T).rgb;

	// compute final color
	vec4 color;
	color.r = (1.0 - fresnel) * refractColor.r + reflectColor.r * fresnel;
	color.g = (1.0 - fresnel) * refractColor.g + reflectColor.g * fresnel;
	color.b = (1.0 - fresnel) * refractColor.b + reflectColor.b * fresnel;
	color.a = 1.0;

	outputColor = color;
}
