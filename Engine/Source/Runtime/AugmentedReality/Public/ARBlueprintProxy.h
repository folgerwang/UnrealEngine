//@joeg -- ARkit 2.0 additions

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Tickable.h"

#include "ARTypes.h"

#include "ARBlueprintProxy.generated.h"

UCLASS(MinimalAPI)
class UARBaseAsyncTaskBlueprintProxy :
	public UObject,
	public FTickableGameObject
{
	GENERATED_UCLASS_BODY()
	
public:
	//~ Begin FTickableObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !HasAnyFlags(RF_ClassDefaultObject) && bShouldTick; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UARBaseAsyncTaskBlueprintProxy, STATGROUP_Tickables); }
	//~ End FTickableObject Interface
	
	/** The async task to check during Tick() */
	TSharedPtr<FARAsyncTask, ESPMode::ThreadSafe> AsyncTask;
	
	virtual void ReportSuccess() { check(0); }
	virtual void ReportFailure() { check(0); }

	static void RegisterAsARSystem(const TSharedPtr<FARSystemBase, ESPMode::ThreadSafe>& NewArSystem);

protected:
	static const TSharedPtr<FARSystemBase, ESPMode::ThreadSafe>& GetARSystem();

private:
	/** True until the async task completes, then false */
	bool bShouldTick;

	static TSharedPtr<FARSystemBase, ESPMode::ThreadSafe> RegisteredARSystem;
};

USTRUCT(BlueprintType)
struct FARSaveWorldResult
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category="Augmented Reality")
	FString Error;
	
	UPROPERTY(BlueprintReadOnly, Category="Augmented Reality")
	TArray<uint8> WorldData;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARSaveWorldDelegate, const FARSaveWorldResult&, SaveWorldResult);

UCLASS(MinimalAPI)
class UARSaveWorldAsyncTaskBlueprintProxy :
	public UARBaseAsyncTaskBlueprintProxy
{
	GENERATED_UCLASS_BODY()
	
public:
	UPROPERTY(BlueprintAssignable)
	FARSaveWorldDelegate OnSuccess;
	
	UPROPERTY(BlueprintAssignable)
	FARSaveWorldDelegate OnFailure;
	
	/**
	 * Saves an AR world to a byte array for network replication or saving to disk
	 *
	 * @param bCompressData whether to compress the data or not
	 */
	UFUNCTION(BlueprintCallable, Meta=(DisplayName="AR Save World"), Category="Augmented Reality")
	static UARSaveWorldAsyncTaskBlueprintProxy* CreateProxyObjectForARSaveWorld(bool bCompressData);
	
	/** The async task to check during Tick() */
	TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe> SaveWorldTask;
	
	UPROPERTY(BlueprintReadOnly, Category="Augmented Reality")
	FARSaveWorldResult SaveWorldResult;

	virtual void ReportSuccess() override;
	virtual void ReportFailure() override;
};

USTRUCT(BlueprintType)
struct FARGetCandidateObjectResult
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category="Augmented Reality")
	FString Error;
	
	UPROPERTY(BlueprintReadOnly, Category="Augmented Reality")
	UARCandidateObject* CandidateObject;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARGetCandidateObjectDelegate, const FARGetCandidateObjectResult&, CandidateObjectResult);

UCLASS(MinimalAPI)
class UARGetCandidateObjectAsyncTaskBlueprintProxy :
	public UARBaseAsyncTaskBlueprintProxy
{
	GENERATED_UCLASS_BODY()
	
public:
	UPROPERTY(BlueprintAssignable)
	FARGetCandidateObjectDelegate OnSuccess;
	
	UPROPERTY(BlueprintAssignable)
	FARGetCandidateObjectDelegate OnFailure;
	
	/**
	 * Saves the point cloud centered at the specified location capturing all of the features within the specified extent as an object that can be detected later
	 *
	 * @param Location the center of the extent to grab features at
	 * @param Extent the size of the region to grab feature points
	 */
	UFUNCTION(BlueprintCallable, Meta=(DisplayName="AR Get Candidate Object"), Category="Augmented Reality")
	static UARGetCandidateObjectAsyncTaskBlueprintProxy* CreateProxyObjectForARGetCandidateObject(FVector Location, FVector Extent);
	
	/** The async task to check during Tick() */
	TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe> CandidateObjectTask;
	
	UPROPERTY(BlueprintReadOnly, Category="Augmented Reality")
	FARGetCandidateObjectResult CandidateObjectResult;

	virtual void ReportSuccess() override;
	virtual void ReportFailure() override;
};
