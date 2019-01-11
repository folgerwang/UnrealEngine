// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


enum class EDisplayClusterSwapSyncPolicy
{
	None = 0,     // no swap sync (V-sync off)
	SoftSwapSync, // software swap synchronization over network
	NvSwapSync    // NVIDIA hardware swap synchronization (nv_swap_lock)
};

class FDisplayClusterViewportArea;
class IDisplayClusterProjectionScreenDataProvider;


/**
 * Stereo device interface
 */
class IDisplayClusterStereoRendering
{
public:
	virtual ~IDisplayClusterStereoRendering()
	{ }

	/**
	* Adds viewport to the rendering pipeline (sub-region on a main viewport)
	*
	* @param ViewportId   - viewport ID from a config file
	* @param DataProvider - interface to an object that provides with projection screen data
	*/
	virtual void AddViewport(const FString& ViewportId, IDisplayClusterProjectionScreenDataProvider* DataProvider) = 0;
	
	/**
	* Removes specified viewport from the rendering pipeline
	*
	* @param ViewportId   - viewport ID to delete
	*/
	virtual void RemoveViewport(const FString& ViewportId) = 0;
	
	/**
	* Removes all viewports from the rendering pipeline
	*
	*/
	virtual void RemoveAllViewports() = 0;

	/**
	* FOV based configuration of projection screen (standalone mode only)
	*
	* @param FOV - field of view
	*/
	virtual void SetDesktopStereoParams(float FOV) = 0;

	/**
	* Custom configuration of projection screen (standalone mode only)
	*
	* @param screenSize - width and height of your monitor's screen (meters)
	* @param screenRes  - horizontal and vertical resolution of target monitor (pixels i.e. 1920, 1080)
	* @param screenDist - distance between the head and monitor (meters)
	*/
	virtual void SetDesktopStereoParams(const FVector2D& screenSize, const FIntPoint& screenRes, float screenDist) = 0;

	/**
	* Configuration of interpupillary (interocular) distance
	*
	* @param dist - distance between eyes (meters, i.e. 0.064).
	*/
	virtual void  SetInterpupillaryDistance(float dist) = 0;

	/**
	* Returns currently used interpupillary distance.
	*
	* @return - distance between eyes (meters)
	*/
	virtual float GetInterpupillaryDistance() const = 0;

	/**
	* Configure eyes swap state
	*
	* @param swap - new eyes swap state. False - normal eyes left|right, true - swapped eyes right|left
	*/
	virtual void SetEyesSwap(bool swap) = 0;

	/**
	* Returns currently used eyes swap
	*
	* @return - eyes swap state. False - normal eyes left|right, true - swapped eyes right|left
	*/
	virtual bool GetEyesSwap() const = 0;

	/**
	* Toggles eyes swap state
	*
	* @return - new eyes swap state. False - normal eyes left|right, true - swapped eyes right|left
	*/
	virtual bool ToggleEyesSwap() = 0;

	/**
	* Set swap synchronization policy
	*
	* @param policy - is swap sync enabled
	*/
	virtual void SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy) = 0;

	/**
	* Returns current swap synchronization policy
	*
	* @return - current synchronization policy
	*/
	virtual EDisplayClusterSwapSyncPolicy GetSwapSyncPolicy() const = 0;

	/**
	* Get camera frustum culling
	*
	* @param NearDistance - near culling plane distance
	* @param FarDistance - far culling plane distance
	*/
	virtual void GetCullingDistance(float& NearDistance, float& FarDistance) const = 0;

	/**
	* Set camera frustum culling
	*
	* @param NearDistance - near culling plane distance
	* @param FarDistance - far culling plane distance
	*/
	virtual void SetCullingDistance(float NearDistance, float FarDistance) = 0;
};
