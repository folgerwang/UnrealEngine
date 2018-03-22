// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreDefines.h"
#include "Containers/ArrayView.h"
#include "Math/Range.h"
#include "Channels/MovieSceneChannelTraits.h"

struct IBatchChannelInterface
{
	virtual ~IBatchChannelInterface() {}

	virtual void ChangeFrameResolution_Batch(TArrayView<void* const> Ptrs, FFrameRate SourceRate, FFrameRate DestinationRate) const = 0;
	virtual TRange<FFrameNumber> ComputeEffectiveRange_Batch(TArrayView<void* const> Ptrs) const = 0;
	virtual int32 GetNumKeys_Batch(TArrayView<void* const> Ptrs) const = 0;
	virtual void Reset_Batch(TArrayView<void* const> Ptrs) const = 0;
	virtual void Offset_Batch(TArrayView<void* const> Ptrs, FFrameNumber DeltaPosition) const = 0;
	virtual void Dilate_Batch(TArrayView<void* const> Ptrs, FFrameNumber Origin, float DilationFactor) const = 0;
	virtual void Optimize_Batch(TArrayView<void* const> Ptrs, const FKeyDataOptimizationParams& InParameters) const = 0;
	virtual void ClearDefaults_Batch(TArrayView<void* const> Ptrs) const = 0;
};

template<typename ChannelType>
struct TBatchChannelInterface : IBatchChannelInterface
{
	virtual TRange<FFrameNumber> ComputeEffectiveRange_Batch(TArrayView<void* const> Ptrs) const
	{
		using namespace MovieScene;

		TRange<FFrameNumber> Range = TRange<FFrameNumber>::Empty();
		for (void* Ptr : Ptrs)
		{
			if (Ptr)
			{
				Range = TRange<FFrameNumber>::Hull(Range, ComputeEffectiveRange(static_cast<ChannelType*>(Ptr)));
			}
		}
		return Range;
	}

	virtual void ChangeFrameResolution_Batch(TArrayView<void* const> Ptrs, FFrameRate SourceRate, FFrameRate DestinationRate) const override
	{
		using namespace MovieScene;
		for (void* Ptr : Ptrs)
		{
			if (Ptr)
			{
				ChangeFrameResolution(static_cast<ChannelType*>(Ptr), SourceRate, DestinationRate);
			}
		}
	}

	virtual int32 GetNumKeys_Batch(TArrayView<void* const> Ptrs) const
	{
		using namespace MovieScene;

		int32 NumKeys = 0;
		for (void* Ptr : Ptrs)
		{
			if (Ptr)
			{
				NumKeys += GetNumKeys(static_cast<ChannelType*>(Ptr));
			}
		}
		return NumKeys;
	}

	virtual void Reset_Batch(TArrayView<void* const> Ptrs) const
	{
		using namespace MovieScene;

		for (void* Ptr : Ptrs)
		{
			if (Ptr)
			{
				Reset(static_cast<ChannelType*>(Ptr));
			}
		}
	}

	virtual void Offset_Batch(TArrayView<void* const> Ptrs, FFrameNumber DeltaPosition) const
	{
		using namespace MovieScene;

		for (void* Ptr : Ptrs)
		{
			if (Ptr)
			{
				Offset(static_cast<ChannelType*>(Ptr), DeltaPosition);
			}
		}
	}

	virtual void Dilate_Batch(TArrayView<void* const> Ptrs, FFrameNumber Origin, float DilationFactor) const
	{
		using namespace MovieScene;

		for (void* Ptr : Ptrs)
		{
			if (Ptr)
			{
				Dilate(static_cast<ChannelType*>(Ptr), Origin, DilationFactor);
			}
		}
	}

	virtual void Optimize_Batch(TArrayView<void* const> Ptrs, const FKeyDataOptimizationParams& InParameters) const
	{
		using namespace MovieScene;

		for (void* Ptr : Ptrs)
		{
			if (Ptr)
			{
				Optimize(static_cast<ChannelType*>(Ptr), InParameters);
			}
		}
	}

	virtual void ClearDefaults_Batch(TArrayView<void* const> Ptrs) const
	{
		using namespace MovieScene;

		for (void* Ptr : Ptrs)
		{
			if (Ptr)
			{
				ClearChannelDefault(static_cast<ChannelType*>(Ptr));
			}
		}
	}
};