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
#include <vector>
#include <array>
#include <algorithm>

namespace STBN {
	
class Maker {
	public:

		typedef std::array<size_t, 4> PixelCoords;

		struct Pixel {
    	bool on = false;
    	int rank = -1;
    	float energy = 0.0f;
		};

		Maker(uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ, uint8_t channelCount, float sigmaX, float sigmaY, float sigmaZ, float sigmaW);

		void make(std::vector<float>& noiseData);

	private:

		PixelCoords pixelIndexToPixelCoords(size_t pixelIndex){
			PixelCoords ret;

			ret[0] = (pixelIndex % mPixelsCountX) / mChannelCount;
			ret[1] = (pixelIndex % mPixelsCountXY) / mPixelsCountX;
			ret[2] = pixelIndex / mPixelsCountXY;
			ret[3] = pixelIndex % (mChannelCount);

			return ret;
		}	

		size_t pixelCoordsToPixelIndex(const PixelCoords& pixelCoords) {
			size_t ret = 0;

			ret += pixelCoords[2];
			ret *= mSizeY;
			ret += pixelCoords[1];
			ret *= mSizeX;
			ret += pixelCoords[0];
			ret *= mChannelCount;
			ret += pixelCoords[3] % mChannelCount;

			return ret;
		}

		size_t getTightestCluster();
		size_t getLargestVoid();

		template <bool ON>
		void splatEnergy(size_t pixelIndex);

		void makeInitialBinaryPattern();
		size_t getOnesCount();
		void runPhase1();
		void runPhase2();
		void runPhase3();

	private:

		void initData(std::vector<int> groups);

		static double constexpr sqrtNewtonRaphson(double x, double curr, double prev) {
			return curr == prev ? curr : sqrtNewtonRaphson(x, 0.5 * (curr + x / curr), curr);
		}

		static double constexpr sqrtNewtonRaphson(double x) {
			return sqrtNewtonRaphson(x, x, 0.0);
		}

		static int constexpr kernelRadius(float sigma) {
			float energyLoss = 0.005f;
			float logEnergyLoss = -5.29831736655f;// log(energyLoss);  not a constexpr unfortunately!
			return int(sqrtNewtonRaphson(-2.0f * sigma * sigma * logEnergyLoss));
		}

		template <bool END>
		static int constexpr kernelRadius(float sigma, int size) {
			int radius = kernelRadius(sigma);
			int start = -radius;
			int end = radius;
			int diameter = radius * 2 + 1;

			if (diameter > size) {
				start = -size / 2;
				end = start + size;
			}

			if (END) return end;
			else return start;
		}

		size_t 		mNumPixels;
		uint32_t 	mSizeX, mSizeY, mSizeZ, mChannelCount;
		uint32_t  mDimsionality;
		uint32_t  mPixelsCountX, mPixelsCountXY;
		float 		mSigmaX, mSigmaY, mSigmaZ, mSigmaW;

		int mKernelRadiusStartX, mKernelRadiusStartY, mKernelRadiusStartZ, mKernelRadiusStartW;
		int mKernelRadiusEndX, mKernelRadiusEndY, mKernelRadiusEndZ, mKernelRadiusEndW;

		std::vector<float> 	mEnergy;
		std::vector<bool> 	mPixelOn;
		std::vector<size_t> mPixelRank;

		std::vector<float> mKernelX;
		std::vector<float> mKernelY;
		std::vector<float> mKernelZ;
		std::vector<float> mKernelW;

		std::vector<Pixel> mPixels;
		std::vector<unsigned int> mMasks;
    std::vector<int> 	mEnergyEmitMin;
    std::vector<int> 	mEnergyEmitMax;
};

}  // namespace STBN
