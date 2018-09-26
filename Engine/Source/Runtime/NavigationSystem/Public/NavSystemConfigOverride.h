// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "AI/NavigationSystemBase.h"
#include "NavSystemConfigOverride.generated.h"


class UNavigationSystemConfig;


UCLASS(hidecategories = (Input, Rendering, Actor, LOD, Cooking))
class NAVIGATIONSYSTEM_API ANavSystemConfigOverride : public AActor
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
private:
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;
#endif // WITH_EDITORONLY_DATA

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Navigation, Instanced,  meta = (NoResetToDefault))
	UNavigationSystemConfig* NavigationSystemConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Navigation, AdvancedDisplay)
	uint8 bLoadOnClient : 1;

public:
	ANavSystemConfigOverride(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

#if WITH_EDITOR
	/** made an explicit function since rebuilding navigation system can be expensive */
	UFUNCTION(Category = Navigation, meta = (CallInEditor = "true"))
	void ApplyChanges();
	//virtual void CheckForErrors() override;
#endif

protected:
#if WITH_EDITOR
	/** Called only in the editor mode*/
	void InitializeForWorld(UNavigationSystemBase* NewNavSys, UWorld* World, const FNavigationSystemRunMode RunMode);
#endif // WITH_EDITOR
};
