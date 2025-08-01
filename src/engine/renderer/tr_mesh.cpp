/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2011 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of Daemon source code.

Daemon source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Daemon source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_mesh.c -- triangle model functions
#include "tr_local.h"
#include "GeometryOptimiser.h"

/*
=============
R_CullMDV
=============
*/
static cullResult_t R_CullMDV( mdvModel_t *model, trRefEntity_t *ent )
{
	mdvFrame_t *oldFrame, *newFrame;
	int        i;

	// compute frame pointers
	newFrame = model->frames + ent->e.frame;
	oldFrame = model->frames + ent->e.oldframe;

	// calculate a bounding box in the current coordinate system
	for ( i = 0; i < 3; i++ )
	{
		ent->localBounds[ 0 ][ i ] =
		  oldFrame->bounds[ 0 ][ i ] < newFrame->bounds[ 0 ][ i ] ? oldFrame->bounds[ 0 ][ i ] : newFrame->bounds[ 0 ][ i ];
		ent->localBounds[ 1 ][ i ] =
		  oldFrame->bounds[ 1 ][ i ] > newFrame->bounds[ 1 ][ i ] ? oldFrame->bounds[ 1 ][ i ] : newFrame->bounds[ 1 ][ i ];
	}

	// setup world bounds for intersection tests
	R_SetupEntityWorldBounds(ent);

	// cull bounding sphere ONLY if this is not an upscaled entity
	if ( !ent->e.nonNormalizedAxes )
	{
		if ( ent->e.frame == ent->e.oldframe )
		{
			switch ( R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius ) )
			{
				case cullResult_t::CULL_OUT:
					tr.pc.c_sphere_cull_mdv_out++;
					return cullResult_t::CULL_OUT;

				case cullResult_t::CULL_IN:
					tr.pc.c_sphere_cull_mdv_in++;
					return cullResult_t::CULL_IN;

				case cullResult_t::CULL_CLIP:
					tr.pc.c_sphere_cull_mdv_clip++;
					break;
			}
		}
		else
		{
			cullResult_t sphereCullB;
			cullResult_t sphereCull = R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius );

			if ( newFrame == oldFrame )
			{
				sphereCullB = sphereCull;
			}
			else
			{
				sphereCullB = R_CullLocalPointAndRadius( oldFrame->localOrigin, oldFrame->radius );
			}

			if ( sphereCull == sphereCullB )
			{
				if ( sphereCull == cullResult_t::CULL_OUT )
				{
					tr.pc.c_sphere_cull_mdv_out++;
					return cullResult_t::CULL_OUT;
				}
				else if ( sphereCull == cullResult_t::CULL_IN )
				{
					tr.pc.c_sphere_cull_mdv_in++;
					return cullResult_t::CULL_IN;
				}
				else
				{
					tr.pc.c_sphere_cull_mdv_clip++;
				}
			}
		}
	}

	switch ( R_CullBox( ent->worldBounds ) )
	{
		case cullResult_t::CULL_IN:
			tr.pc.c_box_cull_mdv_in++;
			return cullResult_t::CULL_IN;

		case cullResult_t::CULL_CLIP:
			tr.pc.c_box_cull_mdv_clip++;
			return cullResult_t::CULL_CLIP;

		case cullResult_t::CULL_OUT:
		default:
			tr.pc.c_box_cull_mdv_out++;
			return cullResult_t::CULL_OUT;
	}
}

/*
=================
R_ComputeLOD
=================
*/
int R_ComputeLOD( trRefEntity_t *ent )
{
	float      radius;
	float      flod, lodscale;
	float      projectedRadius;
	mdvFrame_t *frame;
	int        lod;

	if ( tr.currentModel->numLods < 2 )
	{
		// model has only 1 LOD level, skip computations and bias
		lod = 0;
	}
	else
	{
		// multiple LODs exist, so compute projected bounding sphere
		// and use that as a criteria for selecting LOD

		frame = tr.currentModel->mdv[ 0 ]->frames;
		frame += ent->e.frame;

		radius = RadiusFromBounds( frame->bounds[ 0 ], frame->bounds[ 1 ] );

		if ( ( projectedRadius = R_ProjectRadius( radius, ent->e.origin ) ) != 0 )
		{
			lodscale = r_lodScale->value;

			if ( lodscale > 20 )
			{
				lodscale = 20;
			}

			flod = 1.0f - projectedRadius * lodscale;
		}
		else
		{
			// object intersects near view plane, e.g. view weapon
			flod = 0;
		}

		flod *= tr.currentModel->numLods;
		lod = Q_ftol( flod );

		if ( lod < 0 )
		{
			lod = 0;
		}
		else if ( lod >= tr.currentModel->numLods )
		{
			lod = tr.currentModel->numLods - 1;
		}
	}

	lod += r_lodBias->integer;

	if ( lod >= tr.currentModel->numLods )
	{
		lod = tr.currentModel->numLods - 1;
	}

	if ( lod < 0 )
	{
		lod = 0;
	}

	return lod;
}

static shader_t *GetMDVSurfaceShader( const trRefEntity_t *ent, mdvSurface_t *mdvSurface )
{
	shader_t *shader = nullptr;

	if ( ent->e.customShader )
	{
		shader = R_GetShaderByHandle( ent->e.customShader );
	}
	else if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins )
	{
		skin_t *skin;
		int    j;

		skin = R_GetSkinByHandle( ent->e.customSkin );

		// match the surface name to something in the skin file
		shader = tr.defaultShader;

		for ( j = 0; j < skin->numSurfaces; j++ )
		{
			// the names have both been lowercased
			if ( !strcmp( skin->surfaces[ j ]->name, mdvSurface->name ) )
			{
				shader = skin->surfaces[ j ]->shader;
				break;
			}
		}

		if ( shader == tr.defaultShader )
		{
			Log::Warn("no shader for surface %s in skin %s", mdvSurface->name, skin->name );
		}
		else if ( shader->defaultShader )
		{
			Log::Warn("shader %s in skin %s not found", shader->name, skin->name );
		}
	}
	else
	{
		shader = mdvSurface->shader;
	}

	return shader;
}

/*
=================
R_AddMDVSurfaces
=================
*/
void R_AddMDVSurfaces( trRefEntity_t *ent )
{
	mdvModel_t   *model = nullptr;
	mdvSurface_t *mdvSurface = nullptr;
	int          lod;
	bool     personalModel;
	int          fogNum;

	// don't add third_person objects if not in a portal
	personalModel = ( ent->e.renderfx & RF_THIRD_PERSON ) &&
	  tr.viewParms.portalLevel == 0;

	if ( ent->e.renderfx & RF_WRAP_FRAMES )
	{
		ent->e.frame %= tr.currentModel->mdv[ 0 ]->numFrames;
		ent->e.oldframe %= tr.currentModel->mdv[ 0 ]->numFrames;
	}

	// compute LOD
	if ( ent->e.renderfx & RF_FORCENOLOD )
	{
		lod = 0;
	}
	else
	{
		lod = R_ComputeLOD( ent );
	}

	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	if ( ( ent->e.frame >= tr.currentModel->mdv[ lod ]->numFrames )
	     || ( ent->e.frame < 0 ) || ( ent->e.oldframe >= tr.currentModel->mdv[ lod ]->numFrames ) || ( ent->e.oldframe < 0 ) )
	{
		Log::Debug("R_AddMDVSurfaces: no such frame %d to %d for '%s' (%d)",
		           ent->e.oldframe, ent->e.frame, tr.currentModel->name, tr.currentModel->mdv[ lod ]->numFrames );
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}

	model = tr.currentModel->mdv[ lod ];

	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	if ( R_CullMDV( model, ent ) == CULL_OUT )
	{
		return;
	}

	// see if we are in a fog volume
	fogNum = R_FogWorldBox( ent->worldBounds );

	// draw all surfaces
	if ( r_vboModels.Get() && model->numVBOSurfaces )
	{
		srfVBOMDVMesh_t *vboSurface;

		for ( int i = 0; i < model->numVBOSurfaces; i++ )
		{
			vboSurface = model->vboSurfaces[ i ];
			mdvSurface = vboSurface->mdvSurface;

			shader_t *shader = GetMDVSurfaceShader( ent, mdvSurface );

			// don't add third_person objects if not viewing through a portal
			if ( !personalModel )
			{
				R_AddDrawSurf( ( surfaceType_t * ) vboSurface, shader, -1, fogNum );
			}
		}
	}
	else
	{
		int i;
		for ( i = 0, mdvSurface = model->surfaces; i < model->numSurfaces; i++, mdvSurface++ )
		{
			shader_t *shader = GetMDVSurfaceShader( ent, mdvSurface );

			// we will add shadows even if the main object isn't visible in the view

			// don't add third_person objects if not viewing through a portal
			if ( !personalModel )
			{
				R_AddDrawSurf( ( surfaceType_t * ) mdvSurface, shader, -1, fogNum );
			}
		}
	}
}

void MarkShaderBuildMDV( const mdvModel_t* model ) {
	for ( int i = 0; i < model->numVBOSurfaces; i++ ) {
		srfVBOMDVMesh_t* surface = model->vboSurfaces[i];
		MarkShaderBuild( surface->mdvSurface->shader, -1, false, false, true );

		for ( int j = 0; j < tr.numSkins; j++ ) {
			skin_t* skin = tr.skins[j];
			for ( int k = 0; k < skin->numSurfaces; k++ ) {
				// the names have both been lowercased
				if ( !strcmp( skin->surfaces[k]->name, surface->mdvSurface->name ) ) {
					MarkShaderBuild( skin->surfaces[k]->shader, -1, false, false, true );
				}
			}
		}
	}
}
