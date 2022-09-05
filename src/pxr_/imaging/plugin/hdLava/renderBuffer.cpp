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

#include "renderBuffer.h"
#include "renderParam.h"
#include "lavaApi.h"

#include "pxr/imaging/hd/sceneDelegate.h"

#include "Falcor/Utils/Debug/debug.h"

PXR_NAMESPACE_OPEN_SCOPE

HdLavaRenderBuffer::HdLavaRenderBuffer(SdfPath const& id)
    : HdRenderBuffer(id)
    , m_numMappers(0)
    , m_isConverged(false) {

}

void HdLavaRenderBuffer::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) {
    if (*dirtyBits & DirtyDescription) {
        // hdRpr has the background thread write directly into render buffers,
        // so we need to stop the render thread before reallocating them.
        static_cast<HdLavaRenderParam*>(renderParam)->AcquireLavaApiForEdit();
    }

    HdRenderBuffer::Sync(sceneDelegate, renderParam, dirtyBits);
}

void HdLavaRenderBuffer::Finalize(HdRenderParam* renderParam) {
    // hdRpr has the background thread write directly into render buffers,
    // so we need to stop the render thread before reallocating them.
    static_cast<HdLavaRenderParam*>(renderParam)->AcquireLavaApiForEdit();

    HdRenderBuffer::Finalize(renderParam);
}

bool HdLavaRenderBuffer::Allocate(GfVec3i const& dimensions, HdFormat format, bool multiSampled) {
    LOG_DBG("HdLavaRenderBuffer::Allocate");
    TF_UNUSED(multiSampled);

    if (dimensions[2] != 1) {
        TF_WARN("HdLavaRenderBuffer supports 2D buffers only");
        return false;
    }

    _Deallocate();

    m_width = dimensions[0];
    m_height = dimensions[1];
    LOG_DBG("HdLavaRenderBuffer::Allocate %u %u", m_width, m_height);
    m_format = format;
    size_t dataByteSize = m_width * m_height * HdDataSizeOfFormat(m_format);
    m_mappedBuffer.resize(dataByteSize, 128);

    return false;
}

void HdLavaRenderBuffer::_Deallocate() {
    m_width = 0u;
    m_height = 0u;
    m_format = HdFormatInvalid;
    m_isConverged.store(false);
    m_numMappers.store(0);
    m_mappedBuffer.resize(0);
}

void* HdLavaRenderBuffer::Map() {
    if (!m_isValid) return nullptr;

    ++m_numMappers;
    return m_mappedBuffer.data();
}

void HdLavaRenderBuffer::Unmap() {
    if (!m_isValid) return;

    // XXX We could consider clearing _mappedBuffer here to free RAM.
    //     For now we assume that Map() will be called frequently so we prefer
    //     to avoid the cost of clearing the buffer over memory savings.
    // m_mappedBuffer.clear();
    // m_mappedBuffer.shrink_to_fit();
    --m_numMappers;
}

bool HdLavaRenderBuffer::IsMapped() const {
    return m_numMappers.load() != 0;
}

void HdLavaRenderBuffer::Resolve() {
    // no-op
}

bool HdLavaRenderBuffer::IsConverged() const {
    return m_isConverged.load();
}

void HdLavaRenderBuffer::SetConverged(bool converged) {
    return m_isConverged.store(converged);
}

void HdLavaRenderBuffer::SetStatus(bool isValid) {
    m_isValid = isValid;
}

PXR_NAMESPACE_CLOSE_SCOPE
