/*
 *  Copyright 2018 Laika, LLC. Authored by Peter Stuart
 *
 *  Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
 *  http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
 *  http://opensource.org/licenses/MIT>, at your option. This file may not be
 *  copied, modified, or distributed except according to those terms.
 */

#ifndef BGEO_RTTI_H
#define BGEO_RTTI_H

#include "PrimType.h"

// low-tech rtti scheme

#define TYPE_TYPE const char*

#define RTTI_DECLARE(class_, base) \
    public: \
        static TYPE_TYPE Type; \
\
    virtual TYPE_TYPE getStrType() const override; \
\
    virtual PrimType  getType() const override; \
\
    template <typename T> \
    bool isType() const \
    { \
        return isType(T::Type); \
    } \
\
    template <typename T> \
    const T* cast() const \
    { \
        if (isType<T>()) \
        { \
            return reinterpret_cast<const T*>(this); \
        } \
\
        return nullptr; \
    } \
\
    protected: \
    /*virtual*/ bool isType(TYPE_TYPE type) const override\
    { \
        return (type == Type) || base::isType(type); \
    } \
\
    private:

#define RTTI_DECLARE_BASE(class_) \
    public: \
        static TYPE_TYPE Type; \
    \
        virtual TYPE_TYPE getStrType() const; \
    \
        virtual PrimType  getType() const; \
    \
        template <typename T> \
        bool isType() const \
        { \
            return isType(T::Type); \
        } \
    \
        template <typename T> \
        const T* cast() const \
        { \
            if (isType<T>()) \
            { \
                return reinterpret_cast<const T*>(this); \
            } \
    \
            return nullptr; \
        } \
\
    protected: \
        virtual bool isType(TYPE_TYPE type) const \
        { \
            return (type == Type); \
        } \
\
    private:


#define RTTI_DEFINE(class_, base, type_) \
    /*static*/ TYPE_TYPE class_::Type = #class_; \
    /*virtual*/ TYPE_TYPE class_::getStrType() const \
    { \
        return Type; \
    } \
    /*virtual*/ PrimType class_::getType() const \
    { \
        return type_; \
    }

#define RTTI_DEFINE_BASE(class_) \
    /*static*/ TYPE_TYPE class_::Type = #class_; \
    /*virtual*/ TYPE_TYPE class_::getStrType() const \
    { \
        return Type; \
    } \
    /*virtual*/ PrimType class_::getType() const \
    { \
        return PrimType::UnknownPrimType; \
    }

#endif // BGEO_RTTI_H
