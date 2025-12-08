// debug-texture.h
#pragma once
#include "debug-base.h"

namespace gfx
{
using namespace Slang;

namespace debug
{

class DebugTextureResource : public DebugObject<ITextureResource>
{
public:
    SLANG_COM_OBJECT_IUNKNOWN_ALL;

public:

	virtual bool isSparse() const override;

	virtual uint32_t sparseDataBindsCount() const override;

	virtual const IVirtualTexturePageResource::Extent3D& sparseDataPageRes() const override;

	virtual SLANG_NO_THROW GfxCount getArraySize() const override;

    ITextureResource* getInterface(const Slang::Guid& guid);
    virtual SLANG_NO_THROW Type SLANG_MCALL getType() override;
    virtual SLANG_NO_THROW Desc* SLANG_MCALL getDesc() override;
    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeResourceHandle(InteropHandle* outHandle) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL getSharedHandle(InteropHandle* outHandle) override;

    virtual SLANG_NO_THROW Result SLANG_MCALL setDebugName(const char* pName) override;
    virtual SLANG_NO_THROW const char* SLANG_MCALL getDebugName() override;
};

} // namespace debug
} // namespace gfx
