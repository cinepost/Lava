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
#ifndef SRC_FALCOR_RENDERGRAPH_RENDERPASSLIBRARY_H_
#define SRC_FALCOR_RENDERGRAPH_RENDERPASSLIBRARY_H_

#include "Falcor/Utils/Scripting/Dictionary.h"
#include "RenderPass.h"

namespace Falcor {

class Device;

class dlldecl RenderPassLibrary {
	public:
		RenderPassLibrary() = default;
		RenderPassLibrary(RenderPassLibrary&) = delete;
		~RenderPassLibrary();
		using CreateFunc = std::function<RenderPass::SharedPtr(RenderContext*, const Dictionary&)>;

		struct RenderPassDesc {
			RenderPassDesc() = default;
			RenderPassDesc(const RenderPass::Info& info, CreateFunc func) : info(info), func(func) {}

			RenderPass::Info info;
			CreateFunc func = nullptr;
		};

		using DescVec = std::vector<RenderPassDesc>;

		/** Get an instance of the library. It's a singleton, you'll always get the same object
		*/
		static RenderPassLibrary& instance();

		/** Call this before the app is shutting down to release all the libraries
		*/
		void shutdown();

		/** Register a render pass to the library.
			\param[in] info Render pass info.
			\param[in] func Render pass factory.
			\return The render pass library.
		*/
		RenderPassLibrary& registerPass(const RenderPass::Info& info, CreateFunc func);

		/** Instantiate a new render pass object.
			\param[in] pRenderContext The render context.
			\param[in] className Render pass class name.
			\param[in] dict Dictionary for serialized parameters.
			\return A new object, or an exception is thrown if creation failed. Nullptr is returned if class name cannot be found.
		*/
		RenderPass::SharedPtr createPass(RenderContext* pRenderContext, const char* className, const Dictionary& dict = {});

		/** Get a list of all the registered classes
		*/
		DescVec enumerateClasses() const;

		/** Load a new render-pass library (DLL/DSO)
		*/
		void loadLibrary(Device::SharedPtr pDevice, const std::string& filename);

		/** Release a previously loaded render-pass library (DLL/DSO)
		*/
		void releaseLibrary(const std::string& filename);

		/** Reload render-pass libraries
		*/
		void reloadLibraries(RenderContext* pRenderContext);

		/** A render-pass library should implement a function called `getPasses` with the following signature
		*/
		using LibraryFunc = void(*)(RenderPassLibrary& lib);

		using StrVec = std::vector<std::string>;

		/** Get list of registered render-pass libraries
		*/
		static StrVec enumerateLibraries();

		/** Get a description from one existing render pass class
		*/
		static std::string getClassDescription(const std::string& className);

	private:
		static RenderPassLibrary* spInstance;

		struct ExtendedDesc : RenderPassDesc {
			ExtendedDesc() = default;
			ExtendedDesc(const RenderPass::Info& info, CreateFunc func, SharedLibraryHandle library) : RenderPassDesc(info, func), library(library) {}

			SharedLibraryHandle library = nullptr;
		};

		void registerInternal(const RenderPass::Info& info, CreateFunc func, SharedLibraryHandle library);

		struct LibDesc {
			SharedLibraryHandle library;
			time_t lastModified;
		};
		
		std::unordered_map<std::string, LibDesc> mLibs;
		std::unordered_map<std::string, ExtendedDesc> mPasses;

		std::unordered_map<std::string, std::vector<Device::SharedPtr>> mLibDevices;

		void reloadLibrary(RenderContext* pRenderContext, const std::string& filename);
};

}  // namespace Falcor

#endif  // SRC_FALCOR_RENDERGRAPH_RENDERPASSLIBRARY_H_
