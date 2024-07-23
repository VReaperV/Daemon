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
	std::vector<TextureBin> tmp;
	for ( TextureBin bin : textureBins ) {
		tmp.push_back( bin );
	}
	textureBins.clear();
	borderSize = log2( log2( width > height ? width : height ) + 1 );
	borderSize = borderSize < 5 ? 5 : borderSize;
	borderSize = pow( 2, borderSize );
	width = 0;
	height = 0;

	for ( TextureBin origBin : tmp ) {
		if ( origBin.image ) {
			InsertImage( origBin.image, filter, origBin.image->imageData );
			Log::Warn( "%s", origBin.image->name );
		}
	}

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

	for ( TextureBin textureBin : textureBins ) {
		UploadTexture( textureBin.image );
	}
}

void TextureAtlas::UploadTexture( image_t* image ) {
	if ( !image->useTextureAtlas ) {
		return;
	}

	if ( ( image->textureAtlasWidth == 0 ) || ( image->textureAtlasHeight == 0 ) ) {
		Log::Warn( "empty image: %s %u %u %u %u id: %u width: %u height: %u, %b", image->name, image->textureAtlasX, image->textureAtlasY,
				   image->textureAtlasWidth, image->textureAtlasHeight, id, image->uploadWidth, image->uploadHeight,
				   IsImageCompressed( image->bits ) );
		return;
	}

	GL_Bind( texture );

	glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX + borderSize, image->textureAtlasY + borderSize,
					 image->textureAtlasWidth - 2 * borderSize, image->textureAtlasHeight - 2 * borderSize,
					 format, type, image->imageData );

	byte* border = new byte[image->textureAtlasWidth * 4];
	/* for ( uint32_t i = 0; i < image->textureAtlasWidth * borderSize; i++ ) {
		border[i * 4] = 255;
		border[i * 4 + 1] = 0;
		border[i * 4 + 2] = 0;
		border[i * 4 + 3] = 0;
	}

	glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY, image->textureAtlasWidth, borderSize,
		format, type, border );
	glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY + image->textureAtlasHeight - borderSize,
		image->textureAtlasWidth, borderSize, format, type, border ); */
	
	/* for ( uint32_t u = 0; u < borderSize; u++ ) {
		for ( uint32_t v = 0; v < image->uploadWidth; v++ ) {
			border[( u * image->textureAtlasWidth + v + borderSize ) * 4] = image->imageData[v * 4];
			border[( u * image->textureAtlasWidth + v + borderSize ) * 4 + 1] = image->imageData[v * 4 + 1];
			border[( u * image->textureAtlasWidth + v + borderSize ) * 4 + 2] = image->imageData[v * 4 + 2];
			border[( u * image->textureAtlasWidth + v + borderSize ) * 4 + 3] = image->imageData[v * 4 + 3];
		}
		for ( uint32_t v = 0; v < borderSize; v++ ) {
			border[( u * image->textureAtlasWidth + v ) * 4] = image->imageData[0];
			border[( u * image->textureAtlasWidth + v ) * 4 + 1] = image->imageData[1];
			border[( u * image->textureAtlasWidth + v ) * 4 + 2] = image->imageData[2];
			border[( u * image->textureAtlasWidth + v ) * 4 + 3] = image->imageData[3];

			border[( u * image->textureAtlasWidth + image->uploadWidth + borderSize + v ) * 4] =
				image->imageData[image->uploadWidth * 4 - 4];
			border[( u * image->textureAtlasWidth + image->uploadWidth + borderSize + v ) * 4 + 1] =
				image->imageData[image->uploadWidth * 4 - 3];
			border[( u * image->textureAtlasWidth + image->uploadWidth + borderSize + v ) * 4 + 2] =
				image->imageData[image->uploadWidth * 4 - 2];
			border[( u * image->textureAtlasWidth + image->uploadWidth + borderSize + v ) * 4 + 3] =
				image->imageData[image->uploadWidth * 4 - 1];
		}
	} */

	for ( uint32_t v = 0; v < image->uploadWidth; v++ ) {
		border[( v + borderSize ) * 4] = image->imageData[v * 4];
		border[( v + borderSize ) * 4 + 1] = image->imageData[v * 4 + 1];
		border[( v + borderSize ) * 4 + 2] = image->imageData[v * 4 + 2];
		border[( v + borderSize ) * 4 + 3] = image->imageData[v * 4 + 3];
	}
	for ( uint32_t v = 0; v < borderSize; v++ ) {
		border[( v ) * 4] = image->imageData[0];
		border[( v ) * 4 + 1] = image->imageData[1];
		border[( v ) * 4 + 2] = image->imageData[2];
		border[( v ) * 4 + 3] = image->imageData[3];

		border[( image->uploadWidth + borderSize + v ) * 4] =
			image->imageData[image->uploadWidth * 4 - 4];
		border[( image->uploadWidth + borderSize + v ) * 4 + 1] =
			image->imageData[image->uploadWidth * 4 - 3];
		border[( image->uploadWidth + borderSize + v ) * 4 + 2] =
			image->imageData[image->uploadWidth * 4 - 2];
		border[( image->uploadWidth + borderSize + v ) * 4 + 3] =
			image->imageData[image->uploadWidth * 4 - 1];
	}

	for ( uint i = 0; i < borderSize; i++ ) {
		glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY + i, image->textureAtlasWidth, 1,
			format, type, border );
	}

	for ( uint32_t v = 0; v < image->uploadWidth; v++ ) {
		border[( borderSize + v ) * 4] =
			image->imageData[( ( image->uploadHeight - 1 ) * image->uploadWidth + v ) * 4];
		border[( borderSize + v ) * 4 + 1] =
			image->imageData[( ( image->uploadHeight - 1 ) * image->uploadWidth + v ) * 4 + 1];
		border[( borderSize + v ) * 4 + 2] =
			image->imageData[( ( image->uploadHeight - 1 ) * image->uploadWidth + v ) * 4 + 2];
		border[( borderSize + v ) * 4 + 3] =
			image->imageData[( ( image->uploadHeight - 1 ) * image->uploadWidth + v ) * 4 + 3];
	}
	for ( uint32_t v = 0; v < borderSize; v++ ) {
		border[( v ) * 4] = image->imageData[( image->uploadHeight - 1 ) * image->uploadWidth * 4];
		border[( v ) * 4 + 1] = image->imageData[( image->uploadHeight - 1 ) * image->uploadWidth * 4 + 1];
		border[( v ) * 4 + 2] = image->imageData[( image->uploadHeight - 1 ) * image->uploadWidth * 4 + 2];
		border[( v ) * 4 + 3] = image->imageData[( image->uploadHeight - 1 ) * image->uploadWidth * 4 + 3];

		border[( image->textureAtlasWidth - v ) * 4 - 4] = image->imageData[( image->uploadHeight * image->uploadWidth ) * 4 - 4];
		border[( image->textureAtlasWidth - v ) * 4 - 3] = image->imageData[( image->uploadHeight * image->uploadWidth ) * 4 - 3];
		border[( image->textureAtlasWidth - v ) * 4 - 2] = image->imageData[( image->uploadHeight * image->uploadWidth ) * 4 - 2];
		border[( image->textureAtlasWidth - v ) * 4 - 1] = image->imageData[( image->uploadHeight * image->uploadWidth ) * 4 - 1];
	}

	for ( uint i = 0; i < borderSize; i++ ) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY + borderSize + image->uploadHeight + i,
			image->textureAtlasWidth, 1, format, type, border );
	}

	delete[] border;

	border = new byte[image->uploadHeight * 4];

	for ( uint32_t v = 0; v < image->uploadHeight; v++ ) {
		border[v * 4] = image->imageData[v * image->uploadWidth * 4];
		border[v * 4 + 1] = image->imageData[v * image->uploadWidth * 4 + 1];
		border[v * 4 + 2] = image->imageData[v * image->uploadWidth * 4 + 2];
		border[v * 4 + 3] = image->imageData[v * image->uploadWidth * 4 + 3];
	}

	for ( uint i = 0; i < borderSize; i++ ) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, image->textureAtlasX + i, image->textureAtlasY + borderSize, 1, image->uploadHeight,
			format, type, border );
	}

	for ( uint32_t v = 0; v < image->uploadHeight; v++ ) {
		border[v * 4] = image->imageData[( v + 1 ) * image->uploadWidth * 4 - 4];
		border[v * 4 + 1] = image->imageData[( v + 1 ) * image->uploadWidth * 4 - 3];
		border[v * 4 + 2] = image->imageData[( v + 1 ) * image->uploadWidth * 4 - 2];
		border[v * 4 + 3] = image->imageData[( v + 1 ) * image->uploadWidth * 4 - 1];
	}

	for ( uint i = 0; i < borderSize; i++ ) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, image->textureAtlasX + borderSize + image->uploadWidth + i,
						 image->textureAtlasY + borderSize, 1, image->uploadHeight,
						 format, type, border );
	}

	delete[] border;

	/* border = new byte[image->textureAtlasHeight * borderSize * 4];
	for ( uint32_t i = 0; i < image->textureAtlasHeight * borderSize; i++ ) {
		border[i * 4] = 255;
		border[i * 4 + 1] = 0;
		border[i * 4 + 2] = 0;
		border[i * 4 + 3] = 0;
	}

	glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY, borderSize, image->textureAtlasHeight,
		format, type, border );
	glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX + image->textureAtlasWidth - borderSize, image->textureAtlasY,
		borderSize, image->textureAtlasHeight, format, type, border );

	delete[] border; */

	if ( image->imageData ) {
		// ri.Hunk_FreeTempMemory( image->imageData );
		delete[] image->imageData;
	}

	glGenerateMipmap( GL_TEXTURE_2D );

	GL_CheckErrors();
}

void TextureAtlas::AddImageToTextureBin( image_t* image, byte* imageData, const TextureBin textureBin ) {
	const uint16_t imageWidth = image->uploadWidth + 2 * borderSize;
	const uint16_t imageHeight = image->uploadHeight + 2 * borderSize;

	image->textureAtlasID = id;
	image->textureAtlasX = textureBin.x;
	image->textureAtlasY = textureBin.y;
	image->textureAtlasWidth = imageWidth;
	image->textureAtlasHeight = imageHeight;

	// Minimum dimension size including borders is 3
	if ( textureBin.width - imageWidth >= 1 + 2 * borderSize ) {
		TextureBin splitBin{ textureBin.x + imageWidth, textureBin.y, textureBin.width - imageWidth, imageHeight };
		textureBins.push_back( splitBin );
	}

	// Minimum dimension size including borders is 3
	if ( textureBin.height - imageHeight >= 1 + 2 * borderSize ) {
		TextureBin splitBin{ textureBin.x, textureBin.y + imageHeight, textureBin.width, textureBin.height - imageHeight };
		textureBins.push_back( splitBin );
	}

	image->imageData = new byte[image->uploadWidth * image->uploadHeight * 4];
	memcpy( image->imageData, imageData, image->uploadWidth * image->uploadHeight * 4 );

	// image->imageData = imageData;

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
	const uint16_t imageWidth = image->uploadWidth + 2 * borderSize;
	const uint16_t imageHeight = image->uploadHeight + 2 * borderSize;

	for ( size_t i = 0; i < textureBins.size(); i++ ) {
		TextureBin& textureBin = textureBins[i];
		if ( ( !textureBin.image ) && ( imageWidth <= textureBin.width ) && ( imageHeight <= textureBin.height ) ) {
			AddImageToTextureBin( image, imageData, textureBin );
			// textureBins.erase( textureBins.begin() + i );
			textureBin.width = imageWidth;
			textureBin.height = imageHeight;
			textureBin.image = image;

			width = width > imageWidth ? width : imageWidth;
			height = height > imageHeight ? height : imageHeight;

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
		if ( ( height > imageHeight ) && ( height - imageHeight >= 1 + 2 * borderSize ) ) {
			TextureBin extendBin{ width, imageHeight, imageWidth, height - imageHeight };
			textureBins.push_back( extendBin );
		} else if ( ( width > 0 ) && ( height < imageHeight ) && ( imageHeight - height >= 1 + 2 * borderSize ) ) {
			TextureBin extendBin{ 0, height, width, imageHeight - height };
			textureBins.push_back( extendBin );
		}

		TextureBin tempBin{ width, 0, imageWidth, imageHeight, image };
		textureBins.push_back( tempBin );
		
		width += imageWidth;
		height = height > imageHeight ? height : imageHeight;
		
		AddImageToTextureBin( image, imageData, tempBin );

		return true;
	} else if ( height + imageHeight <= glConfig.maxTextureSize ) {
		if ( ( width > imageWidth ) && ( width - imageWidth >= 1 + 2 * borderSize ) ) {
			TextureBin extendBin{ imageWidth, height, width - imageWidth, imageHeight };
			textureBins.push_back( extendBin );
		} else if ( ( height > 0 ) && ( width < imageWidth ) && ( imageWidth - width >= 1 + 2 * borderSize ) ) {
			TextureBin extendBin{ width, 0, imageWidth - width, height };
			textureBins.push_back( extendBin );
			width = imageWidth;
		}

		TextureBin tempBin{ 0, height, imageWidth, imageHeight, image };
		textureBins.push_back( tempBin );

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

	/* for ( image_t* image : tr.images ) {
		if ( image->useTextureAtlas ) {
			textureAtlases[image->textureAtlasID].UploadTexture( image );
		}
	} */
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
