/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#include "stdafx.h"
#include "Formats.h"

namespace Falcor {

const FormatDesc kFormatDesc[] =  {
    // Format                           Name,           BytesPerBlock ChannelCount  Type          {bDepth,   bStencil, bCompressed},   {CompressionRatio.Width,     CompressionRatio.Height}    {numChannelBits.x, numChannelBits.y, numChannelBits.z, numChannelBits.w}
    {ResourceFormat::Unknown,            "Unknown",         0,              0,  FormatType::Unknown,    {false,  false, false,},        {1, 1},                                                  {0, 0, 0, 0    }},
    {ResourceFormat::R8Unorm,            "R8Unorm",         1,              1,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {8, 0, 0, 0    }},
    {ResourceFormat::R8Snorm,            "R8Snorm",         1,              1,  FormatType::Snorm,      {false,  false, false,},        {1, 1},                                                  {8, 0, 0, 0    }},
    {ResourceFormat::R16Unorm,           "R16Unorm",        2,              1,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {16, 0, 0, 0   }},
    {ResourceFormat::R16Snorm,           "R16Snorm",        2,              1,  FormatType::Snorm,      {false,  false, false,},        {1, 1},                                                  {16, 0, 0, 0   }},
    {ResourceFormat::RG8Unorm,           "RG8Unorm",        2,              2,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {8, 8, 0, 0    }},
    {ResourceFormat::RG8Snorm,           "RG8Snorm",        2,              2,  FormatType::Snorm,      {false,  false, false,},        {1, 1},                                                  {8, 8, 0, 0    }},
    {ResourceFormat::RG16Unorm,          "RG16Unorm",       4,              2,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {16, 16, 0, 0  }},
    {ResourceFormat::RG16Snorm,          "RG16Snorm",       4,              2,  FormatType::Snorm,      {false,  false, false,},        {1, 1},                                                  {16, 16, 0, 0  }},
    {ResourceFormat::RGB16Unorm,         "RGB16Unorm",      6,              3,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 0 }},
    {ResourceFormat::RGB16Snorm,         "RGB16Snorm",      6,              3,  FormatType::Snorm,      {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 0 }},
    {ResourceFormat::R24UnormX8,         "R24UnormX8",      4,              2,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {24, 8, 0, 0   }},
    {ResourceFormat::RGB5A1Unorm,        "RGB5A1Unorm",     2,              4,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {5, 5, 5, 1    }},
    {ResourceFormat::RGB8Unorm,          "RGB8Unorm",       3,              3,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 0    }},
    {ResourceFormat::RGB8Snorm,          "RGB8Snorm",       3,              3,  FormatType::Snorm,      {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 0    }},
    {ResourceFormat::RGBA8Unorm,         "RGBA8Unorm",      4,              4,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 8    }},
    {ResourceFormat::RGBA8Snorm,         "RGBA8Snorm",      4,              4,  FormatType::Snorm,      {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 8    }},
    {ResourceFormat::RGB10A2Unorm,       "RGB10A2Unorm",    4,              4,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {10, 10, 10, 2 }},
    {ResourceFormat::RGB10A2Uint,        "RGB10A2Uint",     4,              4,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {10, 10, 10, 2 }},
    {ResourceFormat::RGBA16Unorm,        "RGBA16Unorm",     8,              4,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 16}},
    {ResourceFormat::RGBA16Snorm,        "RGBA16Snorm",     8,              4,  FormatType::Snorm,      {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 16}},
    {ResourceFormat::RGBA8UnormSrgb,     "RGBA8UnormSrgb",  4,              4,  FormatType::UnormSrgb,  {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 8    }},
    // Format                           Name,           BytesPerBlock ChannelCount  Type          {bDepth,   bStencil, bCompressed},   {CompressionRatio.Width,     CompressionRatio.Height}
    {ResourceFormat::R16Float,           "R16Float",        2,              1,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {16, 0, 0, 0   }},
    {ResourceFormat::RG16Float,          "RG16Float",       4,              2,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {16, 16, 0, 0  }},
    {ResourceFormat::RGB16Float,         "RGB16Float",      6,              3,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 0 }},
    {ResourceFormat::RGBA16Float,        "RGBA16Float",     8,              4,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 16}},
    {ResourceFormat::R32Float,           "R32Float",        4,              1,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {32, 0, 0, 0   }},
    {ResourceFormat::R32FloatX32,        "R32FloatX32",     8,              2,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {32, 32, 0, 0  }},
    {ResourceFormat::RG32Float,          "RG32Float",       8,              2,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {32, 32, 0, 0  }},
    {ResourceFormat::RGB32Float,         "RGB32Float",      12,             3,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {32, 32, 32, 0 }},
    {ResourceFormat::RGBA32Float,        "RGBA32Float",     16,             4,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {32, 32, 32, 32}},
    {ResourceFormat::R11G11B10Float,     "R11G11B10Float",  4,              3,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {11, 11, 10, 0 }},
    {ResourceFormat::RGB9E5Float,        "RGB9E5Float",     4,              3,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {9, 9, 9, 5    }},
    {ResourceFormat::R8Int,              "R8Int",           1,              1,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {8, 0, 0, 0    }},
    {ResourceFormat::R8Uint,             "R8Uint",          1,              1,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {8, 0, 0, 0    }},
    {ResourceFormat::R16Int,             "R16Int",          2,              1,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {16, 0, 0, 0   }},
    {ResourceFormat::R16Uint,            "R16Uint",         2,              1,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {16, 0, 0, 0   }},
    {ResourceFormat::R32Int,             "R32Int",          4,              1,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {32, 0, 0, 0   }},
    {ResourceFormat::R32Uint,            "R32Uint",         4,              1,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {32, 0, 0, 0   }},
    {ResourceFormat::RG8Int,             "RG8Int",          2,              2,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {8, 8, 0, 0    }},
    {ResourceFormat::RGB8Int,            "RGB8Int",         3,              2,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 0    }},
    {ResourceFormat::RG8Uint,            "RG8Uint",         2,              2,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {8, 8, 0, 0    }},
    {ResourceFormat::RGB8Uint,           "RGB8Uint",        3,              3,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 0,   }},
    {ResourceFormat::RG16Int,            "RG16Int",         4,              2,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {16, 16, 0, 0  }},
    {ResourceFormat::RG16Uint,           "RG16Uint",        4,              2,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {16, 16, 0, 0  }},
    {ResourceFormat::RG32Int,            "RG32Int",         8,              2,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {32, 32, 0, 0  }},
    {ResourceFormat::RG32Uint,           "RG32Uint",        8,              2,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {32, 32, 0, 0  }},
    // Format                           Name,           BytesPerBlock ChannelCount  Type          {bDepth,   bStencil, bCompressed},   {CompressionRatio.Width,     CompressionRatio.Height}
    {ResourceFormat::RGB16Int,           "RGB16Int",        6,              3,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 0 }},
    {ResourceFormat::RGB16Uint,          "RGB16Uint",       6,              3,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 0 }},
    {ResourceFormat::RGB32Int,           "RGB32Int",       12,              3,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {32, 32, 32, 0 }},
    {ResourceFormat::RGB32Uint,          "RGB32Uint",      12,              3,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {32, 32, 32, 0 }},
    {ResourceFormat::RGBA8Int,           "RGBA8Int",        4,              4,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 8    }},
    {ResourceFormat::RGBA8Uint,          "RGBA8Uint",       4,              4,  FormatType::Uint,       {false, false, false, },        {1, 1},                                                  {8, 8, 8, 8    }},
    {ResourceFormat::RGBA16Int,          "RGBA16Int",       8,              4,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 16}},
    {ResourceFormat::RGBA16Uint,         "RGBA16Uint",      8,              4,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {16, 16, 16, 16}},
    {ResourceFormat::RGBA32Int,          "RGBA32Int",      16,              4,  FormatType::Sint,       {false,  false, false,},        {1, 1},                                                  {32, 32, 32, 32}},
    {ResourceFormat::RGBA32Uint,         "RGBA32Uint",     16,              4,  FormatType::Uint,       {false,  false, false,},        {1, 1},                                                  {32, 32, 32, 32}},
    {ResourceFormat::BGRA4Unorm,         "BGRA4Unorm",      2,              4,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {4, 4, 4, 4    }},
    {ResourceFormat::BGRA8Unorm,         "BGRA8Unorm",      4,              4,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 8    }},
    {ResourceFormat::BGRA8UnormSrgb,     "BGRA8UnormSrgb",  4,              4,  FormatType::UnormSrgb,  {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 8    }},
    {ResourceFormat::BGRX8Unorm,         "BGRX8Unorm",      4,              4,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 8    }},
    {ResourceFormat::BGRX8UnormSrgb,     "BGRX8UnormSrgb",  4,              4,  FormatType::UnormSrgb,  {false,  false, false,},        {1, 1},                                                  {8, 8, 8, 8    }},
    {ResourceFormat::Alpha8Unorm,        "Alpha8Unorm",     1,              1,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {0, 0, 0, 8    }},
    {ResourceFormat::Alpha32Float,       "Alpha32Float",    4,              1,  FormatType::Float,      {false,  false, false,},        {1, 1},                                                  {0, 0, 0, 32   }},
    // Format                           Name,           BytesPerBlock ChannelCount  Type          {bDepth,   bStencil, bCompressed},   {CompressionRatio.Width,     CompressionRatio.Height}
    {ResourceFormat::R5G6B5Unorm,        "R5G6B5Unorm",     2,              3,  FormatType::Unorm,      {false,  false, false,},        {1, 1},                                                  {5, 6, 5, 0    }},
    {ResourceFormat::D32Float,           "D32Float",        4,              1,  FormatType::Float,      {true,   false, false,},        {1, 1},                                                  {32, 0, 0, 0   }},
    {ResourceFormat::D32FloatS8Uint,     "D32FloatS8Uint",  4,              1,  FormatType::Float,      {true,   true,  false,},        {1, 1},                                                  {32, 0, 0, 0   }},
    {ResourceFormat::D16Unorm,           "D16Unorm",        2,              1,  FormatType::Unorm,      {true,   false, false,},        {1, 1},                                                  {16, 0, 0, 0   }},
    {ResourceFormat::D32FloatS8X24,      "D32FloatS8X24",   8,              2,  FormatType::Float,      {true,   true,  false,},        {1, 1},                                                  {32, 8, 24, 0  }},
    {ResourceFormat::D24UnormS8,         "D24UnormS8",      4,              2,  FormatType::Unorm,      {true,   true,  false,},        {1, 1},                                                  {24, 8, 0, 0   }},

    {ResourceFormat::BC1Unorm,           "BC1Unorm",        8,              3,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {64, 0, 0, 0   }},
    {ResourceFormat::BC1UnormSrgb,       "BC1UnormSrgb",    8,              3,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {64, 0, 0, 0   }},
    {ResourceFormat::BC1RGBUnorm,        "BC1RGBUnorm",     8,              3,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {64, 0, 0, 0   }},
    {ResourceFormat::BC1RGBSrgb,         "BC1RGBSrgb",      8,              3,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {64, 0, 0, 0   }},
    {ResourceFormat::BC1RGBAUnorm,       "BC1RGBAUnorm",    8,              3,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {64, 0, 0, 0   }},
    {ResourceFormat::BC1RGBASrgb,        "BC1RGBASrgb",     8,              3,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {64, 0, 0, 0   }},

    {ResourceFormat::BC2Unorm,           "BC2Unorm",        16,             4,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC2UnormSrgb,       "BC2UnormSrgb",    16,             4,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC2RGBAUnorm,       "BC2RGBAUnorm",    16,             4,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC2RGBASrgb,        "BC2RGBASrgb",     16,             4,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    
    {ResourceFormat::BC3UnormSrgb,       "BC3UnormSrgb",    16,             4,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC3RGBAUnorm,       "BC3RGBAUnorm",    16,             4,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC3RGBASrgb,        "BC3RGBASrgb",     16,             4,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    
    {ResourceFormat::BC3Unorm,           "BC3Unorm",        16,             4,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC4Unorm,           "BC4Unorm",        8,              1,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {64, 0, 0, 0   }},
    {ResourceFormat::BC4Snorm,           "BC4Snorm",        8,              1,  FormatType::Snorm,      {false,  false, true, },        {4, 4},                                                  {64, 0, 0, 0   }},
    
    {ResourceFormat::BC5Unorm,           "BC5Unorm",        16,             2,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC5Snorm,           "BC5Snorm",        16,             2,  FormatType::Snorm,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},

    {ResourceFormat::BC6HS16,            "BC6HS16",         16,             3,  FormatType::Float,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC6HU16,            "BC6HU16",         16,             3,  FormatType::Float,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC7Unorm,           "BC7Unorm",        16,             4,  FormatType::Unorm,      {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC7UnormSrgb,       "BC7UnormSrgb",    16,             4,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    {ResourceFormat::BC7Srgb,            "BC7Srgb",         16,             4,  FormatType::UnormSrgb,  {false,  false, true, },        {4, 4},                                                  {128, 0, 0, 0  }},
    
};

static_assert(arraysize(kFormatDesc) == (uint32_t)ResourceFormat::BC7Srgb + 1, "Format desc table has a wrong size");

gfx::Format getGFXFormat(ResourceFormat format) {
    switch (format) {
        case ResourceFormat::BC1Unorm:
            return gfx::Format::BC1_UNORM;
        case ResourceFormat::BC1UnormSrgb:
            return gfx::Format::BC1_UNORM_SRGB;
        case ResourceFormat::BC2Unorm:
            return gfx::Format::BC2_UNORM;
        case ResourceFormat::BC2UnormSrgb:
            return gfx::Format::BC2_UNORM_SRGB;
        case ResourceFormat::BC3Unorm:
            return gfx::Format::BC3_UNORM;
        case ResourceFormat::BC3UnormSrgb:
            return gfx::Format::BC3_UNORM_SRGB;
        case ResourceFormat::BC4Snorm:
            return gfx::Format::BC4_SNORM;
        case ResourceFormat::BC4Unorm:
            return gfx::Format::BC4_UNORM;
        case ResourceFormat::BC5Snorm:
            return gfx::Format::BC5_SNORM;
        case ResourceFormat::BC5Unorm:
            return gfx::Format::BC5_UNORM;
        case ResourceFormat::BC6HS16:
            return gfx::Format::BC6H_SF16;
        case ResourceFormat::BC6HU16:
            return gfx::Format::BC6H_UF16;
        case ResourceFormat::BC7Unorm:
            return gfx::Format::BC7_UNORM;
        case ResourceFormat::BC7UnormSrgb:
            return gfx::Format::BC7_UNORM_SRGB;
        case ResourceFormat::BGRA4Unorm:
            return gfx::Format::B4G4R4A4_UNORM;
        case ResourceFormat::BGRA8Unorm:
            return gfx::Format::B8G8R8A8_UNORM;
        case ResourceFormat::BGRA8UnormSrgb:
            return gfx::Format::B8G8R8A8_UNORM_SRGB;
        case ResourceFormat::BGRX8Unorm:
            return gfx::Format::B8G8R8X8_UNORM;
        case ResourceFormat::BGRX8UnormSrgb:
            return gfx::Format::B8G8R8X8_UNORM_SRGB;
        case ResourceFormat::D16Unorm:
            return gfx::Format::D16_UNORM;
        case ResourceFormat::D32Float:
            return gfx::Format::D32_FLOAT;
        case ResourceFormat::D32FloatS8Uint:
            return gfx::Format::D32_FLOAT_S8_UINT;
        case ResourceFormat::R11G11B10Float:
            return gfx::Format::R11G11B10_FLOAT;
        case ResourceFormat::R16Float:
            return gfx::Format::R16_FLOAT;
        case ResourceFormat::R16Int:
            return gfx::Format::R16_SINT;
        case ResourceFormat::R16Snorm:
            return gfx::Format::R16_SNORM;
        case ResourceFormat::R16Uint:
            return gfx::Format::R16_UINT;
        case ResourceFormat::R16Unorm:
            return gfx::Format::R16_UNORM;
        case ResourceFormat::R32Float:
            return gfx::Format::R32_FLOAT;
        case ResourceFormat::R32Int:
            return gfx::Format::R32_SINT;
        case ResourceFormat::R32Uint:
            return gfx::Format::R32_UINT;
        case ResourceFormat::R5G6B5Unorm:
            return gfx::Format::B5G6R5_UNORM;
        case ResourceFormat::R8Int:
            return gfx::Format::R8_SINT;
        case ResourceFormat::R8Snorm:
            return gfx::Format::R8_SNORM;
        case ResourceFormat::R8Uint:
            return gfx::Format::R8_UINT;
        case ResourceFormat::R8Unorm:
            return gfx::Format::R8_UNORM;
        case ResourceFormat::RG16Float:
            return gfx::Format::R16G16_FLOAT;
        case ResourceFormat::RG16Int:
            return gfx::Format::R16G16_SINT;
        case ResourceFormat::RG16Snorm:
            return gfx::Format::R16G16_SNORM;
        case ResourceFormat::RG16Uint:
            return gfx::Format::R16G16_UINT;
        case ResourceFormat::RG16Unorm:
            return gfx::Format::R16G16_UNORM;
        case ResourceFormat::RG32Float:
            return gfx::Format::R32G32_FLOAT;
        case ResourceFormat::RG32Int:
            return gfx::Format::R32G32_SINT;
        case ResourceFormat::RG32Uint:
            return gfx::Format::R32G32_UINT;
        case ResourceFormat::RG8Int:
            return gfx::Format::R8G8_SINT;
        case ResourceFormat::RG8Snorm:
            return gfx::Format::R8G8_SNORM;
        case ResourceFormat::RG8Uint:
            return gfx::Format::R8G8_UINT;
        case ResourceFormat::RG8Unorm:
            return gfx::Format::R8G8_UNORM;
        case ResourceFormat::RGB10A2Uint:
            return gfx::Format::R10G10B10A2_UINT;
        case ResourceFormat::RGB10A2Unorm:
            return gfx::Format::R10G10B10A2_UNORM;
        case ResourceFormat::RGB32Float:
            return gfx::Format::R32G32B32_FLOAT;
        case ResourceFormat::RGB32Int:
            return gfx::Format::R32G32B32_SINT;
        case ResourceFormat::RGB32Uint:
            return gfx::Format::R32G32B32_UINT;
        case ResourceFormat::RGB5A1Unorm:
            return gfx::Format::B5G5R5A1_UNORM;
        case ResourceFormat::RGB9E5Float:
            return gfx::Format::R9G9B9E5_SHAREDEXP;
        case ResourceFormat::RGBA16Float:
            return gfx::Format::R16G16B16A16_FLOAT;
        case ResourceFormat::RGBA16Int:
            return gfx::Format::R16G16B16A16_SINT;
        case ResourceFormat::RGBA16Uint:
            return gfx::Format::R16G16B16A16_UINT;
        case ResourceFormat::RGBA16Unorm:
            return gfx::Format::R16G16B16A16_UNORM;
        case ResourceFormat::RGBA16Snorm:
            return gfx::Format::R16G16B16A16_SNORM;
        case ResourceFormat::RGBA32Float:
            return gfx::Format::R32G32B32A32_FLOAT;
        case ResourceFormat::RGBA32Int:
            return gfx::Format::R32G32B32A32_SINT;
        case ResourceFormat::RGBA32Uint:
            return gfx::Format::R32G32B32A32_UINT;
        case ResourceFormat::RGBA8Int:
            return gfx::Format::R8G8B8A8_SINT;
        case ResourceFormat::RGBA8Snorm:
            return gfx::Format::R8G8B8A8_SNORM;
        case ResourceFormat::RGBA8Uint:
            return gfx::Format::R8G8B8A8_UINT;
        case ResourceFormat::RGBA8Unorm:
            return gfx::Format::R8G8B8A8_UNORM;
        case ResourceFormat::RGBA8UnormSrgb:
            return gfx::Format::R8G8B8A8_UNORM_SRGB;
        default:
            return gfx::Format::Unknown;
    }
}

#ifdef SCRIPTING
SCRIPT_BINDING(ResourceFormat) {
    // Resource formats
    pybind11::enum_<ResourceFormat> resourceFormat(m, "ResourceFormat");
    for (uint32_t i = 0; i < (uint32_t)ResourceFormat::Count; i++) {
        resourceFormat.value(to_string(ResourceFormat(i)).c_str(), ResourceFormat(i));
    }
}
#endif

} // namespace Falcor
