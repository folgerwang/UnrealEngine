// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Range.h"
#include "Misc/Guid.h"
#include "Containers/ArrayView.h"
#include "Curves/KeyHandle.h"

#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"
#include "IKeyArea.h"
#include "Misc/FrameRate.h"

/** Simple structure that caches the sorted key times for a given key area */
struct FSequencerCachedKeys
{
	/** Update this cache to store key times and handles from the specified key area */
	void Update(TSharedRef<IKeyArea> KeyArea, FFrameRate SourceResolution);

	/** Get an view of the cached array for keys that fall within the specified range */
	void GetKeysInRange(const TRange<double>& Range, TArrayView<const double>* OutTimes, TArrayView<const FFrameNumber>* OutKeyFrames, TArrayView<const FKeyHandle>* OutHandles) const;

	/** Get the key area this cache was generated for, or nullptr if the cache has never been updated */
	TSharedPtr<IKeyArea> GetKeyArea() const
	{
		return KeyArea;
	}

private:
	/** Cached key information */
	TArray<double> CachedKeyTimes;
	TArray<FFrameNumber> CachedKeyFrames;
	TArray<FKeyHandle> CachedKeyHandles;

	/** The guid with which the above array was generated */
	FGuid CachedSignature;

	/** The tick resolution of the sequence that this cache was generated with */
	FFrameRate CachedTickResolution;

	/** The key area this cache is for */
	TSharedPtr<IKeyArea> KeyArea;
};

