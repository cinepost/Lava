================================================================================
NVIDIA Texture Tools 3
================================================================================

The NVIDIA Texture Tools are a set of command-line and GUI tools and an SDK for
compressing textures into various GPU-friendly formats.

For more information of the use of command-line tools:

   * Open a "cmd" window and run each of the executables without any argument.

For how to build applications with the NVTT library and its APIs:
  
   * ./docs/index.html

For code samples using the NVTT library, check out https://github.com/nvpro-samples/nvtt_samples.

For comments, feature requests, and bugs, please contact texturetools@nvidia.com.

================================================================================
Features
================================================================================
* Compression/decompression of textures using command-line tools
* Compression/decompression of textures using library APIs
* Supported input formats: DDS, BMP, GIF, HDR, JPG, PGM, PIC, PNG, PPM, PSD, TGA
* Output file formats: DDS, BMP, HDR, JPG, PNG, TGA
* Supported texture formats: 
  - BC1(DXT1)
  - BC2(DXT3) 
  - BC3(DXT5)
  - BC4
  - BC5
  - BC6H
  - BC7
  - ASTC: 2D blocks, low dynamic range 
* CUDA acceleration:
  - Supported for ASTC compression at all quality levels
  - Supported for BCx compression at Default quality, and for BC1 and BC4-7 at
  Fastest and BC1-BC3 at Production and Highest quality
  - Supported for basic image preprocessing

================================================================================
Package Structure 
================================================================================
[PackageRoot]/
             /LICENSE.TXT               ----- license
             /nvbatchcompress           ----- batch compressing tool
             /nvcompress                ----- compressing tool
             /nvddsinfo                 ----- tool for showing DDS file information
             /nvdecompress              ----- decompressing tool
             /nvimgdiff                 ----- tool for comparing 2 images
For building programs using NVTT:
             /libnvtt.so.*              ----- redistributable NVTT shared library
             /libcudart.so.*            ----- redistributable CUDA shared library
             /docs                      ----- API documentation and guide
             /include                   ----- headers

================================================================================
System Requirements
================================================================================
Operating System: 64-bit only; Ubuntu 16.04+ or similarly compatible distro

libc.so version 6 or higher

To use the C++ API: Compiler ABI-compatible with g++ 5.4.0

To use the C API: Compiler supporting at least C99 and dynamic linking

For CUDA-accelerated compression:
  Any Maxwell+ NVIDIA GPU
  NVIDIA driver 451.48 or newer

================================================================================
About DDS as a Container of Textures
================================================================================
Compressed texture data is stored into DDS files following the DDS specifications:
http://msdn.microsoft.com/en-us/library/bb943991.aspx
https://msdn.microsoft.com/en-us/library/bb173059.aspx

In addition, ASTC formats are defined as follows:

		DXGI_FORMAT_ASTC_4X4_UNORM              = 134,
		DXGI_FORMAT_ASTC_4X4_UNORM_SRGB         = 135,
		DXGI_FORMAT_ASTC_5X4_TYPELESS           = 137,
		DXGI_FORMAT_ASTC_5X4_UNORM              = 138,
		DXGI_FORMAT_ASTC_5X4_UNORM_SRGB         = 139,
		DXGI_FORMAT_ASTC_5X5_TYPELESS           = 141,
		DXGI_FORMAT_ASTC_5X5_UNORM              = 142,
		DXGI_FORMAT_ASTC_5X5_UNORM_SRGB         = 143,
		DXGI_FORMAT_ASTC_6X5_TYPELESS           = 145,
		DXGI_FORMAT_ASTC_6X5_UNORM              = 146,
		DXGI_FORMAT_ASTC_6X5_UNORM_SRGB         = 147,
		DXGI_FORMAT_ASTC_6X6_TYPELESS           = 149,
		DXGI_FORMAT_ASTC_6X6_UNORM              = 150,
		DXGI_FORMAT_ASTC_6X6_UNORM_SRGB         = 151,
		DXGI_FORMAT_ASTC_8X5_TYPELESS           = 153,
		DXGI_FORMAT_ASTC_8X5_UNORM              = 154,
		DXGI_FORMAT_ASTC_8X5_UNORM_SRGB         = 155,
		DXGI_FORMAT_ASTC_8X6_TYPELESS           = 157,
		DXGI_FORMAT_ASTC_8X6_UNORM              = 158,
		DXGI_FORMAT_ASTC_8X6_UNORM_SRGB         = 159,
		DXGI_FORMAT_ASTC_8X8_TYPELESS           = 161,
		DXGI_FORMAT_ASTC_8X8_UNORM              = 162,
		DXGI_FORMAT_ASTC_8X8_UNORM_SRGB         = 163,
		DXGI_FORMAT_ASTC_10X5_TYPELESS          = 165,
		DXGI_FORMAT_ASTC_10X5_UNORM             = 166,
		DXGI_FORMAT_ASTC_10X5_UNORM_SRGB        = 167,
		DXGI_FORMAT_ASTC_10X6_TYPELESS          = 169,
		DXGI_FORMAT_ASTC_10X6_UNORM             = 170,
		DXGI_FORMAT_ASTC_10X6_UNORM_SRGB        = 171,
		DXGI_FORMAT_ASTC_10X8_TYPELESS          = 173,
		DXGI_FORMAT_ASTC_10X8_UNORM             = 174,
		DXGI_FORMAT_ASTC_10X8_UNORM_SRGB        = 175,
		DXGI_FORMAT_ASTC_10X10_TYPELESS         = 177,
		DXGI_FORMAT_ASTC_10X10_UNORM            = 178,
		DXGI_FORMAT_ASTC_10X10_UNORM_SRGB       = 179,
		DXGI_FORMAT_ASTC_12X10_TYPELESS         = 181,
		DXGI_FORMAT_ASTC_12X10_UNORM            = 182,
		DXGI_FORMAT_ASTC_12X10_UNORM_SRGB       = 183,
		DXGI_FORMAT_ASTC_12X12_TYPELESS         = 185,
		DXGI_FORMAT_ASTC_12X12_UNORM            = 186,
		DXGI_FORMAT_ASTC_12X12_UNORM_SRGB       = 187,

================================================================================
Known Issues
================================================================================
* ASTC HDR modes are not supported.

================================================================================
New in 3.1.x
================================================================================
3.1.6:
* Added the new API nvtt::useCurrentDevice()
* Moved to CUDA 11
* Added sm_80 and sm_86 (Ampere); move minimum SM from sm_30 to sm_35
* Fixed a memory leak in SurfaceSet
* Added stb_image and stb_image_write as readers and writers
3.1.5:
* Fixed an issue with compression on Quadro K2000 devices
3.1.4:
* Moved to VC 2017
3.1.3:
* Faster BC3 CPU encoding
* Fixed GPU/CPU BC3 discrepancy
3.1.2:
* Stability fixes for CPU encoding
3.1.1:
* Added Surface::demultiplyAlpha()
* Added sm_70 (Volta) and sm_75 (Turing) support
3.1.0:
* Reworked ASTC/BC7 for better performance
* New APIs (nvtt_lowlevel.h) for more straightforward texture compression
* Better quality for BC6 - fast mode.

================================================================================
Revision History
================================================================================
2021/11/8   NVIDIA Texture Tools 3.1.6
2020/8/3    NVIDIA Texture Tools 3.1.5
2020/6/13   NVIDIA Texture Tools 3.1.4
2020/2/12   NVIDIA Texture Tools 3.1.3
2020/2/4    NVIDIA Texture Tools 3.1.2
2019/7/8    NVIDIA Texture Tools 3.1.0
2019/2/13   NVIDIA Texture Tools 3.0.2
2018/11/1   NVIDIA Texture Tools 3.0.1
2018/9/20   NVIDIA Texture Tools 3 Beta 9.7
2018/5/30   NVIDIA Texture Tools 3 Beta 9.6
2018/5/15   NVIDIA Texture Tools 3 Beta 9.5
2018/4/23   NVIDIA Texture Tools 3 Beta 9.4
2018/1/19   NVIDIA Texture Tools 3 Beta 9.3
2018/1/12   NVIDIA Texture Tools 3 Beta 9.2
2017/12/19  NVIDIA Texture Tools 3 Beta 9.1
2017/11/1   NVIDIA Texture Tools 3 Beta 9
2017/4/6    NVIDIA Texture Tools 3 Beta 8.4
2017/3/21   NVIDIA Texture Tools 3 Beta 8.3
2017/3/15   NVIDIA Texture Tools 3 Beta 8.2
2017/2/28   NVIDIA Texture Tools 3 Beta 8.1
2017/2/15   NVIDIA Texture Tools 3 Beta 8
2017/2/9    NVIDIA Texture Tools 3 Beta 7.1
2017/2/8    NVIDIA Texture Tools 3 Beta 7
2017/1/18   NVIDIA Texture Tools 3 Beta 6
2016/11/22  NVIDIA Texture Tools 3 Beta 5.2
2016/10/24  NVIDIA Texture Tools 3 Beta 5.1
2016/10/19  NVIDIA Texture Tools 3 Beta 5
2016/9/7    NVIDIA Texture Tools 3 Beta 4
2016/7/8    NVIDIA Texture Tools 3 Beta 3
2016/7/1    NVIDIA Texture Tools 3 Beta 2
2016/3/23   NVIDIA Texture Tools 3 Beta 1
2016/2/5    NVIDIA Texture Tools 3 Alpha 3
2015/11/18  NVIDIA Texture Tools 3 Alpha 2
2015/10/28  NVIDIA Texture Tools 3 Alpha 1

================================================================================
Credits
================================================================================

NVIDIA Texture Tools by Fei Yang, Ignacio Castaño, Neil Bickford,
Roberto Teixeira, Tadahito Kobayashi, and contributors.

================================================================================
Third-Party Dependencies
================================================================================

SimdVector_SSE.h and SimdVector_VE.h

	Copyright (c) 2006 Simon Brown                          si@sjbrown.co.uk

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the 
	"Software"), to	deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to 
	permit persons to whom the Software is furnished to do so, subject to 
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


ClusterFit

    Copyright (c) 2006 Simon Brown                          si@sjbrown.co.uk
    Copyright (c) 2006 Ignacio Castano                      icastano@nvidia.com

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files (the
    "Software"), to	deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the Software, and to
    permit persons to whom the Software is furnished to do so, subject to
    the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


poshlib

Copyright (c) 2004, Brian Hook
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * The names of this package'ss contributors contributors may not
      be used to endorse or promote products derived from this
      software without specific prior written permission.


THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


stb

------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------


squish

	Copyright (c) 2006 Simon Brown                          si@sjbrown.co.uk

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the 
	"Software"), to	deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to 
	permit persons to whom the Software is furnished to do so, subject to 
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


Half

// Copyright 2006 Mike Acton <macton@gmail.com>
// 
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included 
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE