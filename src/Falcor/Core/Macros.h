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
#ifndef SRC_FALCOR_CORE_MACROS_H_
#define SRC_FALCOR_CORE_MACROS_H_

#include "FalcorPlatform.h"

// Define DLL export/import
#if FALCOR_MSVC
#define falcorexport __declspec(dllexport)
#define falcorimport __declspec(dllimport)
#define FALCOR_API_EXPORT __declspec(dllexport)
#define FALCOR_API_IMPORT __declspec(dllimport)
#elif FALCOR_GCC
#define falcorexport __attribute__ ((visibility ("default")))
#define falcorimport  // extern
#define FALCOR_API_EXPORT __attribute__ ((visibility ("default")))
#define FALCOR_API_IMPORT //extern
#endif  // _MSC_VER

#ifdef FALCOR_DLL
#define FALCOR_API FALCOR_API_EXPORT
#define dlldecl falcorexport
#else   // BUILDING_SHARED_DLL
#define FALCOR_API FALCOR_API_IMPORT
#define dlldecl falcorimport
#endif  // BUILDING_SHARED_DLL

#ifdef PASS_DLL
#define PASS_API FALCOR_API_EXPORT
#else   // BUILDING_SHARED_DLL
#define PASS_API FALCOR_API_IMPORT
#endif  // BUILDING_SHARED_DLL

#endif  // SRC_FALCOR_CORE_MACROS_H_
