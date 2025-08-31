/*
===========================================================================

Daemon BSD Source Code
Copyright (c) 2025 Daemon Developers
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
// TexturePack.cpp

#include "TexturePack.h"

std::vector<TexturePack> texturePacks;

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

		GL_CheckErrors();
	}

	constexpr uint16_t atlasStep = 256;

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
			if ( image->level >= numMips || image->uploadWidth < 4 || image->uploadHeight < 4 ) {
				return true;
			}
			GL_CheckErrors();
			uint16_t width = image->uploadWidth > 4 ? image->uploadWidth : 4;
			uint16_t height = image->uploadHeight > 4 ? image->uploadHeight : 4;
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

bool AddToTexturePack( image_t* image, const GLenum format, const GLsizei imageSize, const byte* imageData ) {
	if ( image->assignedTexturePack ) {
		texturePacks[image->texturePackImage].InsertImage( image, format, imageSize, imageData );
		return true;
	}

	uint32_t index = 0;
	for ( TexturePack& texturePack : texturePacks ) {
		if ( texturePack.InsertImage( image, format, imageSize, imageData ) ) {
			image->useTexturePack = true;
			image->texturePackImage = index;
			// Log::Warn( "%u: %s layer: %u", image->texturePackImage, image->name, image->texturePackModifier[2] );
			return true;
		}
		index++;
	}

	TexturePack pack;
	if ( pack.InsertImage( image, format, imageSize, imageData ) ) {
		texturePacks.push_back( pack );
		image->useTexturePack = true;
		image->texturePackImage = texturePacks.size() - 1;
		// Log::Warn( "%u: %s layer: %u", image->texturePackImage, image->name, image->texturePackModifier[2] );
		return true;
	}

	return false;
}
