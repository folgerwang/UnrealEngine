/************************************************************************************

Filename    :   VrIntegration.h
Content     :   VrIntegration Code for engine integration, use for features which need be OTA-able but not a good fit for vrapi
Created     :   November, 2017
Authors     :   Jian Zhang

Copyright   :   Copyright 2017 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrIntegration_h
#define OVR_VrIntegration_h

#include "VrIntegration_Config.h"
#include "VrIntegration_Version.h"
#include "VrIntegration_Types.h"

#if defined( __cplusplus )
extern "C" {
#endif

/// Initializes the VrIntegration API for application use.
/// This is typically called from onCreate() or shortly thereafter.
/// Can be called from any thread.
/// Returns a non-zero value from ovrInitializeStatus on error.
OVR_VRINTEGRATION_EXPORT vrIntegrationInitializeStatus vrintegration_Initialize( const vrIntegrationInitParms * initParms );

/// Check if VrIntegration module has been initilized
OVR_VRINTEGRATION_EXPORT bool vrintegration_HasInitialized( );

/// This function will put tid on a proper performance mode by using the context and knowledge of each specific hardware
/// Including but not limited to CoreAffinities, we can use different policy for different Oculus hardware
/// This function can be called any time from any thread once the VrIntegration is initialized.
OVR_VRINTEGRATION_EXPORT vrIntegrationResult vrintegration_SetThreadPerformance( int tid, vrIntegrationThreadPerformanceState perf );

//-----------------------------------------------------------------
// VrIntegration States
//-----------------------------------------------------------------
OVR_VRINTEGRATION_EXPORT bool vrintegration_GetState( vrIntegrationState state );
OVR_VRINTEGRATION_EXPORT void vrintegration_SetState( vrIntegrationState state );
OVR_VRINTEGRATION_EXPORT void vrintegration_ClearState( vrIntegrationState state );

/// So far, just for fix up thread affinity
/// may do other auto scheduling optimization later on
OVR_VRINTEGRATION_EXPORT vrIntegrationResult vrintegration_AutoThreadScheduling(unsigned int bigCoreMaskFromEngine, unsigned int* threads, vrIntegrationThreadPerformanceState* threadPerfFlags, int threadsCount);

/// Shuts down the VrIntegration API on application exit before vrapi_Shutdown
/// This is typically called from onDestroy() or shortly thereafter.
/// Can be called from any thread.
OVR_VRINTEGRATION_EXPORT void vrintegration_Shutdown();

#if defined( __cplusplus )
}	// extern "C"
#endif

#endif	// OVR_VrIntegration_h
