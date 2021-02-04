/************************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

#ifndef HDLAVA_LAVA_API_AOV_H_
#define HDLAVA_LAVA_API_AOV_H_

#include "aovDescriptor.h"
#include "lavaApiFramebuffer.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/imaging/hd/types.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdLavaApi;
struct RprUsdContextMetadata;

class HdLavaApiAov {
public:
    HdLavaApiAov(rpr_aov rprAovType, int width, int height, HdFormat format,
                rpr::Context* rprContext, RprUsdContextMetadata const& rprContextMetadata, std::unique_ptr<rif::Filter> filter);
    HdLavaApiAov(rpr_aov rprAovType, int width, int height, HdFormat format,
                rpr::Context* rprContext, RprUsdContextMetadata const& rprContextMetadata, rif::Context* rifContext);
    virtual ~HdLavaApiAov() = default;

    virtual void Resize(int width, int height, HdFormat format);
    virtual void Update(HdLavaApi const* rprApi, rif::Context* rifContext);
    virtual void Resolve();

    virtual bool GetData(void* dstBuffer, size_t dstBufferSize);
    void Clear();

    HdFormat GetFormat() const { return m_format; }
    HdLavaAovDescriptor const& GetDesc() const { return m_aovDescriptor; }

    HdLavaApiFramebuffer* GetAovFb() { return m_aov.get(); };
    HdLavaApiFramebuffer* GetResolvedFb();

protected:
    HdLavaApiAov(HdLavaAovDescriptor const& aovDescriptor, HdFormat format)
        : m_aovDescriptor(aovDescriptor), m_format(format) {};

    virtual void OnFormatChange(rif::Context* rifContext);
    virtual void OnSizeChange(rif::Context* rifContext);

protected:
    HdLavaAovDescriptor const& m_aovDescriptor;
    HdFormat m_format;

    std::unique_ptr<HdLavaApiFramebuffer> m_aov;
    std::unique_ptr<HdLavaApiFramebuffer> m_resolved;
    std::unique_ptr<rif::Filter> m_filter;

    enum ChangeTracker {
        Clean = 0,
        AllDirty = ~0u,
        DirtySize = 1 << 0,
        DirtyFormat = 1 << 1,
    };
    uint32_t m_dirtyBits = AllDirty;

private:
    bool GetDataImpl(void* dstBuffer, size_t dstBufferSize);
};

class HdLavaApiColorAov : public HdLavaApiAov {
public:
    HdLavaApiColorAov(HdFormat format, std::shared_ptr<HdLavaApiAov> rawColorAov, rpr::Context* rprContext, RprUsdContextMetadata const& rprContextMetadata);
    ~HdLavaApiColorAov() override = default;

    void Resize(int width, int height, HdFormat format) override;
    void Update(HdLavaApi const* rprApi, rif::Context* rifContext) override;
    bool GetData(void* dstBuffer, size_t dstBufferSize) override;
    void Resolve() override;

    void SetOpacityAov(std::shared_ptr<HdLavaApiAov> opacity);

    void EnableAIDenoise(std::shared_ptr<HdLavaApiAov> albedo,
                         std::shared_ptr<HdLavaApiAov> normal,
                         std::shared_ptr<HdLavaApiAov> linearDepth);
    void EnableEAWDenoise(std::shared_ptr<HdLavaApiAov> albedo,
                          std::shared_ptr<HdLavaApiAov> normal,
                          std::shared_ptr<HdLavaApiAov> linearDepth,
                          std::shared_ptr<HdLavaApiAov> objectId,
                          std::shared_ptr<HdLavaApiAov> worldCoordinate);
    void DisableDenoise(rif::Context* rifContext);

    struct TonemapParams {
        bool enable;
        float exposure;
        float sensitivity;
        float fstop;
        float gamma;

        bool operator==(TonemapParams const& lhs) {
            return exposure == lhs.exposure &&
                sensitivity == lhs.sensitivity &&
                fstop == lhs.fstop &&
                gamma == lhs.gamma;
        }
        bool operator!=(TonemapParams const& lhs) {
            return !(*this == lhs);
        }
    };
    void SetTonemap(TonemapParams const& params);

protected:
    void OnFormatChange(rif::Context* rifContext) override;
    void OnSizeChange(rif::Context* rifContext) override;

private:
    enum Filter {
        kFilterNone = 0,
        kFilterResample = 1 << 0,
        kFilterAIDenoise = 1 << 1,
        kFilterEAWDenoise = 1 << 2,
        kFilterComposeOpacity = 1 << 3,
        kFilterTonemap = 1 << 4,
    };
    void SetFilter(Filter filter, bool enable);
    
    template <typename T>
    void ResizeFilter(int width, int height, Filter filterType, rif::Filter* filter, T input);

    void SetTonemapFilterParams(rif::Filter* filter);

    bool CanComposeAlpha();

private:
    std::shared_ptr<HdLavaApiAov> m_retainedRawColor;
    std::shared_ptr<HdLavaApiAov> m_retainedOpacity;

    Filter m_mainFilterType = kFilterNone;
    std::vector<std::pair<Filter, std::unique_ptr<rif::Filter>>> m_auxFilters;

    uint32_t m_enabledFilters = kFilterNone;
    bool m_isEnabledFiltersDirty = true;

    TonemapParams m_tonemap;

    int m_width = 0;
    int m_height = 0;
};

class HdLavaApiNormalAov : public HdLavaApiAov {
public:
    HdLavaApiNormalAov(int width, int height, HdFormat format,
                      rpr::Context* rprContext, RprUsdContextMetadata const& rprContextMetadata, rif::Context* rifContext);
    ~HdLavaApiNormalAov() override = default;
protected:
    void OnFormatChange(rif::Context* rifContext) override;
    void OnSizeChange(rif::Context* rifContext) override;
};

class HdLavaApiDepthAov : public HdLavaApiAov {
public:
    HdLavaApiDepthAov(HdFormat format,
                     std::shared_ptr<HdLavaApiAov> worldCoordinateAov,
                     rpr::Context* rprContext, RprUsdContextMetadata const& rprContextMetadata, rif::Context* rifContext);
    ~HdLavaApiDepthAov() override = default;

    void Update(HdLavaApi const* rprApi, rif::Context* rifContext) override;
    void Resize(int width, int height, HdFormat format) override;

private:
    std::unique_ptr<rif::Filter> m_retainedFilter;

    rif::Filter* m_ndcFilter;
    rif::Filter* m_remapFilter;

    std::shared_ptr<HdLavaApiAov> m_retainedWorldCoordinateAov;
    int m_width;
    int m_height;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDLAVA_LAVA_API_AOV_H_
