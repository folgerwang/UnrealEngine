// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "LandscapeBPCustomBrush.generated.h"

UCLASS(Blueprintable, hidecategories = (Replication, Input, LOD, Actor, Cooking, Rendering))
class LANDSCAPE_API ALandscapeBlueprintCustomBrush : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category= "Settings", EditAnywhere, NonTransactional)
	bool AffectHeightmap;

	UPROPERTY(Category= "Settings", EditAnywhere, NonTransactional)
	bool AffectWeightmap;

#if WITH_EDITORONLY_DATA
	UPROPERTY(NonTransactional, DuplicateTransient)
	class ALandscape* OwningLandscape;

	UPROPERTY(NonTransactional, DuplicateTransient)
	bool bIsCommited;

	UPROPERTY(Transient)
	bool bIsInitialized;

	UPROPERTY(Transient)
	bool PreviousAffectHeightmap;

	UPROPERTY(Transient)
	bool PreviousAffectWeightmap;
#endif
public:

	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsAffectingHeightmap() const { return AffectHeightmap; }
	bool IsAffectingWeightmap() const { return AffectWeightmap; }

	UFUNCTION(BlueprintImplementableEvent)
	UTextureRenderTarget2D* Render(bool InIsHeightmap, UTextureRenderTarget2D* InCombinedResult);

	UFUNCTION(BlueprintImplementableEvent)
	void Initialize(const FIntPoint& InLandscapeSize, const FIntPoint& InLandscapeRenderTargetSize);

#if WITH_EDITOR
	void SetCommitState(bool InCommited);
	bool IsCommited() const { return bIsCommited; }

	bool IsInitialized() const { return bIsInitialized; }
	void SetIsInitialized(bool InIsInitialized);

	void SetOwningLandscape(class ALandscape* InOwningLandscape);
	class ALandscape* GetOwningLandscape() const;

	virtual void PostEditMove(bool bFinished) override;
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

UCLASS(Blueprintable, hidecategories = (Replication, Input, LOD, Actor, Cooking, Rendering))
class LANDSCAPE_API ALandscapeBlueprintCustomSimulationBrush : public ALandscapeBlueprintCustomBrush
{
	GENERATED_UCLASS_BODY()

public:
	// TODO: To Implement
};


