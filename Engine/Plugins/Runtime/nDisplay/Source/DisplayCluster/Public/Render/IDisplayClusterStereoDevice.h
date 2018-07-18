// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


enum class EDisplayClusterSwapSyncPolicy
{
	None = 0,     // no swap sync (V-sync off)
	SoftSwapSync, // software swap synchronization over network
	NvSwapSync    // NVIDIA hardware swap synchronization (nv_swap_lock)
};


/**
 * Stereo device interface
 */
struct IDisplayClusterStereoDevice
{
	virtual ~IDisplayClusterStereoDevice()
	{ }

	/**
	* Configuration of viewport render area (whore viewport is rendered by default)
	*
	* @param pos    - left up corner offset in viewport (pixels)
	* @param size   - width and height of render rectangle (pixels)
	*/
	virtual void SetViewportArea(const FIntPoint& pos, const FIntPoint& size) = 0;

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
	* Configures output flipping
	*
	* @param flipH - enable horizontal output flip
	* @param flipV - enable vertical output flip
	*/
	virtual void SetOutputFlip(bool flipH, bool flipV) = 0;

	/**
	* Returns current output flip settings
	*
	* @param flipH - (out) current horizontal output flip
	* @param flipV - (out) current vertical output flip
	*/
	virtual void GetOutputFlip(bool& flipH, bool& flipV) const = 0;

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
