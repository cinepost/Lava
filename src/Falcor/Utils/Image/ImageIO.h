/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#ifndef SRC_FALCOR_UTILS_IMAGE_IMAGEIO_H_
#define SRC_FALCOR_UTILS_IMAGE_IMAGEIO_H_

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"

#include "Utils/Image/Bitmap.h"
#include "Core/API/Texture.h"

namespace Falcor {

class dlldecl ImageIO {
	public:
		enum class CompressionMode {
			/** Stores RGB data with 1 bit of alpha.
				8 bytes per block.
			*/
			BC1,

			/** Stores RGBA data. Combines BC1 for RGB with 4 bits of alpha.
				16 bytes per block.
			*/
			BC2,

			/** Stores RGBA data. Combines BC1 for RGB and BC4 for alpha.
				16 bytes per block.
			*/
			BC3,

			/** Stores a single grayscale channel.
				8 bytes per block.
			*/
			BC4,

			/** Stores two channels using BC4 for each channel.
				16 bytes per block.
			*/
			BC5,

			/** Stores RGB 16-bit floating point data.
				16 bytes per block.
			*/
			BC6,

			/** Stores 8-bit RGB or RGBA data.
				16 bytes per block.
			*/
			BC7,

			/** No compression mode specified.
			*/
			None
		};
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_IMAGE_IMAGEIO_H_