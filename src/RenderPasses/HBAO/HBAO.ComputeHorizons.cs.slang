#include "Utils/Math/MathConstants.slangh"
#include "HBAO.Common.slangh"

import Scene.Camera.Camera;

cbuffer PerFrameCB {
  Camera gCamera;

  uint2  gFrameDim;
  uint2  gNoiseOffset;

  float  gAoQuality;
  float  gAoFactor; // No need to keep it here
  float  gRotationOffset;
  float  gAoDistance;
}

SamplerState gNoiseSampler;
SamplerState gPointSampler;
SamplerState gDepthSampler;

Texture2D gDepthTex;
Texture2D gHiMaxZTex;
Texture2D gNoiseTex;

RWTexture2D<float4> gOutput;


/* Return horizon angle cosine. */
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
  
  const uint2 ipos = dispatchThreadId.xy;
  
  float2 uvs = ipos / float2(gFrameDim);
  float depth = sampleDepthTexture(uvs);

  // Early out
  if (depth >= 1) {
    return;
  }

  // Avoid self shadowing.
  //depth = saturate(depth - 3e-6); /* Tweaked for 24bit depth buffer. */

  float3 viewPosition = getViewPositionFromDepth(uvs, depth).xyz;
  float4 noise = gNoiseTex.SampleLevel(gNoiseSampler, ((ipos + gNoiseOffset) / float2(64)), 0);
  float2 max_dir = getMaxDir(viewPosition.z);
  
  float4 dirs;
  dirs.xy = getAoDir(noise.x * 0.5);
  dirs.zw = getAoDir(noise.x * 0.5 + 0.5);

  /* Search in 4 directions. */
  float4 output;
  output.xy = search_horizon_sweep(dirs.xy, viewPosition, uvs, noise.y, max_dir);
  output.zw = search_horizon_sweep(dirs.zw, viewPosition, uvs, noise.z, max_dir);

  /* Resize output for integer texture. */
  gOutput[ipos] = packHorizons(output);

}
