cbuffer CB {
  uint2 frameDim;
  int   gMipLevel;

#ifdef _LAYERED
  int gDepthLayer;  
#endif

}

SamplerState gGatherSampler;     ///< Sampler for linear interpolated fetches.

#ifdef _LAYERED
Texture2DArray gSourceBuffer;
#else
Texture2D gSourceBuffer;
#endif

RWTexture2D<float> gOutputBuffer;

#ifdef _LAYERED
#  define sampleLowerMip(t) gSourceBuffer[int3(t, gDepthLayer)].r
#  define gatherLowerMip(uv) gSourceBuffer.SampleLevel(gGatherSampler, float3(uv, gDepthLayer), 0)
#else
#  define sampleLowerMip(t) gSourceBuffer[t].r
#  define gatherLowerMip(uv) gSourceBuffer.SampleLevel(gGatherSampler, uv, 0)
#endif

#ifdef MIN_PASS
#  define minmax2(a, b) min(a, b)
#  define minmax3(a, b, c) min(min(a, b), c)
#  define minmax4(a, b, c, d) min(min(min(a, b), c), d)
#else /* MAX_PASS */
#  define minmax2(a, b) max(a, b)
#  define minmax3(a, b, c) max(max(a, b), c)
#  define minmax4(a, b, c, d) max(max(max(a, b), c), d)
#endif


[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
  
  uint2 texelPos = dispatchThreadId.xy;
  if (any(texelPos >= frameDim)) return;
  
  uint2 mipsize = frameDim;

//#ifndef COPY_DEPTH
//  texelPos *= 2;
//#endif

//#ifdef COPY_DEPTH
//  float val = sampleLowerMip(texelPos);
//#else
  float4 samp;
#  ifdef GPU_ARB_texture_gather
  /* + 1.0 to gather at the center of target 4 texels. */
  samp = gatherLowerMip((float2(texelPos) + 1.0) / float2(mipsize));
#  else
  samp.x = sampleLowerMip(texelPos*2);
  samp.y = sampleLowerMip(texelPos*2 + int2(1, 0));
  samp.z = sampleLowerMip(texelPos*2 + int2(1, 1));
  samp.w = sampleLowerMip(texelPos*2 + int2(0, 1));
#  endif

  float val = minmax4(samp.x, samp.y, samp.z, samp.w);

  /* if we are reducing an odd-width texture then fetch the edge texels */
  if (((mipsize.x & 1) != 0) && (texelPos.x == mipsize.x - 3)) {
    /* if both edges are odd, fetch the top-left corner texel */
    if (((mipsize.y & 1) != 0) && (texelPos.y == mipsize.y - 3)) {
      samp.x = sampleLowerMip(texelPos*2 + int2(2, 2));
      val = minmax2(val, samp.x);
    }
#  ifdef GPU_ARB_texture_gather
    samp = gatherLowerMip((float2(texelPos) + float2(2.0, 1.0)) / float2(mipsize));
#  else
    samp.y = sampleLowerMip(texelPos*2 + int2(2, 0));
    samp.z = sampleLowerMip(texelPos*2 + int2(2, 1));
#  endif
    val = minmax3(val, samp.y, samp.z);
  }
  /* if we are reducing an odd-height texture then fetch the edge texels */
  if (((mipsize.y & 1) != 0) && (texelPos.y == mipsize.y - 3)) {
#  ifdef GPU_ARB_texture_gather
    samp = gatherLowerMip((float2(texelPos) + float2(1.0, 2.0)) / float2(mipsize));
#  else
    samp.x = sampleLowerMip(texelPos*2 + int2(0, 2));
    samp.y = sampleLowerMip(texelPos*2 + int2(1, 2));
#  endif
    val = minmax3(val, samp.x, samp.y);
  }
//#endif

  gOutputBuffer[texelPos] = val;
}