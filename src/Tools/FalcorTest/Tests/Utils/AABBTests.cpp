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
#include "Testing/UnitTest.h"
#include "Falcor/Utils/Debug/debug.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/io.hpp>

namespace Falcor
{
    namespace
    {
        // Some test points. Use exactly representable fp32 values to not worry about numerical issues.
        const float3 kTestData[] =
        {
            {  1.00f,  2.50f, -0.50f },
            { -3.50f, -0.00f, -1.25f },
            {  4.00f,  2.75f, -2.50f },
            {  0.50f,  1.25f,  4.50f },
        };
    }

    GPU_TEST(AABB)
    {
        const uint32_t resultSize = 5;

        // Setup and run GPU test.
        ctx.createProgram("Tests/Utils/AABBTests.cs.slang", "testAABB2");
        ctx.allocateStructuredBuffer("result", resultSize);
        ctx.allocateStructuredBuffer("testData", arraysize(kTestData), kTestData, sizeof(kTestData));
        ctx["CB"]["n"] = (uint32_t)arraysize(kTestData);
        ctx.runProgram();

        // Verify results.
        const float3* result = ctx.mapBuffer<const float3>("result");
        size_t i = 0;

        for(uint ii = 0; ii < 5; ii++) {
            std::cout << result[ii] << std::endl;
        }

        // Test 0
        EXPECT_EQ(result[i], kTestData[0]) << "i = " << i; i++;
        EXPECT_EQ(result[i], kTestData[0]) << "i = " << i; i++;
        EXPECT_EQ(result[i], kTestData[1]) << "i = " << i; i++;
        EXPECT_EQ(result[i], kTestData[2]) << "i = " << i; i++;

        assert(i <= resultSize);
        ctx.unmapBuffer("result");
    }
}
