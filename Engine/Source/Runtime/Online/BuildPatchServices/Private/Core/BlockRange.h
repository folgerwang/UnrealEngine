// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Math/NumericLimits.h"

namespace BuildPatchServices
{
	struct FBlockRange
	{
	public:
		FBlockRange(uint64 InFirst, uint64 InSize)
			: First(InFirst)
			, Size(InSize)
		{
			checkf((GetSize() == 0) || (GetLast() >= GetFirst()), TEXT("Byte range has uint64 overflow."));
		}

		uint64 GetFirst() const
		{
			checkf(Size > 0, TEXT("Using GetFirst for a 0 size range is invalid."));
			return First;
		}

		uint64 GetLast() const
		{
			checkf(Size > 0, TEXT("Using GetLast for a 0 size range is invalid."));
			return First + (Size - 1);
		}

		uint64 GetSize() const
		{
			return Size;
		}

		bool Overlaps(const FBlockRange& Other) const
		{
			return GetFirst() <= Other.GetLast() && GetLast() >= Other.GetFirst();
		}

	private:
		uint64 First;
		uint64 Size;
	};
}
