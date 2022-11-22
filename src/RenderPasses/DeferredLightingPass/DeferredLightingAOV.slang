struct AOVData {
    float  depth;
    float3 position;
    float3 albedo;
    float3 shadow;
    float3 normal;
    float  occlusion;

    /** Create AOVData with default parameters.
    */
    static AOVData create()
    {
        AOVData d;
        d.depth = 1.0;
        d.position = float3(0.0);
        d.albedo = float3(0.0);
        d.shadow = float3(1.0);
        d.normal = float3(0.0);
        d.occlusion = 1.0;

        return d;
    }
};