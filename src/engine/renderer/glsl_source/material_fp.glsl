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

/*
*  For use with material system
*  All changes to uniform samplers in shaders which use this system must be reflected here
*  Any shader using this system should add #define SHADERNAME_GLSL in the beginning (including lib shaders)
*  It should then be added here in the material system and bindless texture ifdefs:
*  #if defined(SHADERNAME_GLSL)
*  sampler* samplerName = sampler*( samplerName_initial );
*  #endif // !SHADERNAME_GLSL
*  In the main shader add
*  #insert material
*  in the beginning of main() once
*  Any texture samplers should be passed to functions from main() or other functions
*/

#if defined(USE_MATERIAL_SYSTEM)

#if defined(r_texturePacks)
#define samplerPack sampler2DArray
#else
#define samplerPack sampler2D
#endif

#ifdef HAVE_ARB_bindless_texture

#if defined(COMPUTELIGHT_GLSL)
#if defined(USE_REFLECTIVE_SPECULAR)
samplerCube u_EnvironmentMap0 = samplerCube( u_EnvironmentMap0_initial );
samplerCube u_EnvironmentMap1 = samplerCube( u_EnvironmentMap1_initial );
#endif // !USE_REFLECTIVE_SPECULAR
usampler3D u_LightTilesInt = usampler3D( u_LightTilesInt_initial );
#endif // !COMPUTELIGHT_GLSL

#if defined(FOGQUAKE3_GLSL)
samplerPack u_ColorMap = samplerPack( u_ColorMap_initial );
#endif // !FOGQUAKE3_GLSL

#if defined(GENERIC_GLSL)
samplerPack u_ColorMap = samplerPack( u_ColorMap_initial );
#if defined(USE_DEPTH_FADE) || defined(USE_VERTEX_SPRITE)
sampler2D u_DepthMap = sampler2D( u_DepthMap_initial );
#endif // !(USE_DEPTH_FADE || USE_VERTEX_SPRITE)
#endif // !GENERIC_GLSL

#if defined(HEATHAZE_GLSL)
sampler2D u_CurrentMap = sampler2D( u_CurrentMap_initial );
#endif // !HEATHAZE_GLSL

#if defined(LIGHTMAPPING_GLSL)
samplerPack u_DiffuseMap = samplerPack( u_DiffuseMap_initial );
samplerPack u_MaterialMap = samplerPack( u_MaterialMap_initial );
samplerPack u_GlowMap = samplerPack( u_GlowMap_initial );
samplerPack u_LightMap = samplerPack( u_LightMap_initial );
sampler3D u_LightGrid1 = sampler3D( u_LightGrid1_initial );
samplerPack u_DeluxeMap = samplerPack( u_DeluxeMap_initial );
sampler3D u_LightGrid2 = sampler3D( u_LightGrid2_initial );
#endif // !LIGHTMAPPING_GLSL

#if defined(LIQUID_GLSL)
sampler2D u_CurrentMap = sampler2D( u_CurrentMap_initial );
sampler2D u_PortalMap = sampler2D( u_PortalMap_initial );
sampler2D u_DepthMap = sampler2D( u_DepthMap_initial );
sampler3D u_LightGrid1 = sampler3D( u_LightGrid1_initial );
sampler3D u_LightGrid2 = sampler3D( u_LightGrid2_initial );
#endif // !LIQUID_GLSL

#if defined(REFLECTION_CB_GLSL)
samplerCube u_ColorMap = samplerCube( u_ColorMap_initial );
#endif // !REFLECTION_CB_GLSL

#if defined(RELIEFMAPPING_GLSL)
#if defined(r_normalMapping) || defined(USE_HEIGHTMAP_IN_NORMALMAP)
samplerPack u_NormalMap = samplerPack( u_NormalMap_initial );
#endif // r_normalMapping || USE_HEIGHTMAP_IN_NORMALMAP

#if defined(USE_RELIEF_MAPPING)
#if !defined(USE_HEIGHTMAP_IN_NORMALMAP)
samplerPack u_HeightMap = samplerPack( u_HeightMap_initial );
#else
samplerPack u_HeightMap = samplerPack( u_NormalMap_initial );
#endif // !USE_HEIGHTMAP_IN_NORMALMAP
#endif // USE_RELIEF_MAPPING
#endif // !RELIEFMAPPING_GLSL

#if defined(SCREEN_GLSL)
sampler2D u_CurrentMap = sampler2D( u_CurrentMap_initial );
#endif // !SCREEN_GLSL

#if defined(SKYBOX_GLSL)
samplerCube u_ColorMapCube = samplerCube( u_ColorMapCube_initial );
samplerPack u_CloudMap = samplerPack( u_CloudMap_initial );
#endif // !SKYBOX_GLSL

#else // !HAVE_ARB_bindless_texture
#endif

#else // !USE_MATERIAL_SYSTEM

#if defined(USE_HEIGHTMAP_IN_NORMALMAP)
#define u_HeightMap u_NormalMap
#endif // !USE_HEIGHTMAP_IN_NORMALMAP

#endif // !USE_MATERIAL_SYSTEM
