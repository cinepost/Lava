#ifndef LAVA_LAVA_DLL_H_
#define LAVA_LAVA_DLL_H_

// Define DLL export/import
#ifdef _MSC_VER
#define lavaexport __declspec(dllexport)
#define lavaimport __declspec(dllimport)
#define LAVA_API_EXPORT __declspec(dllexport)
#define LAVA_API_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)  // _MSC_VER
#define lavaexport __attribute__ ((visibility ("default")))
#define lavaimport  // extern
#define LAVA_API_EXPORT __attribute__ ((visibility ("default")))
#define LAVA_API_IMPORT //extern
#endif  // _MSC_VER

#ifdef LAVA_DLL
#define LAVA_API LAVA_API_EXPORT
#else   // BUILDING_SHARED_DLL
#define LAVA_API LAVA_API_IMPORT
#endif  // BUILDING_SHARED_DLL

#endif  // LAVA_LAVA_DLL_H_
