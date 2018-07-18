// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

/** Utility class for storing a shared localization resource ID */
class FTextLocalizationResourceId
{
public:
	FTextLocalizationResourceId() = default;

	explicit FTextLocalizationResourceId(const FString& InId)
		: SharedId(MakeShared<FString, ESPMode::ThreadSafe>(InId))
	{
	}

	explicit FTextLocalizationResourceId(FString&& InId)
		: SharedId(MakeShared<FString, ESPMode::ThreadSafe>(MoveTemp(InId)))
	{
	}

	FORCEINLINE bool operator==(const FTextLocalizationResourceId& Other) const
	{
		return Compare(Other) == 0;
	}

	FORCEINLINE bool operator!=(const FTextLocalizationResourceId& Other) const
	{
		return Compare(Other) != 0;
	}

	FORCEINLINE bool IsEmpty() const
	{
		return GetString().IsEmpty();
	}

	FORCEINLINE bool Equals(const FTextLocalizationResourceId& Other) const
	{
		return *this == Other;
	}

	FORCEINLINE int32 Compare(const FTextLocalizationResourceId& Other) const
	{
		return FCString::Strcmp(*GetString(), *Other.GetString());
	}

	FORCEINLINE const FString& GetString() const
	{
		static const FString EmptyId = FString();
		return SharedId.IsValid() ? *SharedId : EmptyId;
	}

private:
	/** Shared localization resource ID */
	TSharedPtr<const FString, ESPMode::ThreadSafe> SharedId;
};
