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

ScopeBase::ScopeBase(SharedPtr pParent): mpParent(pParent) { }

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
	auto pObject = Object::create(shared_from_this());
	if (pObject) {
		mChildren.push_back(pObject);
		mObjects.push_back(pObject);
		return mObjects.back();
	}
	return nullptr;
}

std::shared_ptr<Plane> Global::addPlane() {
	auto pPlane = Plane::create(shared_from_this());
	if (pPlane) {
		mChildren.push_back(pPlane);
		mPlanes.push_back(pPlane);
		return mPlanes.back();
	}
	return nullptr;
}

std::shared_ptr<Light> Global::addLight() {
	auto pLight = Light::create(shared_from_this());
	if (pLight) {
		mChildren.push_back(pLight);
		mLights.push_back(pLight);
		return mLights.back();
	}
	return nullptr;
}

std::shared_ptr<Segment> Global::addSegment() {
	auto pSegment = Segment::create(shared_from_this());
	if (pSegment) {
		mChildren.push_back(pSegment);
		mSegments.push_back(pSegment);
		return mSegments.back();
	}
	return nullptr;
}

std::shared_ptr<Geo> Global::addGeo() {
	auto pGeo = Geo::create(shared_from_this());
	if (pGeo) {
		mChildren.push_back(pGeo);
		mGeos.push_back(pGeo);
		return mGeos.back();
	}
	return nullptr;
}

/* Geo */

ika::bgeo::Bgeo::SharedPtr Geo::bgeo() { 
	if(!mpBgeo)
		mpBgeo = ika::bgeo::Bgeo::create();

	return mpBgeo;
}

Geo::SharedPtr Geo::create(ScopeBase::SharedPtr pParent) {
	auto pSegment = Geo::SharedPtr(new Geo(pParent));
	return std::move(pSegment);
}

void Geo::setDetailFilename(const std::string& filename) {
	mFileName = filename;
	if (filename == "stdin") {
		mIsInline = true;
	}
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
	
	auto pProp = pLight->getProperty(Style::LIGHT, "shader");
	if(!pProp) return nullptr;
	
	auto pSubContainer = pProp->createSubContainer();
	if(!pSubContainer) return nullptr;

	pSubContainer->declareProperty(Style::LIGHT, Type::VECTOR3, "lightcolor", lsd::Vector3{1.0, 1.0, 1.0}, Property::Owner::SYS);
	pSubContainer->declareProperty(Style::LIGHT, Type::STRING, "type", std::string("point"), Property::Owner::SYS);

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

}  // namespace scope

}  // namespace lsd

}  // namespace lava
