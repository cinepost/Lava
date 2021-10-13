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
#include "Falcor/stdafx.h"
#include "Volume.h"

//#include <filesystem>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;


namespace Falcor {

namespace {
    // Constants.
    const float kMaxAnisotropy = 0.99f;
    const double kMinFrameRate = 1.0;
    const double kMaxFrameRate = 1000.0;
}

static_assert(sizeof(VolumeData) % 16 == 0, "Volume::VolumeData size should be a multiple of 16");

Volume::Volume(const std::string& name) : mName(name) {
    mData.transform = glm::identity<glm::mat4>();
    mData.invTransform = glm::identity<glm::mat4>();
}

Volume::SharedPtr Volume::create(const std::string& name) {
    Volume* pVolume = new Volume(name);
    return SharedPtr(pVolume);
}

bool Volume::loadGrid(Device::SharedPtr pDevice, GridSlot slot, const std::string& filename, const std::string& gridname) {
    auto grid = Grid::createFromFile(pDevice, filename, gridname);
    if (grid) setGrid(slot, grid);
    return grid != nullptr;
}

uint32_t Volume::loadGridSequence(Device::SharedPtr pDevice, GridSlot slot, const std::vector<std::string>& filenames, const std::string& gridname, bool keepEmpty) {
    GridSequence grids;
    for (const auto& filename : filenames) {
        auto grid = Grid::createFromFile(pDevice, filename, gridname);
        if (keepEmpty || grid) grids.push_back(grid);
    }
    setGridSequence(slot, grids);
    return (uint32_t)grids.size();
}

uint32_t Volume::loadGridSequence(Device::SharedPtr pDevice, GridSlot slot, const std::string& path, const std::string& gridname, bool keepEmpty) {
    std::string fullpath;
    if (!findFileInDataDirectories(path, fullpath)) {
        logWarning("Cannot find directory '" + path + "'");
        return 0;
    }

    if (!fs::is_directory(fullpath)) {
        logWarning("'" + path + "' is not a directory");
        return 0;
    }

    // Enumerate grid files.
    std::vector<std::string> files;
    for (auto p : fs::directory_iterator(fullpath)) {
        if (p.path().extension() == ".nvdb" || p.path().extension() == ".vdb") files.push_back(p.path().string());
    }

    // Sort by length first, then alpha-numerically.
    auto cmp = [](const std::string& a, const std::string& b) { return a.length() != b.length() ? a.length() < b.length() : a < b; };
    std::sort(files.begin(), files.end(), cmp);

    return loadGridSequence(pDevice, slot, files, gridname, keepEmpty);
}

void Volume::setGridSequence(GridSlot slot, const GridSequence& grids) {
    uint32_t slotIndex = (uint32_t)slot;
    assert(slotIndex >= 0 && slotIndex < (uint32_t)GridSlot::Count);

    if (mGrids[slotIndex] != grids) {
        mGrids[slotIndex] = grids;
        updateSequence();
        updateBounds();
        markUpdates(UpdateFlags::GridsChanged);
    }
}

const Volume::GridSequence& Volume::getGridSequence(GridSlot slot) const {
    uint32_t slotIndex = (uint32_t)slot;
    assert(slotIndex >= 0 && slotIndex < (uint32_t)GridSlot::Count);

    return mGrids[slotIndex];
}

void Volume::setGrid(GridSlot slot, const Grid::SharedPtr& grid) {
    setGridSequence(slot, grid ? GridSequence{grid} : GridSequence{});
}

const Grid::SharedPtr& Volume::getGrid(GridSlot slot) const {
    static const Grid::SharedPtr kNullGrid;

    uint32_t slotIndex = (uint32_t)slot;
    assert(slotIndex >= 0 && slotIndex < (uint32_t)GridSlot::Count);

    const auto& gridSequence = mGrids[slotIndex];
    uint32_t gridIndex = std::min(mGridFrame, (uint32_t)gridSequence.size() - 1);
    return gridSequence.empty() ? kNullGrid : gridSequence[gridIndex];
}

std::vector<Grid::SharedPtr> Volume::getAllGrids() const {
    std::set<Grid::SharedPtr> uniqueGrids;
    for (const auto& grids : mGrids) {
        std::copy_if(grids.begin(), grids.end(), std::inserter(uniqueGrids, uniqueGrids.begin()), [] (const auto& grid) { return grid != nullptr; });
    }
    return std::vector<Grid::SharedPtr>(uniqueGrids.begin(), uniqueGrids.end());
}

void Volume::setGridFrame(uint32_t gridFrame) {
    if (mGridFrame != gridFrame) {
        mGridFrame = gridFrame;
        markUpdates(UpdateFlags::GridsChanged);
        updateBounds();
    }
}

void Volume::setFrameRate(double frameRate) {
    mFrameRate = clamp(frameRate, kMinFrameRate, kMaxFrameRate);
}

void Volume::setPlaybackEnabled(bool enabled) {
    mPlaybackEnabled = enabled;
}

void Volume::updatePlayback(double currentTime) {
    uint32_t frameCount = getGridFrameCount();
    if (mPlaybackEnabled && frameCount > 0) {
        uint32_t frameIndex = (uint32_t)std::floor(std::max(0.0, currentTime) * mFrameRate) % frameCount;
        setGridFrame(frameIndex);
    }
}

void Volume::setDensityScale(float densityScale) {
    if (mData.densityScale != densityScale) {
        mData.densityScale = densityScale;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void Volume::setEmissionScale(float emissionScale) {
    if (mData.emissionScale != emissionScale) {
        mData.emissionScale = emissionScale;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void Volume::setAlbedo(const float3& albedo) {
    auto clampedAlbedo = clamp(albedo, float3(0.f), float3(1.f));
    if (mData.albedo != clampedAlbedo) {
        mData.albedo = clampedAlbedo;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void Volume::setAnisotropy(float anisotropy) {
    auto clampedAnisotropy = clamp(anisotropy, -kMaxAnisotropy, kMaxAnisotropy);
    if (mData.anisotropy != clampedAnisotropy) {
        mData.anisotropy = clampedAnisotropy;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void Volume::setEmissionMode(EmissionMode emissionMode) {
    if (mData.flags != (uint32_t)emissionMode) {
        mData.flags = (uint32_t)emissionMode;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

Volume::EmissionMode Volume::getEmissionMode() const {
    return (EmissionMode)mData.flags;
}

void Volume::setEmissionTemperature(float emissionTemperature) {
    if (mData.emissionTemperature != emissionTemperature) {
        mData.emissionTemperature = emissionTemperature;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void Volume::updateFromAnimation(const glm::mat4& transform) {
    if (mData.transform != transform) {
        mData.transform = transform;
        mData.invTransform = glm::inverse(transform);
        markUpdates(UpdateFlags::TransformChanged);
        updateBounds();
    }
}

void Volume::updateSequence() {
    mGridFrameCount = 1;
    for (const auto& grids : mGrids) mGridFrameCount = std::max(mGridFrameCount, (uint32_t)grids.size());
    setGridFrame(std::min(mGridFrame, mGridFrameCount - 1));
}

void Volume::updateBounds() {
    AABB bounds;
    for (uint32_t slotIndex = 0; slotIndex < (uint32_t)GridSlot::Count; ++slotIndex) {
        const auto& grid = getGrid((GridSlot)slotIndex);
        if (grid && grid->getVoxelCount() > 0) bounds.include(grid->getWorldBounds());
    }
    bounds = bounds.transform(mData.transform);

    if (mBounds != bounds) {
        mBounds = bounds;
        mData.boundsMin = mBounds.minPoint;
        mData.boundsMax = mBounds.maxPoint;
        markUpdates(UpdateFlags::BoundsChanged);
    }
}

void Volume::markUpdates(UpdateFlags updates) {
    mUpdates |= updates;
}

void Volume::setFlags(uint32_t flags) {
    if (mData.flags != flags) {
        mData.flags = flags;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

#ifdef SCRIPTING
SCRIPT_BINDING(Volume) {
    pybind11::class_<Volume, Animatable, Volume::SharedPtr> volume(m, "Volume");
    volume.def_property("name", &Volume::getName, &Volume::setName);
    volume.def_property("gridFrame", &Volume::getGridFrame, &Volume::setGridFrame);
    volume.def_property_readonly("gridFrameCount", &Volume::getGridFrameCount);
    volume.def_property("densityGrid", &Volume::getDensityGrid, &Volume::setDensityGrid);
    volume.def_property("densityScale", &Volume::getDensityScale, &Volume::setDensityScale);
    volume.def_property("emissionGrid", &Volume::getEmissionGrid, &Volume::setEmissionGrid);
    volume.def_property("emissionScale", &Volume::getEmissionScale, &Volume::setEmissionScale);
    volume.def_property("albedo", &Volume::getAlbedo, &Volume::setAlbedo);
    volume.def_property("anisotropy", &Volume::getAnisotropy, &Volume::setAnisotropy);
    volume.def_property("emissionMode", &Volume::getEmissionMode, &Volume::setEmissionMode);
    volume.def_property("emissionTemperature", &Volume::getEmissionTemperature, &Volume::setEmissionTemperature);
    volume.def(pybind11::init(&Volume::create), "name"_a);
    volume.def("loadGrid", &Volume::loadGrid, "slot"_a, "filename"_a, "gridname"_a);
    volume.def("loadGridSequence",
        pybind11::overload_cast<Volume::GridSlot, const std::vector<std::string>&, const std::string&, bool>(&Volume::loadGridSequence),
        "slot"_a, "filenames"_a, "gridname"_a, "keepEmpty"_a = true);
    volume.def("loadGridSequence",
        pybind11::overload_cast<Volume::GridSlot, const std::string&, const std::string&, bool>(&Volume::loadGridSequence),
        "slot"_a, "path"_a, "gridnames"_a, "keepEmpty"_a = true);
    pybind11::enum_<Volume::GridSlot> gridSlot(volume, "GridSlot");
    gridSlot.value("Density", Volume::GridSlot::Density);
    gridSlot.value("Emission", Volume::GridSlot::Emission);

    pybind11::enum_<Volume::EmissionMode> emissionMode(volume, "EmissionMode");
    emissionMode.value("Direct", Volume::EmissionMode::Direct);
    emissionMode.value("Blackbody", Volume::EmissionMode::Blackbody);
}
#endif

}  // namespace Falcor