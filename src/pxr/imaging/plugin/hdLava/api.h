#ifndef HDLAVA_API_H_
#define HDLAVA_API_H_

#include "pxr/base/arch/export.h"

#if defined(PXR_STATIC)
#   define HDLAVA_API
#   define HDLAVA_API_TEMPLATE_CLASS(...)
#   define HDLAVA_API_TEMPLATE_STRUCT(...)
#   define HDLAVA_LOCAL
#else
#   if defined(HDLAVA_EXPORTS)
#       define HDLAVA_API ARCH_EXPORT
#       define HDLAVA_API_TEMPLATE_CLASS(...) ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#       define HDLAVA_API_TEMPLATE_STRUCT(...) ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#   else
#       define HDLAVA_API ARCH_IMPORT
#       define HDLAVA_API_TEMPLATE_CLASS(...) ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#       define HDLAVA_API_TEMPLATE_STRUCT(...) ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#   endif
#   define HDLAVA_LOCAL ARCH_HIDDEN
#endif


#endif // HDLAVA_API_H_
