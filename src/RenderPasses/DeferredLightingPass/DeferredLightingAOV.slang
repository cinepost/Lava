struct AOVData {
    float  depth;
    float3 position;
    float3 albedo;
    float3 emission;
    float3 shadow;
    float3 normal;
    float  occlusion;

    /** Create AOVData with default parameters.
    */
    static AOVData create()
    {
        AOVData d;
        d.depth = 0.f;
        d.position = float3(0.f);
        d.albedo = float3(0.f);
        d.emission = float3(0.f);
        d.shadow = float3(0.f);
        d.normal = float3(0.f);
        d.occlusion = 0.f;

        return d;
    }
};