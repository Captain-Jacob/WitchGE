#ifndef __khrplatform_h_
#define __khrplatform_h_

#if defined(_WIN32) && !defined(__SCITECH_SNAP__)
#   define KHRONOS_APICALL __declspec(dllimport)
#   define KHRONOS_APIENTRY __stdcall
#else
#   define KHRONOS_APICALL
#   define KHRONOS_APIENTRY
#endif

#ifndef APIENTRY
#   define APIENTRY KHRONOS_APIENTRY
#endif

#ifndef APIENTRYP
#   define APIENTRYP APIENTRY *
#endif

typedef signed char        khronos_int8_t;
typedef unsigned char      khronos_uint8_t;
typedef signed short int   khronos_int16_t;
typedef unsigned short int khronos_uint16_t;
typedef int                khronos_int32_t;
typedef unsigned int       khronos_uint32_t;
typedef long long          khronos_int64_t;
typedef unsigned long long khronos_uint64_t;
typedef float              khronos_float_t;

#endif
