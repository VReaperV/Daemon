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
// TexturePack.cpp
#include "tr_local.h"

const uint16_t atlasStep = 256;

bool TexturePack::InsertImage( image_t* image, const GLenum newFormat, const GLsizei imageSize, const byte* imageData ) {
	bool alloc = false;

	if ( ( width == 0 ) || ( height == 0 ) ) {
		width = image->uploadWidth % 256 == 0 ? image->uploadWidth : ( image->uploadWidth / 256 + 1 ) * 256;
		height = image->uploadHeight % 256 == 0 ? image->uploadHeight : ( image->uploadHeight / 256 + 1 ) * 256;
		bits = image->bits;
		filterType = image->filterType;
		wrapType = image->wrapType;
		format = newFormat;

		alloc = true;

		imageParams_t imageParams = {};
		imageParams.bits = bits;
		imageParams.filterType = filterType;
		imageParams.wrapType = wrapType;

		if ( !IsImageCompressed( bits ) ) {
			numLayers = 16777216 / ( width * height * 4 );
		} else {
			numLayers = 16777216 / ( ceil( width / 4.0 ) * ceil( height / 4.0 ) * 8 );
		}
		numLayers = numLayers == 0 ? 1 : numLayers;

		uint16_t maxDimension = width > height ? width : height;
		numMips = log2( maxDimension ) + 1;

		texture = R_Create2DArrayImage( "texturePack", nullptr, width, height, numLayers, numMips, imageParams );

		Log::Warn( "%u compressed: %b", texture->texnum, IsImageCompressed( bits ) );
	}

	if ( ( image->uploadWidth > width ) || ( image->uploadHeight > height )
		|| ( width - image->uploadWidth >= atlasStep ) || ( height - image->uploadHeight >= atlasStep ) ) {
		return false;
	}

	if ( bits != image->bits || filterType != image->filterType || wrapType != image->wrapType || format != newFormat ) {
		return false;
	}

	if( !alloc && !image->assignedTexturePack ) {
		layer++;
	}

	if ( layer == numLayers ) {
		layer--;
		return false;
	}

	if ( image->layer == -1 ) {
		image->layer = layer;
	}
	image->texturePackModifier[0] = ( float ) image->uploadWidth / width;
	image->texturePackModifier[1] = ( float ) image->uploadHeight / height;
	image->texturePackModifier[2] = image->layer;
	image->assignedTexturePack = true;
	
	GL_Bind( texture );

	GL_CheckErrors();

	if ( imageData != nullptr ) {
		if ( !IsImageCompressed( bits ) ) {
			glTexSubImage3D( GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, image->uploadWidth, image->uploadHeight, 1, format, GL_UNSIGNED_BYTE,
				imageData );
			glGenerateMipmap( GL_TEXTURE_2D_ARRAY );
		} else {
			if ( image->level >= numMips || image->levelWidth < 4 || image->levelHeight < 4 ) {
				return true;
			}
			GL_CheckErrors();
			uint16_t width = image->levelWidth > 4 ? image->levelWidth : 4;
			uint16_t height = image->levelHeight > 4 ? image->levelHeight : 4;
			glCompressedTexSubImage3D( GL_TEXTURE_2D_ARRAY, image->level, 0, 0, image->layer, width, height,
				1, format, imageSize, imageData );
			GL_CheckErrors();
			image->level++;
		}
	}

	if ( std::find( images.begin(), images.end(), image ) == images.end() ) {
		images.push_back( image );
	}

	GL_CheckErrors();

	return true;
}
