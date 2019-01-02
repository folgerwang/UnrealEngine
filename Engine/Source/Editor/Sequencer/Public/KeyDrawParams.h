// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Color.h"

struct FSlateBrush;

/**
 * Structure defining how a key should be drawn
 */
struct FKeyDrawParams
{
	FKeyDrawParams()
		: BorderBrush(nullptr), FillBrush(nullptr)
		, BorderTint(FLinearColor::White), FillTint(FLinearColor::White)
		, FillOffset(0.f, 0.f)
	{}

	friend bool operator==(const FKeyDrawParams& A, const FKeyDrawParams& B)
	{
		return A.BorderBrush == B.BorderBrush && A.FillBrush == B.FillBrush && A.BorderTint == B.BorderTint && A.FillTint == B.FillTint && A.FillOffset == B.FillOffset;
	}
	friend bool operator!=(const FKeyDrawParams& A, const FKeyDrawParams& B)
	{
		return A.BorderBrush != B.BorderBrush || A.FillBrush != B.FillBrush || A.BorderTint != B.BorderTint || A.FillTint != B.FillTint || A.FillOffset != B.FillOffset;
	}

	/** Brush to use for drawing the key's border */
	const FSlateBrush* BorderBrush;

	/** Brush to use for drawing the key's filled area */
	const FSlateBrush* FillBrush;

	/** Tint to be used for the key's border */
	FLinearColor BorderTint;

	/** Tint to be used for the key's filled area */
	FLinearColor FillTint;

	/** The amount to offset the fill brush from the keys center */
	FVector2D FillOffset;
};