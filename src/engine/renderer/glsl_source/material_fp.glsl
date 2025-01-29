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

/* material_fp.glsl */

/* For use with material system
   All changes to uniform samplers in shaders which use this system must be reflected here
   Any shader using this system should add #define SHADERNAME_GLSL in the beginning (including lib shaders)
   It should then be added here in the material system and bindless texture ifdefs:
   #if defined(SHADERNAME_GLSL)
	   sampler* samplerName = sampler*( samplerName_initial );
   #endif // !SHADERNAME_GLSL
   In the main shader add
   #insert material_fp
   in the beginning of main() once
   Any texture samplers should be passed to functions from main() or other functions */

#if defined(USE_MATERIAL_SYSTEM)

	#ifdef HAVE_ARB_bindless_texture

	#if defined(COMPUTELIGHT_GLSL)
		// TODO: Source this from an entity buffer once entities are supported by the material system
		/* #if defined(USE_REFLECTIVE_SPECULAR)
			samplerCube u_EnvironmentMap0 = samplerCube( u_EnvironmentMap0_initial );
			samplerCube u_EnvironmentMap1 = samplerCube( u_EnvironmentMap1_initial );
		#endif // !USE_REFLECTIVE_SPECULAR */
		
		#if defined(USE_DELUXE_MAPPING) || defined(USE_GRID_DELUXE_MAPPING) || defined(r_realtimeLighting)
			vec2 u_SpecularExponent = materials[baseInstance & 0xFFF].u_SpecularExponent;
		#endif
	#endif // !COMPUTELIGHT_GLSL

	#if defined(GENERIC_GLSL)
		sampler2D u_ColorMap = sampler2D( u_DiffuseMap_initial );

		float u_AlphaThreshold = materials[baseInstance & 0xFFF].u_AlphaThreshold;
	#endif // !GENERIC_GLSL

	#if defined(LIGHTMAPPING_GLSL)
		sampler2D u_DiffuseMap = sampler2D( u_DiffuseMap_initial );
		sampler2D u_MaterialMap = sampler2D( u_MaterialMap_initial );
		sampler2D u_GlowMap = sampler2D( u_GlowMap_initial );
		sampler2D u_LightMap = sampler2D( u_LightMap_initial );
		sampler2D u_DeluxeMap = sampler2D( u_DeluxeMap_initial );
		
		float u_AlphaThreshold = materials[baseInstance & 0xFFF].u_AlphaThreshold;
		uint u_ColorModulateColorGen = materials[baseInstance & 0xFFF].u_ColorModulateColorGen;
	#endif // !LIGHTMAPPING_GLSL

	#if defined(LIQUID_GLSL)
		float u_FogDensity = materials[baseInstance & 0xFFF].u_FogDensity;
		vec3 u_FogColor = materials[baseInstance & 0xFFF].u_FogColor;
		float u_RefractionIndex = materials[baseInstance & 0xFFF].u_RefractionIndex;
		float u_FresnelPower = materials[baseInstance & 0xFFF].u_FresnelPower;
		float u_FresnelScale = materials[baseInstance & 0xFFF].u_FresnelScale;
		float u_FresnelBias = materials[baseInstance & 0xFFF].u_FresnelBias;
		
		uint u_ColorModulateColorGen = materials[baseInstance & 0xFFF].u_ColorModulateColorGen;
	#endif // !LIQUID_GLSL

	#if defined(REFLECTION_CB_GLSL)
		samplerCube u_ColorMapCube = samplerCube( u_DiffuseMap_initial );
	#endif // !REFLECTION_CB_GLSL

	#if defined(RELIEFMAPPING_GLSL)
		#if defined(r_normalMapping) || defined(USE_HEIGHTMAP_IN_NORMALMAP)
			sampler2D u_NormalMap = sampler2D( u_NormalMap_initial );
		#endif // r_normalMapping || USE_HEIGHTMAP_IN_NORMALMAP

		#if defined(USE_RELIEF_MAPPING)
			#if !defined(USE_HEIGHTMAP_IN_NORMALMAP)
				sampler2D u_HeightMap = sampler2D( u_HeightMap_initial );
			#else
				sampler2D u_HeightMap = sampler2D( u_NormalMap_initial );
			#endif // !USE_HEIGHTMAP_IN_NORMALMAP
		#endif // USE_RELIEF_MAPPING

		#if defined(r_normalMapping)
			vec3 u_NormalScale = materials[baseInstance & 0xFFF].u_NormalScale;
		#endif // !r_normalMapping

		#if defined(USE_RELIEF_MAPPING)
			float u_ReliefDepthScale = materials[baseInstance & 0xFFF].u_ReliefDepthScale;
			float u_ReliefOffsetBias = materials[baseInstance & 0xFFF].u_ReliefOffsetBias;
		#endif // USE_RELIEF_MAPPING
	#endif // !RELIEFMAPPING_GLSL

	#if defined(SKYBOX_GLSL)
		samplerCube u_ColorMapCube = samplerCube( u_DiffuseMap_initial );
		sampler2D u_CloudMap = sampler2D( u_NormalMap_initial );

		float u_AlphaThreshold = materials[baseInstance & 0xFFF].u_AlphaThreshold;
		float u_CloudHeight = materials[baseInstance & 0xFFF].u_CloudHeight;
	#endif // !SKYBOX_GLSL

	#else // !HAVE_ARB_bindless_texture
	#endif

#else // !USE_MATERIAL_SYSTEM

	#if defined(USE_HEIGHTMAP_IN_NORMALMAP)
		#define u_HeightMap u_NormalMap
	#endif // !USE_HEIGHTMAP_IN_NORMALMAP

#endif // !USE_MATERIAL_SYSTEM
