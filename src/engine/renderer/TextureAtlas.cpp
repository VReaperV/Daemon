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
// TextureAtlas.cpp

#include "tr_local.h"
#include "TextureAtlas.h"

std::vector<TextureAtlas> textureAtlases;

TextureAtlas::TextureAtlas( const uint16_t newWidth, const uint16_t newHeight, const GLenum newFormat, const GLint newInternalFormat,
	const filterProxy newFilterProxy, const uint32_t newBits ) :
	width( newWidth ),
	height( newHeight ),
	format( newFormat ),
	internalFormat( newInternalFormat ),
	filter( newFilterProxy ),
	bits( newBits ),
	restrictSize( true ) {
}

TextureAtlas::TextureAtlas( const GLenum newFormat, const GLint newInternalFormat,
	const filterProxy newFilterProxy, const uint32_t newBits ) :
	format( newFormat ),
	internalFormat( newInternalFormat ),
	filter( newFilterProxy ),
	bits( newBits ),
	restrictSize( false ) {
}

TextureAtlas::~TextureAtlas() {
}

void TextureAtlas::CreateTexture() {
	if ( allocated ) {
		Sys::Drop( "Tried to allocate a texture atlas more than once" );
	}

	if ( !restrictSize ) {
		// Re-insert the bins to compact the atlas
		std::vector<TextureBin> tmp;
		for ( TextureBin bin : textureBins ) {
			tmp.push_back( bin );
		}
		textureBins.clear();
		width = 0;
		height = 0;

		for ( TextureBin origBin : tmp ) {
			if ( origBin.image ) {
				InsertImage( origBin.image, filter, origBin.image->imageData );
			}
		}
	}

	imageParams_t imageParams = {};
	imageParams.bits = bits;
	imageParams.isTextureAtlas = true;

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

	// Doesn't really matter since atlases aren't supposed to use driver/hardware wrapping
	imageParams.wrapType = wrapTypeEnum_t::WT_REPEAT;

	texture = R_CreateImage( va( "textureAtlas%u", id ), nullptr, width, height, 1, imageParams );

	allocated = true;

	for ( TextureBin textureBin : textureBins ) {
		if ( textureBin.image ) {
			UploadTexture( textureBin.image );
		}
	}

	GL_fboShim.glGenerateMipmap( texture->type );
	glTexParameteri( texture->type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
}

void TextureAtlas::UploadTexture( image_t* image ) {
	if ( !image->useTextureAtlas ) {
		return;
	}

	if ( ( image->textureAtlasWidth == 0 ) || ( image->textureAtlasHeight == 0 ) ) {
		Log::Warn( "empty image: %s %u %u %u %u id: %u width: %u height: %u", image->name, image->textureAtlasX, image->textureAtlasY,
				   image->textureAtlasWidth, image->textureAtlasHeight, id, image->uploadWidth, image->uploadHeight );
		return;
	}

	GL_Bind( texture );

	image->atlas[0] = (float) image->textureAtlasWidth / width;
	image->atlas[1] = (float) image->textureAtlasHeight / height;
	image->atlas[2] = (float) ( image->textureAtlasX ) / width;
	image->atlas[3] = (float) ( image->textureAtlasY ) / height;

	glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX + 1, image->textureAtlasY + 1,
					 image->textureAtlasWidth - 2, image->textureAtlasHeight - 2,
					 format, type, image->imageData );

	GL_CheckErrors();

	if ( borderSize == 0 ) {
		return;
	}

	byte* border = ( byte* ) ri.Hunk_AllocateTempMemory( image->textureAtlasWidth * texelBytes );

	// Copy bottom border
	for ( uint16_t x = 0; x < image->uploadWidth; x++ ) {
		for ( uint8_t i = 0; i < texelBytes; i++ ) {
			border[( x + borderSize ) * texelBytes + i] = image->imageData[x * texelBytes + i];
		}
	}
	// Copy corners
	for ( uint16_t i = 0; i < borderSize; i++ ) {
		for ( uint8_t j = 0; j < texelBytes; j++ ) {
			border[i * texelBytes] = image->imageData[j];
			border[( image->uploadWidth + borderSize + i ) * texelBytes + j] =
				image->imageData[image->uploadWidth * texelBytes - ( texelBytes - j )];
		}
	}

	for ( uint16_t i = 0; i < borderSize; i++ ) {
		glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY + i, image->textureAtlasWidth, 1,
			format, type, border );
	}

	// Copy top border
	for ( uint16_t x = 0; x < image->uploadWidth; x++ ) {
		for ( uint8_t i = 0; i < texelBytes; i++ ) {
			border[( borderSize + x ) * texelBytes + i] =
				image->imageData[( ( image->uploadHeight - 1 ) * image->uploadWidth + x ) * texelBytes + i];
		}
	}
	// Copy corners
	for ( uint16_t i = 0; i < borderSize; i++ ) {
		for ( uint8_t j = 0; j < texelBytes; j++ ) {
			border[i * texelBytes + j] = image->imageData[( image->uploadHeight - 1 ) * image->uploadWidth * texelBytes + j];
			border[( image->textureAtlasWidth - i ) * texelBytes - ( texelBytes - j )] =
				image->imageData[( image->uploadHeight * image->uploadWidth ) * texelBytes - ( texelBytes - j )];
		}
	}

	for ( uint16_t i = 0; i < borderSize; i++ ) {
		glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY + borderSize + image->uploadHeight + i,
			image->textureAtlasWidth, 1, format, type, border );
	}

	ri.Hunk_FreeTempMemory( border );

	border = ( byte* ) ri.Hunk_AllocateTempMemory( image->uploadHeight * texelBytes );

	// Copy left border
	for ( uint16_t y = 0; y < image->uploadHeight; y++ ) {
		for ( uint8_t i = 0; i < texelBytes; i++ ) {
			border[y * texelBytes + i] = image->imageData[y * image->uploadWidth * texelBytes + i];
		}
	}

	for ( uint16_t i = 0; i < borderSize; i++ ) {
		glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX + i, image->textureAtlasY + borderSize, 1, image->uploadHeight,
			format, type, border );
	}

	// Copy right border
	for ( uint16_t y = 0; y < image->uploadHeight; y++ ) {
		for ( uint8_t i = 0; i < texelBytes; i++ ) {
			border[y * texelBytes + i] = image->imageData[( y + 1 ) * image->uploadWidth * texelBytes - ( texelBytes - i )];
		}
	}

	for ( uint16_t i = 0; i < borderSize; i++ ) {
		glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX + borderSize + image->uploadWidth + i,
						 image->textureAtlasY + borderSize, 1, image->uploadHeight,
						 format, type, border );
	}

	ri.Hunk_FreeTempMemory( border );

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

	// Add bin to the right of the image if there's space
	if ( textureBin.width - imageWidth >= 1 + 2 ) {
		TextureBin splitBin{ textureBin.x + imageWidth, textureBin.y, textureBin.width - imageWidth, imageHeight };
		textureBins.push_back( splitBin );
	}

	// Add bin above the image if there's space
	if ( textureBin.height - imageHeight >= 1 + 2 ) {
		TextureBin splitBin{ textureBin.x, textureBin.y + imageHeight, textureBin.width, textureBin.height - imageHeight };
		textureBins.push_back( splitBin );
	}

	image->imageData = ( byte* ) ri.Hunk_Alloc( image->uploadWidth * image->uploadHeight * texelBytes, ha_pref::h_low );
	memcpy( image->imageData, imageData, image->uploadWidth * image->uploadHeight * texelBytes );
}

// This will only add bin/bins and assign images to them, but not upload them
bool TextureAtlas::InsertImage( image_t* image, const filterProxy newFilterProxy, byte* imageData ) {
	if ( format != image->format || internalFormat != image->internalFormat
		|| filter != newFilterProxy || bits != image->bits ) {
		return false;
	}

	// We'll need to copy border pixels for proper bilinear filtering
	const uint16_t imageWidth = image->uploadWidth + 2;
	const uint16_t imageHeight = image->uploadHeight + 2;

	for ( size_t i = 0; i < textureBins.size(); i++ ) {
		TextureBin& textureBin = textureBins[i];
		if ( ( !textureBin.image ) && ( imageWidth <= textureBin.width ) && ( imageHeight <= textureBin.height ) ) {
			AddImageToTextureBin( image, imageData, textureBin );
			textureBin.width = imageWidth;
			textureBin.height = imageHeight;
			textureBin.image = image;

			width = std::max( width, imageWidth );
			height = std::max( height, imageHeight );

			return true;
		}
	}

	if ( restrictSize ) {
		return false;
	}

	// Check if we'll end up using less memory in total by expanding to the right or up
	uint32_t area1 = ( width + imageWidth ) * ( height > imageHeight ? height : imageHeight );
	uint32_t area2 = ( height + imageHeight ) * ( width > imageWidth ? width : imageWidth );

	const bool extendUp = ( height + imageHeight <= glConfig.maxTextureSize )
		&& ( ( area2 < area1 ) || ( width - height >= 512 ) || ( width + imageWidth > glConfig.maxTextureSize ) );

	// Extend atlas upwards
	if ( extendUp ) {
		// Add a bin to the right of the current atlas right border if the image we insert exceeds the current atlas width
		if ( ( width > imageWidth ) && ( width - imageWidth >= 1 + 2 ) ) {
			TextureBin extendBin{ imageWidth, height, width - imageWidth, imageHeight };
			textureBins.push_back( extendBin );
			// Add a bin to the right of the image we insert if there's space
		} else if ( ( height > 0 ) && ( width < imageWidth ) && ( imageWidth - width >= 1 + 2 ) ) {
			TextureBin extendBin{ width, 0, imageWidth - width, height };
			textureBins.push_back( extendBin );
			width = imageWidth;
		}

		TextureBin tempBin{ 0, height, imageWidth, imageHeight, image };
		textureBins.push_back( tempBin );

		height += imageHeight;
		width = std::max( width, imageWidth );

		AddImageToTextureBin( image, imageData, tempBin );

		return true;
	// Extend atlas to the right
	} else if ( width + imageWidth <= glConfig.maxTextureSize ) {
		// Add a bin above the image we insert if there's space
		if ( ( height > imageHeight ) && ( height - imageHeight >= 1 + 2 ) ) {
			TextureBin extendBin{ width, imageHeight, imageWidth, height - imageHeight };
			textureBins.push_back( extendBin );
			// Add a bin above the current atlas top border if the image we insert exceeds the current atlas height
		} else if ( ( width > 0 ) && ( height < imageHeight ) && ( imageHeight - height >= 1 + 2 ) ) {
			TextureBin extendBin{ 0, height, width, imageHeight - height };
			textureBins.push_back( extendBin );
		}

		TextureBin tempBin{ width, 0, imageWidth, imageHeight, image };
		textureBins.push_back( tempBin );

		width += imageWidth;
		height = std::max( height, imageHeight );

		AddImageToTextureBin( image, imageData, tempBin );

		return true;
	}

	return false;
}

void LoadTextureAtlases() {
	for ( TextureAtlas& textureAtlas : textureAtlases ) {
		textureAtlas.CreateTexture();
	}
}

static uint32_t textureAtlasID = 0;

bool TextureAtlasForImage( image_t* image, byte* imageData ) {
	if ( IsImageCompressed( image->bits ) ) {
		Sys::Drop( "Compressed texture atlases are currently unsupported, image: %s", image->name );
	}

	if ( !( image->bits & IF_NOPICMIP ) ) {
		Sys::Drop( "Images with mipmaps enabled are unsupported in texture atlases, image: %s", image->name );
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
			return true;
		}
	}

	TextureAtlas atlas( image->format, image->internalFormat, filter, image->bits );
	if ( atlas.InsertImage( image, filter, imageData ) ) {
		atlas.id = textureAtlasID;
		image->textureAtlasID = atlas.id;
		textureAtlasID++;

		textureAtlases.push_back( atlas );
		return true;
	} else {
		Log::Warn( "Unable to allocate texture memory in texture atlases for image %s", image->name );
		return false;
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
