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

/* common.glsl */

/* Common defines */

/* Allows accessing each element of a uvec4 array with a singular ID
Useful to avoid wasting memory due to alignment requirements
array must be in the form of uvec4 array[] */

#define UINT_FROM_UVEC4_ARRAY( array, id ) ( array[id / 4][id % 4] )
#define UVEC2_FROM_UVEC4_ARRAY( array, id ) ( id % 2 == 0 ? array[id / 2].xy : array[id / 2].zw )

/* Bit 0: color add
Bit 1: color negate
Bit 2: lightFactor add
Bit 3: alpha add
Bit 4: alpha negate */

float colorModArray[3] = float[3] ( 0.0f, 1.0f, -1.0f );

vec4 ColorModulateToColor( in uint colorMod ) {
	vec4 colorModulate = vec4( colorModArray[colorMod & 3] );
	colorModulate.a = ( colorModArray[( colorMod & 24 ) >> 3] );
	return colorModulate;
}

vec4 ColorModulateToColor( in uint colorMod, const in float lightFactor ) {
	vec4 colorModulate = vec4( colorModArray[colorMod & 3] + ( colorMod & 4 ) * lightFactor );
	colorModulate.a = ( colorModArray[( colorMod & 24 ) >> 3] );
	return colorModulate;
}

#define BITPACK_SIZE 32

#define BitStreamLoad1a( array, offset, startBits, carryover, out ) (\
	out = carryover == 0 ? UINT_FROM_UVEC4_ARRAY( array, offset )\
	: ( UINT_FROM_UVEC4_ARRAY( array, offset ) & ( 1 << startBits ) | ( UINT_FROM_UVEC4_ARRAY( array, offset + 1 ) >> carryover ) & ( 1 << carryover )\
)

#define BitStreamLoadRaw1( array, offset, out ) ( out[0] = UINT_FROM_UVEC4_ARRAY( array, offset ); )

#define BitStreamLoadRaw2( array, offset, out ) (\
	out[0] = UINT_FROM_UVEC4_ARRAY( array, offset );\
	out[1] = UINT_FROM_UVEC4_ARRAY( array, offset + 1 );\
)

#define BitStreamLoadRaw3( array, offset, out ) (\
	out[0] = UINT_FROM_UVEC4_ARRAY( array, offset );\
	out[1] = UINT_FROM_UVEC4_ARRAY( array, offset + 1 );\
	out[2] = UINT_FROM_UVEC4_ARRAY( array, offset + 2 );\
)

#define BitStreamLoadRaw4( array, offset, out ) (\
	out[0] = UINT_FROM_UVEC4_ARRAY( array, offset );\
	out[1] = UINT_FROM_UVEC4_ARRAY( array, offset + 1 );\
	out[2] = UINT_FROM_UVEC4_ARRAY( array, offset + 2 );\
	out[3] = UINT_FROM_UVEC4_ARRAY( array, offset + 3 );\
)

#define BitStreamLoadRaw5( array, offset, out ) (\
	out[0] = UINT_FROM_UVEC4_ARRAY( array, offset );\
	out[1] = UINT_FROM_UVEC4_ARRAY( array, offset + 1 );\
	out[2] = UINT_FROM_UVEC4_ARRAY( array, offset + 2 );\
	out[3] = UINT_FROM_UVEC4_ARRAY( array, offset + 3 );\
	out[4] = UINT_FROM_UVEC4_ARRAY( array, offset + 4 );\
)

/* void BitStreamLoad1( const in input[2], const in uint carryover, inout uint out[1] ) {
	out[0] = carryover == 0 ? input[0]
		: ( input[0] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[1] >> carryover ) & ( 1 << carryover ) );
}

void BitStreamLoad2( const in input[3], const in uint carryover, inout uint out[2] ) {
	out[0] = carryover == 0 ? input[0]
		: ( input[0] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[1] >> carryover ) & ( 1 << carryover ) );
	out[1] = carryover == 0 ? input[1]
		: ( input[1] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[2] >> carryover ) & ( 1 << carryover ) );
}

void BitStreamLoad3( const in input[4], const in uint carryover, inout uint out[3] ) {
	out[0] = carryover == 0 ? input[0]
		: ( input[0] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[1] >> carryover ) & ( 1 << carryover ) );
	out[1] = carryover == 0 ? input[1]
		: ( input[1] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[2] >> carryover ) & ( 1 << carryover ) );
	out[2] = carryover == 0 ? input[2]
		: ( input[2] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[3] >> carryover ) & ( 1 << carryover ) );
}

void BitStreamLoad4( const in input[5], const in uint carryover, inout uint out[4] ) {
	out[0] = carryover == 0 ? input[0]
		: ( input[0] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[1] >> carryover ) & ( 1 << carryover ) );
	out[1] = carryover == 0 ? input[1]
		: ( input[1] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[2] >> carryover ) & ( 1 << carryover ) );
	out[2] = carryover == 0 ? input[2]
		: ( input[2] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[3] >> carryover ) & ( 1 << carryover ) );
	out[3] = carryover == 0 ? input[3]
		: ( input[3] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[4] >> carryover ) & ( 1 << carryover ) );
}

void BitStreamLoad5( const in input[6], const in uint carryover, inout uint out[5] ) {
	out[0] = carryover == 0 ? input[0]
		: ( input[0] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[1] >> carryover ) & ( 1 << carryover ) );
	out[1] = carryover == 0 ? input[1]
		: ( input[1] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[2] >> carryover ) & ( 1 << carryover ) );
	out[2] = carryover == 0 ? input[2]
		: ( input[2] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[3] >> carryover ) & ( 1 << carryover ) );
	out[3] = carryover == 0 ? input[3]
		: ( input[3] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[4] >> carryover ) & ( 1 << carryover ) );
	out[4] = carryover == 0 ? input[4]
		: ( input[4] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[5] >> carryover ) & ( 1 << carryover ) );
}

void BitStreamLoad6( const in input[7], const in uint carryover, inout uint out[6] ) {
	out[0] = carryover == 0 ? input[0]
		: ( input[0] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[1] >> carryover ) & ( 1 << carryover ) );
	out[1] = carryover == 0 ? input[1]
		: ( input[1] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[2] >> carryover ) & ( 1 << carryover ) );
	out[2] = carryover == 0 ? input[2]
		: ( input[2] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[3] >> carryover ) & ( 1 << carryover ) );
	out[3] = carryover == 0 ? input[3]
		: ( input[3] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[4] >> carryover ) & ( 1 << carryover ) );
	out[4] = carryover == 0 ? input[4]
		: ( input[4] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[5] >> carryover ) & ( 1 << carryover ) );
	out[5] = carryover == 0 ? input[5]
		: ( input[5] & ( 1 << ( BITPACK_SIZE - carryover ) ) | ( input[6] >> carryover ) & ( 1 << carryover ) );
} */
