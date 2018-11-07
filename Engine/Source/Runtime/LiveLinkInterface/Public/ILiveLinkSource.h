// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "LiveLinkTypes.h"
#include "ILiveLinkSource.generated.h"

class ILiveLinkClient;
class ULiveLinkSourceSettings;
struct FPropertyChangedEvent;

class ILiveLinkSource
{
public:
	virtual ~ILiveLinkSource() {}
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) = 0;
	virtual void InitializeSettings(ULiveLinkSourceSettings* Settings) {};

	// Can this source be displayed in the Source UI list
	virtual bool CanBeDisplayedInUI() const { return true; }

	virtual bool IsSourceStillValid() = 0;

	virtual bool RequestSourceShutdown() = 0;

	virtual FText GetSourceType() const = 0;
	virtual FText GetSourceMachineName() const = 0;
	virtual FText GetSourceStatus() const = 0;

	virtual UClass* GetCustomSettingsClass() const { return nullptr; }
	virtual void OnSettingsChanged(ULiveLinkSourceSettings* Settings, const FPropertyChangedEvent& PropertyChangedEvent) {}
};

// A Blueprint handle to a specific LiveLink Source
USTRUCT(BlueprintType)
struct FLiveLinkSourceHandle
{
	GENERATED_USTRUCT_BODY()

	FLiveLinkSourceHandle() = default;

	virtual ~FLiveLinkSourceHandle() = default;

	void SetSourcePointer(TSharedPtr<ILiveLinkSource> InSourcePointer)
	{
		SourcePointer = InSourcePointer;
	};

	TSharedPtr<ILiveLinkSource> SourcePointer;
};
