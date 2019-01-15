// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Projection screen runtime data
 */
struct FDisplayClusterProjectionScreenData
{
	// Projection screen runtime data
	FVector   Loc  = FVector::ZeroVector;
	FRotator  Rot  = FRotator::ZeroRotator;
	FVector2D Size = FVector2D::ZeroVector;
};


/**
 * Interface for obtaining projection screen data to properly build a frustum
 */
class IDisplayClusterProjectionScreenDataProvider
{
public:
	virtual ~IDisplayClusterProjectionScreenDataProvider()
	{ }

	/**
	* Provides with all data required to build a frustum
	*
	* @param ScreenId [IN] - ID of projection screen
	*
	* @param OutLocation [OUT] - world location of screen's midpoint
	* @param OutRotation [OUT] - world rotation of screen's midpoint
	* @param OutSize     [OUT] - width/height of projection screen (metric)
	*
	* @return Returns true if the operation succeeded
	*/
	virtual bool GetProjectionScreenData(const FString& ScreenId, FDisplayClusterProjectionScreenData& OutProjectionScreenData) const = 0;
};
