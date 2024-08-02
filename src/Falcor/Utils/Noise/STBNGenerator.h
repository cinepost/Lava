#ifndef SRC_FALCOR_UTILS_NOISE_STBNGNERATOR_H_
#define SRC_FALCOR_UTILS_NOISE_STBNGNERATOR_H_

#include "Falcor/Core/API/Formats.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Core/Program/Program.h"
#include "Falcor/Core/Program/ShaderVar.h"
#include "Falcor/Utils/ThreadPool.h"

#include "STBNGenerator.slangh"

namespace Falcor {

/** Utility class for sample generators on the GPU.

    This class has functions for configuring the shader program and
    uploading the necessary lookup tables (if needed).
    On the GPU, import SampleGenerator.slang in your shader program.
*/
class dlldecl STBNGenerator {
    public:
        using SharedPtr = std::shared_ptr<STBNGenerator>;
        using SharedConstPtr = std::shared_ptr<const STBNGenerator>;

        enum class Type : uint32_t {
            Scalar  = STBN_GENERATOR_SCALAR,
            Vector  = STBN_GENERATOR_VECTOR,
            Default = STBN_GENERATOR_DEFAULT,
        };

        /** Factory function for creating a sample generator of the specified type.
            \param[in] type The type of sample generator. See SampleGeneratorType.slangh.
            \return New object, or throws an exception on error.
        */
        static SharedPtr create(Device::SharedPtr pDevice, uint3 dims, Type type, ResourceFormat format, bool async = false);

        /** Get macro definitions for this sample generator.
            \return Macro definitions that must be set on the shader program that uses this sampler.
        */
        Shader::DefineList getDefines() const;

        /** Binds the data to a program vars object.
            \param[in] pVars ProgramVars of the program to set data into.
            \return false if there was an error, true otherwise.
        */
        bool setShaderData(ShaderVar const& var) const;

        const uint3& getDims() const { return mDims; }

        const ResourceFormat& getFormat() const { return mFormat; }

    protected:
        STBNGenerator(Device::SharedPtr pDevice, uint3 dims, Type type, ResourceFormat format, bool async);

        void generateNoiseDataAsync();
        void generateNoiseData();

        void uploadNoiseData() const;

    private:

        Device::SharedPtr           mpDevice;
        mutable Texture::SharedPtr  mpNoiseTexture;
        Sampler::SharedPtr          mpNoiseSampler;

        uint3           mDims;
        Type            mType;
        ResourceFormat  mFormat;
        bool            mAsync;
        mutable bool    mDirty;

        std::vector<float>          mNoiseData;
        mutable std::future<void>   mGenerateNoiseDataTask;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_NOISE_STBNGNERATOR_H_
