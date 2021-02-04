/*
 *  Copyright 2018 Laika, LLC. Authored by Peter Stuart
 *
 *  Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
 *  http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
 *  http://opensource.org/licenses/MIT>, at your option. This file may not be
 *  copied, modified, or distributed except according to those terms.
 */

#include "Run.h"

#include <cassert>
#include <iostream>

#include "PrimitiveFactory.h"
#include "parser/Run.h"

namespace ika {
namespace bgeo {

RTTI_DEFINE(Run, Primitive, PrimType::RunPrimType)

Run::Run(const Bgeo& bgeo, const parser::Run& run): m_bgeo(bgeo), m_run(run) { 
	std::cout << "Run::Run\n";
}

Bgeo::PrimitivePtr Run::getTemplatePrimitive() const {
    assert(m_run.runPrimitive);
    //return factory::create(m_bgeo, *m_run.runPrimitive);
    return nullptr;
}

} // namespace bgeo
} // namespace ika
