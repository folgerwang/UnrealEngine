// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ILiveLinkClient.h"
#include "LiveLinkBlueprintStructs.h"
#include "LiveLinkTypes.h"
#include "LiveLinkComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLiveLinkTickSignature, float, DeltaTime);

// An actor component to enable accessing LiveLink data in Blueprints. 
// Data can be accessed in Editor through the "OnLiveLinkUpdated" event.
// Any Skeletal Mesh Components on the parent will be set to animate in editor causing their AnimBPs to run.
UCLASS( ClassGroup=(LiveLink), meta=(BlueprintSpawnableComponent) )
class LIVELINK_API ULiveLinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULiveLinkComponent();

protected:
	virtual void OnRegister() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// This Event is triggered any time new LiveLink data is available, including in the editor
	UPROPERTY(BlueprintAssignable, Category = "LiveLink")
	FLiveLinkTickSignature OnLiveLinkUpdated;

	// Returns a list of available Subject Names for LiveLink
	UFUNCTION(BlueprintCallable, Category = "LiveLink")
	void GetAvailableSubjectNames(TArray<FName>& SubjectNames);
	
	// Returns a handle to the current frame of data in LiveLink for a given subject along with a boolean for whether a frame was found.
	// Returns a handle to an empty frame if no frame of data is found.
	UFUNCTION(BlueprintCallable, Category = "LiveLink")
	void GetSubjectData(const FName SubjectName, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle);

private:
	bool HasLiveLinkClient();

	// Record whether we have been recently registered
	bool bIsDirty;	

	ILiveLinkClient* LiveLinkClient;
};