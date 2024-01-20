#ifndef SRC_FALCOR_SCENE_SCENEBUILDER_SCENEBUILDERMESH_H_
#define SRC_FALCOR_SCENE_SCENEBUILDER_SCENEBUILDERMESH_H_

#include <map>
#include <bitset>
#include <string>
#include <unordered_map>

#include "Falcor/Core/API/VAO.h"
#include "Falcor/Scene/Animation/Animatable.h"
#include "Falcor/Scene/Material/Material.h"
#include "Falcor/Scene/Scene.h"
#include "VertexAttrib.slangh"


namespace Falcor {

    /** Mesh description
    */
    struct SceneBuilderMesh {
        using AttribName = std::string;
        using StringList = std::vector<std::string>;
        using AttributesStrings = std::unordered_map<AttribName, StringList>;

        enum class AttributeFrequency {
            None,
            Constant,       ///< Constant value for mesh. The element count must be 1.
            Uniform,        ///< One value per face. The element count must match `faceCount`.
            Vertex,         ///< One value per vertex. The element count must match `vertexCount`.
            FaceVarying,    ///< One value per vertex per face. The element count must match `indexCount`.
        };

        template<typename T>
        struct Attribute {
            const T* pData = nullptr;
            AttributeFrequency frequency = AttributeFrequency::None;
        };

        std::string name;                           ///< The mesh's name.
        uint32_t faceCount = 0;                     ///< The number of primitives the mesh has.
        uint32_t vertexCount = 0;                   ///< The number of vertices the mesh has.
        uint32_t indexCount = 0;                    ///< The number of indices the mesh has.
        const uint32_t* pIndices = nullptr;         ///< Array of indices. The element count must match `indexCount`. This field is required.
        Vao::Topology topology = Vao::Topology::Undefined; ///< The primitive topology of the mesh
        Material::SharedPtr pMaterial;              ///< The mesh's material. Can't be nullptr.

        Attribute<float3> positions;                ///< Array of vertex positions. This field is required.
        Attribute<float3> normals;                  ///< Array of vertex normals. This field is required.
        Attribute<float4> tangents;                 ///< Array of vertex tangents. This field is optional. If set to nullptr, or if BuildFlags::UseOriginalTangentSpace is not set, the tangent space will be generated using MikkTSpace.
        Attribute<float> curveRadii;                ///< Array of vertex curve radii. This field is optional.
        Attribute<float2> texCrds;                  ///< Array of vertex texture coordinates. This field is optional. If set to nullptr, all texCrds will be set to (0,0).
        Attribute<uint4> boneIDs;                   ///< Array of bone IDs. This field is optional. If it's set, that means that the mesh is animated, in which case boneWeights is required.
        Attribute<float4> boneWeights;              ///< Array of bone weights. This field is optional. If it's set, that means that the mesh is animated, in which case boneIDs is required.
        Attribute<int32_t> materialIDs;

        AttributesStrings attributesStrings;


        bool isFrontFaceCW = false;                 ///< Indicate whether front-facing side has clockwise winding in object space.
        bool useOriginalTangentSpace = false;       ///< Indicate whether to use the original tangent space that was loaded with the mesh. By default, we will ignore it and use MikkTSpace to generate the tangent space.
        bool mergeDuplicateVertices = true;         ///< Indicate whether to merge identical vertices and adjust indices.
        uint32_t skeletonNodeId = Animatable::kInvalidNode;     ///< For skinned meshes, the node ID of the skeleton's world transform. If set to -1, the skeleton is based on the mesh's own world position (Assimp behavior pre-multiplies instance transform).

        template<typename T>
        uint32_t getAttributeIndex(const Attribute<T>& attribute, uint32_t face, uint32_t vert) const {
            switch (attribute.frequency) {
                case AttributeFrequency::Constant:
                    return 0;
                case AttributeFrequency::Uniform:
                    return face;
                case AttributeFrequency::Vertex:
                    return pIndices[face * 3 + vert];
                case AttributeFrequency::FaceVarying:
                    return face * 3 + vert;
                default:
                    should_not_get_here();
            }
            return Scene::kInvalidIndex;
        }

        const AttributesStrings& getAttributesStrings() const { return attributesStrings; }

        StringList* attributeStringList(const std::string& name, bool createMissing = false) {
            auto it = attributesStrings.find(name);
            if(it != attributesStrings.end()) return &it->second;
            
            if(createMissing) {
                attributesStrings.insert(std::make_pair(name, StringList()));
                return &attributesStrings[name];
            }
            return nullptr;
        }

        StringList* materialAttributeStrings(bool createMissing) {
            return attributeStringList("material", createMissing);
        }

        bool hasMultipleMaterials() const {
            return materialIDs.pData != nullptr;
        }

        template<typename T>
        T get(const Attribute<T>& attribute, uint32_t index) const {
            if (attribute.pData) {
                return attribute.pData[index];
            }
            return T{};
        }

        template<typename T>
        T get(const Attribute<T>& attribute, uint32_t face, uint32_t vert) const {
            if (attribute.pData) {
                return get(attribute, getAttributeIndex(attribute, face, vert));
            }
            return T{};
        }

        template<typename T>
        size_t getAttributeCount(const Attribute<T>& attribute) {
            switch (attribute.frequency) {
                case AttributeFrequency::Constant:
                    return 1;
                case AttributeFrequency::Uniform:
                    return faceCount;
                case AttributeFrequency::Vertex:
                    return vertexCount;
                case AttributeFrequency::FaceVarying:
                    return 3 * faceCount;
                default:
                    should_not_get_here();
            }
            return 0;
        }


        float3 getPosition(uint32_t face, uint32_t vert) const { return get(positions, face, vert); }
        float3 getNormal(uint32_t face, uint32_t vert) const { return get(normals, face, vert); }
        float4 getTangent(uint32_t face, uint32_t vert) const { return get(tangents, face, vert); }
        float2 getTexCrd(uint32_t face, uint32_t vert) const { return get(texCrds, face, vert); }
        float getCurveRadii(uint32_t face, uint32_t vert) const { return get(curveRadii, face, vert); }

        struct Vertex {
            float3 position;
            float3 normal;
            float4 tangent;
            float2 texCrd;
            float curveRadius;
            uint4 boneIDs;
            float4 boneWeights;
        };

        struct VertexAttributeIndices {
            uint32_t positionIdx;
            uint32_t normalIdx;
            uint32_t tangentIdx;
            uint32_t texCrdIdx;
            uint32_t curveRadiusIdx;
            uint32_t boneIDsIdx;
            uint32_t boneWeightsIdx;
        };


        Vertex getVertex(uint32_t face, uint32_t vert) const {
            Vertex v = {};
            v.position = get(positions, face, vert);
            v.normal = get(normals, face, vert);
            v.tangent = get(tangents, face, vert);
            v.texCrd = get(texCrds, face, vert);
            v.curveRadius = get(curveRadii, face, vert);
            v.boneIDs = get(boneIDs, face, vert);
            v.boneWeights = get(boneWeights, face, vert);
            return v;
        }

        Vertex getVertex(const VertexAttributeIndices& attributeIndices) {
            Vertex v = {};
            v.position = get(positions, attributeIndices.positionIdx);
            v.normal = get(normals, attributeIndices.normalIdx);
            v.tangent = get(tangents, attributeIndices.tangentIdx);
            v.texCrd = get(texCrds, attributeIndices.texCrdIdx);
            v.curveRadius = get(curveRadii, attributeIndices.curveRadiusIdx);
            v.boneIDs = get(boneIDs, attributeIndices.boneIDsIdx);
            v.boneWeights = get(boneWeights, attributeIndices.boneWeightsIdx);
            return v;
        }

        VertexAttributeIndices getAttributeIndices(uint32_t face, uint32_t vert) {
            VertexAttributeIndices v = {};
            v.positionIdx = getAttributeIndex(positions, face, vert);
            v.normalIdx = getAttributeIndex(normals, face, vert);
            v.tangentIdx = getAttributeIndex(tangents, face, vert);
            v.texCrdIdx = getAttributeIndex(texCrds, face, vert);
            v.curveRadiusIdx = getAttributeIndex(curveRadii, face, vert);
            v.boneIDsIdx = getAttributeIndex(boneIDs, face, vert);
            v.boneWeightsIdx = getAttributeIndex(boneWeights, face, vert);
            return v;
        }

        bool hasBones() const {
            return boneWeights.pData || boneIDs.pData;
        }
    };

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_SCENEBUILDER_SCENEBUILDERMESH_H_
