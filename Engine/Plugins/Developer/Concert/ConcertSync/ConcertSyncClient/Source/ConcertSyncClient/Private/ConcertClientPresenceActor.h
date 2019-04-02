// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StructOnScope.h"
#include "GameFramework/Actor.h"
#include "Engine/Scene.h"
#include "ConcertPresenceEvents.h"
#include "ConcertClientMovement.h"
#include "ConcertClientPresenceActor.generated.h"

class IConcertClientSession;
class FConcertClientPresenceMode;


/**
  * A ConcertClientPresenceActor is an editor-only transient actor representing other client presences during a concert client session.
  */
UCLASS(Abstract, Transient, NotPlaceable, Blueprintable)
class AConcertClientPresenceActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	/** AActor interface */
	virtual bool IsEditorOnly() const override final
	{
		return false;
	}

#if WITH_EDITOR
	virtual bool IsSelectable() const override final
	{
		return false;
	}

	virtual bool IsListedInSceneOutliner() const override final
	{
		return false;
	}
#endif

	void SetPresenceName(const FString& InName);

	virtual void SetPresenceColor(const FLinearColor& InColor);

	virtual void HandleEvent(const FStructOnScope& InEvent);

	virtual void InitPresence(const class UConcertAssetContainer& InAssetContainer, FName DeviceType);

	virtual bool ShouldTickIfViewportsOnly() const override;

	virtual void Tick(float DeltaSeconds) override;

protected:
	/* The device type that this presence represent (i.e Oculus, Vive, Desktop) */
	UPROPERTY(BlueprintReadOnly, Category = "Rendering", meta = (AllowPrivateAccess = "true"))
	FName PresenceDeviceType;

	/** The camera mesh component to show visually where the camera is placed */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* PresenceMeshComponent;
	
	/** The text render component to display the associated client's name */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (AllowPrivateAccess = "true"))
	class UTextRenderComponent* PresenceTextComponent;

	/** Dynamic material for the presence actor */
	UPROPERTY()
	class UMaterialInstanceDynamic* PresenceMID;

	/** Dynamic material for the presence text */
	UPROPERTY()
	class UMaterialInstanceDynamic* TextMID;

	TOptional<FConcertClientMovement> PresenceMovement;
};

