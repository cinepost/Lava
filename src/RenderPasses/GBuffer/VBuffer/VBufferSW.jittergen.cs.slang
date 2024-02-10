#include "Utils/Math/MathConstants.slangh"
import Utils.Sampling.SampleGenerator;

#define JITTER_TYPE_SIMPLE 0
#define JITTER_TYPE_R1     1
#define JITTER_TYPE_R2     2
#define JITTER_TYPE_BLUE   3

#ifndef JITTER_TYPE
#define JITTER_TYPE JITTER_TYPE_R1
#endif

cbuffer PerFrameCB {
  uint2             gJitterTextureDim;
  uint              gSampleNumber;                 // Interframe sample number.
};

RWTexture2D<float2> gJitterTexture;


uint part1by1 (uint x) {
    x = (x & 0x0000ffffu);
    x = ((x ^ (x << 8u)) & 0x00ff00ffu);
    x = ((x ^ (x << 4u)) & 0x0f0f0f0fu);
    x = ((x ^ (x << 2u)) & 0x33333333u);
    x = ((x ^ (x << 1u)) & 0x55555555u);
    return x;
}
    
uint compact1by1 (uint x) {
    x = (x & 0x55555555u);
    x = ((x ^ (x >> 1u)) & 0x33333333u);
    x = ((x ^ (x >> 2u)) & 0x0f0f0f0fu);
    x = ((x ^ (x >> 4u)) & 0x00ff00ffu);
    x = ((x ^ (x >> 8u)) & 0x0000ffffu);
    return x;
}
    
uint pack_morton2x16(uint2 v) {
    return part1by1(v.x) | (part1by1(v.y) << 1);
}

uint2 unpack_morton2x16(uint p) {
    return uint2(compact1by1(p), compact1by1(p >> 1));
}

uint inverse_gray32(uint n) {
    n = n ^ (n >> 1);
    n = n ^ (n >> 2);
    n = n ^ (n >> 4);
    n = n ^ (n >> 8);
    n = n ^ (n >> 16);
    return n;
}

// https://www.shadertoy.com/view/llGcDm
int hilbert( int2 p, int level ) {
    int d = 0;
    for( int k=0; k<level; k++ ) {
        int n = level-k-1;
        int2 r = (p>>n)&1;
        d += ((3*r.x)^r.y) << (2*n);
        if (r.y == 0) { if (r.x == 1) { p = (1<<n)-1-p; } p = p.yx; }
    }
    return d;
}

// https://www.shadertoy.com/view/llGcDm
int2 ihilbert( int i, int level ) {
    int2 p = int2(0,0);
    for( int k=0; k<level; k++ ) {
        int2 r = int2( i>>1, i^(i>>1) ) & 1;
        if (r.y==0) { if(r.x==1) { p = (1<<k) - 1 - p; } p = p.yx; }
        p += r<<k;
        i >>= 2;
    }
    return p;
}


// knuth's multiplicative hash function (fixed point R1)
uint kmhf(uint x) {
    return 0x80000000u + 2654435789u * x;
}

uint kmhf_inv(uint x) {
    return (x - 0x80000000u) * 827988741u;
}

// mapping each pixel to a hilbert curve index, then taking a value from the Roberts R1 quasirandom sequence for it
uint hilbert_r1_blue_noise(uint2 p) {
    #if 1
    uint x = uint(hilbert( int2(p), 17 )) % (1u << 17u);
    #else
    //p = p ^ (p >> 1);
    uint x = pack_morton2x16( p ) % (1u << 17u);    
    //x = x ^ (x >> 1);
    x = inverse_gray32(x);
    #endif
    #if 0
    // based on http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    const float phi = 2.0/(sqrt(5.0)+1.0);
    return fract(0.5+phi*float(x));
    #else
    x = kmhf(x);
    return x;
    #endif
}

// mapping each pixel to a hilbert curve index, then taking a value from the Roberts R1 quasirandom sequence for it
float hilbert_r1_blue_noisef(uint2 p) {
    uint x = hilbert_r1_blue_noise(p);
    #if 0
    return float(x >> 24) / 256.0;
    #else
    return float(x) / 4294967296.0;
    #endif
}


[numthreads(16, 16, 1)]
void build(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 pixelPos = dispatchThreadId.xy;

    // early out
    if (any(pixelPos >= gJitterTextureDim)) return;

    float2 jitter = 0.f;

#if JITTER_TYPE == JITTER_TYPE_SIMPLE

    SampleGenerator lsg = SampleGenerator(pixelPos, gSampleNumber);
    jitter = sampleNext2D(lsg);

#elif JITTER_TYPE == JITTER_TYPE_R1

    SampleGenerator sg = SampleGenerator(gJitterTextureDim, gSampleNumber);
    uint2 jitterPosX = pixelPos + gJitterTextureDim * sampleNext2D(sg);
    uint2 jitterPosY = pixelPos + gJitterTextureDim * sampleNext2D(sg);
    jitter.x = hilbert_r1_blue_noisef(jitterPosX);
    jitter.y = hilbert_r1_blue_noisef(jitterPosY);

#endif

    gJitterTexture[pixelPos] = jitter;
}
