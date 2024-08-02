#include <random>

#include "Falcor/Core/API/RenderContext.h"

#include "STBN.h"
#include "STBNGenerator.h"
#include "lava_utils_lib/logging.h"

static const uint32_t kMaxDimsX = 256;
static const uint32_t kMaxDimsY = 256;
static const uint32_t kMaxDimsZ = 128;

static std::random_device rd;
static std::mt19937 rndEngine(rd());

namespace Falcor {


STBNGenerator::SharedPtr STBNGenerator::create(Device::SharedPtr pDevice, uint3 dims, Type type, ResourceFormat format, bool async) {
    if(dims[0] == 0) {
        LLOG_WRN << "STBNGenerator width is zero! Set to 1";
    } else if(dims[0] > kMaxDimsX) {
        LLOG_WRN << "STBNGenerator width " << dims[0] << " exceeds maximum " << kMaxDimsX << ". Set width to " << kMaxDimsX;
    }

    if(dims[1] == 0) {
        LLOG_WRN << "STBNGenerator height is zero! Set to 1";
    } else if(dims[1] > kMaxDimsY) {
        LLOG_WRN << "STBNGenerator height " << dims[1] << " exceeds maximum " << kMaxDimsY << ". Set height to " << kMaxDimsY;
    }

    if(dims[2] == 0) {
        LLOG_WRN << "STBNGenerator depth is zero! Set to 1";
    } else if(dims[2] > kMaxDimsZ) {
        LLOG_WRN << "STBNGenerator depth " << dims[2] << " exceeds maximum " << kMaxDimsZ << ". Set depth to " << kMaxDimsZ;
    }

    uint32_t dim_x = std::max(1u, std::min(dims[0], kMaxDimsX));
    uint32_t dim_y = std::max(1u, std::min(dims[1], kMaxDimsY));
    uint32_t dim_z = std::max(1u, std::min(dims[2], kMaxDimsZ));

    return SharedPtr(new STBNGenerator(pDevice, {dim_x, dim_y, dim_z}, type, format, async));
}

STBNGenerator::STBNGenerator(Device::SharedPtr pDevice, uint3 dims, Type type, ResourceFormat format, bool async) : mpDevice(pDevice), mDims(dims), mType(type), mFormat(format), mAsync(async), mDirty(true) {
    mpNoiseTexture = Texture::create3D(mpDevice, mDims[0], mDims[1], mDims[2], mFormat, 1, nullptr, Resource::BindFlags::ShaderResource, false);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point)
        .setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap)
        .setUnnormalizedCoordinates(true);

    mpNoiseSampler = Sampler::create(pDevice, samplerDesc);

    if(mAsync) {
        generateNoiseDataAsync();
    } else {
        generateNoiseData();
        uploadNoiseData();
    }
}

void STBNGenerator::generateNoiseDataAsync() {
    mGenerateNoiseDataTask = ThreadPool::instance().submit([this]{
        this->generateNoiseData();
    });
}

void STBNGenerator::generateNoiseData() {
    uint32_t data_size = mDims[0] * mDims[1] * mDims[2] * getFormatChannelCount(mFormat);
    mNoiseData.resize(data_size);

#if 1 == 2
    // test rgba noise
    rndEngine.seed(0);

    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    for (uint32_t i = 0; i < data_size; ++i) {
        *(reinterpret_cast<float *>(mNoiseData.data() + i)) = rndDist(rndEngine);
    }
#else
    static const float c_energySigma = 1.9f;
    STBN::Maker maker(mDims[0], mDims[1], mDims[2], getFormatChannelCount(mFormat), c_energySigma, c_energySigma, c_energySigma, c_energySigma);
    maker.make(mNoiseData);
#endif

    mDirty = true;
}

void STBNGenerator::uploadNoiseData() const {
    if(!mDirty) return;

    mpDevice->getRenderContext()->updateTextureData(mpNoiseTexture.get(), mNoiseData.data());
    mDirty = false;
}

Shader::DefineList STBNGenerator::getDefines() const {
    Shader::DefineList defines;

    return defines;
}

 bool STBNGenerator::setShaderData(ShaderVar const& var) const {
    if(mAsync && mDirty) {
        mGenerateNoiseDataTask.get();
        uploadNoiseData();
    }

    var["noiseTexture"] = mpNoiseTexture;
    var["noiseSampler"] = mpNoiseSampler;

    var["dims"] = mDims;
 }

}  // namespace Falcor
