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
// Material.cpp

#include "Material.h"
#include "tr_local.h"

GLSSBO materialsSSBO( "materials", 0, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLSSBO surfaceDescriptorsSSBO( "surfaceDescriptors", 1, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLSSBO surfaceCommandsSSBO( "surfaceCommands", 2, GL_MAP_WRITE_BIT, GL_MAP_FLUSH_EXPLICIT_BIT );
GLBuffer culledCommandsBuffer( "culledCommands", 3, GL_MAP_WRITE_BIT, GL_MAP_FLUSH_EXPLICIT_BIT );
GLUBO surfaceBatchesUBO( "surfaceBatches", 0, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLBuffer atomicCommandCountersBuffer( "atomicCommandCounters", 4, GL_MAP_WRITE_BIT, GL_MAP_FLUSH_EXPLICIT_BIT );
GLSSBO portalSurfacesSSBO( "portalSurfaces", 5, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT, 0 );

PortalView portalStack[MAX_VIEWS];

GLBuffer drawCommandBuffer( "drawCommands", 6, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLSSBO clusterIndexesBuffer( "clusterIndexes", 7, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLBuffer globalIndexesSSBO( "globalIndexes", 8, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLBuffer materialIDsSSBO( "materialIDs", 9, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );

GLBuffer clustersUBO( "clusters", 1, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT, GL_MAP_FLUSH_EXPLICIT_BIT );
GLUBO clusterSurfaceTypesUBO( "clusterSurfaceTypes", 2, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT, GL_MAP_FLUSH_EXPLICIT_BIT );
GLSSBO clusterDataSSBO( "clusterData", 10, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT, GL_MAP_FLUSH_EXPLICIT_BIT );
GLBuffer culledClustersBuffer( "culledClusters", 11, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLBuffer atomicMaterialCountersBuffer( "atomicMaterialCounters", 1, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLBuffer atomicMaterialCountersBuffer2( "atomicMaterialCounters2", 2, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLBuffer clusterCountersBuffer( "clusterCounters", 3, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLBuffer clusterWorkgroupCountersBuffer( "clusterWorkgroupCounters", 5, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );
GLBuffer clusterVertexesBuffer( "clusterVertexes", 14, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );

GLSSBO debugSSBO( "debugSurfaces", 13, GL_MAP_WRITE_BIT, GL_MAP_INVALIDATE_RANGE_BIT );

MaterialSystem materialSystem;

static void ComputeDynamics( shaderStage_t* pStage ) {
	// TODO: Move color and texMatrices stuff to a compute shader
	switch ( pStage->rgbGen ) {
		case colorGen_t::CGEN_IDENTITY:
		case colorGen_t::CGEN_ONE_MINUS_VERTEX:
		default:
		case colorGen_t::CGEN_IDENTITY_LIGHTING:
			/* Historically CGEN_IDENTITY_LIGHTING was done this way:

			  tess.svars.color = Color::White * tr.identityLight;

			But tr.identityLight is always 1.0f in Dæmon engine
			as the as the overbright bit implementation is fully
			software. */
		case colorGen_t::CGEN_VERTEX:
		case colorGen_t::CGEN_CONST:
		case colorGen_t::CGEN_ENTITY:
		case colorGen_t::CGEN_ONE_MINUS_ENTITY:
		{
			// TODO: Move this to some entity buffer once this is extended past BSP surfaces
			if ( backEnd.currentEntity ) {
				//
			} else {
				//
			}
			pStage->colorDynamic = false;

			break;
		}

		case colorGen_t::CGEN_WAVEFORM:
		case colorGen_t::CGEN_CUSTOM_RGB:
		case colorGen_t::CGEN_CUSTOM_RGBs:
		{
			pStage->colorDynamic = true;
			break;
		}
	}

	switch ( pStage->alphaGen ) {
		default:
		case alphaGen_t::AGEN_IDENTITY:
		case alphaGen_t::AGEN_ONE_MINUS_VERTEX:
		case alphaGen_t::AGEN_VERTEX:
		case alphaGen_t::AGEN_CONST: {
		case alphaGen_t::AGEN_ENTITY:
		case alphaGen_t::AGEN_ONE_MINUS_ENTITY:
			// TODO: Move this to some entity buffer once this is extended past BSP surfaces
			/* if ( backEnd.currentEntity ) {
			} else {
			} */
			pStage->colorDynamic = false;
			break;
		}

		case alphaGen_t::AGEN_WAVEFORM:
		case alphaGen_t::AGEN_CUSTOM:
		{
			pStage->colorDynamic = true;
			break;
		}
	}

	for ( textureBundle_t& bundle : pStage->bundle ) {
		for ( size_t i = 0; i < bundle.numTexMods; i++ ) {
			switch ( bundle.texMods[i].type ) {
				case texMod_t::TMOD_NONE:
				case texMod_t::TMOD_SCALE:
				case texMod_t::TMOD_TRANSFORM:
					break;

				case texMod_t::TMOD_TURBULENT:
				case texMod_t::TMOD_ENTITY_TRANSLATE:
				case texMod_t::TMOD_SCROLL:
				{
					pStage->texMatricesDynamic = true;
					break;
				}

				case texMod_t::TMOD_STRETCH:
				{
					if( bundle.texMods->wave.func != genFunc_t::GF_NONE ) {
						pStage->texMatricesDynamic = true;
					}
					break;
				}

				case texMod_t::TMOD_ROTATE:
				{
					pStage->texMatricesDynamic = true;
					break;
				}

				case texMod_t::TMOD_SCROLL2:
				case texMod_t::TMOD_SCALE2:
				case texMod_t::TMOD_CENTERSCALE:
				case texMod_t::TMOD_SHEAR:
				{
					if ( bundle.texMods[i].sExp.active || bundle.texMods[i].tExp.active ) {
						pStage->texMatricesDynamic = true;
					}
					break;
				}

				case texMod_t::TMOD_ROTATE2:
				{
					if( bundle.texMods[i].rExp.active ) {
						pStage->texMatricesDynamic = true;
					}
					break;
				}

				default:
					break;
			}
		}
	}

	// TODO: Move this to a different buffer?
	for ( const textureBundle_t& bundle : pStage->bundle ) {
		if ( bundle.isVideoMap || bundle.numImages > 1 ) {
			pStage->texturesDynamic = true;
			break;
		}
	}

	// Can we move this to a compute shader too?
	// Doesn't seem to be used much if at all, so probably not worth the effort to do that
	pStage->dynamic = pStage->dynamic || pStage->ifExp.active;
	pStage->dynamic = pStage->dynamic || pStage->alphaExp.active || pStage->alphaTestExp.active;
	pStage->dynamic = pStage->dynamic || pStage->rgbExp.active || pStage->redExp.active || pStage->greenExp.active || pStage->blueExp.active;
	pStage->dynamic = pStage->dynamic || pStage->deformMagnitudeExp.active;
	pStage->dynamic = pStage->dynamic || pStage->depthScaleExp.active || pStage->etaExp.active || pStage->etaDeltaExp.active
		|| pStage->fogDensityExp.active || pStage->fresnelBiasExp.active || pStage->fresnelPowerExp.active
		|| pStage->fresnelScaleExp.active || pStage->normalIntensityExp.active || pStage->refractionIndexExp.active;

	pStage->dynamic = pStage->dynamic || pStage->colorDynamic || pStage->texMatricesDynamic || pStage->texturesDynamic;
}

static image_t* GetLightMap( drawSurf_t* drawSurf ) {
	if ( static_cast<size_t>( drawSurf->lightmapNum() ) < tr.lightmaps.size() ) {
		return tr.lightmaps[drawSurf->lightmapNum()];
	} else {
		return tr.whiteImage;
	}
}

static image_t* GetDeluxeMap( drawSurf_t* drawSurf ) {
	if ( static_cast<size_t>( drawSurf->lightmapNum() ) < tr.deluxemaps.size() ) {
		return tr.deluxemaps[drawSurf->lightmapNum()];
	} else {
		return tr.blackImage;
	}
}

// UpdateSurface*() functions will actually write the uniform values to the SSBO
// Mirrors parts of the Render_*() functions in tr_shade.cpp

static void UpdateSurfaceDataGeneric( uint32_t* materials, Material& material, drawSurf_t* drawSurf, const uint32_t stage ) {
	shader_t* shader = drawSurf->shader;
	shaderStage_t* pStage = &shader->stages[stage];

	const uint32_t paddedOffset = drawSurf->materialsSSBOOffset[stage] * material.shader->GetPaddedSize();
	materials += paddedOffset;

	bool updated = !drawSurf->initialized[stage] || pStage->colorDynamic || pStage->texMatricesDynamic || pStage->dynamic;
	if ( !updated ) {
		return;
	}
	drawSurf->initialized[stage] = true;

	gl_genericShaderMaterial->BindProgram( material.deformIndex );

	gl_genericShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );
	gl_genericShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );

	// u_AlphaThreshold
	gl_genericShaderMaterial->SetUniform_AlphaTest( pStage->stateBits );

	// u_InverseLightFactor
	// We should cancel overbrightBits if there is no light,
	// and it's not using blendFunc dst_color.
	bool blendFunc_dstColor = ( pStage->stateBits & GLS_SRCBLEND_BITS ) == GLS_SRCBLEND_DST_COLOR;
	float inverseLightFactor = ( pStage->shaderHasNoLight && !blendFunc_dstColor ) ? tr.mapInverseLightFactor : 1.0f;
	gl_genericShaderMaterial->SetUniform_InverseLightFactor( inverseLightFactor );

	// u_ColorModulate
	colorGen_t rgbGen;
	alphaGen_t alphaGen;
	SetRgbaGen( pStage, &rgbGen, &alphaGen );
	gl_genericShaderMaterial->SetUniform_ColorModulate( rgbGen, alphaGen );

	Tess_ComputeColor( pStage );
	gl_genericShaderMaterial->SetUniform_Color( tess.svars.color );

	Tess_ComputeTexMatrices( pStage );
	gl_genericShaderMaterial->SetUniform_TextureMatrix( tess.svars.texMatrices[TB_COLORMAP] );

	// u_DeformGen
	gl_genericShaderMaterial->SetUniform_Time( backEnd.refdef.floatTime - backEnd.currentEntity->e.shaderTime );

	// bind u_ColorMap
	if ( pStage->type == stageType_t::ST_STYLELIGHTMAP ) {
		gl_genericShaderMaterial->SetUniform_ColorMapBindless(
			GL_BindToTMU( 0, GetLightMap( drawSurf ) )
		);

		if ( r_texturePacks.Get() ) {
			gl_genericShaderMaterial->SetUniform_ColorMapModifier( GetLightMap( drawSurf )->texturePackModifier );
		}
	} else {
		gl_genericShaderMaterial->SetUniform_ColorMapBindless( BindAnimatedImage( 0, &pStage->bundle[TB_COLORMAP] ) );

		if ( r_texturePacks.Get() ) {
			gl_genericShaderMaterial->SetUniform_ColorMapModifier( tr.currentImage->texturePackModifier );
		}
	}

	bool needDepthMap = pStage->hasDepthFade || shader->autoSpriteMode;
	if ( needDepthMap ) {
		gl_genericShaderMaterial->SetUniform_DepthMapBindless( GL_BindToTMU( 1, tr.currentDepthImage ) );
	}

	bool hasDepthFade = pStage->hasDepthFade && !shader->autoSpriteMode;
	if ( hasDepthFade ) {
		gl_genericShaderMaterial->SetUniform_DepthScale( pStage->depthFadeValue );
	}

	gl_genericShaderMaterial->SetUniform_VertexInterpolation( false );

	gl_genericShaderMaterial->WriteUniformsToBuffer( materials );
}

static void UpdateSurfaceDataLightMapping( uint32_t* materials, Material& material, drawSurf_t* drawSurf, const uint32_t stage ) {
	shader_t* shader = drawSurf->shader;
	shaderStage_t* pStage = &shader->stages[stage];

	const uint32_t paddedOffset = drawSurf->materialsSSBOOffset[stage] * material.shader->GetPaddedSize();
	materials += paddedOffset;

	bool updated = !drawSurf->initialized[stage] || pStage->colorDynamic || pStage->texMatricesDynamic || pStage->dynamic;
	if ( !updated ) {
		return;
	}
	drawSurf->initialized[stage] = true;

	gl_lightMappingShaderMaterial->BindProgram( material.deformIndex );

	gl_lightMappingShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );

	lightMode_t lightMode = lightMode_t::FULLBRIGHT;
	deluxeMode_t deluxeMode = deluxeMode_t::NONE;

	/* TODO: investigate what this is. It's probably a hack to detect some
	specific use case. Without knowing which use case this takes care about,
	any change in the following code may break it. Or it may be a hack we
	should drop if it is for a bug we don't have anymore. */
	bool hack = shader->lastStage != shader->stages
		&& shader->stages[0].rgbGen == colorGen_t::CGEN_VERTEX;

	if ( ( shader->surfaceFlags & SURF_NOLIGHTMAP ) && !hack ) {
		// Use fullbright on “surfaceparm nolightmap” materials.
	} else if ( pStage->type == stageType_t::ST_COLLAPSE_COLORMAP ) {
		/* Use fullbright for collapsed stages without lightmaps,
		for example:

		  {
			map textures/texture_d
			heightMap textures/texture_h
		  }

		This is doable for some complex multi-stage materials. */
	} else if ( drawSurf->bspSurface ) {
		lightMode = tr.worldLight;
		deluxeMode = tr.worldDeluxe;

		if ( lightMode == lightMode_t::MAP ) {
			bool hasLightMap = static_cast<size_t>( drawSurf->lightmapNum() ) < tr.lightmaps.size();

			if ( !hasLightMap ) {
				lightMode = lightMode_t::VERTEX;
				deluxeMode = deluxeMode_t::NONE;
			}
		}
	} else {
		lightMode = tr.modelLight;
		deluxeMode = tr.modelDeluxe;
	}

	// u_Map, u_DeluxeMap
	image_t* lightmap = tr.whiteImage;
	image_t* deluxemap = tr.whiteImage;

	// u_ColorModulate
	colorGen_t rgbGen;
	alphaGen_t alphaGen;
	SetRgbaGen( pStage, &rgbGen, &alphaGen );

	switch ( lightMode ) {
		case lightMode_t::VERTEX:
			// Do not rewrite pStage->rgbGen.
			rgbGen = colorGen_t::CGEN_VERTEX;
			tess.svars.color.SetRed( 0.0f );
			tess.svars.color.SetGreen( 0.0f );
			tess.svars.color.SetBlue( 0.0f );
			break;

		case lightMode_t::GRID:
			// Store lightGrid1 as lightmap,
			// the GLSL code will know how to deal with it.
			lightmap = tr.lightGrid1Image;
			break;

		case lightMode_t::MAP:
			lightmap = GetLightMap( drawSurf );

			break;

		default:
			break;
	}

	switch ( deluxeMode ) {
		case deluxeMode_t::MAP:
			// Deluxe mapping for world surface.
			deluxemap = GetDeluxeMap( drawSurf );
			break;

		case deluxeMode_t::GRID:
			// Deluxe mapping emulation from grid light for game models.
			// Store lightGrid2 as deluxemap,
			// the GLSL code will know how to deal with it.
			deluxemap = tr.lightGrid2Image;
			break;

		default:
			break;
	}

	bool enableGridLighting = ( lightMode == lightMode_t::GRID );
	bool enableGridDeluxeMapping = ( deluxeMode == deluxeMode_t::GRID );

	// TODO: Update this when this is extended to MDV support
	gl_lightMappingShaderMaterial->SetUniform_VertexInterpolation( false );

	if ( glConfig2.dynamicLight ) {
		gl_lightMappingShaderMaterial->SetUniformBlock_Lights( tr.dlightUBO );

		// bind u_LightTiles
		if ( r_dynamicLightRenderer.Get() == Util::ordinal( dynamicLightRenderer_t::TILED ) ) {
			gl_lightMappingShaderMaterial->SetUniform_LightTilesIntBindless(
				GL_BindToTMU( BIND_LIGHTTILES, tr.lighttileRenderImage )
			);
		}
	}

	// u_DeformGen
	gl_lightMappingShaderMaterial->SetUniform_Time( backEnd.refdef.floatTime - backEnd.currentEntity->e.shaderTime );

	// u_InverseLightFactor
	/* HACK: use sign to know if there is a light or not, and
	then if it will receive overbright multiplication or not. */
	bool blendFunc_dstColor = ( pStage->stateBits & GLS_SRCBLEND_BITS ) == GLS_SRCBLEND_DST_COLOR;
	bool noLight = pStage->shaderHasNoLight || lightMode == lightMode_t::FULLBRIGHT;
	float inverseLightFactor = ( noLight && !blendFunc_dstColor ) ? tr.mapInverseLightFactor : -tr.mapInverseLightFactor;
	gl_lightMappingShaderMaterial->SetUniform_InverseLightFactor( inverseLightFactor );

	// u_ColorModulate
	gl_lightMappingShaderMaterial->SetUniform_ColorModulate( rgbGen, alphaGen );

	// u_Color
	Tess_ComputeColor( pStage );
	gl_lightMappingShaderMaterial->SetUniform_Color( tess.svars.color );

	// u_AlphaThreshold
	gl_lightMappingShaderMaterial->SetUniform_AlphaTest( pStage->stateBits );

	// bind u_HeightMap
	if ( pStage->enableReliefMapping ) {
		float depthScale = RB_EvalExpression( &pStage->depthScaleExp, r_reliefDepthScale->value );
		depthScale *= shader->reliefDepthScale;

		gl_lightMappingShaderMaterial->SetUniform_ReliefDepthScale( depthScale );
		gl_lightMappingShaderMaterial->SetUniform_ReliefOffsetBias( shader->reliefOffsetBias );

		// FIXME: if there is both, embedded heightmap in normalmap is used instead of standalone heightmap
		if ( !pStage->hasHeightMapInNormalMap ) {
			gl_lightMappingShaderMaterial->SetUniform_HeightMapBindless(
				GL_BindToTMU( BIND_HEIGHTMAP, pStage->bundle[TB_HEIGHTMAP].image[0] )
			);

			if ( r_texturePacks.Get() ) {
				gl_lightMappingShaderMaterial->SetUniform_HeightMapModifier( pStage->bundle[TB_HEIGHTMAP].image[0]->texturePackModifier );
			}
		}
	}

	// bind u_DiffuseMap
	gl_lightMappingShaderMaterial->SetUniform_DiffuseMapBindless(
		GL_BindToTMU( BIND_DIFFUSEMAP, pStage->bundle[TB_DIFFUSEMAP].image[0] )
	);

	if ( r_texturePacks.Get() ) {
		gl_lightMappingShaderMaterial->SetUniform_DiffuseMapModifier( pStage->bundle[TB_DIFFUSEMAP].image[0]->texturePackModifier );
	}

	if ( pStage->type != stageType_t::ST_LIGHTMAP ) {
		Tess_ComputeTexMatrices( pStage );
		gl_lightMappingShaderMaterial->SetUniform_TextureMatrix( tess.svars.texMatrices[TB_DIFFUSEMAP] );
	}

	// bind u_NormalMap
	if ( !!r_normalMapping->integer || pStage->hasHeightMapInNormalMap ) {
		gl_lightMappingShaderMaterial->SetUniform_NormalMapBindless(
			GL_BindToTMU( BIND_NORMALMAP, pStage->bundle[TB_NORMALMAP].image[0] )
		);

		if ( r_texturePacks.Get() ) {
			gl_lightMappingShaderMaterial->SetUniform_NormalMapModifier( pStage->bundle[TB_NORMALMAP].image[0]->texturePackModifier );
		}
	}

	// bind u_NormalScale
	if ( pStage->enableNormalMapping ) {
		vec3_t normalScale;
		SetNormalScale( pStage, normalScale );

		gl_lightMappingShaderMaterial->SetUniform_NormalScale( normalScale );
	}

	// bind u_MaterialMap
	if ( pStage->enableSpecularMapping || pStage->enablePhysicalMapping ) {
		gl_lightMappingShaderMaterial->SetUniform_MaterialMapBindless(
			GL_BindToTMU( BIND_MATERIALMAP, pStage->bundle[TB_MATERIALMAP].image[0] )
		);

		if ( r_texturePacks.Get() ) {
			gl_lightMappingShaderMaterial->SetUniform_MaterialMapModifier( pStage->bundle[TB_MATERIALMAP].image[0]->texturePackModifier );
		}
	}

	if ( pStage->enableSpecularMapping ) {
		float specExpMin = RB_EvalExpression( &pStage->specularExponentMin, r_specularExponentMin->value );
		float specExpMax = RB_EvalExpression( &pStage->specularExponentMax, r_specularExponentMax->value );

		gl_lightMappingShaderMaterial->SetUniform_SpecularExponent( specExpMin, specExpMax );
	}

	// TODO: Move this to a per-entity buffer
	// specular reflection
	if ( tr.cubeHashTable != nullptr ) {
		cubemapProbe_t* cubeProbeNearest;
		cubemapProbe_t* cubeProbeSecondNearest;

		image_t* cubeMap0 = nullptr;
		image_t* cubeMap1 = nullptr;

		float interpolation = 0.0;

		bool isWorldEntity = backEnd.currentEntity == &tr.worldEntity;

		if ( backEnd.currentEntity && !isWorldEntity ) {
			R_FindTwoNearestCubeMaps( backEnd.currentEntity->e.origin, &cubeProbeNearest, &cubeProbeSecondNearest );
		} else {
			// FIXME position
			R_FindTwoNearestCubeMaps( backEnd.viewParms.orientation.origin, &cubeProbeNearest, &cubeProbeSecondNearest );
		}

		if ( cubeProbeNearest == nullptr && cubeProbeSecondNearest == nullptr ) {
			GLimp_LogComment( "cubeProbeNearest && cubeProbeSecondNearest == NULL\n" );

			cubeMap0 = tr.whiteCubeImage;
			cubeMap1 = tr.whiteCubeImage;
		} else if ( cubeProbeNearest == nullptr ) {
			GLimp_LogComment( "cubeProbeNearest == NULL\n" );

			cubeMap0 = cubeProbeSecondNearest->cubemap;
		} else if ( cubeProbeSecondNearest == nullptr ) {
			GLimp_LogComment( "cubeProbeSecondNearest == NULL\n" );

			cubeMap0 = cubeProbeNearest->cubemap;
		} else {
			float cubeProbeNearestDistance, cubeProbeSecondNearestDistance;

			if ( backEnd.currentEntity && !isWorldEntity ) {
				cubeProbeNearestDistance = Distance( backEnd.currentEntity->e.origin, cubeProbeNearest->origin );
				cubeProbeSecondNearestDistance = Distance( backEnd.currentEntity->e.origin, cubeProbeSecondNearest->origin );
			} else {
				// FIXME position
				cubeProbeNearestDistance = Distance( backEnd.viewParms.orientation.origin, cubeProbeNearest->origin );
				cubeProbeSecondNearestDistance = Distance( backEnd.viewParms.orientation.origin, cubeProbeSecondNearest->origin );
			}

			interpolation = cubeProbeNearestDistance / ( cubeProbeNearestDistance + cubeProbeSecondNearestDistance );

			if ( r_logFile->integer ) {
				GLimp_LogComment( va( "cubeProbeNearestDistance = %f, cubeProbeSecondNearestDistance = %f, interpolation = %f\n",
					cubeProbeNearestDistance, cubeProbeSecondNearestDistance, interpolation ) );
			}

			cubeMap0 = cubeProbeNearest->cubemap;
			cubeMap1 = cubeProbeSecondNearest->cubemap;
		}

		/* TODO: Check why it is required to test for this, why
		cubeProbeNearest->cubemap and cubeProbeSecondNearest->cubemap
		can be nullptr while cubeProbeNearest and cubeProbeSecondNearest
		are not. Maybe this is only required while cubemaps are building. */
		if ( cubeMap0 == nullptr ) {
			cubeMap0 = tr.whiteCubeImage;
		}

		if ( cubeMap1 == nullptr ) {
			cubeMap1 = tr.whiteCubeImage;
		}

		// bind u_EnvironmentMap0
		gl_lightMappingShaderMaterial->SetUniform_EnvironmentMap0Bindless(
			GL_BindToTMU( BIND_ENVIRONMENTMAP0, cubeMap0 )
		);

		// bind u_EnvironmentMap1
		gl_lightMappingShaderMaterial->SetUniform_EnvironmentMap1Bindless(
			GL_BindToTMU( BIND_ENVIRONMENTMAP1, cubeMap1 )
		);

		// bind u_EnvironmentInterpolation
		gl_lightMappingShaderMaterial->SetUniform_EnvironmentInterpolation( interpolation );

		updated = true;
	}

	// bind u_LightMap
	if ( !enableGridLighting ) {
		gl_lightMappingShaderMaterial->SetUniform_LightMapBindless(
			GL_BindToTMU( BIND_LIGHTMAP, lightmap )
		);

		if ( r_texturePacks.Get() ) {
			gl_lightMappingShaderMaterial->SetUniform_LightMapModifier( lightmap->texturePackModifier );
		}
	} else {
		gl_lightMappingShaderMaterial->SetUniform_LightGrid1Bindless( GL_BindToTMU( BIND_LIGHTMAP, lightmap ) );
	}

	// bind u_DeluxeMap
	if ( !enableGridDeluxeMapping ) {
		gl_lightMappingShaderMaterial->SetUniform_DeluxeMapBindless(
			GL_BindToTMU( BIND_DELUXEMAP, deluxemap )
		);

		if ( r_texturePacks.Get() ) {
			gl_lightMappingShaderMaterial->SetUniform_DeluxeMapModifier( deluxemap->texturePackModifier );
		}
	} else {
		gl_lightMappingShaderMaterial->SetUniform_LightGrid2Bindless( GL_BindToTMU( BIND_DELUXEMAP, deluxemap ) );
	}

	// bind u_GlowMap
	if ( !!r_glowMapping->integer ) {
		gl_lightMappingShaderMaterial->SetUniform_GlowMapBindless(
			GL_BindToTMU( BIND_GLOWMAP, pStage->bundle[TB_GLOWMAP].image[0] )
		);

		if ( r_texturePacks.Get() ) {
			gl_lightMappingShaderMaterial->SetUniform_GlowMapModifier( pStage->bundle[TB_GLOWMAP].image[0]->texturePackModifier );
		}
	}

	gl_lightMappingShaderMaterial->WriteUniformsToBuffer( materials );
}

static void UpdateSurfaceDataReflection( uint32_t* materials, Material& material, drawSurf_t* drawSurf, const uint32_t stage ) {
	shader_t* shader = drawSurf->shader;
	shaderStage_t* pStage = &shader->stages[stage];

	const uint32_t paddedOffset = drawSurf->materialsSSBOOffset[stage] * material.shader->GetPaddedSize();
	materials += paddedOffset;

	bool updated = !drawSurf->initialized[stage] || pStage->colorDynamic || pStage->texMatricesDynamic || pStage->dynamic;
	if ( !updated ) {
		return;
	}
	drawSurf->initialized[stage] = true;

	gl_reflectionShaderMaterial->SetUniform_VertexInterpolation( false );

	// bind u_NormalMap
	gl_reflectionShaderMaterial->SetUniform_NormalMapBindless(
		GL_BindToTMU( 1, pStage->bundle[TB_NORMALMAP].image[0] )
	);

	// bind u_ColorMap
	if ( backEnd.currentEntity && ( backEnd.currentEntity != &tr.worldEntity ) ) {
		GL_BindNearestCubeMap( gl_reflectionShaderMaterial->GetUniformLocation_ColorMap(), backEnd.currentEntity->e.origin );
	} else {
		GL_BindNearestCubeMap( gl_reflectionShaderMaterial->GetUniformLocation_ColorMap(), backEnd.viewParms.orientation.origin );
	}

	if ( pStage->enableNormalMapping ) {
		vec3_t normalScale;
		SetNormalScale( pStage, normalScale );

		gl_reflectionShaderMaterial->SetUniform_NormalScale( normalScale );
	}

	// bind u_HeightMap u_depthScale u_reliefOffsetBias
	if ( pStage->enableReliefMapping ) {
		float depthScale = RB_EvalExpression( &pStage->depthScaleExp, r_reliefDepthScale->value );
		float reliefDepthScale = shader->reliefDepthScale;
		depthScale *= reliefDepthScale == 0 ? 1 : reliefDepthScale;
		gl_reflectionShaderMaterial->SetUniform_ReliefDepthScale( depthScale );
		gl_reflectionShaderMaterial->SetUniform_ReliefOffsetBias( shader->reliefOffsetBias );

		// FIXME: if there is both, embedded heightmap in normalmap is used instead of standalone heightmap
		if ( !pStage->hasHeightMapInNormalMap ) {
			gl_reflectionShaderMaterial->SetUniform_HeightMapBindless(
				GL_BindToTMU( 15, pStage->bundle[TB_HEIGHTMAP].image[0] )
			);
		}
	}

	gl_reflectionShaderMaterial->WriteUniformsToBuffer( materials );
}

static void UpdateSurfaceDataSkybox( uint32_t* materials, Material& material, drawSurf_t* drawSurf, const uint32_t stage ) {
	shader_t* shader = drawSurf->shader;
	shaderStage_t* pStage = &shader->stages[stage];

	const uint32_t paddedOffset = drawSurf->materialsSSBOOffset[stage] * material.shader->GetPaddedSize();
	materials += paddedOffset;

	bool updated = !drawSurf->initialized[stage] || pStage->colorDynamic || pStage->texMatricesDynamic || pStage->dynamic;
	if ( !updated ) {
		return;
	}
	drawSurf->initialized[stage] = true;

	gl_skyboxShaderMaterial->BindProgram( material.deformIndex );

	// bind u_ColorMap
	gl_skyboxShaderMaterial->SetUniform_ColorMapCubeBindless(
		GL_BindToTMU( 0, pStage->bundle[TB_COLORMAP].image[0] )
	);

	if ( r_texturePacks.Get() ) {
		gl_skyboxShaderMaterial->SetUniform_CloudMapModifier( pStage->bundle[TB_COLORMAP].image[0]->texturePackModifier );
	}

	// u_AlphaThreshold
	gl_skyboxShaderMaterial->SetUniform_AlphaTest( GLS_ATEST_NONE );

	// u_InverseLightFactor
	gl_skyboxShaderMaterial->SetUniform_InverseLightFactor( tr.mapInverseLightFactor );

	gl_skyboxShaderMaterial->WriteUniformsToBuffer( materials );
}

static void UpdateSurfaceDataScreen( uint32_t* materials, Material& material, drawSurf_t* drawSurf, const uint32_t stage ) {
	shader_t* shader = drawSurf->shader;
	shaderStage_t* pStage = &shader->stages[stage];

	const uint32_t paddedOffset = drawSurf->materialsSSBOOffset[stage] * material.shader->GetPaddedSize();
	materials += paddedOffset;

	bool updated = !drawSurf->initialized[stage] || pStage->colorDynamic || pStage->texMatricesDynamic || pStage->dynamic;
	if ( !updated ) {
		return;
	}
	drawSurf->initialized[stage] = true;

	gl_screenShaderMaterial->BindProgram( pStage->deformIndex );

	// bind u_CurrentMap
	gl_screenShaderMaterial->SetUniform_CurrentMapBindless( BindAnimatedImage( 0, &drawSurf->shader->stages[stage].bundle[TB_COLORMAP] ) );

	gl_screenShaderMaterial->WriteUniformsToBuffer( materials );
}

static void UpdateSurfaceDataHeatHaze( uint32_t* materials, Material& material, drawSurf_t* drawSurf, const uint32_t stage ) {
	shader_t* shader = drawSurf->shader;
	shaderStage_t* pStage = &shader->stages[stage];

	const uint32_t paddedOffset = drawSurf->materialsSSBOOffset[stage] * material.shader->GetPaddedSize();
	materials += paddedOffset;

	bool updated = !drawSurf->initialized[stage] || pStage->colorDynamic || pStage->texMatricesDynamic || pStage->dynamic;
	if ( !updated ) {
		return;
	}
	drawSurf->initialized[stage] = true;

	// bind u_NormalMap
	gl_heatHazeShaderMaterial->SetUniform_NormalMapBindless(
		GL_BindToTMU( 0, pStage->bundle[TB_NORMALMAP].image[0] )
	);

	if ( r_texturePacks.Get() ) {
		gl_heatHazeShader->SetUniform_NormalMapModifier( pStage->bundle[TB_NORMALMAP].image[0]->texturePackModifier );
	}

	float deformMagnitude = RB_EvalExpression( &pStage->deformMagnitudeExp, 1.0 );
	gl_heatHazeShaderMaterial->SetUniform_DeformMagnitude( deformMagnitude );

	if ( pStage->enableNormalMapping ) {
		vec3_t normalScale;
		SetNormalScale( pStage, normalScale );

		// bind u_NormalScale
		gl_heatHazeShaderMaterial->SetUniform_NormalScale( normalScale );
	}

	// bind u_CurrentMap
	gl_heatHazeShaderMaterial->SetUniform_CurrentMapBindless(
		GL_BindToTMU( 1, tr.currentRenderImage[backEnd.currentMainFBO] )
	);

	gl_heatHazeShaderMaterial->WriteUniformsToBuffer( materials );
}

static void UpdateSurfaceDataLiquid( uint32_t* materials, Material& material, drawSurf_t* drawSurf, const uint32_t stage ) {
	shader_t* shader = drawSurf->shader;
	shaderStage_t* pStage = &shader->stages[stage];

	const uint32_t paddedOffset = drawSurf->materialsSSBOOffset[stage] * material.shader->GetPaddedSize();
	materials += paddedOffset;

	bool updated = !drawSurf->initialized[stage] || pStage->colorDynamic || pStage->texMatricesDynamic || pStage->dynamic;
	if ( !updated ) {
		return;
	}
	drawSurf->initialized[stage] = true;

	float fogDensity = RB_EvalExpression( &pStage->fogDensityExp, 0.001 );
	vec4_t fogColor;
	Tess_ComputeColor( pStage );
	VectorCopy( tess.svars.color.ToArray(), fogColor );

	gl_liquidShaderMaterial->SetUniform_RefractionIndex( RB_EvalExpression( &pStage->refractionIndexExp, 1.0 ) );
	gl_liquidShaderMaterial->SetUniform_FresnelPower( RB_EvalExpression( &pStage->fresnelPowerExp, 2.0 ) );
	gl_liquidShaderMaterial->SetUniform_FresnelScale( RB_EvalExpression( &pStage->fresnelScaleExp, 1.0 ) );
	gl_liquidShaderMaterial->SetUniform_FresnelBias( RB_EvalExpression( &pStage->fresnelBiasExp, 0.05 ) );
	gl_liquidShaderMaterial->SetUniform_FogDensity( fogDensity );
	gl_liquidShaderMaterial->SetUniform_FogColor( fogColor );

	gl_liquidShaderMaterial->SetUniform_UnprojectMatrix( backEnd.viewParms.unprojectionMatrix );
	gl_liquidShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );
	gl_liquidShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );

	// NOTE: specular component is computed by shader.
	// FIXME: physical mapping is not implemented.
	if ( pStage->enableSpecularMapping ) {
		float specMin = RB_EvalExpression( &pStage->specularExponentMin, r_specularExponentMin->value );
		float specMax = RB_EvalExpression( &pStage->specularExponentMax, r_specularExponentMax->value );
		gl_liquidShaderMaterial->SetUniform_SpecularExponent( specMin, specMax );
	}

	// bind u_CurrentMap
	gl_liquidShaderMaterial->SetUniform_CurrentMapBindless( GL_BindToTMU( 0, tr.currentRenderImage[backEnd.currentMainFBO] ) );

	// bind u_PortalMap
	gl_liquidShaderMaterial->SetUniform_PortalMapBindless( GL_BindToTMU( 1, tr.portalRenderImage ) );

	// depth texture
	gl_liquidShaderMaterial->SetUniform_DepthMapBindless( GL_BindToTMU( 2, tr.currentDepthImage ) );

	// bind u_HeightMap u_depthScale u_reliefOffsetBias
	if ( pStage->enableReliefMapping ) {
		float depthScale;
		float reliefDepthScale;

		depthScale = RB_EvalExpression( &pStage->depthScaleExp, r_reliefDepthScale->value );
		reliefDepthScale = tess.surfaceShader->reliefDepthScale;
		depthScale *= reliefDepthScale == 0 ? 1 : reliefDepthScale;
		gl_liquidShaderMaterial->SetUniform_ReliefDepthScale( depthScale );
		gl_liquidShaderMaterial->SetUniform_ReliefOffsetBias( tess.surfaceShader->reliefOffsetBias );

		// FIXME: if there is both, embedded heightmap in normalmap is used instead of standalone heightmap
		if ( !pStage->hasHeightMapInNormalMap ) {
			gl_liquidShaderMaterial->SetUniform_HeightMapBindless( GL_BindToTMU( 15, pStage->bundle[TB_HEIGHTMAP].image[0] ) );
		}
	}

	// bind u_NormalMap
	gl_liquidShaderMaterial->SetUniform_NormalMapBindless( GL_BindToTMU( 3, pStage->bundle[TB_NORMALMAP].image[0] ) );

	// bind u_NormalScale
	if ( pStage->enableNormalMapping ) {
		vec3_t normalScale;
		// FIXME: NormalIntensity default was 0.5
		SetNormalScale( pStage, normalScale );

		gl_liquidShaderMaterial->SetUniform_NormalScale( normalScale );
	}

	gl_liquidShaderMaterial->WriteUniformsToBuffer( materials );
}

/*
* Buffer layout:
* // Static surfaces data:
* // Material0
* // Surface/stage0_0:
* uniform0_0
* uniform0_1
* ..
* uniform0_x
* optional_struct_padding
* // Surface/stage0_1:
* ..
* // Surface/stage0_y:
* uniform0_0
* uniform0_1
* ..
* uniform0_x
* optional_struct_padding
* optional_material1_padding
* // Material1
* // Surface/stage1_0:
* ..
* // Surface/stage1_y:
* ..
* ..
* // Materialz:
* ..
* ..
* // Dynamic surfaces data:
* // Same as the static layout
*/
// Buffer is separated into static and dynamic parts so we can just update the whole dynamic range at once
// This will generate the actual buffer with per-stage values AFTER materials are generated
void MaterialSystem::GenerateWorldMaterialsBuffer() {
	Log::Debug( "Generating materials buffer" );

	uint32_t offset = 0;

	materialsSSBO.BindBuffer();

	// Compute data size for static surfaces
	for ( MaterialPack& pack : materialPacks ) {
		for ( Material& material : pack.materials ) {
			// Any new material in the buffer must start on an offset that is an integer multiple of
			// the padded size of the material struct
			const uint32_t paddedSize = material.shader->GetPaddedSize();
			const uint32_t padding = ( offset % paddedSize == 0 ) ? 0 : paddedSize - ( offset % paddedSize );

			offset += padding;
			material.staticMaterialsSSBOOffset = offset;
			offset += paddedSize * material.totalStaticDrawSurfCount;
		}
	}

	dynamicDrawSurfsOffset = offset;

	// Compute data size for dynamic surfaces
	for ( MaterialPack& pack : materialPacks ) {
		for ( Material& material : pack.materials ) {
			// Any new material in the buffer must start on an offset that is an integer multiple of
			// the padded size of the material struct
			const uint32_t paddedSize = material.shader->GetPaddedSize();
			const uint32_t padding = ( offset % paddedSize == 0 ) ? 0 : paddedSize - ( offset % paddedSize );

			offset += padding;
			material.dynamicMaterialsSSBOOffset = offset;
			offset += paddedSize * material.totalDynamicDrawSurfCount;
		}
	}

	dynamicDrawSurfsSize = offset - dynamicDrawSurfsOffset;

	// 4 bytes per component
	glBufferData( GL_SHADER_STORAGE_BUFFER, offset * sizeof( uint32_t ), nullptr, GL_DYNAMIC_DRAW );
	uint32_t* materialsData = materialsSSBO.MapBufferRange( offset );
	memset( materialsData, 0, offset * sizeof( uint32_t ) );

	for ( uint32_t materialPackID = 0; materialPackID < 3; materialPackID++ ) {
		for ( Material& material : materialPacks[materialPackID].materials ) {

			for ( drawSurf_t* drawSurf : material.drawSurfs ) {
				bool hasDynamicStages = false;

				uint32_t stage = 0;
				for ( shaderStage_t* pStage = drawSurf->shader->stages; pStage < drawSurf->shader->lastStage; pStage++ ) {
					if ( drawSurf->materialIDs[stage] != material.id || drawSurf->materialPackIDs[stage] != materialPackID ) {
						stage++;
						continue;
					}
					
					uint32_t SSBOOffset = 0;
					uint32_t drawSurfCount = 0;
					if ( pStage->dynamic ) {
						SSBOOffset = material.dynamicMaterialsSSBOOffset;
						drawSurfCount = material.currentDynamicDrawSurfCount;
						material.currentDynamicDrawSurfCount++;
					} else {
						SSBOOffset = material.staticMaterialsSSBOOffset;
						drawSurfCount = material.currentStaticDrawSurfCount;
						material.currentStaticDrawSurfCount++;
					}

					drawSurf->materialsSSBOOffset[stage] = ( SSBOOffset + drawSurfCount * material.shader->GetPaddedSize() ) /
						material.shader->GetPaddedSize();

					if ( pStage->dynamic ) {
						hasDynamicStages = true;
					}

					switch ( pStage->type ) {
						case stageType_t::ST_COLORMAP:
							// generic2D
							UpdateSurfaceDataGeneric( materialsData, material, drawSurf, stage );
							break;
						case stageType_t::ST_STYLELIGHTMAP:
						case stageType_t::ST_STYLECOLORMAP:
							UpdateSurfaceDataGeneric( materialsData, material, drawSurf, stage );
							break;
						case stageType_t::ST_LIGHTMAP:
						case stageType_t::ST_DIFFUSEMAP:
						case stageType_t::ST_COLLAPSE_COLORMAP:
						case stageType_t::ST_COLLAPSE_DIFFUSEMAP:
							UpdateSurfaceDataLightMapping( materialsData, material, drawSurf, stage );
							break;
						case stageType_t::ST_REFLECTIONMAP:
						case stageType_t::ST_COLLAPSE_REFLECTIONMAP:
							UpdateSurfaceDataReflection( materialsData, material, drawSurf, stage );
							break;
						case stageType_t::ST_REFRACTIONMAP:
						case stageType_t::ST_DISPERSIONMAP:
							// Not implemented yet
							break;
						case stageType_t::ST_SKYBOXMAP:
							UpdateSurfaceDataSkybox( materialsData, material, drawSurf, stage );
							break;
						case stageType_t::ST_SCREENMAP:
							UpdateSurfaceDataScreen( materialsData, material, drawSurf, stage );
							break;
						case stageType_t::ST_PORTALMAP:
							// This is supposedly used for alphagen portal and portal surfaces should never get here
							ASSERT_UNREACHABLE();
							break;
						case stageType_t::ST_HEATHAZEMAP:
							UpdateSurfaceDataHeatHaze( materialsData, material, drawSurf, stage );
							break;
						case stageType_t::ST_LIQUIDMAP:
							UpdateSurfaceDataLiquid( materialsData, material, drawSurf, stage );
							break;

						default:
							break;
					}

					tess.currentDrawSurf = drawSurf;

					tess.currentSSBOOffset = tess.currentDrawSurf->materialsSSBOOffset[stage];
					tess.materialID = tess.currentDrawSurf->materialIDs[stage];
					tess.materialPackID = tess.currentDrawSurf->materialPackIDs[stage];

					tess.multiDrawPrimitives = 0;
					tess.numIndexes = 0;
					tess.numVertexes = 0;
					tess.attribsSet = 0;

					rb_surfaceTable[Util::ordinal( *drawSurf->surface )]( drawSurf->surface );

					pStage->colorRenderer( pStage );

					drawSurf->drawCommandIDs[stage] = lastCommandID;

					if ( pStage->dynamic ) {
						drawSurf->materialsSSBOOffset[stage] = ( SSBOOffset - dynamicDrawSurfsOffset + drawSurfCount *
							material.shader->GetPaddedSize() ) / material.shader->GetPaddedSize();
					}

					stage++;
				}

				if ( hasDynamicStages ) {
					// We need a copy here because the memory pointed to by drawSurf will change later
					// We'll probably need a separate buffer for entities other than world entity + ensure we don't store a drawSurf with
					// invalid pointers
					dynamicDrawSurfs.emplace_back( *drawSurf );
				}
			}
		}
	}

	materialsSSBO.UnmapBuffer();
}

static void SetAttributeLayoutsStatic( vboAttributeLayout_t* attribs ) {
	const GLsizei sizeShaderVertex = sizeof( shaderVertex_t );

	attribs[ATTR_INDEX_POSITION].numComponents = 3;
	attribs[ATTR_INDEX_POSITION].componentType = GL_FLOAT;
	attribs[ATTR_INDEX_POSITION].normalize = GL_FALSE;
	attribs[ATTR_INDEX_POSITION].ofs = offsetof( shaderVertex_t, xyz );
	attribs[ATTR_INDEX_POSITION].stride = sizeShaderVertex;
	attribs[ATTR_INDEX_POSITION].frameOffset = 0;

	attribs[ATTR_INDEX_COLOR].numComponents = 4;
	attribs[ATTR_INDEX_COLOR].componentType = GL_UNSIGNED_BYTE;
	attribs[ATTR_INDEX_COLOR].normalize = GL_TRUE;
	attribs[ATTR_INDEX_COLOR].ofs = offsetof( shaderVertex_t, color );
	attribs[ATTR_INDEX_COLOR].stride = sizeShaderVertex;
	attribs[ATTR_INDEX_COLOR].frameOffset = 0;

	attribs[ATTR_INDEX_QTANGENT].numComponents = 4;
	attribs[ATTR_INDEX_QTANGENT].componentType = GL_SHORT;
	attribs[ATTR_INDEX_QTANGENT].normalize = GL_TRUE;
	attribs[ATTR_INDEX_QTANGENT].ofs = offsetof( shaderVertex_t, qtangents );
	attribs[ATTR_INDEX_QTANGENT].stride = sizeShaderVertex;
	attribs[ATTR_INDEX_QTANGENT].frameOffset = 0;

	attribs[ATTR_INDEX_TEXCOORD].numComponents = 4;
	attribs[ATTR_INDEX_TEXCOORD].componentType = GL_HALF_FLOAT;
	attribs[ATTR_INDEX_TEXCOORD].normalize = GL_FALSE;
	attribs[ATTR_INDEX_TEXCOORD].ofs = offsetof( shaderVertex_t, texCoords );
	attribs[ATTR_INDEX_TEXCOORD].stride = sizeShaderVertex;
	attribs[ATTR_INDEX_TEXCOORD].frameOffset = 0;

	attribs[ATTR_INDEX_ORIENTATION].numComponents = 4;
	attribs[ATTR_INDEX_ORIENTATION].componentType = GL_HALF_FLOAT;
	attribs[ATTR_INDEX_ORIENTATION].normalize = GL_FALSE;
	attribs[ATTR_INDEX_ORIENTATION].ofs = offsetof( shaderVertex_t, spriteOrientation );
	attribs[ATTR_INDEX_ORIENTATION].stride = sizeShaderVertex;
	attribs[ATTR_INDEX_ORIENTATION].frameOffset = 0;
}

// This generates the buffer GLIndirect commands
void MaterialSystem::GenerateWorldCommandBuffer() {
	Log::Debug( "Generating world command buffer" );

	totalBatchCount = 0;

	uint32_t batchOffset = 0;
	uint32_t globalID = 0;
	for ( MaterialPack& pack : materialPacks ) {
		for ( Material& material : pack.materials ) {
			material.surfaceCommandBatchOffset = batchOffset;

			const uint32_t cmdCount = material.drawCommands.size();
			const uint32_t batchCount = cmdCount % SURFACE_COMMANDS_PER_BATCH == 0 ? cmdCount / SURFACE_COMMANDS_PER_BATCH
				: cmdCount / SURFACE_COMMANDS_PER_BATCH + 1;

			material.surfaceCommandBatchOffset = batchOffset;
			material.surfaceCommandBatchCount = batchCount;

			batchOffset += batchCount;
			material.globalID = globalID;

			totalBatchCount += batchCount;
			globalID++;
		}
	}

	Log::Debug( "Total batch count: %u", totalBatchCount );

	skipDrawCommands = true;
	drawSurf_t* drawSurf;

	surfaceDescriptorsSSBO.BindBuffer();
	surfaceDescriptorsCount = totalDrawSurfs;
	descriptorSize = BOUNDING_SPHERE_SIZE + maxStages;
	glBufferData( GL_SHADER_STORAGE_BUFFER, surfaceDescriptorsCount * descriptorSize * sizeof( uint32_t ),
				  nullptr, GL_STATIC_DRAW );
	uint32_t* surfaceDescriptors = surfaceDescriptorsSSBO.MapBufferRange( surfaceDescriptorsCount * descriptorSize );

	culledCommandsCount = totalBatchCount * SURFACE_COMMANDS_PER_BATCH;
	surfaceCommandsCount = totalBatchCount * SURFACE_COMMANDS_PER_BATCH + 1;

	surfaceCommandsSSBO.BindBuffer();
	surfaceCommandsSSBO.BufferStorage( surfaceCommandsCount * SURFACE_COMMAND_SIZE * MAX_VIEWFRAMES, 1, nullptr );
	surfaceCommandsSSBO.MapAll();
	SurfaceCommand* surfaceCommands = ( SurfaceCommand* ) surfaceCommandsSSBO.GetData();
	memset( surfaceCommands, 0, surfaceCommandsCount * sizeof( SurfaceCommand ) * MAX_VIEWFRAMES );

	culledCommandsBuffer.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	culledCommandsBuffer.BufferStorage( GL_SHADER_STORAGE_BUFFER,
		culledCommandsCount * INDIRECT_COMMAND_SIZE * MAX_VIEWFRAMES, 1, nullptr );
	culledCommandsBuffer.MapAll( GL_SHADER_STORAGE_BUFFER );
	GLIndirectBuffer::GLIndirectCommand* culledCommands = ( GLIndirectBuffer::GLIndirectCommand* ) culledCommandsBuffer.GetData();
	memset( culledCommands, 0, culledCommandsCount * sizeof( GLIndirectBuffer::GLIndirectCommand ) * MAX_VIEWFRAMES );
	culledCommandsBuffer.FlushAll( GL_SHADER_STORAGE_BUFFER );

	surfaceBatchesUBO.BindBuffer();
	// Multiply by 2 because we write a uvec2, which is aligned as vec4
	glBufferData( GL_UNIFORM_BUFFER, MAX_SURFACE_COMMAND_BATCHES * 2 * sizeof( SurfaceCommandBatch ), nullptr, GL_STATIC_DRAW );
	SurfaceCommandBatch* surfaceCommandBatches =
		( SurfaceCommandBatch* ) surfaceBatchesUBO.MapBufferRange( MAX_SURFACE_COMMAND_BATCHES * 2 * SURFACE_COMMAND_BATCH_SIZE );

	// memset( (void*) surfaceCommandBatches, 0, MAX_SURFACE_COMMAND_BATCHES * 2 * sizeof( SurfaceCommandBatch ) );
	// Fuck off gcc
	for ( int i = 0; i < MAX_SURFACE_COMMAND_BATCHES * 2; i++ ) {
		surfaceCommandBatches[i] = {};
	}

	uint32_t id = 0;
	uint32_t matID = 0;
	uint32_t subID = 0;
	for ( MaterialPack& pack : materialPacks ) {
		for ( Material& mat : pack.materials ) {
			for ( uint32_t i = 0; i < mat.surfaceCommandBatchCount; i++ ) {
				surfaceCommandBatches[id * 4 + subID].materialIDs[0] = matID;
				surfaceCommandBatches[id * 4 + subID].materialIDs[1] = mat.surfaceCommandBatchOffset;
				subID++;
				if ( subID == 4 ) {
					id++;
					subID = 0;
				}
			}
			matID++;
		}
	}

	atomicCommandCountersBuffer.BindBuffer( GL_ATOMIC_COUNTER_BUFFER );
	atomicCommandCountersBuffer.BufferStorage( GL_ATOMIC_COUNTER_BUFFER,
		MAX_COMMAND_COUNTERS * MAX_VIEWS, MAX_FRAMES, nullptr );
	atomicCommandCountersBuffer.MapAll( GL_ATOMIC_COUNTER_BUFFER );
	uint32_t* atomicCommandCounters = (uint32_t*) atomicCommandCountersBuffer.GetData();
	memset( atomicCommandCounters, 0, MAX_COMMAND_COUNTERS * MAX_VIEWFRAMES * sizeof(uint32_t) );

	//

	drawCommandBuffer.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	glBufferData( GL_SHADER_STORAGE_BUFFER, MAX_MATERIALS * MAX_VIEWFRAMES * INDIRECT_COMMAND_SIZE * sizeof( uint32_t ),
				  nullptr, GL_DYNAMIC_COPY );
	drawCommandBuffer.UnBindBuffer( GL_SHADER_STORAGE_BUFFER );

	clusterIndexesBuffer.BindBuffer();
	glBufferData( GL_SHADER_STORAGE_BUFFER, MAX_BASE_TRIANGLES * 3 * sizeof( uint32_t ),
		nullptr, GL_STATIC_DRAW );
	uint32_t* clusterIndexes = clusterIndexesBuffer.MapBufferRange( MAX_BASE_TRIANGLES * 3 );

	globalIndexesSSBO.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	glBufferData( GL_SHADER_STORAGE_BUFFER, MAX_VIEWFRAME_TRIANGLES * MAX_VIEWFRAMES * 3 * sizeof( uint32_t ),
		nullptr, GL_DYNAMIC_COPY );
	globalIndexesSSBO.UnBindBuffer( GL_SHADER_STORAGE_BUFFER );

	materialIDsSSBO.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	glBufferData( GL_SHADER_STORAGE_BUFFER, MAX_VIEWFRAME_TRIANGLES * MAX_VIEWFRAMES * 3 * sizeof( uint32_t ),
		nullptr, GL_STATIC_DRAW );
	uint32_t* materialIDs = materialIDsSSBO.MapBufferRange( GL_SHADER_STORAGE_BUFFER, MAX_VIEWFRAME_TRIANGLES * MAX_VIEWFRAMES * 3 );

	clustersUBO.BindBuffer( GL_UNIFORM_BUFFER );
	clustersUBO.BufferStorage( GL_UNIFORM_BUFFER, MAX_CLUSTERS_NEW, 1, nullptr );
	clustersUBO.MapAll( GL_UNIFORM_BUFFER );
	uint8_t* baseClusters = ( uint8_t* ) clustersUBO.GetData();
	memset( baseClusters, 0, MAX_CLUSTERS_NEW * sizeof( uint32_t ) );

	clusterSurfaceTypesUBO.BindBuffer();
	clusterSurfaceTypesUBO.BufferStorage( MAX_CLUSTERS_NEW, 1, nullptr );
	clusterSurfaceTypesUBO.MapAll();
	uint32_t* surfaceTypes = ( uint32_t* ) clusterSurfaceTypesUBO.GetData();
	memset( surfaceTypes, 0, MAX_CLUSTERS_NEW * sizeof( uint32_t ) );

	clusterDataSSBO.BindBuffer();
	clusterDataSSBO.BufferStorage( MAX_CLUSTERS_NEW * ( 8 + maxStages ), 1, nullptr );
	clusterDataSSBO.MapAll();
	uint32_t* clusterData = clusterDataSSBO.GetData();
	memset( clusterData, 0, MAX_CLUSTERS_NEW * ( 8 + maxStages ) * sizeof( uint32_t ) );

	culledClustersBuffer.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	glBufferData( GL_SHADER_STORAGE_BUFFER, MAX_CLUSTERS_NEW * MAX_VIEWFRAMES * sizeof( uint32_t ),
		nullptr, GL_DYNAMIC_COPY );
	culledClustersBuffer.UnBindBuffer( GL_SHADER_STORAGE_BUFFER );

	atomicMaterialCountersBuffer.BindBuffer( GL_ATOMIC_COUNTER_BUFFER );
	glBufferData( GL_ATOMIC_COUNTER_BUFFER, MAX_MATERIALS * MAX_VIEWFRAMES * sizeof( uint32_t ),
		nullptr, GL_DYNAMIC_COPY );
	atomicMaterialCountersBuffer.UnBindBuffer( GL_ATOMIC_COUNTER_BUFFER );

	atomicMaterialCountersBuffer2.BindBuffer( GL_ATOMIC_COUNTER_BUFFER );
	glBufferData( GL_ATOMIC_COUNTER_BUFFER, MAX_MATERIALS * MAX_VIEWFRAMES * sizeof( uint32_t ),
		nullptr, GL_DYNAMIC_COPY );
	atomicMaterialCountersBuffer2.UnBindBuffer( GL_ATOMIC_COUNTER_BUFFER );

	clusterCountersBuffer.BindBuffer( GL_ATOMIC_COUNTER_BUFFER );
	glBufferData( GL_ATOMIC_COUNTER_BUFFER, ( MAX_VIEWFRAMES * 2 + MAX_FRAMES ) * sizeof( uint32_t ),
		nullptr, GL_DYNAMIC_COPY );
	clusterCountersBuffer.UnBindBuffer( GL_ATOMIC_COUNTER_BUFFER );

	clusterWorkgroupCountersBuffer.BindBuffer( GL_ATOMIC_COUNTER_BUFFER );
	glBufferData( GL_ATOMIC_COUNTER_BUFFER, MAX_VIEWFRAMES * 3 * sizeof( uint32_t ),
		nullptr, GL_DYNAMIC_COPY );
	clusterWorkgroupCountersBuffer.UnBindBuffer( GL_ATOMIC_COUNTER_BUFFER );

	clusterVertexesBuffer.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	glBufferData( GL_SHADER_STORAGE_BUFFER, MAX_FRAMES * MAX_FRAME_TRIANGLES * 3 * sizeof( shaderVertex_t ),
		nullptr, GL_STATIC_DRAW );
	shaderVertex_t* clusterVertexes =
		(shaderVertex_t*) clusterVertexesBuffer.MapBufferRange( GL_SHADER_STORAGE_BUFFER, MAX_FRAMES * MAX_FRAME_TRIANGLES * 3 * 8 );
	clusterVertexesBuffer.UnBindBuffer( GL_SHADER_STORAGE_BUFFER );

	//

	VBO_t* lastVBO = nullptr;
	IBO_t* lastIBO = nullptr;

	const uint totalVertexCount = GetTotalVertexCount();
	Log::Warn( "total vertexes: %u", totalVertexCount );

	for ( int i = 0; i < tr.refdef.numDrawSurfs; i++ ) {
		drawSurf = &tr.refdef.drawSurfs[i];
		if ( drawSurf->entity != &tr.worldEntity ) {
			continue;
		}

		shader_t* shader = drawSurf->shader;
		if ( !shader ) {
			continue;
		}

		shader = shader->remappedShader ? shader->remappedShader : shader;
		if ( shader->isSky || shader->isPortal ) {
			continue;
		}

		// Don't add SF_SKIP surfaces
		if ( *drawSurf->surface == surfaceType_t::SF_SKIP ) {
			continue;
		}

		tess.multiDrawPrimitives = 0;
		tess.numIndexes = 0;
		tess.numVertexes = 0;
		tess.attribsSet = 0;

		rb_surfaceTable[Util::ordinal( *( drawSurf->surface ) )]( drawSurf->surface );
		// Depth prepass surfaces are added as stages to the main surface instead
		if ( drawSurf->materialSystemSkip ) {
			continue;
		}

		if ( !glState.currentVBO->mapped ) {
			if ( lastVBO != nullptr && lastVBO->mapped ) {
				Tess_UnMapVBO( lastVBO );
			}
			Tess_MapVBO( glState.currentVBO );
			lastVBO = glState.currentVBO;
		}
		if ( !glState.currentIBO->mapped ) {
			if ( lastIBO != nullptr && lastIBO->mapped ) {
				Tess_UnMapIBO( lastIBO );
			}
			Tess_MapIBO( glState.currentIBO );
			lastIBO = glState.currentIBO;
		}

		shaderVertex_t* verts = ( shaderVertex_t* ) lastVBO->data;
		glIndex_t* indices = ( glIndex_t* ) lastIBO->data;

		SurfaceDescriptor surface;
		VectorCopy( ( ( srfGeneric_t* ) drawSurf->surface )->origin, surface.boundingSphere.origin );
		surface.boundingSphere.radius = ( ( srfGeneric_t* ) drawSurf->surface )->radius;

		const bool depthPrePass = drawSurf->depthSurface != nullptr;

		const Material* material = &materialPacks[drawSurf->materialPackIDs[0]].materials[drawSurf->materialIDs[0]];
		const GLIndirectBuffer::GLIndirectCommand& drawCmd = material->drawCommands[drawSurf->drawCommandIDs[0]].cmd;
		GenerateDrawSurfClusters( drawSurf, drawCmd.count, drawCmd.firstIndex,
								  baseClusters, surfaceTypes, clusterData, clusterVertexes, materialIDs, 
								  totalVertexCount, clusterIndexes, lastVBO, lastIBO );

		if ( depthPrePass ) {
			const drawSurf_t* depthDrawSurf = drawSurf->depthSurface;
			const Material* material = &materialPacks[depthDrawSurf->materialPackIDs[0]].materials[depthDrawSurf->materialIDs[0]];
			uint cmdID = material->surfaceCommandBatchOffset * SURFACE_COMMANDS_PER_BATCH + depthDrawSurf->drawCommandIDs[0];
			cmdID++; // Add 1 because the first surface command is always reserved as a fake command
			surface.surfaceCommandIDs[0] = cmdID;

			SurfaceCommand surfaceCommand;
			surfaceCommand.enabled = 0;
			surfaceCommand.drawCommand = material->drawCommands[depthDrawSurf->drawCommandIDs[0]].cmd;
			surfaceCommands[cmdID] = surfaceCommand;
		}

		uint32_t stage = 0;
		for ( shaderStage_t* pStage = drawSurf->shader->stages; pStage < drawSurf->shader->lastStage; pStage++ ) {
			const Material* material = &materialPacks[drawSurf->materialPackIDs[stage]].materials[drawSurf->materialIDs[stage]];
			uint32_t cmdID = material->surfaceCommandBatchOffset * SURFACE_COMMANDS_PER_BATCH + drawSurf->drawCommandIDs[stage];
			cmdID++; // Add 1 because the first surface command is always reserved as a fake command
			surface.surfaceCommandIDs[stage + ( depthPrePass ? 1 : 0 )] = cmdID;

			SurfaceCommand surfaceCommand;
			surfaceCommand.enabled = 0;
			surfaceCommand.drawCommand = material->drawCommands[drawSurf->drawCommandIDs[stage]].cmd;
			surfaceCommands[cmdID] = surfaceCommand;

			stage++;
		}
		memcpy( surfaceDescriptors, &surface, descriptorSize * sizeof( uint32_t ) );
		surfaceDescriptors += descriptorSize;
	}

	debugSSBO.GenBuffer();
	debugSSBO.BindBuffer();
	glBufferData( GL_SHADER_STORAGE_BUFFER, clusterCount * 20 * sizeof( uint32_t ),
		nullptr, GL_STATIC_DRAW );
	uint32_t* debugSurfaces = debugSSBO.MapBufferRange( clusterCount * 20 );
	memset( debugSurfaces, 0, clusterCount * 20 * sizeof( uint32_t ) );
	debugSSBO.UnmapBuffer();

	Log::Warn( "clusters: %u cluster tris: %u total tris: %u", clusterCount, clusterTriangles, totalTriangles );

	if ( lastVBO != nullptr && lastVBO->mapped ) {
		Tess_UnMapVBO( lastVBO );
	}
	
	if ( lastIBO != nullptr && lastIBO->mapped ) {
		Tess_UnMapIBO( lastIBO );
	}

	for ( int i = 0; i < MAX_VIEWFRAMES; i++ ) {
		memcpy( surfaceCommands + surfaceCommandsCount * i, surfaceCommands, surfaceCommandsCount * sizeof( SurfaceCommand ) );
	}

	/* for ( uint i = 0; i < totalVertexCount * totalMaterialCount; i++ ) {
		Log::Warn( "v %u: %f %f %f m: %u", i, clusterVertexes[i].xyz[0], clusterVertexes[i].xyz[1], clusterVertexes[i].xyz[2],
				   materialIDs[i] );
	} */

	SetAttributeLayoutsStatic( clusterVertexLayout );

	surfaceDescriptorsSSBO.BindBuffer();
	surfaceDescriptorsSSBO.UnmapBuffer();

	surfaceCommandsSSBO.BindBuffer();
	surfaceCommandsSSBO.UnmapBuffer();

	culledCommandsBuffer.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	culledCommandsBuffer.UnmapBuffer();

	atomicCommandCountersBuffer.BindBuffer( GL_ATOMIC_COUNTER_BUFFER);
	atomicCommandCountersBuffer.UnmapBuffer();

	surfaceBatchesUBO.BindBuffer();
	surfaceBatchesUBO.UnmapBuffer();

	clusterIndexesBuffer.BindBuffer();
	clusterIndexesBuffer.UnmapBuffer();

	materialIDsSSBO.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	materialIDsSSBO.UnmapBuffer();

	culledClustersBuffer.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	culledClustersBuffer.UnmapBuffer();

	atomicMaterialCountersBuffer.BindBuffer( GL_ATOMIC_COUNTER_BUFFER );
	atomicMaterialCountersBuffer.UnmapBuffer();

	atomicMaterialCountersBuffer2.BindBuffer( GL_ATOMIC_COUNTER_BUFFER );
	atomicMaterialCountersBuffer2.UnmapBuffer();

	clusterCountersBuffer.BindBuffer( GL_ATOMIC_COUNTER_BUFFER );
	clusterCountersBuffer.UnmapBuffer();

	clusterVertexesBuffer.BindBuffer( GL_SHADER_STORAGE_BUFFER );
	clusterVertexesBuffer.UnmapBuffer();

	Log::Notice( "SurfaceTypes: %u", clusterSurfaceTypes.size() );
	for ( const SurfaceType& surfaceType : clusterSurfaceTypes ) {
		std::string surfaceMats = "id: " + std::to_string( surfaceType.id ) + " count: " + std::to_string( surfaceType.count ) + " | ";
		for ( uint i = 0; i < surfaceType.count; i++ ) {
			surfaceMats += std::to_string( surfaceType.materialIDs[i] ) + " ";
		}
		Log::Notice( surfaceMats );
	}

	GL_CheckErrors();
}

uint MaterialSystem::GetTotalVertexCount() {
	uint minIndex = UINT32_MAX;
	uint maxIndex = -UINT32_MAX;
	VBO_t* VBO = nullptr;
	IBO_t* IBO = nullptr;

	for ( int i = 0; i < tr.refdef.numDrawSurfs; i++ ) {
		drawSurf_t* drawSurf = &tr.refdef.drawSurfs[i];
		if ( drawSurf->entity != &tr.worldEntity ) {
			continue;
		}

		shader_t* shader = drawSurf->shader;
		if ( !shader ) {
			continue;
		}

		shader = shader->remappedShader ? shader->remappedShader : shader;
		if ( shader->isSky || shader->isPortal ) {
			continue;
		}

		// Don't add SF_SKIP surfaces
		if ( *drawSurf->surface == surfaceType_t::SF_SKIP ) {
			continue;
		}

		tess.multiDrawPrimitives = 0;
		tess.numIndexes = 0;
		tess.numVertexes = 0;
		tess.attribsSet = 0;

		rb_surfaceTable[Util::ordinal( *( drawSurf->surface ) )]( drawSurf->surface );

		// Depth prepass surfaces are added as stages to the main surface instead
		if ( drawSurf->materialSystemSkip ) {
			continue;
		}

		VBO = glState.currentVBO;
		IBO = glState.currentIBO;

		const Material* material = &materialPacks[drawSurf->materialPackIDs[0]].materials[drawSurf->materialIDs[0]];
		const GLIndirectBuffer::GLIndirectCommand& drawCmd = material->drawCommands[drawSurf->drawCommandIDs[0]].cmd;
		const uint indexCount = drawCmd.count;
		const uint firstIndex = drawCmd.firstIndex;

		totalTriangles += indexCount / 3;
		// Log::Warn( "drawSurf: %s tris: %u", drawSurf->shader->name, indexCount / 3 );

		uint index = 0;
		glIndex_t* indexes = IBO->savedData;
		bool firstCluster = true;
		shaderVertex_t* vertexes = VBO->shaderVertexData;
		while ( index < indexCount ) {
			for ( uint i = 0; i < 3; i++ ) {
				minIndex = indexes[index + firstIndex + i] < minIndex ? indexes[index + firstIndex + i] : minIndex;
				maxIndex = indexes[index + firstIndex + i] > maxIndex ? indexes[index + firstIndex + i] : maxIndex;
			}

			index += 3;
		}
	}

	return maxIndex - minIndex + 1;
}

void MaterialSystem::GenerateDrawSurfClusters( drawSurf_t* drawSurf, const uint indexCount, const uint firstIndex,
											   uint8_t* baseClusters, uint32_t* surfaceTypes, uint32_t* clusterData,
											   shaderVertex_t* clusterVertexes, uint32_t* materialIDs, const uint totalVertexCount,
											   uint32_t* clusterIndexes, VBO_t* VBO, IBO_t* IBO ) {
	SurfaceType surfaceType;

	const bool depthPrePass = drawSurf->depthSurface != nullptr;
	const drawSurf_t* depthDrawSurf = drawSurf->depthSurface;

	const Material* material = &materialPacks[drawSurf->materialPackIDs[0]].materials[drawSurf->materialIDs[0]];
	const GLIndirectBuffer::GLIndirectCommand& drawCmd = material->drawCommands[drawSurf->drawCommandIDs[0]].cmd;

	if ( depthPrePass ) {
		const Material* material = &materialPacks[depthDrawSurf->materialPackIDs[0]].materials[depthDrawSurf->materialIDs[0]];
		surfaceType.materialIDs[0] = material->globalID;
		surfaceType.count++;
	}

	for ( int i = 0; i < MAX_SHADER_STAGES; i++ ) {
		if ( drawSurf->initialized[i] ) {
			surfaceType.materialIDs[i + ( depthPrePass ? 1 : 0 )] =
				materialPacks[drawSurf->materialPackIDs[i]].materials[drawSurf->materialIDs[i]].globalID;
			surfaceType.count++;
		}
	}

	std::vector<SurfaceType>::iterator it = std::find( clusterSurfaceTypes.begin(), clusterSurfaceTypes.end(), surfaceType );

	uint8_t surfaceTypeID = 0;
	if ( it == clusterSurfaceTypes.end() ) {
		surfaceTypeID = surfaceTypeLast;
		surfaceType.id = surfaceTypeID;
		clusterSurfaceTypes.emplace_back( surfaceType );

		surfaceTypes[surfaceTypeLast] = surfaceType.count;
		surfaceTypeLast++;
		for ( uint i = 0; i < surfaceType.count; i++ ) {
			surfaceTypes[surfaceTypeLast] = surfaceType.materialIDs[i];
			surfaceTypeLast++;
		}
	} else {
		surfaceTypeID = it->id;
		surfaceType.id = surfaceTypeID;
	}

	uint index = 0;
	glIndex_t* indexes = IBO->savedData;
	bool firstCluster = true;
	shaderVertex_t* vertexes = VBO->shaderVertexData;
	Log::Warn( "cluster: %u %s: %u material: %u surfaceData: %u",
		clusterCount, drawSurf->shader->name, indexCount, material->globalID, drawCmd.baseInstance );
	Log::Warn( "surfaceTypeID: %u material0: %u material1: %u", surfaceType.id, surfaceType.materialIDs[0], surfaceType.materialIDs[1] );
	while ( index < indexCount ) {
		uint triangleCount = 0;
		const uint clusterIndexOffset = currentBaseClusterIndex;
		BoundingSphere boundingSphere{ 0.0, 0.0, 0.0, 0.0 };
		vec3_t clusterCenter{ 0.0, 0.0, 0.0 };
		vec3_t clusterBBoxMin{ FLT_MAX, FLT_MAX, FLT_MAX };
		vec3_t clusterBBoxMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		uint mat[16];

		// currentClusterVertex = firstIndex;
		uint clusterVertexTemp = currentClusterVertex;

		uint minIndex = UINT32_MAX;
		uint maxIndex = -UINT32_MAX;

		const uint32_t maxTris = ( indexCount - index ) / 3;
		Log::Warn( "index: %u maxTris: %u", index, maxTris );

		while ( triangleCount < std::min( 256u, maxTris ) ) {
			for ( uint i = 0; i < 3; i++ ) {
				clusterIndexes[currentBaseClusterIndex + i] = indexes[index + firstIndex + i];

				clusterVertexes[indexes[index + firstIndex + i]] = vertexes[indexes[index + firstIndex + i]];
				minIndex = indexes[index + firstIndex + i] < minIndex ? indexes[index + firstIndex + i] : minIndex;
				maxIndex = indexes[index + firstIndex + i] > maxIndex ? indexes[index + firstIndex + i] : maxIndex;
				// currentClusterVertex++;

				VectorMin( clusterBBoxMin, vertexes[indexes[index + firstIndex + i]].xyz, clusterBBoxMin );
				VectorMax( clusterBBoxMax, vertexes[indexes[index + firstIndex + i]].xyz, clusterBBoxMax );
			}

			currentBaseClusterIndex += 3;
			index += 3;
			triangleCount++;
		}
		VectorAdd( clusterBBoxMin, clusterBBoxMax, clusterCenter );
		VectorScale( clusterCenter, 0.5, clusterCenter );
		vec3_t tempVec;
		VectorSubtract( clusterBBoxMax, clusterCenter, tempVec );
		float radius = VectorLength( tempVec );
		// Log::Warn( "%f %f %f %f", clusterCenter[0], clusterCenter[1], clusterCenter[2], radius );
		// Log::Warn( "drawSurf: %s cluster: %u tris: %u", drawSurf->shader->name, clusterCount, triangleCount );

		baseClusters[currentBaseCluster] = surfaceTypeID;
		ClusterData data;
		data.baseIndexOffset = clusterIndexOffset; // ( ( triangleCount - 1 ) << 24 ) | clusterIndexOffset;
		data.indexOffset = 0;
		data.entityID = ( triangleCount - 1 ) << 22;
		VectorCopy( clusterCenter, data.boundingSphere.origin );
		data.boundingSphere.radius = radius;

		if ( depthPrePass ) {
			const Material* material = &materialPacks[depthDrawSurf->materialPackIDs[0]].materials[depthDrawSurf->materialIDs[0]];
			data.materials[0] =
				material->drawCommands[depthDrawSurf->drawCommandIDs[0]].cmd.baseInstance;
			mat[0] = material->globalID;
		}

		for ( uint i = 0; i < surfaceType.count - ( depthPrePass ? 1 : 0 ); i++ ) {
			const Material* material = &materialPacks[drawSurf->materialPackIDs[i]].materials[drawSurf->materialIDs[i]];

			data.materials[i + ( depthPrePass ? 1 : 0 )] =
				material->drawCommands[drawSurf->drawCommandIDs[i]].cmd.baseInstance;
			mat[i + ( depthPrePass ? 1 : 0 )] = material->globalID;
		}

		uint vertexCount = maxIndex - minIndex + 1;
		for ( uint i = 0; i < surfaceType.count; i++ ) {
			// memcpy( &clusterVertexes[currentClusterVertex], &clusterVertexes[clusterVertexTemp],
			// 		vertexCount * sizeof( shaderVertex_t ) );
			if ( i > 0 ) {
				memcpy( &clusterVertexes[minIndex + totalVertexCount * mat[i]], &clusterVertexes[minIndex],
					vertexCount * sizeof( shaderVertex_t ) );
			}

			for ( uint j = 0; j < vertexCount; j++ ) {
				materialIDs[minIndex + totalVertexCount * mat[i] + j] = data.materials[i];
			}
			data.materials[i] = totalVertexCount * mat[i];
			currentClusterVertex += vertexCount;
		}
		memcpy( &clusterData[currentBaseCluster * ( 8 + maxStages )], &data, ( 8 + maxStages ) * sizeof( uint32_t ) );
		// Log::Warn( "mat0: %u mat1: %u", data.materials[0], data.materials[1] );

		if ( firstCluster ) {
			drawSurf->baseCluster = currentBaseCluster;
			firstCluster = false;
		}

		/* Log::Warn("surfaceType: %u %u %u %u %u", surfaceType.count, surfaceType.materialIDs[0], surfaceType.materialIDs[1], surfaceType.materialIDs[2],
			surfaceType.materialIDs[3] );
		Log::Warn( "mat: %u %u %u %u", mat[0], mat[1], mat[2], mat[3] ); */

		drawSurf->clusterCount++;
		currentBaseCluster++;
		clusterCount++;
		clusterTriangles += triangleCount;
	}
}

void MaterialSystem::GenerateDepthImages( const int width, const int height, imageParams_t imageParms ) {
	int size = std::max( width, height );
	imageParms.bits ^= ( IF_NOPICMIP | IF_PACKED_DEPTH24_STENCIL8 );
	imageParms.bits |= IF_ONECOMP32F;

	depthImageLevels = 0;
	while ( size > 0 ) {
		depthImageLevels++;
		size >>= 1; // mipmaps round down
	}

	depthImage = R_CreateImage( "_depthImage", nullptr, width, height, depthImageLevels, imageParms );
	GL_Bind( depthImage );
	int mipmapWidth = width;
	int mipmapHeight = height;
	for ( int j = 0; j < depthImageLevels; j++ ) {
		glTexImage2D( GL_TEXTURE_2D, j, GL_R32F, mipmapWidth, mipmapHeight, 0, GL_RED, GL_FLOAT, nullptr );
		mipmapWidth = mipmapWidth > 1 ? mipmapWidth >> 1 : 1;
		mipmapHeight = mipmapHeight > 1 ? mipmapHeight >> 1 : 1;
	}
}

static void BindShaderGeneric( Material* material ) {
	gl_genericShaderMaterial->SetVertexAnimation( material->vertexAnimation );

	gl_genericShaderMaterial->SetTCGenEnvironment( material->tcGenEnvironment );
	gl_genericShaderMaterial->SetTCGenLightmap( material->tcGen_Lightmap );

	gl_genericShaderMaterial->SetDepthFade( material->hasDepthFade );
	gl_genericShaderMaterial->SetVertexSprite( material->vertexSprite );

	gl_genericShaderMaterial->BindProgram( material->deformIndex );
}

static void BindShaderLightMapping( Material* material ) {
	gl_lightMappingShaderMaterial->SetVertexAnimation( material->vertexAnimation );
	gl_lightMappingShaderMaterial->SetBspSurface( material->bspSurface );

	gl_lightMappingShaderMaterial->SetDeluxeMapping( material->enableDeluxeMapping );

	gl_lightMappingShaderMaterial->SetGridLighting( material->enableGridLighting );

	gl_lightMappingShaderMaterial->SetGridDeluxeMapping( material->enableGridDeluxeMapping );

	gl_lightMappingShaderMaterial->SetHeightMapInNormalMap( material->hasHeightMapInNormalMap );

	gl_lightMappingShaderMaterial->SetReliefMapping( material->enableReliefMapping );

	gl_lightMappingShaderMaterial->SetReflectiveSpecular( material->enableNormalMapping && tr.cubeHashTable != nullptr );

	gl_lightMappingShaderMaterial->SetPhysicalShading( material->enablePhysicalMapping );

	gl_lightMappingShaderMaterial->BindProgram( material->deformIndex );
}

static void BindShaderReflection( Material* material ) {
	gl_reflectionShaderMaterial->SetHeightMapInNormalMap( material->hasHeightMapInNormalMap );

	gl_reflectionShaderMaterial->SetReliefMapping( material->enableReliefMapping );

	gl_reflectionShaderMaterial->SetVertexAnimation( material->vertexAnimation );

	gl_reflectionShaderMaterial->BindProgram( material->deformIndex );
}

static void BindShaderSkybox( Material* material ) {
	gl_skyboxShaderMaterial->BindProgram( material->deformIndex );
}

static void BindShaderScreen( Material* material ) {
	gl_screenShaderMaterial->BindProgram( material->deformIndex );
}

static void BindShaderHeatHaze( Material* material ) {
	gl_heatHazeShaderMaterial->SetVertexAnimation( material->vertexAnimation );

	gl_heatHazeShaderMaterial->SetVertexSprite( material->vertexSprite );

	gl_heatHazeShaderMaterial->BindProgram( material->deformIndex );
}

static void BindShaderLiquid( Material* material ) {
	gl_liquidShaderMaterial->SetHeightMapInNormalMap( material->hasHeightMapInNormalMap );

	gl_liquidShaderMaterial->SetReliefMapping( material->enableReliefMapping );

	gl_liquidShaderMaterial->BindProgram( material->deformIndex );
}

// ProcessMaterial*() are essentially same as BindShader*(), but only set the GL program id to the material,
// without actually binding it
static void ProcessMaterialGeneric( Material* material, shaderStage_t* pStage, shader_t* shader ) {
	material->shader = gl_genericShaderMaterial;

	material->vertexAnimation = false;
	material->tcGenEnvironment = pStage->tcGen_Environment;
	material->tcGen_Lightmap = pStage->tcGen_Lightmap;
	material->vertexSprite = shader->autoSpriteMode != 0;
	material->deformIndex = pStage->deformIndex;

	gl_genericShaderMaterial->SetVertexAnimation( false );

	gl_genericShaderMaterial->SetTCGenEnvironment( pStage->tcGen_Environment );
	gl_genericShaderMaterial->SetTCGenLightmap( pStage->tcGen_Lightmap );

	bool hasDepthFade = pStage->hasDepthFade && !shader->autoSpriteMode;
	material->hasDepthFade = hasDepthFade;
	gl_genericShaderMaterial->SetDepthFade( hasDepthFade );
	gl_genericShaderMaterial->SetVertexSprite( shader->autoSpriteMode != 0 );

	material->program = gl_genericShaderMaterial->GetProgram( pStage->deformIndex );
}

static void ProcessMaterialLightMapping( Material* material, shaderStage_t* pStage, drawSurf_t* drawSurf ) {
	material->shader = gl_lightMappingShaderMaterial;

	material->vertexAnimation = false;
	material->bspSurface = false;

	gl_lightMappingShaderMaterial->SetVertexAnimation( false );
	gl_lightMappingShaderMaterial->SetBspSurface( drawSurf->bspSurface );

	lightMode_t lightMode = lightMode_t::FULLBRIGHT;
	deluxeMode_t deluxeMode = deluxeMode_t::NONE;

	bool hack = drawSurf->shader->lastStage != drawSurf->shader->stages
		&& drawSurf->shader->stages[0].rgbGen == colorGen_t::CGEN_VERTEX;
	if ( ( tess.surfaceShader->surfaceFlags & SURF_NOLIGHTMAP ) && !hack ) {
		// Use fullbright on “surfaceparm nolightmap” materials.
	} else if ( pStage->type == stageType_t::ST_COLLAPSE_COLORMAP ) {
		/* Use fullbright for collapsed stages without lightmaps,
		for example:
		  {
			map textures/texture_d
			heightMap textures/texture_h
		  }

		This is doable for some complex multi-stage materials. */
	} else if ( drawSurf->bspSurface ) {
		lightMode = tr.worldLight;
		deluxeMode = tr.worldDeluxe;

		if ( lightMode == lightMode_t::MAP ) {
			bool hasLightMap = ( drawSurf->lightmapNum() >= 0 );

			if ( !hasLightMap ) {
				lightMode = lightMode_t::VERTEX;
				deluxeMode = deluxeMode_t::NONE;
			}
		}
	} else {
		lightMode = tr.modelLight;
		deluxeMode = tr.modelDeluxe;
	}

	bool enableDeluxeMapping = ( deluxeMode == deluxeMode_t::MAP );
	bool enableGridLighting = ( lightMode == lightMode_t::GRID );
	bool enableGridDeluxeMapping = ( deluxeMode == deluxeMode_t::GRID );

	DAEMON_ASSERT( !( enableDeluxeMapping && enableGridDeluxeMapping ) );

	material->enableDeluxeMapping = enableDeluxeMapping;
	material->enableGridLighting = enableGridLighting;
	material->enableGridDeluxeMapping = enableGridDeluxeMapping;
	material->hasHeightMapInNormalMap = pStage->hasHeightMapInNormalMap;
	material->enableReliefMapping = pStage->enableReliefMapping;
	material->enableNormalMapping = pStage->enableNormalMapping && tr.cubeHashTable != nullptr;
	material->enablePhysicalMapping = pStage->enablePhysicalMapping;
	material->deformIndex = pStage->deformIndex;

	gl_lightMappingShaderMaterial->SetDeluxeMapping( enableDeluxeMapping );

	gl_lightMappingShaderMaterial->SetGridLighting( enableGridLighting );

	gl_lightMappingShaderMaterial->SetGridDeluxeMapping( enableGridDeluxeMapping );

	gl_lightMappingShaderMaterial->SetHeightMapInNormalMap( pStage->hasHeightMapInNormalMap );

	gl_lightMappingShaderMaterial->SetReliefMapping( pStage->enableReliefMapping );

	gl_lightMappingShaderMaterial->SetReflectiveSpecular( pStage->enableNormalMapping && tr.cubeHashTable != nullptr );

	gl_lightMappingShaderMaterial->SetPhysicalShading( pStage->enablePhysicalMapping );

	material->program = gl_lightMappingShaderMaterial->GetProgram( pStage->deformIndex );
}

static void ProcessMaterialReflection( Material* material, shaderStage_t* pStage ) {
	material->shader = gl_reflectionShaderMaterial;

	material->hasHeightMapInNormalMap = pStage->hasHeightMapInNormalMap;
	material->enableReliefMapping = pStage->enableReliefMapping;
	material->vertexAnimation = false;
	material->deformIndex = pStage->deformIndex;

	gl_reflectionShaderMaterial->SetHeightMapInNormalMap( pStage->hasHeightMapInNormalMap );

	gl_reflectionShaderMaterial->SetReliefMapping( pStage->enableReliefMapping );

	gl_reflectionShaderMaterial->SetVertexAnimation( false );

	material->program = gl_reflectionShaderMaterial->GetProgram( pStage->deformIndex );
}

static void ProcessMaterialSkybox( Material* material, shaderStage_t* pStage ) {
	material->shader = gl_skyboxShaderMaterial;

	material->deformIndex = pStage->deformIndex;

	material->program = gl_skyboxShaderMaterial->GetProgram( pStage->deformIndex );
}

static void ProcessMaterialScreen( Material* material, shaderStage_t* pStage ) {
	material->shader = gl_screenShaderMaterial;

	material->deformIndex = pStage->deformIndex;

	material->program = gl_screenShaderMaterial->GetProgram( pStage->deformIndex );
}

static void ProcessMaterialHeatHaze( Material* material, shaderStage_t* pStage, shader_t* shader ) {
	material->shader = gl_heatHazeShaderMaterial;

	material->vertexAnimation = false;
	material->deformIndex = pStage->deformIndex;

	gl_heatHazeShaderMaterial->SetVertexAnimation( false );
	if ( shader->autoSpriteMode ) {
		gl_heatHazeShaderMaterial->SetVertexSprite( true );
	} else {
		gl_heatHazeShaderMaterial->SetVertexSprite( false );
	}

	material->program = gl_heatHazeShaderMaterial->GetProgram( pStage->deformIndex );
}

static void ProcessMaterialLiquid( Material* material, shaderStage_t* pStage ) {
	material->shader = gl_liquidShaderMaterial;

	material->hasHeightMapInNormalMap = pStage->hasHeightMapInNormalMap;
	material->enableReliefMapping = pStage->enableReliefMapping;
	material->deformIndex = pStage->deformIndex;

	gl_liquidShaderMaterial->SetHeightMapInNormalMap( pStage->hasHeightMapInNormalMap );

	gl_liquidShaderMaterial->SetReliefMapping( pStage->enableReliefMapping );

	material->program = gl_liquidShaderMaterial->GetProgram( pStage->deformIndex );
}

void MaterialSystem::ProcessStage( drawSurf_t* drawSurf, shaderStage_t* pStage, shader_t* shader, uint* packIDs, uint& stage,
						  uint& previousMaterialID ) {
	Material material;

	uint materialPack = 0;
	if ( shader->sort == Util::ordinal( shaderSort_t::SS_DEPTH ) ) {
		materialPack = 0;
	} else if ( shader->sort >= Util::ordinal( shaderSort_t::SS_ENVIRONMENT_FOG )
		&& shader->sort <= Util::ordinal( shaderSort_t::SS_OPAQUE ) ) {
		materialPack = 1;
	} else {
		materialPack = 2;
	}
	uint32_t id = packIDs[materialPack];

	// In surfaces with multiple stages each consecutive stage must be drawn after the previous stage,
	// except if an opaque stage follows a transparent stage etc.
	if ( stage > 0 ) {
		material.useSync = true;
		material.syncMaterial = previousMaterialID;
	}

	material.stateBits = pStage->stateBits;
	// GLS_ATEST_BITS don't matter here as they don't change GL state
	material.stateBits &= GLS_DEPTHFUNC_BITS | GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_POLYMODE_LINE | GLS_DEPTHTEST_DISABLE
		| GLS_COLORMASK_BITS | GLS_DEPTHMASK_TRUE;
	material.stageType = pStage->type;
	material.cullType = shader->cullType;
	material.usePolygonOffset = shader->polygonOffset;

	material.vbo = glState.currentVBO;
	material.ibo = glState.currentIBO;

	ComputeDynamics( pStage );

	if ( pStage->texturesDynamic ) {
		drawSurf->texturesDynamic[stage] = true;
	}

	AddStageTextures( drawSurf, pStage, &material );

	switch ( pStage->type ) {
		case stageType_t::ST_COLORMAP:
			// generic2D also uses this, but it's for ui only, so skip that for now
			ProcessMaterialGeneric( &material, pStage, drawSurf->shader );
			break;
		case stageType_t::ST_STYLELIGHTMAP:
		case stageType_t::ST_STYLECOLORMAP:
			ProcessMaterialGeneric( &material, pStage, drawSurf->shader );
			break;
		case stageType_t::ST_LIGHTMAP:
		case stageType_t::ST_DIFFUSEMAP:
		case stageType_t::ST_COLLAPSE_COLORMAP:
		case stageType_t::ST_COLLAPSE_DIFFUSEMAP:
			ProcessMaterialLightMapping( &material, pStage, drawSurf );
			break;
		case stageType_t::ST_REFLECTIONMAP:
		case stageType_t::ST_COLLAPSE_REFLECTIONMAP:
			ProcessMaterialReflection( &material, pStage );
			break;
		case stageType_t::ST_REFRACTIONMAP:
		case stageType_t::ST_DISPERSIONMAP:
			// Not implemented yet
			break;
		case stageType_t::ST_SKYBOXMAP:
			ProcessMaterialSkybox( &material, pStage );
			break;
		case stageType_t::ST_SCREENMAP:
			ProcessMaterialScreen( &material, pStage );
			break;
		case stageType_t::ST_PORTALMAP:
			// This is supposedly used for alphagen portal and portal surfaces should never get here
			ASSERT_UNREACHABLE();
			break;
		case stageType_t::ST_HEATHAZEMAP:
			// FIXME: This requires 2 draws per surface stage rather than 1
			ProcessMaterialHeatHaze( &material, pStage, drawSurf->shader );
			break;
		case stageType_t::ST_LIQUIDMAP:
			ProcessMaterialLiquid( &material, pStage );
			break;

		default:
			break;
	}

	std::vector<Material>& materials = materialPacks[materialPack].materials;
	std::vector<Material>::iterator currentSearchIt = materials.begin();
	std::vector<Material>::iterator materialIt;
	// Look for this material in the ones we already have
	while ( true ) {
		materialIt = std::find( currentSearchIt, materials.end(), material );
		if ( materialIt == materials.end() ) {
			break;
		}
		if ( material.useSync && materialIt->id < material.syncMaterial ) {
			currentSearchIt = materialIt + 1;
		} else {
			break;
		}
	}

	// Add it at the back if not found
	if ( materialIt == materials.end() ) {
		material.id = id;
		previousMaterialID = id;
		materials.emplace_back( material );
		id++;
	} else {
		previousMaterialID = materialIt->id;
	}

	pStage->useMaterialSystem = true;
	materials[previousMaterialID].totalDrawSurfCount++;
	if ( pStage->dynamic ) {
		materials[previousMaterialID].totalDynamicDrawSurfCount++;
	} else {
		materials[previousMaterialID].totalStaticDrawSurfCount++;
	}

	if ( std::find( materials[previousMaterialID].drawSurfs.begin(), materials[previousMaterialID].drawSurfs.end(), drawSurf )
		== materials[previousMaterialID].drawSurfs.end() ) {
		materials[previousMaterialID].drawSurfs.emplace_back( drawSurf );
	}

	drawSurf->materialIDs[stage] = previousMaterialID;
	drawSurf->materialPackIDs[stage] = materialPack;

	packIDs[materialPack] = id;

	stage++;
}

/* This will only generate the materials themselves
*  A material represents a distinct global OpenGL state (e. g. blend function, depth test, depth write etc.)
*  Materials can have a dependency on other materials to make sure that consecutive stages are rendered in the proper order */
void MaterialSystem::GenerateWorldMaterials() {
	const int current_r_nocull = r_nocull->integer;
	const int current_r_drawworld = r_drawworld->integer;
	r_nocull->integer = 1;
	r_drawworld->integer = 1;
	generatingWorldCommandBuffer = true;

	Log::Debug( "Generating world materials" );

	R_SyncRenderThread();

	R_AddWorldSurfaces();

	Log::Notice( "World bounds: min: %f %f %f max: %f %f %f", tr.viewParms.visBounds[0][0], tr.viewParms.visBounds[0][1],
		tr.viewParms.visBounds[0][2], tr.viewParms.visBounds[1][0], tr.viewParms.visBounds[1][1], tr.viewParms.visBounds[1][2] );
	VectorCopy( tr.viewParms.visBounds[0], worldViewBounds[0] );
	VectorCopy( tr.viewParms.visBounds[1], worldViewBounds[1] );

	backEnd.currentEntity = &tr.worldEntity;

	drawSurf_t* drawSurf;
	totalDrawSurfs = 0;

	uint32_t previousMaterialID = 0;
	uint32_t packIDs[3] = { 0, 0, 0 };
	skipDrawCommands = true;

	for ( int i = 0; i < tr.refdef.numDrawSurfs; i++ ) {
		drawSurf = &tr.refdef.drawSurfs[i];
		if ( drawSurf->entity != &tr.worldEntity ) {
			continue;
		}

		shader_t* shader = drawSurf->shader;
		if ( !shader ) {
			continue;
		}

		shader = shader->remappedShader ? shader->remappedShader : shader;
		if ( shader->isSky || shader->isPortal ) {
			continue;
		}

		// Don't add SF_SKIP surfaces
		if ( *drawSurf->surface == surfaceType_t::SF_SKIP ) {
			continue;
		}

		tess.multiDrawPrimitives = 0;
		tess.numIndexes = 0;
		tess.numVertexes = 0;
		tess.attribsSet = 0;

		rb_surfaceTable[Util::ordinal( *( drawSurf->surface ) )]( drawSurf->surface );

		// Only add the main surface for surfaces with depth pre-pass to the total count
		if ( !drawSurf->materialSystemSkip ) {
			totalDrawSurfs++;
		}

		uint32_t stage = 0;
		Log::Warn( "%i: %s", i, drawSurf->shader->name );
		for ( shaderStage_t* pStage = drawSurf->shader->stages; pStage < drawSurf->shader->lastStage; pStage++ ) {
			Log::Warn( "stage %u", stage );
			ProcessStage( drawSurf, pStage, shader, packIDs, stage, previousMaterialID );

			uint32_t texturePack = materialPacks[drawSurf->materialPackIDs[stage - 1]].materials[drawSurf->materialIDs[stage - 1]]
				.texturePacks[IH_COLORMAP];
			if ( texturePack != -1 ) {
				for ( const image_t* image : tr.texturePacks[texturePack].images ) {
					Log::Warn( "%s", image->name );
				}
				/* Log::Warn("%s %u: %u %u %u", shader->name, stage,
					texturePack, tr.texturePacks[texturePack].texture->texnum,
					tr.texturePacks[texturePack].texture->texture->bindlessTextureHandle );*/
			} else {
				// Log::Warn( "%s %u: %u", shader->name, stage, texturePack );
			}

			stage++;
		}
	}
	skipDrawCommands = false;

	GenerateWorldMaterialsBuffer();

	totalMaterialCount = 0;
	for ( MaterialPack& pack : materialPacks ) {
		totalMaterialCount += pack.materials.size();
	}
	Log::Notice( "Generated %u materials from %u surfaces", totalMaterialCount, tr.refdef.numDrawSurfs );

	r_nocull->integer = current_r_nocull;
	r_drawworld->integer = current_r_drawworld;
	AddAllWorldSurfaces();

	for ( const MaterialPack& materialPack : materialPacks ) {
		Log::Notice( "materialPack sort: %i %i", Util::ordinal( materialPack.fromSort ), Util::ordinal( materialPack.toSort ) );
		for ( const Material& material : materialPack.materials ) {
			Log::Notice( "id: %u, useSync: %b, sync: %u, program: %i, stateBits: %u, totalDrawSurfCount: %u, shader: %s, vbo: %s, ibo: %s"
				", staticDrawSurfs: %u, dynamicDrawSurfs: %u, culling: %i",
				material.globalID, material.useSync, material.syncMaterial, material.program, material.stateBits, material.totalDrawSurfCount,
				material.shader->GetName(), material.vbo->name, material.ibo->name, material.currentStaticDrawSurfCount,
				material.currentDynamicDrawSurfCount, material.cullType );
			for ( uint32_t i = 0; i < 8; i++ ) {
				Log::Notice( "TexturePack %u: %u texture id: %u", i, material.texturePacks[i],
					material.texturePacks[i] != -1 ? tr.texturePacks[material.texturePacks[i]].texture->texnum : -1 );
			}
		}
	}

	skipDrawCommands = true;
	GeneratePortalBoundingSpheres();
	skipDrawCommands = false;

	generatedWorldCommandBuffer = true;
}

void MaterialSystem::AddAllWorldSurfaces() {
	GenerateWorldCommandBuffer();

	generatingWorldCommandBuffer = false;
}

void MaterialSystem::AddStageTextures( drawSurf_t* drawSurf, shaderStage_t* pStage, Material* material ) {
	for ( const textureBundle_t& bundle : pStage->bundle ) {
		if ( bundle.isVideoMap ) {
			image_t* image = tr.cinematicImage[bundle.videoMapHandle];
			if ( r_texturePacks.Get() && image->assignedTexturePack ) {
				image = tr.texturePacks[image->texturePackImage].texture;
			}
			material->AddTexture( image->texture );
			continue;
		}

		for ( image_t* image : bundle.image ) {
			if ( image ) {
				if ( r_texturePacks.Get() && image->assignedTexturePack ) {
					Log::Warn( "%u %s %u", image->hint, image->name, image->texturePackImage );
					material->texturePacks[image->hint] = image->texturePackImage;
					image = tr.texturePacks[image->texturePackImage].texture;
				}
				material->AddTexture( image->texture );
			}
		}
	}

	// Add lightmap and deluxemap for this surface to the material as well

	lightMode_t lightMode = lightMode_t::FULLBRIGHT;
	deluxeMode_t deluxeMode = deluxeMode_t::NONE;

	bool hack = drawSurf->shader->lastStage != drawSurf->shader->stages
		&& drawSurf->shader->stages[0].rgbGen == colorGen_t::CGEN_VERTEX;

	if ( ( drawSurf->shader->surfaceFlags & SURF_NOLIGHTMAP ) && !hack ) {
		// Use fullbright on “surfaceparm nolightmap” materials.
	} else if ( pStage->type == stageType_t::ST_COLLAPSE_COLORMAP ) {
		/* Use fullbright for collapsed stages without lightmaps,
		for example:

		  {
			map textures/texture_d
			heightMap textures/texture_h
		  }

		This is doable for some complex multi-stage materials. */
	} else if ( drawSurf->bspSurface ) {
		lightMode = tr.worldLight;
		deluxeMode = tr.worldDeluxe;

		if ( lightMode == lightMode_t::MAP ) {
			bool hasLightMap = static_cast< size_t >( drawSurf->lightmapNum() ) < tr.lightmaps.size();

			if ( !hasLightMap ) {
				lightMode = lightMode_t::VERTEX;
				deluxeMode = deluxeMode_t::NONE;
			}
		}
	} else {
		lightMode = tr.modelLight;
		deluxeMode = tr.modelDeluxe;
	}

	// u_Map, u_DeluxeMap
	image_t* lightmap = tr.whiteImage;
	image_t* deluxemap = tr.whiteImage;

	uint32_t lightmapTexPack = 0;
	uint32_t deluxemapTexPack = 0;

	switch ( lightMode ) {
		case lightMode_t::VERTEX:
			break;

		case lightMode_t::GRID:
			lightmap = tr.lightGrid1Image;
			break;

		case lightMode_t::MAP:
			lightmap = GetLightMap( drawSurf );
			if ( r_texturePacks.Get() && lightmap->assignedTexturePack ) {
				lightmapTexPack = lightmap->texturePackImage;
				lightmap = tr.texturePacks[lightmap->texturePackImage].texture;
			}
			break;

		default:
			break;
	}

	switch ( deluxeMode ) {
		case deluxeMode_t::MAP:
			deluxemap = GetDeluxeMap( drawSurf );
			if ( r_texturePacks.Get() && deluxemap->assignedTexturePack ) {
				deluxemapTexPack = deluxemap->texturePackImage;
				deluxemap = tr.texturePacks[deluxemap->texturePackImage].texture;
			}
			break;

		case deluxeMode_t::GRID:
			deluxemap = tr.lightGrid2Image;
			break;

		default:
			break;
	}

	material->AddTexture( lightmap->texture );
	material->AddTexture( deluxemap->texture );

	material->texturePacks[IH_LIGHTMAP] = lightmapTexPack;
	material->texturePacks[IH_DELUXEMAP] = deluxemapTexPack;

	Log::Warn( "lightmap %s %u", lightmap->name, lightmap->texturePackImage );
	Log::Warn( "deluxemap %s %u", deluxemap->name, deluxemap->texturePackImage );

	if ( glConfig2.dynamicLight ) {
		if ( r_dynamicLightRenderer.Get() == Util::ordinal( dynamicLightRenderer_t::TILED ) ) {
			material->AddTexture( tr.lighttileRenderImage->texture );
		}
	}
}

// Dynamic surfaces are those whose values in the SSBO can be updated
void MaterialSystem::UpdateDynamicSurfaces() {
	if ( dynamicDrawSurfsSize == 0 ) {
		return;
	}

	materialsSSBO.BindBuffer();
	uint32_t* materialsData = materialsSSBO.MapBufferRange( dynamicDrawSurfsOffset, dynamicDrawSurfsSize );
	// Shader uniforms are set to 0 if they're not specified, so make sure we do that here too
	memset( materialsData, 0, dynamicDrawSurfsSize * sizeof( uint32_t ) );
	for ( drawSurf_t& drawSurf : dynamicDrawSurfs ) {
		uint32_t stage = 0;
		for ( shaderStage_t* pStage = drawSurf.shader->stages; pStage < drawSurf.shader->lastStage; pStage++ ) {
			Material& material = materialPacks[drawSurf.materialPackIDs[stage]].materials[drawSurf.materialIDs[stage]];

			switch ( pStage->type ) {
				case stageType_t::ST_COLORMAP:
					// generic2D also uses this, but it's for ui only, so skip that for now
					UpdateSurfaceDataGeneric( materialsData, material, &drawSurf, stage );
					break;
				case stageType_t::ST_STYLELIGHTMAP:
				case stageType_t::ST_STYLECOLORMAP:
					UpdateSurfaceDataGeneric( materialsData, material, &drawSurf, stage );
					break;
				case stageType_t::ST_LIGHTMAP:
				case stageType_t::ST_DIFFUSEMAP:
				case stageType_t::ST_COLLAPSE_COLORMAP:
				case stageType_t::ST_COLLAPSE_DIFFUSEMAP:
					UpdateSurfaceDataLightMapping( materialsData, material, &drawSurf, stage );
					break;
				case stageType_t::ST_REFLECTIONMAP:
				case stageType_t::ST_COLLAPSE_REFLECTIONMAP:
					UpdateSurfaceDataReflection( materialsData, material, &drawSurf, stage );
					break;
				case stageType_t::ST_REFRACTIONMAP:
				case stageType_t::ST_DISPERSIONMAP:
					// Not implemented yet
					break;
				case stageType_t::ST_SKYBOXMAP:
					UpdateSurfaceDataSkybox( materialsData, material, &drawSurf, stage );
					break;
				case stageType_t::ST_SCREENMAP:
					UpdateSurfaceDataScreen( materialsData, material, &drawSurf, stage );
					break;
				case stageType_t::ST_PORTALMAP:
					// This is supposedly used for alphagen portal and portal surfaces should never get here
					ASSERT_UNREACHABLE();
					break;
				case stageType_t::ST_HEATHAZEMAP:
					UpdateSurfaceDataHeatHaze( materialsData, material, &drawSurf, stage );
					break;
				case stageType_t::ST_LIQUIDMAP:
					UpdateSurfaceDataLiquid( materialsData, material, &drawSurf, stage );
					break;

				default:
					break;
			}

			stage++;
		}
	}
	materialsSSBO.UnmapBuffer();
}

void MaterialSystem::UpdateFrameData() {
	atomicCommandCountersBuffer.BindBufferBase( GL_SHADER_STORAGE_BUFFER );
	drawCommandBuffer.BindBufferBase( GL_SHADER_STORAGE_BUFFER );
	atomicMaterialCountersBuffer.BindBufferBase( GL_SHADER_STORAGE_BUFFER, 0 );
	atomicMaterialCountersBuffer2.BindBufferBase( GL_SHADER_STORAGE_BUFFER, 1 );
	clusterCountersBuffer.BindBufferBase( GL_SHADER_STORAGE_BUFFER, 2 );
	clusterWorkgroupCountersBuffer.BindBufferBase( GL_SHADER_STORAGE_BUFFER, 3 );

	gl_clearSurfacesShader->BindProgram( 0 );
	gl_clearSurfacesShader->SetUniform_Frame( nextFrame );
	gl_clearSurfacesShader->SetUniform_MaxViewFrameTriangles( MAX_VIEWFRAME_TRIANGLES );
	gl_clearSurfacesShader->DispatchCompute( MAX_MATERIALS / 64 * MAX_VIEWS, 1, 1 );
	
	atomicCommandCountersBuffer.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER );
	drawCommandBuffer.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER );
	atomicMaterialCountersBuffer.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0 );
	atomicMaterialCountersBuffer2.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1 );
	clusterCountersBuffer.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2 );
	clusterWorkgroupCountersBuffer.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3 );

	GL_CheckErrors();
}

void MaterialSystem::QueueSurfaceCull( const uint32_t viewID, const vec3_t origin, const frustum_t* frustum ) {
	VectorCopy( origin, frames[nextFrame].viewFrames[viewID].origin );
	memcpy( frames[nextFrame].viewFrames[viewID].frustum, frustum, sizeof( frustum_t ) );
	frames[nextFrame].viewCount++;
}

void MaterialSystem::DepthReduction() {
	if ( r_lockpvs->integer ) {
		if ( !PVSLocked ) {
			lockedDepthImage = depthImage;
		}

		return;
	}

	int width = depthImage->width;
	int height = depthImage->height;

	gl_depthReductionShader->BindProgram( 0 );

	uint32_t globalWorkgroupX = ( width + 7 ) / 8;
	uint32_t globalWorkgroupY = ( height + 7 ) / 8;

	GL_Bind( tr.currentDepthImage );
	glBindImageTexture( 2, depthImage->texnum, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F );

	gl_depthReductionShader->SetUniform_InitialDepthLevel( true );
	gl_depthReductionShader->SetUniform_ViewWidth( width );
	gl_depthReductionShader->SetUniform_ViewHeight( height );
	gl_depthReductionShader->DispatchCompute( globalWorkgroupX, globalWorkgroupY, 1 );

	for ( int i = 0; i < depthImageLevels; i++ ) {
		width = width > 1 ? width >> 1 : 1;
		height = height > 1 ? height >> 1 : 1;

		globalWorkgroupX = ( width + 7 ) / 8;
		globalWorkgroupY = ( height + 7 ) / 8;

		glBindImageTexture( 1, depthImage->texnum, i, GL_FALSE, 0, GL_READ_ONLY, GL_R32F );
		glBindImageTexture( 2, depthImage->texnum, i + 1, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F );

		gl_depthReductionShader->SetUniform_InitialDepthLevel( false );
		gl_depthReductionShader->SetUniform_ViewWidth( width );
		gl_depthReductionShader->SetUniform_ViewHeight( height );
		gl_depthReductionShader->DispatchCompute( globalWorkgroupX, globalWorkgroupY, 1 );

		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
}

void MaterialSystem::CullSurfaces() {
	if ( r_gpuOcclusionCulling.Get() ) {
		DepthReduction();
	}

	surfaceDescriptorsSSBO.BindBufferBase();
	surfaceCommandsSSBO.BindBufferBase();
	culledCommandsBuffer.BindBufferBase( GL_SHADER_STORAGE_BUFFER );
	surfaceBatchesUBO.BindBufferBase();
	atomicCommandCountersBuffer.BindBufferBase( GL_ATOMIC_COUNTER_BUFFER );

	if ( totalPortals > 0 ) {
		portalSurfacesSSBO.BindBufferBase();
	}

	GL_CheckErrors();
	drawCommandBuffer.BindBufferBase( GL_ATOMIC_COUNTER_BUFFER, 0 );
	clusterIndexesBuffer.BindBufferBase();
	globalIndexesSSBO.BindBufferBase( GL_SHADER_STORAGE_BUFFER );
	clustersUBO.BindBufferBase( GL_UNIFORM_BUFFER );
	clustersUBO.BindBufferBase( GL_SHADER_STORAGE_BUFFER, 12 );
	clusterSurfaceTypesUBO.BindBufferBase();
	clusterDataSSBO.BindBufferBase();
	culledClustersBuffer.BindBufferBase( GL_SHADER_STORAGE_BUFFER );
	atomicMaterialCountersBuffer.BindBufferBase( GL_ATOMIC_COUNTER_BUFFER );
	clusterCountersBuffer.BindBufferBase( GL_ATOMIC_COUNTER_BUFFER );
	clusterWorkgroupCountersBuffer.BindBufferBase( GL_ATOMIC_COUNTER_BUFFER );

	debugSSBO.BindBufferBase();

	gl_cullShader->BindProgram( 0 );
	gl_cullShader->SetUniform_Frame( nextFrame );
	gl_cullShader->SetUniform_TotalDrawSurfs( clusterCount );
	gl_cullShader->SetUniform_MaxViewFrameTriangles( MAX_VIEWFRAME_TRIANGLES );
	gl_cullShader->SetUniform_UseFrustumCulling( r_gpuFrustumCulling.Get() );
	gl_cullShader->SetUniform_UseOcclusionCulling( r_gpuOcclusionCulling.Get() );
	gl_cullShader->SetUniform_ViewWidth( depthImage->width );
	gl_cullShader->SetUniform_ViewHeight( depthImage->height );
	for ( uint32_t view = 0; view < frames[nextFrame].viewCount; view++ ) {
		vec3_t origin;
		frustum_t* frustum = &frames[nextFrame].viewFrames[view].frustum;

		vec4_t frustumPlanes[6];
		for ( int i = 0; i < 6; i++ ) {
			VectorCopy( PVSLocked ? lockedFrustum[i].normal : frustum[0][i].normal, frustumPlanes[i] );
			frustumPlanes[i][3] = PVSLocked ? lockedFrustum[i].dist : frustum[0][i].dist;
		}
		matrix_t viewMatrix;
		if ( PVSLocked ) {
			MatrixCopy( lockedViewMatrix, viewMatrix );
		} else {
			VectorCopy( frames[nextFrame].viewFrames[view].origin, origin );
			MatrixCopy( backEnd.viewParms.world.modelViewMatrix, viewMatrix );
		}

		uint globalWorkGroupX = clusterCount % MAX_COMMAND_COUNTERS == 0 ?
			clusterCount / MAX_COMMAND_COUNTERS : clusterCount / MAX_COMMAND_COUNTERS + 1;
		GL_Bind( depthImage );
		gl_cullShader->SetUniform_FirstPortalGroup( globalWorkGroupX );
		gl_cullShader->SetUniform_TotalPortals( totalPortals );
		gl_cullShader->SetUniform_ViewID( view );
		gl_cullShader->SetUniform_CameraPosition( backEnd.viewParms.pvsOrigin );
		gl_cullShader->SetUniform_ModelViewMatrix( backEnd.viewParms.world.modelViewMatrix );
		gl_cullShader->SetUniform_SurfaceCommandsOffset( surfaceCommandsCount * ( MAX_VIEWS * nextFrame + view ) );
		gl_cullShader->SetUniform_P00( glState.projectionMatrix[glState.stackIndex][0] );
		gl_cullShader->SetUniform_P11( glState.projectionMatrix[glState.stackIndex][5] );

		if ( totalPortals > 0 ) {
			globalWorkGroupX += totalPortals % 64 == 0 ?
				totalPortals / 64 : totalPortals / 64 + 1;
		}

		if ( PVSLocked ) {
			if ( r_lockpvs->integer == 0 ) {
				PVSLocked = false;
			}
		}
		if ( r_lockpvs->integer == 1 && !PVSLocked ) {
			PVSLocked = true;
			for ( int i = 0; i < 6; i++ ) {
				VectorCopy( frustum[0][i].normal, lockedFrustum[i].normal );
				lockedFrustum[i].dist = frustum[0][i].dist;
			}
			MatrixCopy( viewMatrix, lockedViewMatrix );
		}

		gl_cullShader->SetUniform_Frustum( frustumPlanes );

		gl_cullShader->DispatchCompute( globalWorkGroupX, 1, 1 );
	}
	culledClustersBuffer.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER );
	clusterCountersBuffer.UnBindBufferBase( GL_ATOMIC_COUNTER_BUFFER );

	materialIDsSSBO.BindBufferBase( GL_SHADER_STORAGE_BUFFER );
	clusterCountersBuffer.BindBufferBase( GL_UNIFORM_BUFFER, 4 );
	atomicMaterialCountersBuffer.UnBindBufferBase( GL_ATOMIC_COUNTER_BUFFER );
	atomicMaterialCountersBuffer.BindBufferBase( GL_UNIFORM_BUFFER, 5 );
	atomicMaterialCountersBuffer2.BindBufferBase( GL_ATOMIC_COUNTER_BUFFER );
	clusterWorkgroupCountersBuffer.BindBuffer( GL_DISPATCH_INDIRECT_BUFFER );
	clusterVertexesBuffer.BindBufferBase( GL_SHADER_STORAGE_BUFFER );

	gl_processSurfacesShader->BindProgram( 0 );
	gl_processSurfacesShader->SetUniform_Frame( nextFrame );
	gl_processSurfacesShader->SetUniform_MaxViewFrameTriangles( MAX_VIEWFRAME_TRIANGLES );

	glMemoryBarrier( GL_UNIFORM_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT );
	for ( uint view = 0; view < frames[nextFrame].viewCount; view++ ) {
		culledClustersBuffer.BindBufferRange( GL_UNIFORM_BUFFER, 3, ( nextFrame * MAX_VIEWS + view ) * MAX_CLUSTERS_NEW, MAX_CLUSTERS_NEW );

		gl_processSurfacesShader->SetUniform_ViewID( view );
		gl_processSurfacesShader->SetUniform_CameraPosition( backEnd.viewParms.pvsOrigin );
		gl_processSurfacesShader->SetUniform_SurfaceCommandsOffset( surfaceCommandsCount * ( MAX_VIEWS * nextFrame + view ) );
		gl_processSurfacesShader->SetUniform_CulledCommandsOffset( culledCommandsCount * ( MAX_VIEWS * nextFrame + view ) );

		gl_processSurfacesShader->DispatchComputeIndirect( ( MAX_VIEWS * nextFrame + view ) * 3 );
	}

	surfaceDescriptorsSSBO.UnBindBufferBase();
	surfaceCommandsSSBO.UnBindBufferBase();
	culledCommandsBuffer.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER );
	surfaceBatchesUBO.UnBindBufferBase();
	atomicCommandCountersBuffer.UnBindBufferBase( GL_ATOMIC_COUNTER_BUFFER );

	if ( totalPortals > 0 ) {
		portalSurfacesSSBO.UnBindBufferBase();
	}
	drawCommandBuffer.UnBindBufferBase( GL_ATOMIC_COUNTER_BUFFER, 0 );
	clusterIndexesBuffer.UnBindBufferBase();
	globalIndexesSSBO.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER );
	clustersUBO.UnBindBufferBase( GL_UNIFORM_BUFFER );
	clustersUBO.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER, 12 );
	clusterSurfaceTypesUBO.UnBindBufferBase();
	clusterDataSSBO.UnBindBufferBase();
	materialIDsSSBO.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER );

	culledClustersBuffer.UnBindBufferBase( GL_UNIFORM_BUFFER );
	atomicMaterialCountersBuffer.UnBindBufferBase( GL_UNIFORM_BUFFER );
	atomicMaterialCountersBuffer2.UnBindBufferBase( GL_ATOMIC_COUNTER_BUFFER );
	clusterCountersBuffer.UnBindBufferBase( GL_UNIFORM_BUFFER, 5 );
	clusterWorkgroupCountersBuffer.UnBindBufferBase( GL_ATOMIC_COUNTER_BUFFER );
	clusterWorkgroupCountersBuffer.UnBindBuffer( GL_DISPATCH_INDIRECT_BUFFER );
	clusterVertexesBuffer.UnBindBufferBase( GL_SHADER_STORAGE_BUFFER );

	debugSSBO.UnBindBufferBase();

	GL_CheckErrors();
}

void MaterialSystem::StartFrame() {
	if ( !generatedWorldCommandBuffer ) {
		return;
	}
	frames[nextFrame].viewCount = 0;

	// renderedMaterials.clear();
	// UpdateDynamicSurfaces();
	// UpdateFrameData();
}

void MaterialSystem::EndFrame() {
	if ( !generatedWorldCommandBuffer ) {
		return;
	}

	currentFrame = nextFrame;
	nextFrame++;
	if ( nextFrame >= MAX_FRAMES ) {
		nextFrame = 0;
	}

	currentView = 0;
	return;
}

void MaterialSystem::GeneratePortalBoundingSpheres() {
	Log::Debug( "Generating portal bounding spheres" );

	totalPortals = portalSurfacesTmp.size();

	if ( totalPortals == 0 ) {
		return;
	}

	// FIXME: This only requires distance, origin and radius can be moved to surfaceDescriptors SSBO,
	// drawSurfID is not needed as it's the same as the index in portalSurfacesSSBO
	PortalSurface* portalSurfs = new PortalSurface[totalPortals * sizeof( PortalSurface ) * MAX_VIEWFRAMES];

	uint32_t index = 0;
	for ( drawSurf_t* drawSurf : portalSurfacesTmp ) {
		tess.numVertexes = 0;
		rb_surfaceTable[Util::ordinal( *( drawSurf->surface ) )]( drawSurf->surface );
		const int numVerts = tess.numVertexes;
		vec3_t portalCenter{ 0.0, 0.0, 0.0 };
		for ( int vertIndex = 0; vertIndex < numVerts; vertIndex++ ) {
			VectorAdd( portalCenter, tess.verts[vertIndex].xyz, portalCenter );
		}
		VectorScale( portalCenter, 1.0 / numVerts, portalCenter );

		float furthestDistance = 0.0;
		for ( int vertIndex = 0; vertIndex < numVerts; vertIndex++ ) {
			const float distance = Distance( portalCenter, tess.verts[vertIndex].xyz );
			furthestDistance = distance > furthestDistance ? distance : furthestDistance;
		}

		portalSurfaces.emplace_back( *drawSurf );
		PortalSurface sphere;
		VectorCopy( portalCenter, sphere.origin );
		sphere.radius = furthestDistance;
		sphere.drawSurfID = portalSurfaces.size() - 1;
		sphere.distance = -1;

		portalBounds.emplace_back( sphere );
		for ( uint32_t i = 0; i < MAX_FRAMES; i++ ) {
			for ( uint32_t j = 0; j < MAX_VIEWS; j++ ) {
				portalSurfs[index + ( i * MAX_VIEWS + j ) * totalPortals] = sphere;
			}
		}
		index++;
	}

	portalSurfacesSSBO.BindBuffer();
	portalSurfacesSSBO.BufferStorage( totalPortals * PORTAL_SURFACE_SIZE * MAX_VIEWS, 2, portalSurfs );
	portalSurfacesSSBO.MapAll();
	portalSurfacesSSBO.UnBindBuffer();

	portalSurfacesTmp.clear();
}

void MaterialSystem::Free() {
	generatedWorldCommandBuffer = false;

	dynamicDrawSurfs.clear();
	portalSurfaces.clear();
	portalSurfacesTmp.clear();
	portalBounds.clear();
	skyShaders.clear();
	renderedMaterials.clear();

	R_SyncRenderThread();

	surfaceCommandsSSBO.UnmapBuffer();
	culledCommandsBuffer.UnmapBuffer();
	atomicCommandCountersBuffer.UnmapBuffer();

	if ( totalPortals > 0 ) {
		portalSurfacesSSBO.UnmapBuffer();
	}

	currentFrame = 0;
	nextFrame = 1;
	maxStages = 0;

	for ( MaterialPack& pack : materialPacks ) {
		for ( Material& material : pack.materials ) {
			material.drawCommands.clear();
			material.drawSurfs.clear();
		}
		pack.materials.clear();
	}
}

// This gets the information for the surface vertex/index data through Tess
void MaterialSystem::AddDrawCommand( const uint32_t materialID, const uint32_t materialPackID, const uint32_t materialsSSBOOffset,
									 const GLuint count, const GLuint firstIndex ) {
	// Don't add surfaces here if we're just trying to get some VBO/IBO information
	if ( skipDrawCommands ) {
		return;
	}

	cmd.cmd.count = count;
	cmd.cmd.instanceCount = 1;
	cmd.cmd.firstIndex = firstIndex;
	cmd.cmd.baseVertex = 0;
	cmd.cmd.baseInstance = materialsSSBOOffset;
	cmd.materialsSSBOOffset = materialsSSBOOffset;

	materialPacks[materialPackID].materials[materialID].drawCommands.emplace_back(cmd);
	lastCommandID = materialPacks[materialPackID].materials[materialID].drawCommands.size() - 1;
	cmd.textureCount = 0;
}

void MaterialSystem::AddTexture( Texture* texture ) {
	if ( cmd.textureCount > MAX_DRAWCOMMAND_TEXTURES ) {
		Sys::Drop( "Exceeded max DrawCommand textures" );
	}
	cmd.textures[cmd.textureCount] = texture;
	cmd.textureCount++;
}

bool MaterialSystem::AddPortalSurface( uint32_t viewID, PortalSurface* portalSurfs ) {
	uint32_t portalViews[MAX_VIEWS] {};
	uint32_t count = 0;

	frames[nextFrame].viewFrames[viewID].viewCount = 0;
	portalStack[viewID].count = 0;

	PortalSurface* tmpSurfs = new PortalSurface[totalPortals];
	memcpy( tmpSurfs, portalSurfs + viewID * totalPortals, totalPortals * sizeof( PortalSurface ) );
	std::sort( tmpSurfs, tmpSurfs + totalPortals,
		[]( const PortalSurface& lhs, const PortalSurface& rhs ) {
			return lhs.distance < rhs.distance;
		} );

	for ( uint32_t i = 0; i < totalPortals; i++ ) {
		PortalSurface* portalSurface = &tmpSurfs[i];
		if ( portalSurface->distance == -1 ) { // -1 is set if the surface is culled
			continue;
		}
		
		uint32_t portalViewID = viewCount + 1;
		// This check has to be done first so we can correctly determine when we get to MAX_VIEWS - 1 amount of views
		screenRect_t surfRect;
		if( PortalOffScreenOrOutOfRange( &portalSurfaces[portalSurface->drawSurfID], surfRect ) ) {
			if ( portalSurfaces[portalSurface->drawSurfID].shader->portalOutOfRange ) {
				continue;
			}
		}

		if ( portalViewID == MAX_VIEWS ) {
			continue;
		}

		portalViews[count] = portalViewID;
		frames[nextFrame].viewFrames[portalViewID].portalSurfaceID = portalSurface->drawSurfID;
		frames[nextFrame].viewFrames[viewID].viewCount++;

		portalStack[viewID].views[count] = portalViewID;
		portalStack[portalViewID].drawSurf = &portalSurfaces[portalSurface->drawSurfID];
		portalStack[viewID].count++;

		count++;
		viewCount++;

		if ( count == MAX_VIEWS || viewCount == MAX_VIEWS ) {
			return false;
		}

		for ( uint32_t j = 0; j < frames[currentFrame].viewFrames[viewID].viewCount; j++ ) {
			uint32_t subView = frames[currentFrame].viewFrames[viewID].portalViews[j];
			if ( subView != 0 && portalSurface->drawSurfID == frames[currentFrame].viewFrames[subView].portalSurfaceID ) {
				if ( !AddPortalSurface( subView, portalSurfs ) ) {
					return false;
				}

				portalViewID = subView;
				break;
			}
		}
	}

	memcpy( frames[nextFrame].viewFrames[viewID].portalViews, portalViews, MAX_VIEWS * sizeof( uint32_t ) );

	return true;
}

void MaterialSystem::AddPortalSurfaces() {
	if ( totalPortals == 0 ) {
		return;
	}

	portalSurfacesSSBO.BindBufferBase();
	PortalSurface* portalSurfs = ( PortalSurface* ) portalSurfacesSSBO.GetCurrentAreaData();
	viewCount = 0;
	// This will recursively find potentially visible portals in each view based on the data read back from the GPU
	// It only fills up an array up to MAX_VIEWS, the actual views are still added in R_MirrowViewBySurface()
	AddPortalSurface( 0, portalSurfs );
	portalSurfacesSSBO.AreaIncr();
}

void MaterialSystem::VertexAttribsState( uint32_t stateBits ) {
	VertexAttribPointers( stateBits );
	glState.vertexAttribsState = 0;

	for ( uint i = 0; i < ATTR_INDEX_MAX; i++ ) {
		uint32_t bit = BIT( i );
		if ( ( stateBits & bit ) ) {
			if ( r_logFile->integer ) {
				static char buf[MAX_STRING_CHARS];
				Q_snprintf( buf, sizeof( buf ), "glEnableVertexAttribArray( %s )\n", attributeNames[i] );

				GLimp_LogComment( buf );
			}

			glEnableVertexAttribArray( i );
		} else {
			if ( r_logFile->integer ) {
				static char buf[MAX_STRING_CHARS];
				Q_snprintf( buf, sizeof( buf ), "glDisableVertexAttribArray( %s )\n", attributeNames[i] );

				GLimp_LogComment( buf );
			}

			glDisableVertexAttribArray( i );
		}
	}
}

void MaterialSystem::VertexAttribPointers( uint32_t attribBits ) {
	if ( r_logFile->integer ) {
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment( va( "--- GL_VertexAttribPointers( %s ) ---\n", glState.currentVBO->name ) );
	}

	for ( uint i = 0; i < ATTR_INDEX_MAX; i++ ) {
		uint32_t bit = BIT( i );
		if ( ( attribBits & bit ) != 0 ) {
			const vboAttributeLayout_t* layout = &clusterVertexLayout[i];

			if ( r_logFile->integer ) {
				static char buf[MAX_STRING_CHARS];
				Q_snprintf( buf, sizeof( buf ), "glVertexAttribPointer( %s )\n", attributeNames[i] );

				GLimp_LogComment( buf );
			}

			glVertexAttribPointer( i, layout->numComponents, layout->componentType, layout->normalize, layout->stride, BUFFER_OFFSET( layout->ofs ) );
		}
	}
}

void MaterialSystem::RenderMaterials( const shaderSort_t fromSort, const shaderSort_t toSort, const uint32_t viewID ) {
	if ( !r_drawworld->integer ) {
		return;
	}

	if ( frameStart ) {
		renderedMaterials.clear();
		UpdateDynamicSurfaces();
		UpdateFrameData();
		// StartFrame();

		// Make sure compute dispatches from the last frame finished writing to memory
		glMemoryBarrier( GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT );
		frameStart = false;
	}

	materialsSSBO.BindBufferBase();

	for ( MaterialPack& materialPack : materialPacks ) {
		if ( materialPack.fromSort >= fromSort && materialPack.toSort <= toSort ) {
			for ( Material& material : materialPack.materials ) {
				RenderMaterial( material, viewID );
				renderedMaterials.emplace_back( &material );
			}
		}
	}

	// Draw the skybox here because we skipped R_AddWorldSurfaces()
	const bool environmentFogDraw = ( fromSort <= shaderSort_t::SS_ENVIRONMENT_FOG ) && ( toSort >= shaderSort_t::SS_ENVIRONMENT_FOG );
	const bool environmentNoFogDraw = ( fromSort <= shaderSort_t::SS_ENVIRONMENT_NOFOG ) && toSort >= ( shaderSort_t::SS_ENVIRONMENT_NOFOG );
	if ( tr.hasSkybox && ( environmentFogDraw || environmentNoFogDraw ) ) {
		const bool noFogPass = toSort >= shaderSort_t::SS_ENVIRONMENT_NOFOG;
		for ( shader_t* skyShader : skyShaders ) {
			if ( skyShader->noFog != noFogPass ) {
				continue;
			}

			tr.drawingSky = true;
			Tess_Begin( Tess_StageIteratorSky, skyShader, nullptr, false, -1, 0, false );
			Tess_End();
		}
	}
}

void MaterialSystem::RenderMaterial( Material& material, const uint32_t viewID ) {
	backEnd.currentEntity = &tr.worldEntity;

	GL_State( material.stateBits );
	if ( material.usePolygonOffset ) {
		glEnable( GL_POLYGON_OFFSET_FILL );
		GL_PolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	} else {
		glDisable( GL_POLYGON_OFFSET_FILL );
	}
	GL_Cull( material.cullType );

	backEnd.orientation = backEnd.viewParms.world;
	GL_LoadModelViewMatrix( backEnd.orientation.modelViewMatrix );

	switch ( material.stageType ) {
		case stageType_t::ST_COLORMAP:
		case stageType_t::ST_STYLELIGHTMAP:
		case stageType_t::ST_STYLECOLORMAP:
			BindShaderGeneric( &material );

			if ( material.tcGenEnvironment || material.vertexSprite ) {
				gl_genericShaderMaterial->SetUniform_ViewOrigin( backEnd.orientation.viewOrigin );
				gl_genericShaderMaterial->SetUniform_ViewUp( backEnd.orientation.axis[2] );
			}

			gl_genericShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );
			gl_genericShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );

			if ( material.texturePacks[IH_COLORMAP] != -1 ) {
				gl_genericShaderMaterial->SetUniform_ColorMapBindless( GL_BindToTMU( 0, tr.texturePacks[material.texturePacks[IH_COLORMAP]].texture ) );
			}
			break;
		case stageType_t::ST_LIGHTMAP:
		case stageType_t::ST_DIFFUSEMAP:
		case stageType_t::ST_COLLAPSE_COLORMAP:
		case stageType_t::ST_COLLAPSE_DIFFUSEMAP:
			BindShaderLightMapping( &material );
			if ( tr.world ) {
				gl_lightMappingShaderMaterial->SetUniform_LightGridOrigin( tr.world->lightGridGLOrigin );
				gl_lightMappingShaderMaterial->SetUniform_LightGridScale( tr.world->lightGridGLScale );
			}
			// FIXME: else

			gl_lightMappingShaderMaterial->SetUniform_ViewOrigin( backEnd.orientation.viewOrigin );
			gl_lightMappingShaderMaterial->SetUniform_numLights( backEnd.refdef.numLights );
			gl_lightMappingShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );
			gl_lightMappingShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );

			if ( !material.hasHeightMapInNormalMap ) {
				if ( material.texturePacks[IH_HEIGHTMAP] != -1 ) {
					gl_lightMappingShaderMaterial->SetUniform_HeightMapBindless(
						GL_BindToTMU( BIND_HEIGHTMAP, tr.texturePacks[material.texturePacks[IH_HEIGHTMAP]].texture )
					);
				}
			}
			if ( material.texturePacks[IH_COLORMAP] != -1 ) {
				gl_lightMappingShaderMaterial->SetUniform_DiffuseMapBindless(
					GL_BindToTMU( BIND_DIFFUSEMAP, tr.texturePacks[material.texturePacks[IH_COLORMAP]].texture )
				);
			}
			if ( material.texturePacks[IH_NORMALMAP] != -1 ) {
				gl_lightMappingShaderMaterial->SetUniform_NormalMapBindless(
					GL_BindToTMU( BIND_NORMALMAP, tr.texturePacks[material.texturePacks[IH_NORMALMAP]].texture )
				);
			}
			if ( material.texturePacks[IH_MATERIALMAP] != -1 ) {
				gl_lightMappingShaderMaterial->SetUniform_MaterialMapBindless(
					GL_BindToTMU( BIND_MATERIALMAP, tr.texturePacks[material.texturePacks[IH_MATERIALMAP]].texture )
				);
			}
			if ( material.texturePacks[IH_LIGHTMAP] != -1 ) {
				gl_lightMappingShaderMaterial->SetUniform_LightMapBindless(
					GL_BindToTMU( BIND_LIGHTMAP, tr.texturePacks[material.texturePacks[IH_LIGHTMAP]].texture )
				);
			}
			if ( material.texturePacks[IH_DELUXEMAP] != -1 ) {
				gl_lightMappingShaderMaterial->SetUniform_DeluxeMapBindless(
					GL_BindToTMU( BIND_DELUXEMAP, tr.texturePacks[material.texturePacks[IH_DELUXEMAP]].texture )
				);
			}
			if ( material.texturePacks[IH_GLOWMAP] != -1 ) {
				gl_lightMappingShaderMaterial->SetUniform_GlowMapBindless(
					GL_BindToTMU( BIND_GLOWMAP, tr.texturePacks[material.texturePacks[IH_GLOWMAP]].texture )
				);
			}
			break;
		case stageType_t::ST_LIQUIDMAP:
			BindShaderLiquid( &material );
			gl_liquidShaderMaterial->SetUniform_ViewOrigin( backEnd.viewParms.orientation.origin );
			gl_liquidShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );
			gl_liquidShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );
			break;
		case stageType_t::ST_REFLECTIONMAP:
		case stageType_t::ST_COLLAPSE_REFLECTIONMAP:
			BindShaderReflection( &material );
			gl_reflectionShaderMaterial->SetUniform_ViewOrigin( backEnd.viewParms.orientation.origin );
			gl_reflectionShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );
			gl_reflectionShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );
			break;
		case stageType_t::ST_REFRACTIONMAP:
		case stageType_t::ST_DISPERSIONMAP:
			// Not implemented yet
			break;
		case stageType_t::ST_SKYBOXMAP:
			BindShaderSkybox( &material );
			gl_skyboxShaderMaterial->SetUniform_ViewOrigin( backEnd.viewParms.orientation.origin );
			gl_skyboxShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );
			gl_skyboxShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );
			break;
		case stageType_t::ST_SCREENMAP:
			BindShaderScreen( &material );
			gl_screenShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );
			break;
		case stageType_t::ST_PORTALMAP:
			// This is supposedly used for alphagen portal and portal surfaces should never get here
			ASSERT_UNREACHABLE();
			break;
		case stageType_t::ST_HEATHAZEMAP:
			// FIXME: This requires 2 draws per surface stage rather than 1
			BindShaderHeatHaze( &material );

			if ( material.vertexSprite ) {
				gl_heatHazeShaderMaterial->SetUniform_ViewOrigin( backEnd.orientation.viewOrigin );
				gl_heatHazeShaderMaterial->SetUniform_ViewUp( backEnd.orientation.axis[2] );
			}

			gl_heatHazeShaderMaterial->SetUniform_ModelMatrix( backEnd.orientation.transformMatrix );
			gl_heatHazeShaderMaterial->SetUniform_ModelViewProjectionMatrix( glState.modelViewProjectionMatrix[glState.stackIndex] );
			break;
		default:
			break;
	}

	materialIDsSSBO.BindBuffer( GL_ARRAY_BUFFER );
	glEnableVertexAttribArray( 8 );
	// glVertexAttribPointer( 8, 1, GL_UNSIGNED_INT, GL_FALSE, 0,
	// 					   BUFFER_OFFSET( MAX_VIEWFRAME_TRIANGLES * ( MAX_VIEWS * currentFrame + viewID ) * 3 * sizeof( uint32_t ) ) );
	glVertexAttribIPointer( 8, 1, GL_UNSIGNED_INT, 0, BUFFER_OFFSET( 0 ) );
	// glState.currentVBO = nullptr;
	// R_BindVBO( material.vbo );
	// R_BindIBO( material.ibo );
	// material.shader->SetRequiredVertexPointers();

	if ( !material.texturesResident ) {
		for ( Texture* texture : material.textures ) {
			if ( !texture->IsResident() ) {
				texture->MakeResident();

				bool resident = glIsTextureHandleResidentARB( texture->bindlessTextureHandle );

				if ( resident ) {
					continue;
				}

				for ( Material* mat : renderedMaterials ) {
					Log::Warn( "Making material %u textures non-resident (%u)", mat->id, mat->textures.size() );
					for ( Texture* tex : mat->textures ) {
						if ( tex->IsResident() ) {
							tex->MakeNonResident();
						}
					}
					mat->texturesResident = false;
				}

				texture->MakeResident();

				resident = glIsTextureHandleResidentARB( texture->bindlessTextureHandle );

				if( !resident ) {
					Log::Warn( "Not enough texture space! Some textures may be missing" );
					break;
				}
			}
		}
	}
	material.texturesResident = true;

	// culledCommandsBuffer.BindBuffer( GL_DRAW_INDIRECT_BUFFER );

	// atomicCommandCountersBuffer.BindBuffer( GL_PARAMETER_BUFFER_ARB );

	drawCommandBuffer.BindBuffer( GL_DRAW_INDIRECT_BUFFER );
	globalIndexesSSBO.BindBuffer( GL_ELEMENT_ARRAY_BUFFER );
	clusterVertexesBuffer.BindBuffer( GL_ARRAY_BUFFER );
	// GL_VertexAttribsState( ATTR_POSITION | ATTR_TEXCOORD | ATTR_QTANGENT | ATTR_COLOR, true );
	VertexAttribsState( ATTR_POSITION | ATTR_TEXCOORD | ATTR_QTANGENT | ATTR_COLOR );
	glState.currentVBO = nullptr;
	glState.currentIBO = nullptr;

	/* glMultiDrawElementsIndirectCountARB(GL_TRIANGLES, GL_UNSIGNED_INT,
		BUFFER_OFFSET( material.surfaceCommandBatchOffset * SURFACE_COMMANDS_PER_BATCH * sizeof( GLIndirectBuffer::GLIndirectCommand )
					   + ( culledCommandsCount * ( MAX_VIEWS * currentFrame + viewID )
					   * sizeof( GLIndirectBuffer::GLIndirectCommand ) ) ),
		material.globalID * sizeof( uint32_t )
		+ ( MAX_COMMAND_COUNTERS * ( MAX_VIEWS * currentFrame + viewID ) ) * sizeof( uint32_t ),
		material.drawCommands.size(), 0 ); */
	glMultiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_INT,
		BUFFER_OFFSET( ( material.globalID + MAX_MATERIALS * MAX_VIEWS * currentFrame ) * sizeof( GLIndirectBuffer::GLIndirectCommand ) ),
		1, 0 );

	if( r_showTris->integer && ( material.stateBits & GLS_DEPTHMASK_TRUE ) == 0 ) {
		switch ( material.stageType ) {
			case stageType_t::ST_LIGHTMAP:
			case stageType_t::ST_DIFFUSEMAP:
			case stageType_t::ST_COLLAPSE_COLORMAP:
			case stageType_t::ST_COLLAPSE_DIFFUSEMAP:
				gl_lightMappingShaderMaterial->SetUniform_ShowTris( 1 );
				GL_State( GLS_DEPTHTEST_DISABLE );
				/* glMultiDrawElementsIndirectCountARB(GL_LINES, GL_UNSIGNED_INT,
					BUFFER_OFFSET( material.surfaceCommandBatchOffset * SURFACE_COMMANDS_PER_BATCH * sizeof( GLIndirectBuffer::GLIndirectCommand )
					+ ( culledCommandsCount * ( MAX_VIEWS * currentFrame + viewID )
					* sizeof( GLIndirectBuffer::GLIndirectCommand ) ) ),
					material.globalID * sizeof( uint32_t )
					+ ( MAX_COMMAND_COUNTERS * ( MAX_VIEWS * currentFrame + viewID ) ) * sizeof( uint32_t ),
					material.drawCommands.size(), 0 ); */
				glMultiDrawElementsIndirect( GL_LINES, GL_UNSIGNED_INT,
					BUFFER_OFFSET( ( material.globalID + MAX_MATERIALS * MAX_VIEWS * currentFrame ) * sizeof( GLIndirectBuffer::GLIndirectCommand ) ),
					1, 0 );
				gl_lightMappingShaderMaterial->SetUniform_ShowTris( 0 );
			default:
				break;
		}
	}

	glDisableVertexAttribArray( 8 );
	
	drawCommandBuffer.UnBindBuffer( GL_DRAW_INDIRECT_BUFFER );
	globalIndexesSSBO.UnBindBuffer( GL_ELEMENT_ARRAY_BUFFER );

	// culledCommandsBuffer.UnBindBuffer( GL_DRAW_INDIRECT_BUFFER );

	// atomicCommandCountersBuffer.UnBindBuffer( GL_PARAMETER_BUFFER_ARB );

	if ( material.usePolygonOffset ) {
		glDisable( GL_POLYGON_OFFSET_FILL );
	}
}
