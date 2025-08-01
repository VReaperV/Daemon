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
// tr_models.c -- model loading and caching
#include "tr_local.h"
#include "GLUtils.h"

#define LL(x) x = LittleLong(x)
#define LF(x) x = LittleFloat(x)

bool R_LoadMD3( model_t *mod, int lod, const void *buffer, const char *name );
bool R_LoadMD5( model_t *mod, const char *buffer, const char *name );
bool R_LoadIQModel( model_t *mod, const void *buffer, int bufferSize, const char *name );

/*
** R_GetModelByHandle
*/
model_t        *R_GetModelByHandle( qhandle_t index )
{
	model_t *mod;

	// out of range gets the default model
	if ( index < 0 || index >= tr.numModels )
	{
		Log::Warn("R_GetModelByHandle: index=%d out of range", index);
		return tr.models[ 0 ];
	}

	mod = tr.models[ index ];

	return mod;
}

//===============================================================================

/*
** R_AllocModel
*/
model_t        *R_AllocModel()
{
	model_t *mod;

	if ( tr.numModels == MAX_MOD_KNOWN )
	{
		return nullptr;
	}

	mod = (model_t*) ri.Hunk_Alloc( sizeof( *tr.models[ tr.numModels ] ), ha_pref::h_low );
	mod->index = tr.numModels;
	tr.models[ tr.numModels ] = mod;
	tr.numModels++;

	return mod;
}

/*
====================
RE_RegisterModel

Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
qhandle_t RE_RegisterModel( const char *name )
{
	model_t   *mod;
	int       lod;
	bool  loaded;
	qhandle_t hModel;
	int       numLoaded;

	if ( !name || !name[ 0 ] )
	{
		Log::Notice("RE_RegisterModel: NULL name" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH )
	{
		Log::Notice( "Model name exceeds MAX_QPATH" );
		return 0;
	}

	// search the currently loaded models
	for ( hModel = 1; hModel < tr.numModels; hModel++ )
	{
		mod = tr.models[ hModel ];

		if ( !strcmp( mod->name, name ) )
		{
			if ( mod->type == modtype_t::MOD_BAD )
			{
				return 0;
			}

			return hModel;
		}
	}

	// allocate a new model_t
	if ( ( mod = R_AllocModel() ) == nullptr )
	{
		Log::Warn("RE_RegisterModel: R_AllocModel() failed for '%s'", name );
		return 0;
	}

	// only set the name after the model has been successfully loaded
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );

	// make sure the render thread is stopped
	R_SyncRenderThread();

	mod->numLods = 0;

	// load the files
	numLoaded = 0;

	if ( strstr( name, ".iqm" ) || strstr( name, ".md5mesh" ) )
	{
		// try loading skeletal file

		loaded = false;
		std::error_code err;
		std::string buffer = FS::PakPath::ReadFile( name, err );

		if ( !err )
		{
			if ( Str::IsIPrefix( MD5_IDENTSTRING, buffer ) )
			{
				loaded = R_LoadMD5( mod, buffer.c_str(), name );
			}
			else if ( Str::IsIPrefix( "INTERQUAKEMODEL", buffer ) ) {
				loaded = R_LoadIQModel( mod, buffer.data(), buffer.size(), name );
			}
		}

		if ( loaded )
		{
			return mod->index;
		}
	}

	for ( lod = MD3_MAX_LODS - 1; lod >= 0; lod-- )
	{
		char filename[ 1024 ];

		strcpy( filename, name );

		if ( lod != 0 )
		{
			char namebuf[ 80 ];

			if ( strrchr( filename, '.' ) )
			{
				*strrchr( filename, '.' ) = 0;
			}

			Com_sprintf( namebuf, sizeof( namebuf ), "_%d.md3", lod );
			strcat( filename, namebuf );
		}

		filename[ strlen( filename ) - 1 ] = '3';  // try MD3 first

		std::error_code err;
		std::string buffer = FS::PakPath::ReadFile( filename, err );

		// LoDs are optional
		if ( err )
		{
			continue;
		}

		if ( Str::IsPrefix( "IDP3", buffer ) )
		{
			loaded = R_LoadMD3( mod, lod, buffer.data(), name );
		}
		else
		{
			Log::Warn("RE_RegisterModel: unknown fileid for %s", name );
			goto fail;
		}

		if ( !loaded )
		{
			if ( lod == 0 )
			{
				goto fail;
			}
			else
			{
				break;
			}
		}
		else
		{
			mod->numLods++;
			numLoaded++;
		}
	}

	if ( numLoaded )
	{
		// duplicate into higher lod spots that weren't
		// loaded, in case the user changes r_lodbias on the fly
		for ( lod--; lod >= 0; lod-- )
		{
			mod->numLods++;
			mod->mdv[ lod ] = mod->mdv[ lod + 1 ];
		}

		return mod->index;
	}

#ifndef NDEBUG
	else
	{
		Log::Warn("couldn't load '%s'", name );
	}

#endif

fail:
	// we still keep the model_t around, so if the model name is asked for
	// again, we won't bother scanning the filesystem
	mod->type = modtype_t::MOD_BAD;

	return 0;
}

//=============================================================================

/*
** RE_BeginRegistration
*/
bool RE_BeginRegistration( glconfig_t *glconfigOut, glconfig2_t *glconfig2Out )
{
	if ( !R_Init() )
	{
		return false;
	}

	*glconfigOut = glConfig;
	*glconfig2Out = glConfig2;

	R_SyncRenderThread();

	tr.visIndex = 0;
	// force markleafs to regenerate
	for (size_t i = 0; i < MAX_VISCOUNTS; ++i)
	{
		tr.visClusters[i] = -1;
	}

	RE_ClearScene();

	// HACK: give world entity white color for "colored" shader keyword
	tr.worldEntity.e.shaderRGBA = Color::White;

	tr.worldEntity.e.nonNormalizedAxes = false;

	tr.registered = true;

	return true;
}

//=============================================================================

/*
===============
R_ModelInit
===============
*/
void R_ModelInit()
{
	model_t *mod;

	// leave a space for nullptr model
	tr.numModels = 0;

	mod = R_AllocModel();
	mod->type = modtype_t::MOD_BAD;
}

// shows MD3 per-frame info if there is an argument
class ListModelsCmd : public Cmd::StaticCmd
{
public:
	ListModelsCmd() : StaticCmd("listModels", Cmd::RENDERER, "list loaded 3D models") {}

	void Run( const Cmd::Args &args ) const override
	{
		int      i, j, k;
		model_t  *mod;
		int      total;
		int      totalDataSize;
		bool showFrames;

		showFrames = args.Argc() > 1;

		total = 0;
		totalDataSize = 0;

		for ( i = 1; i < tr.numModels; i++ )
		{
			mod = tr.models[ i ];

			if ( mod->type == modtype_t::MOD_MESH )
			{
				for ( j = 0; j < MD3_MAX_LODS; j++ )
				{
					if ( mod->mdv[ j ] && ( j == 0 || mod->mdv[ j ] != mod->mdv[ j - 1 ] ) )
					{
						mdvModel_t   *mdvModel;
						mdvSurface_t *mdvSurface;
						mdvTagName_t *mdvTagName;

						mdvModel = mod->mdv[ j ];

						total++;
						Print( "%d.%02d MB '%s' LOD = %i",      mod->dataSize / ( 1024 * 1024 ),
						       ( mod->dataSize % ( 1024 * 1024 ) ) * 100 / ( 1024 * 1024 ),
						       mod->name, j );

						if ( showFrames && mdvModel->numFrames > 1 )
						{
							Print("    numSurfaces = %i", mdvModel->numSurfaces );
							Print("    numFrames = %i", mdvModel->numFrames );

							for ( k = 0, mdvSurface = mdvModel->surfaces; k < mdvModel->numSurfaces; k++, mdvSurface++ )
							{
								Print("        mesh = '%s'", mdvSurface->name );
								Print("            numVertexes = %i", mdvSurface->numVerts );
								Print("            numTriangles = %i", mdvSurface->numTriangles );
							}
						}

						Print("        numTags = %i", mdvModel->numTags );

						for ( k = 0, mdvTagName = mdvModel->tagNames; k < mdvModel->numTags; k++, mdvTagName++ )
						{
							Print("            tagName = '%s'", mdvTagName->name );
						}
					}
				}
			}
			else
			{
				Print( "%d.%02d MB '%s'",       mod->dataSize / ( 1024 * 1024 ),
				       ( mod->dataSize % ( 1024 * 1024 ) ) * 100 / ( 1024 * 1024 ),
				       mod->name );

				total++;
			}

			totalDataSize += mod->dataSize;
		}

		Print(" %d.%02d MB total model memory", totalDataSize / ( 1024 * 1024 ),
					  ( totalDataSize % ( 1024 * 1024 ) ) * 100 / ( 1024 * 1024 ) );
		Print(" %i total models", total );
	}
};
static ListModelsCmd listModelsCmdRegistration;

//=============================================================================

/*
================
R_GetTag
================
*/
static int R_GetTag( mdvModel_t *model, int frame, const char *_tagName, int startTagIndex, mdvTag_t **outTag )
{
	int          i;
	mdvTag_t     *tag;
	mdvTagName_t *tagName;

	// it is possible to have a bad frame while changing models, so don't error
	frame = Math::Clamp( frame, 0, model->numFrames - 1 );

	if ( startTagIndex > model->numTags )
	{
		*outTag = nullptr;
		return -1;
	}

	tag = model->tags + frame * model->numTags;
	tagName = model->tagNames;

	for ( i = 0; i < model->numTags; i++, tag++, tagName++ )
	{
		if ( ( i >= startTagIndex ) && !strcmp( tagName->name, _tagName ) )
		{
			*outTag = tag;
			return i;
		}
	}

	*outTag = nullptr;
	return -1;
}

/*
================
RE_LerpTag
================
*/
int RE_LerpTagET( orientation_t *tag, const refEntity_t *refent, const char *tagNameIn, int startIndex )
{
	mdvTag_t  *start, *end;
	int       i;
	float     frontLerp, backLerp;
	model_t   *model;
	char      tagName[ MAX_QPATH ]; //, *ch;
	int       retval;
	qhandle_t handle;
	int       startFrame, endFrame;
	float     frac;

	handle = refent->hModel;
	startFrame = refent->oldframe;
	endFrame = refent->frame;
	frac = 1.0 - refent->backlerp;

	Q_strncpyz( tagName, tagNameIn, MAX_QPATH );

	model = R_GetModelByHandle( handle );

	frontLerp = frac;
	backLerp = 1.0 - frac;

	start = end = nullptr;
	if ( model->type == modtype_t::MOD_MD5 || model->type == modtype_t::MOD_IQM )
	{
		vec3_t tmp;

		retval = RE_BoneIndex( handle, tagName );

		if ( retval <= 0 )
		{
			return -1;
		}

		VectorScale( refent->skeleton.bones[ retval ].t.trans,
			     refent->skeleton.scale, tag->origin );
		QuatToAxis( refent->skeleton.bones[ retval ].t.rot, tag->axis );
		VectorCopy( tag->axis[ 2 ], tmp );
		VectorCopy( tag->axis[ 1 ], tag->axis[ 2 ] );
		VectorCopy( tag->axis[ 0 ], tag->axis[ 1 ] );
		VectorCopy( tmp, tag->axis[ 0 ] );
		VectorNormalize( tag->axis[ 0 ] );
		VectorNormalize( tag->axis[ 1 ] );
		VectorNormalize( tag->axis[ 2 ] );
		return retval;
	}
	else if ( model->type == modtype_t::MOD_MESH )
	{
		// old MD3 style
		retval = R_GetTag( model->mdv[ 0 ], startFrame, tagName, startIndex, &start );
		retval = R_GetTag( model->mdv[ 0 ], endFrame, tagName, startIndex, &end );


		if ( !start || !end )
		{
			AxisClear( tag->axis );
			VectorClear( tag->origin );
			return -1;
		}

		for ( i = 0; i < 3; i++ )
		{
			tag->origin[ i ] = start->origin[ i ] * backLerp + end->origin[ i ] * frontLerp;
			tag->axis[ 0 ][ i ] = start->axis[ 0 ][ i ] * backLerp + end->axis[ 0 ][ i ] * frontLerp;
			tag->axis[ 1 ][ i ] = start->axis[ 1 ][ i ] * backLerp + end->axis[ 1 ][ i ] * frontLerp;
			tag->axis[ 2 ][ i ] = start->axis[ 2 ][ i ] * backLerp + end->axis[ 2 ][ i ] * frontLerp;
		}

		VectorNormalize( tag->axis[ 0 ] );
		VectorNormalize( tag->axis[ 1 ] );
		VectorNormalize( tag->axis[ 2 ] );

		return retval;
	}

	return -1;
}

/*
================
RE_BoneIndex
================
*/
int RE_BoneIndex( qhandle_t hModel, const char *boneName )
{
	int        i = 0;
	model_t    *model;

	model = R_GetModelByHandle( hModel );

	switch ( model->type )
	{
		case modtype_t::MOD_MD5:
		{
			md5Bone_t  *bone;
			md5Model_t *md5 = model->md5;

			for ( i = 0, bone = md5->bones; i < md5->numBones; i++, bone++ )
			{
				if ( !Q_stricmp( bone->name, boneName ) )
				{
					return i;
				}
			}
		}
		break;

		case modtype_t::MOD_IQM:
		{
			char *str = model->iqm->jointNames;

			while (  i < model->iqm->num_joints )
			{
				if ( !Q_stricmp( boneName, str ) )
				{
					return i;
				}

				str += strlen( str ) + 1;
				++i;
			}
		}
		break;
        default:
        break;
	}


	return -1;
}

/*
====================
R_ModelBounds
====================
*/
void R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs )
{
	model_t    *model;
	mdvModel_t *header;
	mdvFrame_t *frame;

	model = R_GetModelByHandle( handle );

	switch ( model->type )
	{
		case modtype_t::MOD_BSP:
			VectorCopy( model->bsp->bounds[ 0 ], mins );
			VectorCopy( model->bsp->bounds[ 1 ], maxs );
			break;

		case modtype_t::MOD_MESH:
			header = model->mdv[ 0 ];

			frame = header->frames;

			VectorCopy( frame->bounds[ 0 ], mins );
			VectorCopy( frame->bounds[ 1 ], maxs );
			break;

		case modtype_t::MOD_MD5:
			VectorCopy( model->md5->bounds[ 0 ], mins );
			VectorCopy( model->md5->bounds[ 1 ], maxs );
			break;

		case modtype_t::MOD_IQM:
			VectorCopy( model->iqm->bounds[ 0 ], mins );
			VectorCopy( model->iqm->bounds[ 1 ], maxs );
			break;

		default:
			VectorClear( mins );
			VectorClear( maxs );
			break;
	}
}
