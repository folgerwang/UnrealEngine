// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/IDisplayClusterProjectionScreenDataProvider.h"
#include "Render/Devices/DisplayClusterViewportArea.h"


struct FDisplayClusterRenderViewportContext
{
	// Projection screen runtime data
	FDisplayClusterProjectionScreenData ProjectionScreenData;

	// Camera location and orientation
	FVector   EyeLoc[3] = { FVector::ZeroVector };
	FRotator  EyeRot[3] = { FRotator::ZeroRotator };
};


/**
 * Rendering viewport (sub-region of the main viewport)
 */
class FDisplayClusterRenderViewport
{
public:
	FDisplayClusterRenderViewport(const FString& ScreenId, IDisplayClusterProjectionScreenDataProvider* DataProvider, const FDisplayClusterViewportArea& ViewportArea)
		: ProjScreenId(ScreenId)
		, ProjDataProvider(DataProvider)
		, ProjViewportArea(ViewportArea)
	{
		check(ProjDataProvider);
	}

	FDisplayClusterRenderViewport(const FDisplayClusterRenderViewport& Viewport)
		: ProjScreenId(Viewport.ProjScreenId)
		, ProjDataProvider(Viewport.ProjDataProvider)
		, ProjViewportArea(Viewport.ProjViewportArea)
	{
		check(ProjDataProvider);
	}

	virtual ~FDisplayClusterRenderViewport()
	{ }

public:
	IDisplayClusterProjectionScreenDataProvider* GetProjectionDataProvider() const
	{ return ProjDataProvider; }

	FString GetProjectionScreenId() const
	{ return ProjScreenId; }

	FDisplayClusterViewportArea GetViewportArea() const
	{ return ProjViewportArea; }

	FDisplayClusterRenderViewportContext GetViewportContext() const
	{ return ViewportContext; }

	void SetViewportContext(const FDisplayClusterRenderViewportContext& InViewportContext)
	{ ViewportContext = InViewportContext; }

private:
	// A projection screen linked to this viewport
	FString ProjScreenId;
	// Provides with spatial data of projection screen
	IDisplayClusterProjectionScreenDataProvider* ProjDataProvider;
	// 2D screen space area for view projection
	FDisplayClusterViewportArea ProjViewportArea;
	// Viewport context
	FDisplayClusterRenderViewportContext ViewportContext;
};
