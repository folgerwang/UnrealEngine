// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Viewport area
 */
class FDisplayClusterViewportArea
{
public:
	FDisplayClusterViewportArea() :
		Location(FIntPoint::ZeroValue),
		Size(FIntPoint::ZeroValue)
	{ }

	FDisplayClusterViewportArea(const FIntPoint& loc, const FIntPoint& size) :
		Location(loc),
		Size(size)
	{ }

	FDisplayClusterViewportArea(int32 x, int32 y, int32 w, int32 h) :
		Location(FIntPoint(x, y)),
		Size(FIntPoint(w, h))
	{ }

public:
	bool IsValid() const
	{ return Size.X > 0 && Size.Y > 0; }

	FIntPoint GetLocation() const
	{ return Location; }
	
	FIntPoint GetSize() const
	{ return Size; }

	void SetLocation(const FIntPoint& loc)
	{ Location = loc; }

	void SetLocation(int32 x, int32 y)
	{ Location = FIntPoint(x, y); }

	void SetSize(const FIntPoint& size)
	{ Size = size; }

	void SetSize(int32 w, int32 h)
	{ Size = FIntPoint(w, h); }

private:
	FIntPoint Location;
	FIntPoint Size;
};
