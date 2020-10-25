// UAV output channels
RWTexture2D<uint2>  gVBuffer;
RWTexture2D<float2> gMotionVectors;
RWTexture2D<float4> gFaceNormalW;
RWTexture2D<float2> gPosNormalFwidth;
RWTexture2D<float2> gLinearZAndDeriv;
RWTexture2D<float>  gSurfaceSpreadAngle;

struct GBufferPSOut
{
    float4 posW             : SV_TARGET0;
    float4 normW            : SV_TARGET1;
    float4 tangentW         : SV_TARGET2;
    float4 texC             : SV_TARGET3;
    float4 diffuseOpacity   : SV_TARGET4;
    float4 specRough        : SV_TARGET5;
    float4 emissive         : SV_TARGET6;
    float4 matlExtra        : SV_TARGET7;
};

VSOut vsMain(VSIn vsIn)
{
    return vsIn;
}

GBufferPSOut psMain(VSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics)
{
    GBufferPSOut psOut;


    return psOut;
}
