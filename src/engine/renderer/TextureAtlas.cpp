#include "TextureAtlas.h"
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
// TextureAtlas.cpp

#include "tr_local.h"
#include "TextureAtlas.h"

std::vector<TextureAtlas> textureAtlases;

TextureAtlas::TextureAtlas( const GLenum newFormat, const GLint newInternalFormat,
							const filterProxy newFilterProxy, const uint32_t newBits ) :
	format( newFormat ),
	internalFormat( newInternalFormat ),
	filter( newFilterProxy ),
	bits( newBits ) {
}

TextureAtlas::~TextureAtlas() {
}

void TextureAtlas::CreateTexture() {
	imageParams_t imageParams = {};
	imageParams.bits = bits;

	switch ( filter ) {
		case FP_DEFAULT:
			imageParams.filterType = filterType_t::FT_DEFAULT;
			break;
		case FP_LINEAR:
			imageParams.filterType = filterType_t::FT_LINEAR;
			break;
		case FP_NEAREST:
			imageParams.filterType = filterType_t::FT_NEAREST;
			break;
	}
	imageParams.wrapType = wrapTypeEnum_t::WT_REPEAT;

	texture = R_CreateImage( va( "textureAtlas%u", id ), nullptr, width, height, 1, imageParams );

	allocated = true;

	Log::Warn( "%u", texture->texnum );
}

void TextureAtlas::UploadTexture( image_t* image ) {
	if ( !image->useTextureAtlas ) {
		return;
	}

	if ( ( image->textureAtlasWidth == 0 ) || ( image->textureAtlasHeight == 0 ) ) {
		return;
	}

	GL_Bind( texture );

	glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY, image->textureAtlasWidth - 2, image->textureAtlasHeight - 2,
					 format, type, image->imageData );

	if ( image->imageData ) {
		ri.Hunk_FreeTempMemory( image->imageData );
	}

	glGenerateMipmap( GL_TEXTURE_2D );

	GL_CheckErrors();
}

void TextureAtlas::AddImageToTextureBin( image_t* image, byte* imageData, const TextureBin textureBin ) {
	const uint16_t imageWidth = image->uploadWidth + 2;
	const uint16_t imageHeight = image->uploadHeight + 2;

	image->textureAtlasID = id;
	image->textureAtlasX = textureBin.x;
	image->textureAtlasY = textureBin.y;
	image->textureAtlasWidth = imageWidth;
	image->textureAtlasHeight = imageHeight;

	// Minimum dimension size including borders is 3
	if ( textureBin.width - imageWidth >= 3 ) {
		TextureBin splitBin{ textureBin.x + imageWidth, textureBin.y, textureBin.width - imageWidth, imageHeight };
		textureBins.push_back( splitBin );
	}

	// Minimum dimension size including borders is 3
	if ( textureBin.height - imageHeight >= 3 ) {
		TextureBin splitBin{ textureBin.x, textureBin.y + imageHeight, textureBin.width, textureBin.height - imageHeight };
		textureBins.push_back( splitBin );
	}

	image->imageData = imageData;

	/* glTexSubImage2D(GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY, image->textureAtlasWidth, image->textureAtlasHeight,
		             format, type, imageData );

	GL_CheckErrors();

	glGenerateMipmap( GL_TEXTURE_2D );

	GL_CheckErrors(); */
}

bool TextureAtlas::InsertImage( image_t* image, const filterProxy newFilterProxy, byte* imageData ) {
	if ( format != image->format || internalFormat != image->internalFormat
		 || filter != newFilterProxy || bits != image->bits ) {
		return false;
	}

	// Add 2 because we'll need to copy border pixels for proper bilinear filtering
	const uint16_t imageWidth = image->uploadWidth + 2;
	const uint16_t imageHeight = image->uploadHeight + 2;

	for ( size_t i = 0; i < textureBins.size(); i++ ) {
		TextureBin& textureBin = textureBins[i];
		if ( ( imageWidth <= textureBin.width ) && ( imageHeight <= textureBin.height ) ) {
			AddImageToTextureBin( image, imageData, textureBin );
			textureBins.erase( textureBins.begin() + i );

			return true;
		}
	}

	if ( restrictSize ) {
		return false;
	}

	uint32_t area1 = ( width + imageWidth ) * ( height > imageHeight ? height : imageHeight );
	uint32_t area2 = ( height + imageHeight ) * ( width > imageWidth ? width : imageWidth );

	if ( ( width + imageWidth <= glConfig.maxTextureSize )
		&& ( ( area1 <= area2 ) || ( height + imageHeight > glConfig.maxTextureSize ) ) ) {
		if ( ( height > imageHeight ) && ( height - imageHeight >= 3 ) ) {
			TextureBin extendBin{ width, imageHeight, imageWidth, height - imageHeight };
			textureBins.push_back( extendBin );
		} else if ( ( width > 0 ) && ( height < imageHeight ) && ( imageHeight - height >= 3 ) ) {
			TextureBin extendBin{ 0, height, width, imageHeight - height };
			textureBins.push_back( extendBin );
		}

		TextureBin tempBin{ width, 0, imageWidth, imageHeight };
		
		width += imageWidth;
		height = height > imageHeight ? height : imageHeight;
		
		AddImageToTextureBin( image, imageData, tempBin );

		return true;
	} else if ( height + imageHeight <= glConfig.maxTextureSize ) {
		if ( ( width > imageWidth ) && ( width - imageWidth >= 3 ) ) {
			TextureBin extendBin{ imageWidth, height, width - imageWidth, imageHeight };
			textureBins.push_back( extendBin );
		} else if ( ( height > 0 ) && ( width < imageWidth ) && ( imageWidth - width >= 3 ) ) {
			TextureBin extendBin{ width, 0, imageWidth - width, height };
			textureBins.push_back( extendBin );
			width = imageWidth;
		}

		TextureBin tempBin{ 0, height, imageWidth, imageHeight };

		height += imageHeight;
		width = width > imageWidth ? width : imageWidth;

		AddImageToTextureBin( image, imageData, tempBin );

		return true;
	}

	return false;
}

void LoadTextureAtlases() {
	for ( TextureAtlas& textureAtlas : textureAtlases ) {
		textureAtlas.CreateTexture();
	}

	for ( image_t* image : tr.images ) {
		if ( image->useTextureAtlas ) {
			textureAtlases[image->textureAtlasID].UploadTexture( image );
		}
	}
}

static uint32_t textureAtlasID = 0;

TextureAtlas* TextureAtlasForImage( image_t* image, byte* imageData ) {
	if ( false ) {
		return nullptr;
	}

	filterProxy filter;
	switch ( image->filterType ) {
		case filterType_t::FT_DEFAULT:
			filter = FP_DEFAULT;
			break;
		case filterType_t::FT_LINEAR:
			filter = FP_LINEAR;
			break;
		case filterType_t::FT_NEAREST:
			filter = FP_NEAREST;
			break;
	}

	for ( TextureAtlas& atlas : textureAtlases ) {
		if ( atlas.InsertImage( image, filter, imageData ) ) {
			image->textureAtlasID = atlas.id;
			return &atlas;
		}
	}

	TextureAtlas atlas( image->format, image->internalFormat, filter, image->bits );
	if ( atlas.InsertImage( image, filter, imageData ) ) {
		atlas.id = textureAtlasID;
		image->textureAtlasID = atlas.id;
		textureAtlasID++;

		textureAtlases.push_back( atlas );
		return &textureAtlases[textureAtlases.size() - 1];
	} else {
		Log::Warn( "Unable to allocate texture memory in texture atlases for image %s", image->name );
		return nullptr;
	}
}

void FreeTextureAtlases() {
	textureAtlases.clear();
	textureAtlasID = 0;
}

void TextureAtlas::print() {
	Log::Warn( "atlas w h: %u %u", width, height );
	for ( TextureBin bin : textureBins ) {
		Log::Warn( "bin x y w h: %u %u %u %u", bin.x, bin.y, bin.width, bin.height );
	}
}

void TextureAtlas::AddTextureBin( const TextureBin textureBin ) {
	if ( ( textureBin.width == 0 ) || ( textureBin.height == 0 ) ) {
		return;
	}

	textureBins.push_back( textureBin );
}

void TextureAtlas::DeleteTextureBinIfEmpty( const size_t index ) {
	if ( ( textureBins[index].width == 0 ) || ( textureBins[index].height == 0 ) ) {
		textureBins.erase( textureBins.begin() + index );
	}
}
