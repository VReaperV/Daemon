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
// TextureAtlas.h

#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <vector>
#include "GL/glew.h"

struct image_t;
struct TextureBin {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    image_t* image;
};

enum filterProxy {
    FP_DEFAULT,
    FP_LINEAR,
    FP_NEAREST
};

class TextureAtlas {
    public:
    uint32_t id;
    image_t* texture;
    bool allocated = false;

    TextureAtlas() = delete;
    TextureAtlas( const uint16_t newWidth, const uint16_t newHeight, const GLenum newFormat, const GLint newInternalFormat,
        const filterProxy newFilterProxy, const uint32_t newBits );
    TextureAtlas( const GLenum newFormat, const GLint newInternalFormat, const filterProxy newFilterProxy,
        const uint32_t newBits );
    ~TextureAtlas();

    void CreateTexture();
    void UploadTexture( image_t* image );
    void AddImageToTextureBin( image_t* image, byte* imageData, const TextureBin textureBin );
    bool InsertImage( image_t* image, const filterProxy newFilterProxy, byte* imageData );
    void print();

    private:
    static const uint8_t texelBytes = 4;

    uint16_t allocatedWidth = 0;
    uint16_t allocatedHeight = 0;
    const bool restrictSize;
    std::vector<TextureBin> textureBins;

    uint16_t width = 0;
    uint16_t height = 0;
    const uint16_t borderSize = 1;
    const uint32_t bits;
    const GLenum format;
    const GLint internalFormat;
    const GLenum type = GL_UNSIGNED_BYTE;
    const filterProxy filter;

    void AddTextureBin( const TextureBin textureBin );
    void DeleteTextureBinIfEmpty( const size_t index );
};

void LoadTextureAtlases();
bool TextureAtlasForImage( image_t* image, byte* imageData );
void FreeTextureAtlases();

extern std::vector<TextureAtlas> textureAtlases;

#endif // TEXTURE_ATLAS_H
