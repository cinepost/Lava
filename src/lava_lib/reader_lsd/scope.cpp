#include "scope.h"
#include "grammar_lsd.h"

#include "lava_utils_lib/logging.h"

namespace lava {

namespace lsd {

namespace scope {

using namespace lsd;

using Style = ast::Style;
using Type = ast::Type;

static inline std::ostream& indentStream(std::ostream& os, uint indent = 0) {
	for(uint i = 0; i < indent; i++) {
        os << " ";
    }
    return os;
}

const void ScopeBase::printSummary(std::ostream& os, uint indent) const {
	indentStream(os, indent) << this->type() << " {\n";
	PropertiesContainer::printSummary(os, indent);

	for( auto const& child: mChildren) {
		child->printSummary(os, indent + 4); 
	}
	indentStream(os, indent) << "}\n";
}

ScopeBase::ScopeBase(SharedPtr pParent): PropertiesContainer::PropertiesContainer(pParent) { }

EmbeddedData& ScopeBase::getEmbeddedData(const std::string& name) {
	auto it = mEmbeddedDataMap.find(name);
	if( it == mEmbeddedDataMap.end()) {
		mEmbeddedDataMap.insert({name, EmbeddedData()});
	}
	
	return mEmbeddedDataMap[name];
}

/* Transformable */

Transformable::Transformable(ScopeBase::SharedPtr pParent):ScopeBase(pParent) {
	mTransformList.clear();
	mTransformList.push_back(glm::mat4( 1.0 ));
}

void Transformable::setTransform(const lsd::Matrix4& mat) {
	if(mTransformList.size() > 1) {
		LLOG_WRN << "Setting transfrorm on transfrorm list that is larger than 1 !!!";
		return;
	}

	mTransformList[0] = {
		mat[0], mat[1], mat[2], mat[3],
		mat[4], mat[5], mat[6], mat[7],
		mat[8], mat[9], mat[10], mat[11],
		mat[12], mat[13], mat[14], mat[15]
	};
}

void Transformable::addTransform(const lsd::Matrix4& mat) {
	mTransformList.push_back({
		mat[0], mat[1], mat[2], mat[3],
		mat[4], mat[5], mat[6], mat[7],
		mat[8], mat[9], mat[10], mat[11],
		mat[12], mat[13], mat[14], mat[15]
	});
}

/* Global */

Global::SharedPtr Global::create() {
	auto pGlobal = Global::SharedPtr(new Global());
	if(!pGlobal->declareProperty(Style::RENDERER, Type::STRING, "rendertype", std::string("unknown"), Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::RENDERER, Type::STRING, "renderlabel", std::string("unnamed"), Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::RENDERER, Type::STRING, "udim_file_mask", std::string("<UDIM>"), Property::Owner::SYS)) return nullptr;
	
	if(!pGlobal->declareProperty(Style::IMAGE, Type::BOOL, "tiling", bool(false), Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::IMAGE, Type::INT2, "tilesize", lsd::Int2{256, 256}, Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::IMAGE, Type::INT,  "sampleupdate", 0, Property::Owner::SYS)) return nullptr;

	if(!pGlobal->declareProperty(Style::IMAGE, Type::INT2, "resolution", lsd::Int2{1280, 720}, Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::IMAGE, Type::INT, "samples", 16, Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::IMAGE, Type::FLOAT, "pixelaspect", 1.0, Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::IMAGE, Type::VECTOR4, "crop", lsd::Vector4{0.0, 1.0, 0.0, 1.0}, Property::Owner::SYS)) return nullptr;
	
	if(!pGlobal->declareProperty(Style::CAMERA, Type::VECTOR2, "clip", lsd::Vector2{0.01, 1000.0}, Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::CAMERA, Type::STRING, "projection", std::string("perspective"), Property::Owner::SYS)) return nullptr;
	
	if(!pGlobal->declareProperty(Style::OBJECT, Type::FLOAT, "velocityscale", 1.0 , Property::Owner::SYS)) return nullptr;
	if(!pGlobal->declareProperty(Style::OBJECT, Type::INT, "xformsamples", 1 , Property::Owner::SYS)) return nullptr;

	return std::move(pGlobal);
}

// TODO: get rid of mPlanes, mObjects ... etc. use std::move
std::shared_ptr<Object> Global::addObject(){
	auto pObject = Object::create(std::dynamic_pointer_cast<ScopeBase>(shared_from_this()));
	if (pObject) {
		mChildren.push_back(pObject);
		mObjects.push_back(pObject);
		return mObjects.back();
	}
	return nullptr;
}

std::shared_ptr<Plane> Global::addPlane() {
	auto pPlane = Plane::create(std::dynamic_pointer_cast<ScopeBase>(shared_from_this()));
	if (pPlane) {
		mChildren.push_back(pPlane);
		mPlanes.push_back(pPlane);
		return mPlanes.back();
	}
	return nullptr;
}

std::shared_ptr<Light> Global::addLight() {
	auto pLight = Light::create(std::dynamic_pointer_cast<ScopeBase>(shared_from_this()));
	if (pLight) {
		mChildren.push_back(pLight);
		mLights.push_back(pLight);
		return mLights.back();
	}
	return nullptr;
}

std::shared_ptr<Segment> Global::addSegment() {
	auto pSegment = Segment::create(std::dynamic_pointer_cast<ScopeBase>(shared_from_this()));
	if (pSegment) {
		mChildren.push_back(pSegment);
		mSegments.push_back(pSegment);
		return mSegments.back();
	}
	return nullptr;
}

std::shared_ptr<Geo> Global::addGeo() {
	auto pGeo = Geo::create(std::dynamic_pointer_cast<ScopeBase>(shared_from_this()));
	if (pGeo) {
		mChildren.push_back(pGeo);
		mGeos.push_back(pGeo);
		return mGeos.back();
	}
	return nullptr;
}

std::shared_ptr<Material> Global::addMaterial() {
	auto pMat = Material::create(std::dynamic_pointer_cast<ScopeBase>(shared_from_this()));
	if (pMat) {
		mChildren.push_back(pMat);
		mMaterials.push_back(pMat);
		return mMaterials.back();
	}
	return nullptr;
}


/* Geo */

ika::bgeo::Bgeo::SharedPtr Geo::bgeo() { 
	if(!mpBgeo) {
		mpBgeo = ika::bgeo::Bgeo::create();
	}

	return mpBgeo;
}

Geo::SharedPtr Geo::create(ScopeBase::SharedPtr pParent) {
	auto pSegment = Geo::SharedPtr(new Geo(pParent));
	return std::move(pSegment);
}

void Geo::setDetailFilePath(const fs::path& path) {
	mFilePath = path;
	if (path.string() == "stdin") {
		mIsInline = true;
	}
}

void Geo::setTemporary(bool t) {
	if (mIsInline) {
		mIsTemporary = false;
		return;
	} else {
		mIsTemporary = t;
	}
}

void Geo::cleanUpGeometry() {
	if (mpBgeo) mpBgeo.reset();
}

/* Object */

Object::SharedPtr Object::create(ScopeBase::SharedPtr pParent) {
	auto pObject = Object::SharedPtr(new Object(pParent));
	if(!pObject->declareProperty(Style::OBJECT, Type::STRING, "name", std::string(), Property::Owner::SYS)) return nullptr;

	if(!pObject->declareProperty(Style::OBJECT, Type::STRING, "surface", std::string(), Property::Owner::SYS)) return nullptr;	

	auto pProp = pObject->getProperty(Style::OBJECT, "surface");
	if(!pProp) return nullptr;
	
	auto pSubContainer = pProp->createSubContainer();
	if(!pSubContainer) return nullptr;

	pSubContainer->declareProperty(Style::OBJECT, Type::VECTOR3, "basecolor", lsd::Vector3{1.0, 1.0, 1.0}, Property::Owner::SYS);
	pSubContainer->declareProperty(Style::OBJECT, Type::BOOL, 	 "basecolor_useTexture", bool(false), Property::Owner::SYS);
	pSubContainer->declareProperty(Style::OBJECT, Type::STRING,  "basecolor_texture", std::string(), Property::Owner::SYS);

	pSubContainer->declareProperty(Style::OBJECT, Type::BOOL, 	 "baseBumpAndNormal_enable", bool(false), Property::Owner::SYS);
	pSubContainer->declareProperty(Style::OBJECT, Type::FLOAT,   "baseNormal_scale", 0.5, Property::Owner::SYS);
	pSubContainer->declareProperty(Style::OBJECT, Type::STRING,  "baseNormal_texture", std::string(), Property::Owner::SYS);

	pSubContainer->declareProperty(Style::OBJECT, Type::BOOL, 	 "metallic_useTexture", bool(false), Property::Owner::SYS);
	pSubContainer->declareProperty(Style::OBJECT, Type::STRING,  "metallic_texture", std::string(), Property::Owner::SYS);

	pSubContainer->declareProperty(Style::OBJECT, Type::BOOL, 	 "rough_useTexture", bool(false), Property::Owner::SYS);
	pSubContainer->declareProperty(Style::OBJECT, Type::STRING,  "rough_texture", std::string(), Property::Owner::SYS);
	
	pSubContainer->declareProperty(Style::OBJECT, Type::FLOAT, "rough", 0.3, Property::Owner::SYS);
	pSubContainer->declareProperty(Style::OBJECT, Type::FLOAT, "ior", 1.5, Property::Owner::SYS);
	pSubContainer->declareProperty(Style::OBJECT, Type::FLOAT, "metallic", 0.0, Property::Owner::SYS);

	return std::move(pObject);
}

/* Plane */

Plane::SharedPtr Plane::create(ScopeBase::SharedPtr pParent) {
	auto pPlane = Plane::SharedPtr(new Plane(pParent));
	if(!pPlane->declareProperty(Style::PLANE, Type::STRING, "variable", std::string("Cf+Af"), Property::Owner::SYS)) return nullptr;
	if(!pPlane->declareProperty(Style::PLANE, Type::STRING, "vextype", std::string("vector4"), Property::Owner::SYS)) return nullptr;
	if(!pPlane->declareProperty(Style::PLANE, Type::STRING, "channel", std::string("C"), Property::Owner::SYS)) return nullptr;

	return std::move(pPlane);
}

/* Light */

Light::SharedPtr Light::create(ScopeBase::SharedPtr pParent) {
	auto pLight = Light::SharedPtr(new Light(pParent));
	if(!pLight->declareProperty(Style::OBJECT, Type::STRING, "name", std::string(), Property::Owner::SYS)) return nullptr;
	if(!pLight->declareProperty(Style::LIGHT, Type::STRING, "projection", std::string("perspective"), Property::Owner::SYS)) return nullptr;
	if(!pLight->declareProperty(Style::LIGHT, Type::VECTOR2, "zoom", lsd::Vector2{0.01, 1000.0}, Property::Owner::SYS)) return nullptr;

	if(!pLight->declareProperty(Style::LIGHT, Type::STRING, "shader", std::string(), Property::Owner::SYS)) return nullptr;	
	if(!pLight->declareProperty(Style::LIGHT, Type::STRING, "shadow", std::string(), Property::Owner::SYS)) return nullptr;	
	
	auto pShaderProp = pLight->getProperty(Style::LIGHT, "shader");
	if(!pShaderProp) return nullptr;

	auto pShadowProp = pLight->getProperty(Style::LIGHT, "shadow");
	if(!pShadowProp) return nullptr;
	
	auto pShaderSubContainer = pShaderProp->createSubContainer();
	if(!pShaderSubContainer) return nullptr;

	auto pShadowSubContainer = pShadowProp->createSubContainer();
	if(!pShadowSubContainer) return nullptr;

	pShaderSubContainer->declareProperty(Style::LIGHT, Type::VECTOR3, "lightcolor", lsd::Vector3{1.0, 1.0, 1.0}, Property::Owner::SYS);
	pShaderSubContainer->declareProperty(Style::LIGHT, Type::STRING, "type", std::string("point"), Property::Owner::SYS);

	pShadowSubContainer->declareProperty(Style::LIGHT, Type::STRING, "shadowtype", std::string(""), Property::Owner::SYS);
	pShadowSubContainer->declareProperty(Style::LIGHT, Type::VECTOR3, "shadow_color", lsd::Vector3{0.0, 0.0, 0.0}, Property::Owner::SYS);

	return std::move(pLight);
}

/* Segment */

Segment::SharedPtr Segment::create(ScopeBase::SharedPtr pParent) {
	auto pSegment = Segment::SharedPtr(new Segment(pParent));
	if(!pSegment->declareProperty(Style::CAMERA, Type::FLOAT, "orthowidth", 3.427819, Property::Owner::SYS)) return nullptr;
	if(!pSegment->declareProperty(Style::CAMERA, Type::FLOAT, "zoom", 1.0, Property::Owner::SYS)) return nullptr;
	if(!pSegment->declareProperty(Style::IMAGE, Type::VECTOR4, "window", lsd::Vector4{0.0, 1.0, 0.0, 1.0}, Property::Owner::SYS)) return nullptr;

	return std::move(pSegment);
}

/* Material */

Material::SharedPtr Material::create(ScopeBase::SharedPtr pParent) {
	auto pMat = Material::SharedPtr(new Material(pParent));

	if(!pMat->declareProperty(Style::OBJECT, Type::STRING, "material_name", std::string(""), Property::Owner::SYS)) return nullptr;
	
	return std::move(pMat);
}

bool Material::insertNode(const NodeUUID& uuid, Falcor::MxNode::SharedPtr pNode) {
	if ( mNodesMap.find(uuid) == mNodesMap.end() ) {
  		// node not exist
  		mNodesMap.insert({uuid, pNode});
		return true;
	} 
	// node already exist
	return false;
}

Falcor::MxNode::SharedPtr Material::node(const NodeUUID& uuid) {
	if ( mNodesMap.find(uuid) == mNodesMap.end() ) {
		// node not found
		return nullptr;
	}

	return mNodesMap[uuid];
}

/* Node */

Node::SharedPtr Node::create(ScopeBase::SharedPtr pParent) {
	auto pNode = Node::SharedPtr(new Node(pParent));

	if(!pNode->declareProperty(Style::OBJECT, Type::BOOL, "is_subnet", bool(false), Property::Owner::SYS)) return nullptr;
	if(!pNode->declareProperty(Style::OBJECT, Type::STRING, "node_namespace", std::string(""), Property::Owner::SYS)) return nullptr;
	if(!pNode->declareProperty(Style::OBJECT, Type::STRING, "node_name", std::string(""), Property::Owner::SYS)) return nullptr;
	if(!pNode->declareProperty(Style::OBJECT, Type::STRING, "node_type", std::string(""), Property::Owner::SYS)) return nullptr;
	if(!pNode->declareProperty(Style::OBJECT, Type::STRING, "node_path", std::string(""), Property::Owner::SYS)) return nullptr;

	return std::move(pNode);
}

Node::SharedPtr Node::addChildNode() {
	auto pNode = Node::create(std::dynamic_pointer_cast<ScopeBase>(shared_from_this()));
	if (pNode) {
		mChildren.push_back(pNode);
		mChildNodes.push_back(pNode);
		return mChildNodes.back();
	}
	return nullptr;
}

void Node::addChildEdge(const std::string& src_node_uuid, const std::string& src_node_output_socket, const std::string& dst_node_uuid, const std::string& dst_node_input_socket) {
	EdgeInfo edge = {};
	edge.src_node_uuid = src_node_uuid;
	edge.src_node_output_socket = src_node_output_socket;
	edge.dst_node_uuid = dst_node_uuid;
	edge.dst_node_input_socket = dst_node_input_socket;
	mChildEdges.push_back(edge);
}

void Node::addDataSocketTemplate(const std::string& name, Falcor::MxSocketDataType dataType, Falcor::MxSocketDirection direction) {
	DataSocketTemplate tmpl = {};
	tmpl.name = name;
	tmpl.dataType = dataType;
	tmpl.direction = direction;
	mSocketTemplates.push_back(tmpl);
}

const void Node::printSummary(std::ostream& os, uint indent) const {
	indentStream(os, indent) << this->type() << " {\n";
	PropertiesContainer::printSummary(os, indent);

	for( auto const& child: mChildren) {
		child->printSummary(os, indent + 4); 
	}

	for( auto const& edge: mChildEdges) {
		for(uint i = 0; i < indent; i++) {
        	os << " ";
    	}
    	os << "Edge: " << edge.src_node_uuid << " " << edge.src_node_output_socket << " " << edge.dst_node_uuid << " " << edge.dst_node_input_socket << std::endl;
	}
	indentStream(os, indent) << "}\n";
}

}  // namespace scope

}  // namespace lsd

}  // namespace lava
