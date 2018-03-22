/************************************************************************************

Filename    :   VrIntegration_Types.h
Content     :   Types for VrIntegration module
Created     :   NOV 29 2017
Authors     :   Jian Zhang
Language    :   C99

Copyright   :   Copyright 2017 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrIntegration_Types_h
#define OVR_VrIntegration_Types_h

#include <stdbool.h>
#include <stdint.h>
#include "VrIntegration_Config.h"   // needed for VrIntegration_EXPORT

//-----------------------------------------------------------------
// Java
//-----------------------------------------------------------------

#if defined( ANDROID )
#include <jni.h>
#elif defined( __cplusplus )
typedef struct _JNIEnv JNIEnv;
typedef struct _JavaVM JavaVM;
typedef class _jobject * jobject;
#else
typedef const struct JNINativeInterface * JNIEnv;
typedef const struct JNIInvokeInterface * JavaVM;
typedef void * jobject;
#endif

typedef struct
{
	JavaVM *	Vm;					// Java Virtual Machine
	JNIEnv *	Env;				// Thread specific environment
	jobject		ActivityObject;		// Java activity object
} vrIntegrationJava;

OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_32_BIT( vrIntegrationJava, 12 );
OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_64_BIT( vrIntegrationJava, 24 );

//-----------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------

typedef enum
{
	VRINTEGRATION_INITIALIZE_SUCCESS			=  0,
	VRINTEGRATION_INITIALIZE_UNKNOWN_ERROR		= -1,
	VRINTEGRATION_INITIALIZE_PERMISSIONS_ERROR	= -2,
} vrIntegrationInitializeStatus;

//-----------------------------------------------------------------
// Basic Types
//-----------------------------------------------------------------

typedef enum vrIntegrationResult_
{
	vrIntegrationResult_Success						= 0,
	vrIntegrationResult_MemoryAllocationFailure	    = -1000,
	vrIntegrationResult_NotInitialized				= -1004,
	vrIntegrationResult_InvalidParameter			= -1005,
    vrIntegrationResult_InvalidOperation			= -1015,

	vrIntegrationResult_NotImplemented				= -1052,	// executed an incomplete code path - this should not be possible in public releases.
    vrIntegrationResult_DummyOperation			    = -2000,    // not a failure but not doing anything

    vrIntegrationResult_EnumSize 					= 0x7fffffff
} vrIntegrationResult;

typedef struct
{
	int					ProductVersion;
	int					MajorVersion;
	int					MinorVersion;
	int					PatchVersion;
	vrIntegrationJava	Java;
} vrIntegrationInitParms;
OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_32_BIT( vrIntegrationInitParms, 28 );
OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_64_BIT( vrIntegrationInitParms, 40 );


typedef enum
{
    VRINTEGRATION_PERF_DEADLINE_NORMAL = 0,
    VRINTEGRATION_PERF_DEADLINE_HARD = 1,
    VRINTEGRATION_PERF_DEADLINE_SOFT = 2,
} vrIntegrationThreadPerformanceState;

typedef enum
{
    VRINTEGRATION_REQUIRE_LEGACY_CORE_AFFINITY = 0, // specificlly for Unity app which set core affinity wrong.
} vrIntegrationState;


#endif	// OVR_VrIntegration_Types_h
