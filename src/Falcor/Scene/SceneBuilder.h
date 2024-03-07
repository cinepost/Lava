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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#ifndef SRC_FALCOR_SCENE_SCENEBUILDER_H_
#define SRC_FALCOR_SCENE_SCENEBUILDER_H_

#include <map>
#include <bitset>
#include <string>
#include <unordered_map>

#include "Falcor/Utils/Scripting/Dictionary.h"
#include "Falcor/Utils/ThreadPool.h"

#include "Scene.h"
#include "SceneCache.h"
#include "Transform.h"
#include "TriangleMesh.h"
#include "Material/MaterialTextureLoader.h"
#include "MaterialX/MaterialX.h"
#include "VertexAttrib.slangh"

#include "Falcor/Scene/Lights/LightLinker.h"

#include "Geometry.h"


namespace Falcor {

class Device;
class MeshletBuilder;

class dlldecl SceneBuilder {
 public:
    using SharedPtr = std::shared_ptr<SceneBuilder>;

    static constexpr uint32_t kInvalidNodeID = Animatable::kInvalidNode;
    static constexpr uint32_t kInvalidMeshletID = Animatable::kInvalidNode;     ///< Largest uint32 value (-1)
    static constexpr uint32_t kInvalidMeshID = Animatable::kInvalidNode;        ///< Largest uint32 value (-1)

    using Mesh = Geometry::Mesh;
    using MeshSpec = Geometry::MeshSpec;
    using MeshInstanceSpec = Geometry::MeshInstanceSpec;
    using InstanceShadingSpec = Geometry::InstanceShadingSpec;
    using InstanceVisibilitySpec = Geometry::InstanceVisibilitySpec;
    using InstanceExportedDataSpec = Geometry::InstanceExportedDataSpec;
    using MeshInstanceCreationSpec = Geometry::MeshInstanceCreationSpec;
    using MeshletSpec = Geometry::MeshletSpec;
    using MeshAttributeIndices = std::vector<Mesh::VertexAttributeIndices>;

    class MeshID {
        public:
            MeshID(): v(kInvalidMeshID) {};
            MeshID(uint32_t id): v(id) {};
            MeshID(std::shared_future<uint32_t> f): v(f) {};
        
            uint32_t _get() const;

        private:
            mutable std::variant<uint32_t, std::shared_future<uint32_t>> v;

        public:
            operator uint32_t () const { return _get(); };

            void operator=(uint32_t id);
            void operator=(std::shared_future<uint32_t> f);
    };
    
    enum class UpdateMode: uint8_t {
        None,
        IPR,
        Batch,
        Default = None
    };


    /** Flags that control how the scene will be built. They can be combined together.
    */
    enum class Flags: uint32_t {
        None                            = 0x0,      ///< None
        DontMergeMaterials              = 0x1,      ///< Don't merge materials that have the same properties. Use this option to preserve the original material names.
        UseOriginalTangentSpace         = 0x2,      ///< Use the original tangent space that was loaded with the mesh. By default, we will ignore it and use MikkTSpace to generate the tangent space. We will always generate tangent space if it is missing.
        AssumeLinearSpaceTextures       = 0x4,      ///< By default, textures representing colors (diffuse/specular) are interpreted as sRGB data. Use this flag to force linear space for color textures.
        DontMergeMeshes                 = 0x8,      ///< Preserve the original list of meshes in the scene, don't merge meshes with the same material. This flag only applies to scenes imported by 'AssimpImporter'.
        UseSpecGlossMaterials           = 0x10,     ///< Set materials to use Spec-Gloss shading model. Otherwise default is Spec-Gloss for OBJ, Metal-Rough for everything else.
        UseMetalRoughMaterials          = 0x20,     ///< Set materials to use Metal-Rough shading model. Otherwise default is Spec-Gloss for OBJ, Metal-Rough for everything else.
        NonIndexedVertices              = 0x40,     ///< Convert meshes to use non-indexed vertices. This requires more memory but may increase performance.
        Force32BitIndices               = 0x80,     ///< Force 32-bit indices for all meshes. By default, 16-bit indices are used for small meshes.
        RTDontMergeStatic               = 0x100,    ///< For raytracing, don't merge all static non-instanced meshes into single pre-transformed BLAS.
        RTDontMergeDynamic              = 0x200,    ///< For raytracing, don't merge dynamic non-instanced meshes with identical transforms into single BLAS.
        RTDontMergeInstanced            = 0x400,    ///< For raytracing, don't merge instanced meshes with identical instances into single BLAS.
        FlattenStaticMeshInstances      = 0x800,    ///< Flatten static mesh instances by duplicating mesh data and composing transformations. Animated instances are not affected. Can lead to a large increase in memory use.
        DontOptimizeGraph               = 0x1000,   ///< Don't optimize the scene graph to remove unnecessary nodes.
        DontOptimizeMaterials           = 0x2000,   ///< Don't optimize materials by removing constant textures. The optimizations are lossless so should generally be enabled.
        DontUseDisplacement             = 0x4000,   ///< Don't use displacement mapping.
        UseCompressedHitInfo            = 0x8000,   ///< Use compressed hit info (on scenes with triangle meshes only).
        TessellateCurvesIntoPolyTubes   = 0x10000,  ///< Tessellate curves into poly-tubes (the default is linear swept spheres).
        UseRaytracing                   = 0x20000,  ///< Use raytracing
        UseCryptomatte                  = 0x40000,  ///< Use cryptomatte system
        GenerateMeshlets                = 0x80000,  ///< Generate meshlets data
        KeepMeshData                    = 0x100000, ///< Keep mesh list for batch mode updates
        KeepLocalMeshData               = 0x200000, ///< Keep local mesh data for scene rebuilds
        KeepLocalMeshletSpecData        = 0x400000, ///< Keep local meshlet spec data for scene rebuilds

        UseCache                        = 0x10000000, ///< Enable scene caching. This caches the runtime scene representation on disk to reduce load time.
        RebuildCache                    = 0x20000000, ///< Rebuild scene cache.

        Default = None
    };

    /** Pre-processed mesh data.
        This data is formatted such that it can directly be copied
        to the global scene buffers.
    */
    struct ProcessedMesh {
        std::string name;
        Vao::Topology topology = Vao::Topology::Undefined;
        Material::SharedPtr pMaterial;
        uint32_t skeletonNodeId = kInvalidNodeID; ///< Forwarded from Mesh struct.

        uint64_t indexCount = 0;            ///< Number of indices, or zero if non-indexed.
        bool use16BitIndices = false;       ///< True if the indices are in 16-bit format.
        bool isFrontFaceCW = false;         ///< Indicate whether front-facing side has clockwise winding in object space.
        std::vector<uint32_t> indexData;    ///< Vertex indices in either 32-bit or 16-bit format packed tightly, or empty if non-indexed.
        std::vector<StaticVertexData> staticData;
        std::vector<SkinningVertexData> skinningData;
        std::vector<int32_t> perPrimitiveMaterialIDsData;

        bool hasMultipleMaterials() const { return !perPrimitiveMaterialIDsData.empty(); }
    };

    /** Curve description.
    */
    struct Curve {
        template<typename T>
        struct Attribute {
            const T* pData = nullptr;
        };

        std::string name;                           ///< The curve's name.
        uint32_t degree = 1;                        ///< Polynomial degree of the curve; linear (1) by default.
        uint32_t vertexCount = 0;                   ///< The number of vertices.
        uint32_t indexCount = 0;                    ///< The number of indices (i.e., tube segments).
        const uint32_t* pIndices = nullptr;         ///< Array of indices. The element count must match `indexCount`. This field is required.
        Material::SharedPtr pMaterial;              ///< The curve's material. Can't be nullptr.

        Attribute<float3> positions;                ///< Array of vertex positions. This field is required.
        Attribute<float> radius;                    ///< Array of sphere radius. This field is required.
        Attribute<float2> texCrds;                  ///< Array of vertex texture coordinates. This field is optional. If set to nullptr, all texCrds will be set to (0,0).
    };

    /** Pre-processed curve data.
        This data is formatted such that it can directly be copied
        to the global scene buffers.
    */
    struct ProcessedCurve {
        std::string name;
        Vao::Topology topology = Vao::Topology::LineStrip;
        Material::SharedPtr pMaterial;

        std::vector<uint32_t> indexData;
        std::vector<StaticCurveVertexData> staticData;
    };

    struct Node {
        std::string name;
        float4x4 transform;
        float4x4 meshBind;          // For skinned meshes. World transform at bind time.
        float4x4 localToBindPose;   // For bones. Inverse bind transform.
        uint32_t parent = kInvalidNodeID;
    };

    using InstanceMatrices = std::vector<float4x4>;

    std::shared_ptr<Device> device() { return mpDevice; };
    std::shared_ptr<Device> device() const { return mpDevice; };

    /** Create a new object
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice, Flags mFlags = Flags::Default);

    /** Create a new builder and import a scene/model file
        \param filename The filename to load
        \param flags The build flags
        \param instances A list of instance matrices to load. This is optional, by default a single instance will be load
        \return A new object with the imported file already initialized. If an import error occurred, a nullptr will be returned
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice, const std::string& filename, Flags buildFlags = Flags::Default, const InstanceMatrices& instances = InstanceMatrices());

    /** Import a scene/model file
        \param filename The filename to load
        \param instances A list of instance matrices to load. This is optional, by default a single instance will be load
        \return true if the import succeeded, otherwise false
    */
    bool import(const std::string& filename, const InstanceMatrices& instances = InstanceMatrices(), const Dictionary& dict = Dictionary());

    /** Get the scene. Make sure to add all the objects before calling this function
        \return nullptr if something went wrong, otherwise a new Scene object
    */
    Scene::SharedPtr getScene();

    /** Add a mesh instance to a node
    */
    bool addMeshInstance(uint32_t nodeID, uint32_t meshID, const MeshInstanceCreationSpec* pCreationSpec = nullptr);
    
    bool meshHasInstance(uint32_t meshID, const std::string& instance_name);

    bool updateMeshInstance(uint32_t meshID, const MeshInstanceCreationSpec* pCreationSpec, const Node& node);

    bool deleteMeshInstance(const std::string& name);

    bool deleteMesh(const std::string& meshName);

    /** Add a mesh. This function will throw an exception if something went wrong.
        \param meshDesc The mesh's description.
        \return The ID of the mesh in the scene. Note that all of the instances share the same mesh ID.
    */
    uint32_t addMesh(const Mesh& meshDesc);

    uint32_t getMeshID(const std::string& name);

    bool meshExist(const std::string& name);

    /** Add a triangle mesh.
        \param The triangle mesh to add.
        \param pMaterial The material to use for the mesh.
        \return The ID of the mesh in the scene.
    */
    uint32_t addTriangleMesh(const TriangleMesh::SharedPtr& pTriangleMesh, const Material::SharedPtr& pMaterial);

    /** Pre-process a mesh into the data format that is used in the global scene buffers.
        Throws an exception if something went wrong.
        \param mesh The mesh to pre-process.
        \return The pre-processed mesh.
    */
    ProcessedMesh processMesh(const Mesh& mesh, MeshAttributeIndices* pAttributeIndices = nullptr) const;

    /** Generate tangents for a mesh.
        \param mesh The mesh to generate tangents for. If successful, the tangent attribute on the mesh will be set to the output vector.
        \param tangents Output for generated tangents.
    */
    void generateTangents(Mesh& mesh, std::vector<float4>& tangents) const;

    /** Add a pre-processed mesh.
        \param mesh The pre-processed mesh.
        \return The ID of the mesh in the scene. Note that all of the instances share the same mesh ID.
    */
    uint32_t addProcessedMesh(const ProcessedMesh& mesh);

    /** Set mesh vertex cache for animation.
        \param[in] cachedCurves The mesh vertex cache data.
    */
    void setCachedMeshes(const std::vector<CachedMesh>&& cachedMeshes) { mSceneData.cachedMeshes = cachedMeshes; }

    // Custom primitives

    /** Add an AABB defining a custom primitive.
        \param[in] typeID The intersection shader ID that will be run on this primitive.
        \param[in] aabb An AABB describing the bounds of the primitive.
    */
    void addCustomPrimitive(uint32_t userID, const AABB& aabb);

    // Curves

    /** Add a curve.
        Throws an exception if something went wrong.
        \param curve The curve to add.
        \return The ID of the curve in the scene. Note that all of the instances share the same curve ID.
    */
    uint32_t addCurve(const Curve& curve);

    /** Pre-process a curve into the data format that is used in the global scene buffers.
        Throws an exception if something went wrong.
        \param curve The curve to pre-process.
        \return The pre-processed curve.
    */
    ProcessedCurve processCurve(const Curve& curve) const;

    /** Add a pre-processed curve.
        \param curve The pre-processed curve.
        \return The ID of the curve in the scene. Note that all of the instances share the same curve ID.
    */
    uint32_t addProcessedCurve(const ProcessedCurve& curve);

    // Materials

    /** Get the list of materials.
    */
    //const std::vector<Material::SharedPtr>& getMaterials() const { return mSceneData.materials; }

    /** Get a material by name.
        Note: This returns the first material found with a matching name.
        \param name Material name.
        \return Returns the first material with a matching name or nullptr if none was found.
    */
    Material::SharedPtr getMaterial(const std::string& name) const;

    bool updateMaterial(const std::string& name, const Material::SharedPtr& pNewMaterial);

    bool getMaterialID(const std::string& name, uint32_t& materialId) const;

    /** Add a material.
        \param pMaterial The material.
        \return The ID of the material in the scene.
    */
    uint32_t addMaterial(const Material::SharedPtr& pMaterial);

    /** Request loading a material texture.
        \param[in] pMaterial Material to load texture into.
        \param[in] slot Slot to load texture into.
        \param[in] path Texture file path.
    */
    bool loadMaterialTexture(const Material::SharedPtr& pMaterial, Material::TextureSlot slot, const fs::path& path, bool loadAsSparse = false);

    /** Wait until all material textures are loaded.
    */
    void waitForMaterialTextureLoading();

    /** Add a node based material.
        \param pMaterial The material.
        \return The ID of the material in the scene.
    */
    uint32_t addMaterialX(const MaterialX::SharedPtr& pMaterial);

    // Volumes

    /** Get the list of grid volumes.
    */
    const std::vector<GridVolume::SharedPtr>& getGridVolumes() const { return mSceneData.gridVolumes; }

    /** Get a grid volume by name.
        Note: This returns the first volume found with a matching name.
        \param name Volume name.
        \return Returns the first volume with a matching name or nullptr if none was found.
    */
    GridVolume::SharedPtr getGridVolume(const std::string& name) const;

    /** Add a grid volume.
        \param pGridVolume The grid volume.
        \param nodeID The node to attach the volume to (optional).
        \return The ID of the volume in the scene.
    */
    uint32_t addGridVolume(const GridVolume::SharedPtr& pGridVolume, uint32_t nodeID = kInvalidNodeID);

    // Lights

    /** Get the list of lights.
    */
    const std::vector<Light::SharedPtr>& getLights() const { return mSceneData.lights; }

    /** Get a light by name.
        Note: This returns the first light found with a matching name.
        \param name Light name.
        \return Returns the first light with a matching name or nullptr if none was found.
    */
    Light::SharedPtr getLight(const std::string& name) const;

    Light::SharedPtr getLight(uint32_t lightID) const;

    /** Add a light source
        \param pLight The light object.
        \return The light ID
    */
    uint32_t addLight(const Light::SharedPtr& pLight);

    bool updateLight(const std::string& name, const Light& newLight);

    void setLightsActive(bool state);

    void deleteLight(const std::string& name);

    void setLightActive(const std::string& name, bool state);

    // Environment map

    /** Get the environment map.
    */
    const EnvMap::SharedPtr& getEnvMap() const { return mSceneData.pEnvMap; }

    /** Set an environment map.
        \param[in] pEnvMap Environment map. Can be nullptr.
    */
    void setEnvMap(EnvMap::SharedPtr pEnvMap) { mSceneData.pEnvMap = pEnvMap; }

    // Cameras

    /** Get the list of cameras.
    */
    const std::vector<Camera::SharedPtr>& getCameras() const { return mSceneData.cameras; }

    /** Add a camera.
        \param pCamera Camera to be added.
        \return The camera ID
    */
    uint32_t addCamera(const Camera::SharedPtr& pCamera);

    /** Get the selected camera.
    */
     Camera::SharedPtr getSelectedCamera() const;

    /** Set the selected camera.
        \param pCamera Camera to use as selected camera (needs to be added first).
    */
    void setSelectedCamera(const Camera::SharedPtr& pCamera);

    /** Get the camera speed.
    */
    float getCameraSpeed() const { return mSceneData.cameraSpeed; }

    /** Set the camera's speed
    */
    void setCameraSpeed(float speed) { mSceneData.cameraSpeed = speed; }

    /** Get the build flags
    */
    Flags getFlags() const { return mFlags; }


    /** Get the list of animations.
    */
    const std::vector<Animation::SharedPtr>& getAnimations() const { return mSceneData.animations; }

    /** Add an animation
        \param animation The animation
    */
    void addAnimation(const Animation::SharedPtr& pAnimation);

    /** Create an animation for an animatable object.
        \param pAnimatable Animatable object.
        \param name Name of the animation.
        \param duration Duration of the animation in seconds.
        \return Returns a new animation or nullptr if an animation already exists.
    */
    Animation::SharedPtr createAnimation(Animatable::SharedPtr pAnimatable, const std::string& name, double duration);

    // Scene graph

    /** Adds a node to the graph.
        \return The node ID.
    */
    uint32_t addNode(const Node& node);

    /** Gets a node from the graph.
        \return The node ID.
    */
    uint32_t getInternalNode(const std::string& name);

    /** Updates a node in the graph.
        \return The updated node ID.
    */
    uint32_t updateNode(const Node& node);

    /** Add a curve instance to a node.
    */
    void addCurveInstance(uint32_t nodeID, uint32_t curveID);

    /** Check if a scene node is animated. This check is done recursively through parent nodes.
        \return Returns true if node is animated.
    */
    bool isNodeAnimated(uint32_t nodeID) const;

    /** Set the animation interpolation mode for a given scene node. This sets the mode recursively for all parent nodes.
    */
    void setNodeInterpolationMode(uint32_t nodeID, Animation::InterpolationMode interpolationMode, bool enableWarping);

    const std::unordered_map<std::string, MeshID>& meshMap() const { return mMeshMap; }; 

    void freeTemporaryResources();

    ~SceneBuilder();

private:
    void resetScene(bool reuseExisting = false);

public:

    // TODO: Add support for dynamic curves
    struct CurveSpec {
        std::string name;
        Vao::Topology topology;
        uint32_t materialId = 0;            ///< Global material ID.
        uint32_t staticVertexOffset = 0;    ///< Offset into the shared 'staticData' array. This is calculated in createCurveGlobalBuffers().
        uint32_t staticVertexCount = 0;     ///< Number of static curve vertices.
        uint32_t indexOffset = 0;           ///< Offset into the shared 'indexData' array. This is calculated in createCurveGlobalBuffers().
        uint32_t indexCount = 0;            ///< Number of indices.
        uint32_t vertexCount = 0;           ///< Number of vertices.
        uint32_t degree = 1;                ///< Polynomial degree of curve; linear (1) by default.
        std::vector<uint32_t> instances;    ///< Node IDs of all instances of this curve.

        // Pre-processed curve vertex data.
        std::vector<uint32_t> indexData;    ///< Vertex indices in 32-bit.
        std::vector<StaticCurveVertexData> staticData;
    };

protected:
    SceneBuilder(std::shared_ptr<Device> pDevice, Flags buildFlags);

    struct InternalNode : Node {
        InternalNode() = default;
        InternalNode(const Node& n) : Node(n) {}
        std::vector<uint32_t> children;     ///< Node IDs of all child nodes.
        std::vector<uint32_t> meshes;       ///< Node IDs of all child nodes.
        std::vector<uint32_t> curves;       ///< Curve IDs of all curves this node transforms.
        std::vector<uint32_t> sdfGrids;         ///< SDF grid IDs of all SDF grids this node transforms.
        std::vector<Animatable*> animatable;    ///< Pointers to all animatable objects attached to this node.
        bool dontOptimize = false;              ///< Whether node should be ignored in optimization passes

        /** Returns true if node has any attached scene objects.
        */
        bool hasObjects() const { return !meshes.empty() || !curves.empty() || !animatable.empty(); }
    };
    
    using SceneGraph = std::vector<InternalNode>;
    using MeshList = std::vector<MeshSpec>;
    using MeshletList = std::vector<MeshletData>;
    using MeshGroup = Scene::MeshGroup;
    using MeshGroupList = std::vector<MeshGroup>;
    using CurveList = std::vector<CurveSpec>;

    std::shared_ptr<Device> mpDevice;
    std::unique_ptr<MeshletBuilder> mpMeshletBuilder;

    Scene::SceneData mSceneData;
    Scene::SharedPtr mpScene;
    SceneCache::Key mSceneCacheKey;
    bool mWriteSceneCache = false;  ///< True if scene cache should be written after import.

    SceneGraph mSceneGraph;
    const Flags mFlags;

    // Meshes
    MeshList mMeshes;
    MeshGroupList mMeshGroups; ///< Groups of meshes. Each group represents all the geometries in a BLAS for ray tracing.
    //std::unordered_map<std::string, std::variant<uint32_t, std::shared_future<uint32_t>>>   mMeshMap;     // mesh name to SceneBuilder mesh id or it's async future
    std::unordered_map<std::string, MeshID>   mMeshMap;     // mesh name to SceneBuilder mesh id or it's async future

    // Instances
    std::unordered_map<std::string, uint32_t> mInstanceToMeshMap;

    // Curves
    CurveList mCurves;

    // Meshlets
    std::vector<MeshletList> mMeshletLists;
    std::vector<uint32_t> mMeshletVertices;    ///< Meshlet vertices that point to global scene vertex data.
    std::vector<uint8_t>  mMeshletIndices;     ///< Indices of a primitive verices. Vector size should be equal to indexCount.
    std::vector<uint32_t> mMeshletPrimIndices; ///< Primitive indices in a global scene buffer. It's used in case if meshlet primitives order differs from original mesh.

    std::unique_ptr<MaterialTextureLoader> mpMaterialTextureLoader;
    GpuFence::SharedPtr mpFence;

    std::vector<Material::SharedPtr> mMaterials;
    std::vector<MaterialX::SharedPtr> mMaterialXs;
    std::unordered_map<const Material*, uint32_t> mMaterialToId;


    // mt
    std::mutex mMeshesMutex;
    std::mutex mMeshletsMutex;

    BS::multi_future<uint32_t> mAddGeoTasks;


    // Mesh helpers
    bool doesNodeHaveAnimation(uint32_t nodeID) const;
    void updateLinkedObjects(uint32_t nodeID, uint32_t newNodeID);
    bool collapseNodes(uint32_t parentNodeID, uint32_t childNodeID);
    bool mergeNodes(uint32_t dstNodeID, uint32_t srcNodeID);
    void flipTriangleWinding(MeshSpec& mesh);
    void updateSDFGridID(uint32_t oldID, uint32_t newID);

    /** Split a mesh by the given axis-aligned splitting plane.
        \return Pair of optional mesh IDs for the meshes on the left and right side, respectively.
    */
    std::pair<std::optional<uint32_t>, std::optional<uint32_t>> splitMesh(uint32_t meshID, const int axis, const float pos);

    void splitIndexedMesh(const MeshSpec& mesh, MeshSpec& leftMesh, MeshSpec& rightMesh, const int axis, const float pos);
    void splitNonIndexedMesh(const MeshSpec& mesh, MeshSpec& leftMesh, MeshSpec& rightMesh, const int axis, const float pos);

    // Mesh group helpers
    size_t countTriangles(const MeshGroup& meshGroup) const;
    AABB calculateBoundingBox(const MeshGroup& meshGroup) const;
    bool needsSplit(const MeshGroup& meshGroup, size_t& triangleCount) const;
    MeshGroupList splitMeshGroupSimple(MeshGroup& meshGroup) const;
    MeshGroupList splitMeshGroupMedian(MeshGroup& meshGroup) const;
    MeshGroupList splitMeshGroupMidpointMeshes(MeshGroup& meshGroup);

    // Post processing
    void preparePerPrimMaterialIndices(ProcessedMesh& processedMesh, const Mesh::StringList* pStrings, const int32_t* pSourceIDs, size_t count) const;
    void prepareDisplacementMaps();
    void prepareSceneGraph();
    void prepareMeshes();
    void removeUnusedMeshes();
    void flattenStaticMeshInstances();
    void optimizeSceneGraph();
    void pretransformStaticMeshes();
    void unifyTriangleWinding();
    void calculateMeshBoundingBoxes();
    void createMeshGroups();
    void optimizeGeometry();
    void sortMeshes();
    void createGlobalBuffers();
    void createCurveGlobalBuffers();
    void optimizeMaterials();
    void removeDuplicateMaterials();
    void collectVolumeGrids();
    void quantizeTexCoords();
    void removeDuplicateSDFGrids();

    // Scene setup
    void createMeshData();
    void createMeshletsData();
    void createMeshInstanceData(uint32_t& tlasInstanceIndex);
    void createCurveData();
    void createCurveInstanceData(uint32_t& tlasInstanceIndex);
    void createSceneGraph();
    void createMeshBoundingBoxes();
    void calculateCurveBoundingBoxes();

    enum class DynamicUpdateFlags: uint32_t {
        None                    = 0x0,
        UpdateSceneMaterials    = 0x1,
        UpdateSceneNodes        = 0x2,
        UpdateSceneInstances    = 0x4,
        UpdateMeshGroups        = 0x8,
    };

    UpdateMode mUpdateMode = UpdateMode::Batch;

    // Scene dynamic update flags
    bool mUpdateSceneMaterials = false;
    bool mUpdateSceneNodes = false;
    bool mUpdateSceneInstances = false;
    bool mReBuildMeshGroups = true;

    friend class SceneCache;
    friend class MeshletBuilder;
};

inline std::string to_string(SceneBuilder::Flags f) {
    return std::bitset<32>(static_cast<uint32_t>(f)).to_string();
}

enum_class_operators(SceneBuilder::Flags);

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_SCENEBUILDER_H_
