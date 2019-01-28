/**********************************************************************************
\file      OVR_Avatar_Defs.h
\brief     OVR Avatar SDK public header file
\copyright (c) Facebook Technologies, LLC and its affiliates.  All rights reserved.
***********************************************************************************/
#ifndef OVR_Avatar_Defs_h
#define OVR_Avatar_Defs_h

//-----------------------------------------------------------------------------------
// ***** OVRP_CDECL
//
/// LibOVR calling convention for 32-bit Windows builds.
//
#if !defined(OVRP_CDECL)
	#if defined(_WIN32)
		#define OVRP_CDECL __cdecl
	#else
		#define OVRP_CDECL
	#endif
#endif

//-----------------------------------------------------------------------------------
// ***** OVRP_EXTERN_C
//
/// Defined as extern "C" when built from C++ code.
//
#if !defined(OVRP_EXTERN_C)
	#ifdef __cplusplus
		#define OVRP_EXTERN_C extern "C"
	#else
		#define OVRP_EXTERN_C
	#endif
#endif

//-----------------------------------------------------------------------------------
// ***** OVRN_EXPORT
//
// OVRN_EXPORT  - Functions that externally visible from a shared library. Corresponds to Microsoft __dllexport.

#if !defined(OVRN_EXPORT)
	#if defined(OVR_AVATAR_DLL)
		#if defined(_WIN32)
			#define OVRN_EXPORT(rval) OVRP_EXTERN_C __declspec(dllexport) rval OVRP_CDECL
		#else
			#define OVRN_EXPORT(rval) OVRP_EXTERN_C __attribute__((visibility("default"))) rval OVRP_CDECL /* Requires GCC 4.0+ */
		#endif
	#elif defined(OVRP_STATIC_BUILD)
		#define OVRN_EXPORT(rval)     OVRP_EXTERN_C rval OVRP_CDECL
	#else
		#if defined(_WIN32)
			#define OVRN_EXPORT(rval) OVRP_EXTERN_C __declspec(dllimport) rval OVRP_CDECL
		#else
			#define OVRN_EXPORT(rval) OVRP_EXTERN_C rval OVRP_CDECL
		#endif
	#endif
#endif

// Master version numbers
#define AVATAR_PRODUCT_VERSION 1 // Product version doesn't participate in semantic versioning.
#define AVATAR_MAJOR_VERSION   1 // If you change these values then you need to also make sure to change LibOVRPlatform.props in parallel.
#define AVATAR_MINOR_VERSION   31
#define AVATAR_PATCH_VERSION   0
#define AVATAR_BUILD_NUMBER    0
#define AVATAR_DRIVER_VERSION  0

/// Describes the various results possible when attempting to initialize the SDK.
/// Anything other than ovrAvatarInitialize_Success should generally be considered a
/// fatal error with respect to using the SDK, as the SDK is not guaranteed to be
/// legitimate or work correctly.
typedef enum ovrAvatarInitializeResult_ {
	ovrAvatarInitialize_Success = 0,
	ovrAvatarInitialize_Uninitialized = -1,
	ovrAvatarInitialize_PreLoaded = -2,
	ovrAvatarInitialize_FileInvalid = -3,
	ovrAvatarInitialize_SignatureInvalid = -4,
	ovrAvatarInitialize_UnableToVerify = -5,
	ovrAvatarInitialize_VersionMismatch = -6,
} ovrAvatarInitializeResult;


#endif // OVR_Avatar_Defs_h
