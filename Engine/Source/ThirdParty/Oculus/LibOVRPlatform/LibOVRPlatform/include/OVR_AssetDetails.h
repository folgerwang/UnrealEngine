// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_ASSETDETAILS_H
#define OVR_ASSETDETAILS_H

#include "OVR_Platform_Defs.h"
#include "OVR_Types.h"

typedef struct ovrAssetDetails *ovrAssetDetailsHandle;

/// ID of the asset file
OVRP_PUBLIC_FUNCTION(ovrID) ovr_AssetDetails_GetAssetId(const ovrAssetDetailsHandle obj);

/// One of 'installed', 'available', or 'in-progress'
OVRP_PUBLIC_FUNCTION(const char *) ovr_AssetDetails_GetDownloadStatus(const ovrAssetDetailsHandle obj);

/// File path of the asset file
OVRP_PUBLIC_FUNCTION(const char *) ovr_AssetDetails_GetFilepath(const ovrAssetDetailsHandle obj);

/// One of 'free', 'entitled', or 'not-entitled'
OVRP_PUBLIC_FUNCTION(const char *) ovr_AssetDetails_GetIapStatus(const ovrAssetDetailsHandle obj);


#endif
