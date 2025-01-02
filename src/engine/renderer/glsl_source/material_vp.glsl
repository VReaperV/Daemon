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

/* material_vp.glsl */

#if defined(USE_MATERIAL_SYSTEM)

	#ifdef HAVE_ARB_shader_draw_parameters
		in_drawID = drawID;
		in_baseInstance = baseInstance;
	#endif // !HAVE_ARB_shader_draw_parameters

    #if defined(GENERIC_GLSL)
		uint u_ColorModulateColorGen = materials[baseInstance & 0xFFF].u_ColorModulateColorGen;
		uint u_Color = materials[baseInstance & 0xFFF].u_Color;
		
		#if defined(USE_DEPTH_FADE)
			float u_DepthScale = materials[baseInstance & 0xFFF].u_DepthScale;
		#endif
	#endif // !GENERIC_GLSL

	#if defined(LIGHTMAPPING_GLSL)
		uint u_ColorModulateColorGen = materials[baseInstance & 0xFFF].u_ColorModulateColorGen;
		uint u_Color = materials[baseInstance & 0xFFF].u_Color;
	#endif // !LIGHTMAPPING_GLSL

	#if defined(HEATHAZE_GLSL)
		float u_DeformMagnitude = materials[baseInstance & 0xFFF].u_DeformMagnitude;
	#endif // !HEATHAZE_GLSL

	/* #if defined(RELIEFMAPPING_GLSL)
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
	#endif // !RELIEFMAPPING_GLSL */

	#if defined(SKYBOX_GLSL)
		samplerCube u_ColorMapCube = samplerCube( u_DiffuseMap_initial );
		sampler2D u_CloudMap = sampler2D( u_NormalMap_initial );
	#endif // !SKYBOX_GLSL

#endif // !USE_MATERIAL_SYSTEM
