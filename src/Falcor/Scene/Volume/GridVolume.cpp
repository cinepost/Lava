/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
#include "GridVolume.h"
#include "Grid.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"
#include "GlobalState.h"

#include <set>

namespace Falcor {

namespace {
    // Constants.
    const float kMaxAnisotropy = 0.99f;
    const double kMinFrameRate = 1.0;
    const double kMaxFrameRate = 1000.0;
}

static_assert(sizeof(GridVolumeData) % 16 == 0, "GridVolumeData size should be a multiple of 16");

GridVolume::GridVolume(ref<Device> pDevice, const std::string& name): mpDevice(pDevice), mName(name) {
    mData.transform = float4x4::identity();
    mData.invTransform = float4x4::identity();
}

bool GridVolume::loadGrid(GridSlot slot, const fs::path& path, const std::string& gridname) {
    auto grid = Grid::createFromFile(mpDevice, path, gridname);
    if (grid) setGrid(slot, grid);
    return grid != nullptr;
}

GridVolume::GridSequence GridVolume::createGridSequence(ref<Device> pDevice, const std::vector<fs::path>& paths, const std::string& gridname, bool keepEmpty) {
    GridSequence grids;
    for (const auto& path : paths) {
        auto grid = Grid::createFromFile(pDevice, path, gridname);
        if (keepEmpty || grid) grids.push_back(grid);
    }

    return grids;
}

uint32_t GridVolume::loadGridSequence(GridSlot slot, const std::vector<fs::path>& paths, const std::string& gridname, bool keepEmpty) {
    GridVolume::GridSequence grids = GridVolume::createGridSequence(mpDevice, paths, gridname, keepEmpty);
    setGridSequence(slot, grids);
    return (uint32_t)grids.size();
}

uint32_t GridVolume::loadGridSequence(GridSlot slot, const fs::path& path, const std::string& gridname, bool keepEmpty) {
    if (!fs::exists(path)) {
        logWarning("'{}' does not exist.", path);
        return 0;
    }

    if (!fs::is_directory(path)) {
        logWarning("'{}' is not a directory.", path);
        return 0;
    }

    // Enumerate grid files.
    std::vector<fs::path> paths;
    for (auto it : fs::directory_iterator(path)) {
        if (hasExtension(it.path(), "nvdb") || hasExtension(it.path(), "vdb")) paths.push_back(it.path());
    }

    // Sort by length first, then alpha-numerically.
    auto cmp = [](const fs::path& a, const fs::path& b) {
        auto sa = a.string();
        auto sb = b.string();
        return sa.length() != sb.length() ? sa.length() < sb.length() : sa < sb;
    };
    std::sort(paths.begin(), paths.end(), cmp);

    return loadGridSequence(slot, paths, gridname, keepEmpty);
}

void GridVolume::setGridSequence(GridSlot slot, const GridSequence& grids) {
    uint32_t slotIndex = (uint32_t)slot;
    FALCOR_ASSERT(slotIndex >= 0 && slotIndex < (uint32_t)GridSlot::Count);

    if (mGrids[slotIndex] != grids) {
        mGrids[slotIndex] = grids;
        updateSequence();
        updateBounds();
        markUpdates(UpdateFlags::GridsChanged);
    }
}

const GridVolume::GridSequence& GridVolume::getGridSequence(GridSlot slot) const {
    uint32_t slotIndex = (uint32_t)slot;
    FALCOR_ASSERT(slotIndex >= 0 && slotIndex < (uint32_t)GridSlot::Count);

    return mGrids[slotIndex];
}

void GridVolume::setGrid(GridSlot slot, const ref<Grid>& grid) {
    setGridSequence(slot, grid ? GridSequence{grid} : GridSequence{});
}

const ref<Grid>& GridVolume::getGrid(GridSlot slot) const {
    static const ref<Grid> kNullGrid;

    uint32_t slotIndex = (uint32_t)slot;
    FALCOR_ASSERT(slotIndex >= 0 && slotIndex < (uint32_t)GridSlot::Count);

    const auto& gridSequence = mGrids[slotIndex];
    uint32_t gridIndex = std::min(mGridFrame, (uint32_t)gridSequence.size() - 1);
    return gridSequence.empty() ? kNullGrid : gridSequence[gridIndex];
}

std::vector<ref<Grid>> GridVolume::getAllGrids() const {
    std::set<ref<Grid>> uniqueGrids;
    for (const auto& grids : mGrids) {
        std::copy_if(grids.begin(), grids.end(), std::inserter(uniqueGrids, uniqueGrids.begin()), [] (const auto& grid) { return grid != nullptr; });
    }
    return std::vector<ref<Grid>>(uniqueGrids.begin(), uniqueGrids.end());
}

void GridVolume::setGridFrame(uint32_t gridFrame) {
    if (mGridFrame != gridFrame) {
        mGridFrame = gridFrame;
        markUpdates(UpdateFlags::GridsChanged);
        updateBounds();
    }
}

void GridVolume::setFrameRate(double frameRate) {
    mFrameRate = math::clamp(frameRate, kMinFrameRate, kMaxFrameRate);
}

void GridVolume::setPlaybackEnabled(bool enabled) {
    mPlaybackEnabled = enabled;
}

void GridVolume::updatePlayback(double currentTime) {
    if (mPlaybackEnabled && mGridFrameCount > 0) {
        uint32_t frameIndex = (mStartFrame + (uint32_t)std::floor(std::max(0.0, currentTime) * mFrameRate)) % mGridFrameCount;
        setGridFrame(frameIndex);
    }
}

void GridVolume::setDensityScale(float densityScale) {
    if (mData.densityScale != densityScale) {
        mData.densityScale = densityScale;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void GridVolume::setEmissionScale(float emissionScale) {
    if (mData.emissionScale != emissionScale) {
        mData.emissionScale = emissionScale;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void GridVolume::setAlbedo(const float3& albedo) {
    auto clampedAlbedo = clamp(albedo, float3(0.f), float3(1.f));
    if (any(mData.albedo != clampedAlbedo)) {
        mData.albedo = clampedAlbedo;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void GridVolume::setAnisotropy(float anisotropy) {
    auto clampedAnisotropy = math::clamp(anisotropy, -kMaxAnisotropy, kMaxAnisotropy);
    if (mData.anisotropy != clampedAnisotropy) {
        mData.anisotropy = clampedAnisotropy;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void GridVolume::setEmissionMode(EmissionMode emissionMode) {
    if (mData.flags != (uint32_t)emissionMode) {
        mData.flags = (uint32_t)emissionMode;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

GridVolume::EmissionMode GridVolume::getEmissionMode() const {
    return (EmissionMode)mData.flags;
}

void GridVolume::setEmissionTemperature(float emissionTemperature) {
    if (mData.emissionTemperature != emissionTemperature) {
        mData.emissionTemperature = emissionTemperature;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}

void GridVolume::updateFromAnimation(const float4x4& transform) {
    if (mData.transform != transform) {
        mData.transform = transform;
        mData.invTransform = inverse(transform);
        markUpdates(UpdateFlags::TransformChanged);
        updateBounds();
    }
}

void GridVolume::updateSequence() {
    mGridFrameCount = 1;
    for (const auto& grids : mGrids) mGridFrameCount = std::max(mGridFrameCount, (uint32_t)grids.size());
    setGridFrame(std::min(mGridFrame, mGridFrameCount - 1));
}

void GridVolume::updateBounds() {
    AABB bounds;
    for (uint32_t slotIndex = 0; slotIndex < (uint32_t)GridSlot::Count; ++slotIndex) {
        const auto& grid = getGrid((GridSlot)slotIndex);
        if (grid && grid->getVoxelCount() > 0) bounds.include(grid->getWorldBounds());
    }
    bounds = bounds.transform(mData.transform);

    if (mBounds != bounds) {
        mBounds = bounds;
        mData.boundsMin = mBounds.valid() ? mBounds.minPoint : float3(0.f);
        mData.boundsMax = mBounds.valid() ? mBounds.maxPoint : float3(0.f);
        markUpdates(UpdateFlags::BoundsChanged);
    }
}

void GridVolume::markUpdates(UpdateFlags updates) {
    mUpdates |= updates;
}

void GridVolume::setFlags(uint32_t flags) {
    if (mData.flags != flags) {
        mData.flags = flags;
        markUpdates(UpdateFlags::PropertiesChanged);
    }
}


#ifdef SCRIPTING
    SCRIPT_BINDING(GridVolume) {
        using namespace pybind11::literals;

        SCRIPT_BINDING_DEPENDENCY(Animatable)
        SCRIPT_BINDING_DEPENDENCY(Grid)

        pybind11::class_<GridVolume, Animatable, ref<GridVolume>> volume(m, "GridVolume");

        pybind11::enum_<GridVolume::GridSlot> gridSlot(volume, "GridSlot");
        gridSlot.value("Density", GridVolume::GridSlot::Density);
        gridSlot.value("Emission", GridVolume::GridSlot::Emission);

        pybind11::enum_<GridVolume::EmissionMode> emissionMode(volume, "EmissionMode");
        emissionMode.value("Direct", GridVolume::EmissionMode::Direct);
        emissionMode.value("Blackbody", GridVolume::EmissionMode::Blackbody);

        volume.def_property("name", &GridVolume::getName, &GridVolume::setName);
        volume.def_property("gridFrame", &GridVolume::getGridFrame, &GridVolume::setGridFrame);
        volume.def_property_readonly("gridFrameCount", &GridVolume::getGridFrameCount);
        volume.def_property("frameRate", &GridVolume::getFrameRate, &GridVolume::setFrameRate);
        volume.def_property("startFrame", &GridVolume::getStartFrame, &GridVolume::setStartFrame);
        volume.def_property("playbackEnabled", &GridVolume::isPlaybackEnabled, &GridVolume::setPlaybackEnabled);
        volume.def_property("densityGrid", &GridVolume::getDensityGrid, &GridVolume::setDensityGrid);
        volume.def_property("densityScale", &GridVolume::getDensityScale, &GridVolume::setDensityScale);
        volume.def_property("emissionGrid", &GridVolume::getEmissionGrid, &GridVolume::setEmissionGrid);
        volume.def_property("emissionScale", &GridVolume::getEmissionScale, &GridVolume::setEmissionScale);
        volume.def_property("albedo", &GridVolume::getAlbedo, &GridVolume::setAlbedo);
        volume.def_property("anisotropy", &GridVolume::getAnisotropy, &GridVolume::setAnisotropy);
        volume.def_property("emissionMode", &GridVolume::getEmissionMode, &GridVolume::setEmissionMode);
        volume.def_property("emissionTemperature", &GridVolume::getEmissionTemperature, &GridVolume::setEmissionTemperature);
        auto create = [] (const std::string& name)
        {
            return GridVolume::create(accessActivePythonSceneBuilder().getDevice(), name);
        };
        volume.def(pybind11::init(create), "name"_a); // PYTHONDEPRECATED
        volume.def("loadGrid",
            [](GridVolume& self, GridVolume::GridSlot slot, const fs::path& path, const std::string& gridname)
            { return self.loadGrid(slot, getActiveAssetResolver().resolvePath(path), gridname); },
            "slot"_a, "path"_a, "gridname"_a
        ); // PYTHONDEPRECATED
        volume.def("loadGridSequence",
            [](GridVolume& self, GridVolume::GridSlot slot, const std::vector<fs::path>& paths, const std::string& gridname, bool keepEmpty)
            {
                std::vector<fs::path> resolvedPaths;
                for (const auto& path : paths)
                    resolvedPaths.push_back(getActiveAssetResolver().resolvePath(path));
                return self.loadGridSequence(slot, resolvedPaths, gridname, keepEmpty);
            },
            "slot"_a, "paths"_a, "gridname"_a, "keepEmpty"_a = true
        ); // PYTHONDEPRECATED
        volume.def("loadGridSequence",
            [](GridVolume& self, GridVolume::GridSlot slot, const fs::path& path, const std::string& gridname, bool keepEmpty)
            { return self.loadGridSequence(slot, getActiveAssetResolver().resolvePath(path), gridname, keepEmpty); },
            "slot"_a, "path"_a, "gridnames"_a, "keepEmpty"_a = true
        ); // PYTHONDEPRECATED

        m.attr("Volume") = m.attr("GridVolume"); // PYTHONDEPRECATED
    }
#endif  // SCRIPTING

}  // namespace Falcor
