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
							const GLint newMinFilterType, const GLint newMaxFilterType ) :
	format( newFormat ),
	internalFormat( newInternalFormat ),
	minFilterType( newMinFilterType ),
	maxFilterType( newMaxFilterType ) {
}

TextureAtlas::~TextureAtlas() {
	glDeleteTextures( 1, &id );
}

void TextureAtlas::AddImageToTextureBin( image_t* image, byte* imageData, const TextureBin textureBin ) {
	const uint16_t imageWidth = image->uploadWidth + 2;
	const uint16_t imageHeight = image->uploadHeight + 2;

	// image->textureAtlas = this;
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

	const int currentTexture = tr.currenttextures[glState.currenttmu];
	if ( !allocated ) {

		glGenTextures( 1, &id );
		glBindTexture( GL_TEXTURE_2D, id );
		Log::Warn( "alloc id: %u", id );
		glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr );
		
		if ( glConfig2.textureAnisotropyAvailable ) {
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, glConfig2.textureAnisotropy );
		}

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilterType );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxFilterType );

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

		GL_CheckErrors();

		allocated = true;
		allocatedWidth = width;
		allocatedHeight = height;
	} else if ( width > allocatedWidth || height > allocatedHeight ) {
		const FBO_t* currentFBO = glState.currentFBO;

		GLuint tmpFBOID;
		glGenFramebuffers( 1, &tmpFBOID );
		GL_fboShim.glBindFramebuffer( GL_READ_FRAMEBUFFER, tmpFBOID );
		GL_fboShim.glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0 );

		GLuint newID;
		glGenTextures( 1, &newID );
		glBindTexture( GL_TEXTURE_2D, newID );
		Log::Warn( "deleted id: %u copy id: %u", id, newID );
		glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr );

		if ( glConfig2.textureAnisotropyAvailable ) {
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, glConfig2.textureAnisotropy );
		}

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilterType );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxFilterType );

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

		glReadBuffer( GL_COLOR_ATTACHMENT0 );
		glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, allocatedWidth, allocatedHeight );

		glDeleteTextures( 1, &id );

		id = newID;
		allocatedWidth = width;
		allocatedHeight = height;

		if ( currentFBO ) {
			GL_fboShim.glBindFramebuffer( GL_FRAMEBUFFER, currentFBO->frameBuffer );
		} else {
			GL_fboShim.glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		}

		glDeleteFramebuffers( 1, &tmpFBOID );

		GL_CheckErrors();
	} else {
		glBindTexture( GL_TEXTURE_2D, id );
	}

	glTexSubImage2D( GL_TEXTURE_2D, 0, image->textureAtlasX, image->textureAtlasY, image->textureAtlasWidth, image->textureAtlasHeight,
		             format, type, imageData );

	GL_CheckErrors();

	glGenerateMipmap( GL_TEXTURE_2D );

	glBindTexture( GL_TEXTURE_2D, currentTexture );

	GL_CheckErrors();
}

bool TextureAtlas::InsertImage( image_t* image, const GLint imageMinFilterType, const GLint imageMaxFilterType, byte* imageData ) {
	if ( format != image->format || internalFormat != image->internalFormat
		 || minFilterType != imageMinFilterType || maxFilterType != imageMaxFilterType ) {
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

TextureAtlas* TextureAtlasForImage( image_t* image, byte* imageData ) {
	if ( false ) {
		return nullptr;
	}

	GLint minFilterType;
	GLint maxFilterType;
	switch ( image->filterType ) {
		case filterType_t::FT_DEFAULT:
			minFilterType = gl_filter_min;
			maxFilterType = gl_filter_max;
			break;
		case filterType_t::FT_LINEAR:
			minFilterType = GL_LINEAR;
			maxFilterType = GL_LINEAR;
			break;
		case filterType_t::FT_NEAREST:
			minFilterType = GL_NEAREST;
			maxFilterType = GL_NEAREST;
			break;
	}

	for ( TextureAtlas& atlas : textureAtlases ) {
		if ( atlas.InsertImage( image, minFilterType, maxFilterType, imageData ) ) {
			return &atlas;
		}
	}

	TextureAtlas atlas( image->format, image->internalFormat, minFilterType, maxFilterType );
	if ( atlas.InsertImage( image, minFilterType, maxFilterType, imageData ) ) {
		textureAtlases.push_back( atlas );
		return &textureAtlases[textureAtlases.size() - 1];
	} else {
		Log::Warn( "Unable to allocate texture memory in texture atlases for image %s", image->name );
		return nullptr;
	}
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
