/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

//#include "../Shared/shared.h"

#include <random>
#include <unordered_set>

#include "pcg_basic.h"
#include "STBN.h"
#include "lava_utils_lib/logging.h"


// If true, will use the same random numbers each run
#define DETERMINISTIC() true

// Settings for the void and cluster algorithm
static const float kInitialBinaryPatternDensity = 0.1f;
static const float kEenergySigma = 1.9f;
static const float kEnergyLoss = 0.005f; // 0.5% lost, so 99.5% accounted for

static inline pcg32_random_t getRNG() {
    pcg32_random_t rng;
#if DETERMINISTIC()
    pcg32_srandom_r(&rng, 0x1337FEED, 0);
#else
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<uint32_t> dist;
    pcg32_srandom_r(&rng, dist(generator), 0);
#endif
    return rng;
}

template <typename T>
static inline T clamp(const T& value, const T& minimum, const T& maximum) {
  if (value <= minimum)
    return minimum;
  else if (value >= maximum)
    return maximum;
  else
    return value;
}

namespace STBN {

Maker::Maker(uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ, uint8_t channelCount, float sigmaX, float sigmaY, float sigmaZ, float sigmaW) {
	mSizeX = sizeX;
	mSizeY = sizeY;
	mSizeZ = sizeZ;
	mChannelCount = channelCount;

	mPixelsCountX = channelCount * mSizeX;
	mPixelsCountXY = mPixelsCountX * mSizeY;

	mDimsionality = 0;
  if(mSizeX > 1) mDimsionality++;
  if(mSizeY > 1) mDimsionality++;
  if(mSizeZ > 1) mDimsionality++;
  if(mChannelCount > 1) mDimsionality++;

	mSigmaX = sigmaX; 
	mSigmaY = sigmaY; 
	mSigmaZ = sigmaZ; 
	mSigmaW = sigmaW;

	mNumPixels = mSizeX * mSizeY * mSizeZ * mChannelCount;

	/*
	// test
	const PixelCoords pixelCoords = {5,6,7,3};
	size_t pixelIndex = pixelCoordsToPixelIndex(pixelCoords);
	PixelCoords calcCoords = pixelIndexToPixelCoords(pixelIndex);
	LLOG_WRN << "PixelCoords: " << pixelCoords[0] << " " << pixelCoords[1] << " " << pixelCoords[2] << " " << pixelCoords[3];
	LLOG_WRN << "Pixel index: " << pixelIndex;
	LLOG_WRN << "PixelCoords: " << calcCoords[0] << " " << calcCoords[1] << " " << calcCoords[2] << " " << calcCoords[3];
	*/

	mKernelRadiusStartX = kernelRadius<false>(mSigmaX, mSizeX);
	mKernelRadiusEndX = kernelRadius<true>(mSigmaX, mSizeX);
	const int kernelRadiusMaxX = std::max(-mKernelRadiusStartX, mKernelRadiusEndX);

	mKernelRadiusStartY = kernelRadius<false>(sigmaY, mSizeY);
	mKernelRadiusEndY = kernelRadius<true>(mSigmaY, mSizeY);
	const int kernelRadiusMaxY = std::max(-mKernelRadiusStartY, mKernelRadiusEndY);
	
	mKernelRadiusStartZ = kernelRadius<false>(sigmaZ, mSizeZ);
	mKernelRadiusEndZ = kernelRadius<true>(mSigmaZ, mSizeZ);
	const int kernelRadiusMaxZ = std::max(-mKernelRadiusStartZ, mKernelRadiusEndZ);

	mKernelRadiusStartW = kernelRadius<false>(sigmaW, mChannelCount);
	mKernelRadiusEndW = kernelRadius<true>(mSigmaW, mChannelCount);
	const int kernelRadiusMaxW = std::max(-mKernelRadiusStartW, mKernelRadiusEndW);

	// Init buffers
	mKernelX.resize(kernelRadiusMaxX + 1);
	mKernelY.resize(kernelRadiusMaxY + 1);
	mKernelZ.resize(kernelRadiusMaxZ + 1);
	mKernelW.resize(kernelRadiusMaxW + 1);

	mEnergy.resize(mNumPixels, 0.0f);
	mPixelOn.resize(mNumPixels, false);
	mPixelRank.resize(mNumPixels, mNumPixels);

	// calculate the kernels
	{
		mKernelX[0] = 1.0f;
		float sum = 0.0f;
		for (size_t index = 1; index <= kernelRadiusMaxX; ++index) {
			float x = float(index);
			mKernelX[index] = exp(-(x * x) / (2.0f * sigmaX * sigmaX));
			sum += mKernelX[index];
		}

		for (float& f : mKernelX) f /= sum;
	}

	{
		mKernelY[0] = 1.0f;
		float sum = 0.0f;
		for (size_t index = 1; index <= kernelRadiusMaxY; ++index) {
			float x = float(index);
			mKernelY[index] = exp(-(x * x) / (2.0f * sigmaY * sigmaY));
			sum += mKernelY[index];
		}

		for (float& f : mKernelY) f /= sum;
	}

	{
		mKernelZ[0] = 1.0f;
		float sum = 0.0f;
		for (size_t index = 1; index <= kernelRadiusMaxZ; ++index) {
			float x = float(index);
			mKernelZ[index] = exp(-(x * x) / (2.0f * sigmaZ * sigmaZ));
			sum += mKernelZ[index];
		}
		for (float& f : mKernelZ) f /= sum;
	}

	{
		mKernelW[0] = 1.0f;
		float sum = 0.0f;
		for (size_t index = 1; index <= kernelRadiusMaxW; ++index) {
			float x = float(index);
			mKernelW[index] = exp(-(x * x) / (2.0f * sigmaW * sigmaW));
			sum += mKernelW[index];
		}
		for (float& f : mKernelW) f /= sum;
	}
}

void Maker::make(std::vector<float>& noiseData) {
	// make the texture slices
	LLOG_DBG << "STBN::Maker creating 2Dx1Dx1D blue noise: " << mSizeX << "x" << mSizeY << "x" << mSizeZ << "x" << mChannelCount;
	LLOG_DBG << "STBN::Maker making initial binary pattern.";

	makeInitialBinaryPattern();

	runPhase1();
	runPhase2();
	runPhase3();

	// Init blue noise
	
	//textures.Init({ sizeX, sizeY, sizeZ, sizeW }, { sigmaX, sigmaY, sigmaZ, sigmaW }, { 0, 0, 1, 2 });
	initData({ 0, 0, 1, 2 });
	
	for (size_t pixelIndex = 0; pixelIndex < mNumPixels; ++pixelIndex) {
		Pixel& pixel = mPixels[pixelIndex];
		pixel.energy = 0.0f;
		pixel.on = true;
		pixel.rank = (int)mPixelRank[pixelIndex];
	}

	// Turn ranks into noise values
  noiseData.resize(mPixels.size());
  for (size_t index = 0; index < noiseData.size(); ++index) {
      float* dest = &noiseData[index];
      const Pixel& src = mPixels[index];

      *dest = float(src.rank) / float(noiseData.size());
      //*dest = (float)clamp(rankPercent * 256.0f, 0.0f, 255.0f);
  }

	LLOG_DBG << "STBN::Maker Done.";
}

void Maker::initData(std::vector<int> groups) {
  mPixels.clear();
  mPixels.resize(mNumPixels);

  std::array<uint32_t, 4> size = {mSizeX, mSizeY, mSizeZ, mChannelCount};
 	std::array<float, 4> sigmas = {mSigmaX, mSigmaY, mSigmaZ, mSigmaW};

  LLOG_DBG << "STBN::Maker initializing data for " << mDimsionality << " dimensions.";

  // make the masks
  if (groups.size() == 0)
    groups.resize(mDimsionality, 0);

  std::unordered_set<int> uniqueGroupNumbers;
  for (int g : groups)
    uniqueGroupNumbers.insert(g);
  
  for (int group : uniqueGroupNumbers) {
    unsigned int mask = 0;
    for (int index = 0; index < mDimsionality; ++index) {
      if (groups[index] != group) mask |= (1 << index);
    }
    mMasks.push_back(mask);
  }

  // calculate energy radii so we don't have to consider all pixels for energy changes
  mEnergyEmitMin.resize(mDimsionality);
  mEnergyEmitMax.resize(mDimsionality);
  
  const float logEnergyLoss = log(kEnergyLoss);
  
  for (int i = 0; i < mDimsionality; ++i) {
    int radius = int(sqrtf(-2.0f * sigmas[i] * sigmas[i] * logEnergyLoss));

    if (radius * 2 + 1 >= size[i]) {
        radius = (size[i] - 1) / 2;

        mEnergyEmitMin[i] = -radius;
        mEnergyEmitMax[i] = -radius + size[i] - 1;
    } else {
        mEnergyEmitMin[i] = -radius;
        mEnergyEmitMax[i] = radius;
    }
  }
}

size_t Maker::getTightestCluster() {
	size_t clusterPixelIndex = 0;
	float maxEnergy = -FLT_MAX;

	for (int pixelIndex = 0; pixelIndex < mNumPixels; ++pixelIndex) {
		if (!mPixelOn[pixelIndex]) continue;

		if (mEnergy[pixelIndex] > maxEnergy) {
			maxEnergy = mEnergy[pixelIndex];
			clusterPixelIndex = pixelIndex;
		}
	}

	return clusterPixelIndex;
}

size_t Maker::getLargestVoid() {
	size_t voidPixelIndex = 0;
	float minEnergy = FLT_MAX;

	for (int pixelIndex = 0; pixelIndex < mNumPixels; ++pixelIndex){
		if (mPixelOn[pixelIndex]) continue;

		if (mEnergy[pixelIndex] < minEnergy) {
			minEnergy = mEnergy[pixelIndex];
			voidPixelIndex = pixelIndex;
		}
	}

	return voidPixelIndex;
}

template <bool ON>
void Maker::splatEnergy(size_t pixelIndex) {
	const Maker::PixelCoords pixelCoords = pixelIndexToPixelCoords(pixelIndex);

	// splat the 2D gaussian across XY
	size_t imageBegin = pixelCoordsToPixelIndex({ 0, 0, pixelCoords[2], pixelCoords[3] });
	for (int iy = mKernelRadiusStartY; iy <= mKernelRadiusEndY; ++iy) {
		float kernelY = mKernelY[abs(iy)];
		int pixelY = ((int)pixelCoords[1] + iy + (int)mSizeY) % (int)mSizeY;

		for (int ix = mKernelRadiusStartX; ix < mKernelRadiusEndX; ++ix) {
			float kernelX = mKernelX[abs(ix)];
			int pixelX = (int)(pixelCoords[0] + ix + (int)mSizeX) % (int)mSizeX;

			if (ON) {
				mEnergy[imageBegin + pixelY * mPixelsCountX + pixelX * mChannelCount] += kernelX * kernelY;
			} else {
				mEnergy[imageBegin + pixelY * mPixelsCountX + pixelX * mChannelCount] -= kernelX * kernelY;
			}
		}
	}

	// splat the 1D gaussian along z			
	for (int iz = mKernelRadiusStartZ; iz <= mKernelRadiusEndZ; ++iz) {
		int pixelZ = ((int)pixelCoords[2] + iz + (int)mSizeZ) % (int)mSizeZ;

		size_t pixelIndex = pixelCoordsToPixelIndex({ pixelCoords[0], pixelCoords[1], (size_t)pixelZ, pixelCoords[3] });

		if (ON)
			mEnergy[pixelIndex] += mKernelZ[abs(iz)];
		else
			mEnergy[pixelIndex] -= mKernelZ[abs(iz)];
	}
	

	// splat the 1D gaussian along w			
	for (int iw = mKernelRadiusStartW; iw <= mKernelRadiusEndW; ++iw) {
		int pixelW = ((int)pixelCoords[3] + iw + (int)mChannelCount) % (int)mChannelCount;

		size_t pixelIndex = pixelCoordsToPixelIndex({ pixelCoords[0], pixelCoords[1], pixelCoords[2], (size_t)pixelW });

		if (ON)
			mEnergy[pixelIndex] += mKernelW[abs(iw)];
		else
			mEnergy[pixelIndex] -= mKernelW[abs(iw)];
	}
}

void Maker::makeInitialBinaryPattern() {
	// generate an initial set of on pixels, with a max density of kInitialBinaryPatternDensity
	// If we get duplicate random numbers, we won't get the full targetCount of on pixels, but that is ok.
	LLOG_DBG << "STBN::Maker initializing to white noise";
	pcg32_random_t rng = getRNG();
	size_t targetCount = std::max(size_t(float(mNumPixels) * kInitialBinaryPatternDensity), (size_t)2);
	for (size_t index = 0; index < targetCount; ++index) {
		size_t pixelIndex = pcg32_boundedrand_r(&rng, (int)mNumPixels - 1);
		mPixelOn[pixelIndex] = true;
		splatEnergy<true>(pixelIndex);
	}
	
	// Make these into blue noise distributed points by removing the point at the tightest
	// cluster and placing it into the largest void. Repeat until those are the same location.
	LLOG_DBG << "STBN::Maker reorganizing to blue noise";
	while (1) {
		size_t tightestClusterIndex = getTightestCluster();
		mPixelOn[tightestClusterIndex] = false;
		splatEnergy<false>(tightestClusterIndex);

		size_t largestVoidIndex = getLargestVoid();
		mPixelOn[largestVoidIndex] = true;
		splatEnergy<true>(largestVoidIndex);

		if (tightestClusterIndex == largestVoidIndex) break;
	}
	LLOG_DBG << "STBN::Maker blue noise reorganization done";
}

size_t Maker::getOnesCount() {
	size_t ret = 0;
	for (bool b : mPixelOn) {
		if (b) ret++;
	}
	return ret;
}

void Maker::runPhase1() {
	// Make the initial pattern progressive.
	// Find the tightest cluster and remove it. The rank for that pixel
	// is the number of ones left in the pattern.
	// Go until no more ones are in the texture.

	LLOG_DBG << "STBN::Maker running phase 1";

	size_t onesCount = getOnesCount();
	size_t initialOnesCount = onesCount;

	while (onesCount > 0) {
		size_t tightestClusterIndex = getTightestCluster();

		onesCount--;

		mPixelOn[tightestClusterIndex] = false;
		mPixelRank[tightestClusterIndex] = onesCount;

		splatEnergy<false>(tightestClusterIndex);

	}

	// restore the "on" states
	for (int pixelIndex = 0; pixelIndex < mNumPixels; ++pixelIndex) {
		if (mPixelRank[pixelIndex] < mNumPixels) {
			mPixelOn[pixelIndex] = true;
			splatEnergy<true>(pixelIndex);
		}
	}
	LLOG_DBG << "STBN::Maker running phase 1 done";
}

void Maker::runPhase2() {
	// Add new samples until half are ones.
	// Do this by repeatedly inserting a one into the largest void, and the
	// rank is the number of ones before you added it.

	LLOG_DBG << "STBN::Maker running phase 2";

	size_t onesCount = getOnesCount();
	const size_t targetOnesCount = mNumPixels / 2;

	while (onesCount < targetOnesCount) {
		size_t largestVoidIndex = getLargestVoid();

		mPixelOn[largestVoidIndex] = true;
		mPixelRank[largestVoidIndex] = onesCount;

		splatEnergy<true>(largestVoidIndex);

		onesCount++;
	}
	LLOG_DBG << "STBN::Maker running phase 2 done";
}

void Maker::runPhase3() {
	// Reverse the meaning of zeros and ones.
	// Remove the tightest cluster and give it the rank of the number of zeros in the binary pattern before you removed it.
	// Go until there are no more ones

	LLOG_DBG << "STBN::Maker running phase 3";

	std::fill(mEnergy.begin(), mEnergy.end(), 0.0f);
	for (size_t pixelIndex = 0; pixelIndex < mNumPixels; ++pixelIndex) {
		mPixelOn[pixelIndex] = !mPixelOn[pixelIndex];
		if (mPixelOn[pixelIndex])splatEnergy<true>(pixelIndex);
	}

	size_t onesCount = getOnesCount();

	while (onesCount > 0) {
		size_t tightestClusterIndex = getTightestCluster();

		mPixelOn[tightestClusterIndex] = false;
		mPixelRank[tightestClusterIndex] = mNumPixels - onesCount;

		onesCount--;

		splatEnergy<false>(tightestClusterIndex);
	}

	LLOG_DBG << "STBN::Maker running phase 3 done";
}


}  // namespace STBN
