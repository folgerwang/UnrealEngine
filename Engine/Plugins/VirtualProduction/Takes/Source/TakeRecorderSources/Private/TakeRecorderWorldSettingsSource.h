// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "UObject/SoftObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "TakeRecorderWorldSettingsSource.generated.h"

class UTexture;
class UTakeRecorderActorSource;

/** A recording source that records world settings */
UCLASS(DisplayName="World Settings", Category="Actors")
class UTakeRecorderWorldSettingsSource : public UTakeRecorderSource
{
public:
	GENERATED_BODY()

	UTakeRecorderWorldSettingsSource(const FObjectInitializer& ObjInit);

private:

	// UTakeRecorderSource
	virtual TArray<UTakeRecorderSource*> PreRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) override;
	virtual TArray<UTakeRecorderSource*> PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence) override;
	virtual bool SupportsTakeNumber() const override { return false; }
	virtual FText GetDisplayTextImpl() const override;
	virtual bool CanAddSource(UTakeRecorderSources* InSources) const override;

	// This source does not support subscenes (ie. "World Settings subscene"), but the world settings actor would be placed in subscenes if the option is enabled
	virtual bool SupportsSubscenes() const override { return false; }

	TWeakObjectPtr<UTakeRecorderActorSource> WorldSettingsSource;
};
