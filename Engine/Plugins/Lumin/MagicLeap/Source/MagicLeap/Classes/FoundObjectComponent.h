// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#pragma once

#include "Components/SceneComponent.h"
#include "FoundObjectComponent.generated.h"

UENUM(BlueprintType)
enum class EFoundObjectType : uint8
{
	None              UMETA(DisplayName = "None"),
	PersistentPoint   UMETA(DisplayName = "Persistent Point"),
	Plane             UMETA(DisplayName = "Plane"),
	Generic           UMETA(DisplayName = "Generic")
};

/** Key-value pair used for either filters or characteristics of an object. */
USTRUCT(BlueprintType)
struct FFoundObjectProperty
{
	GENERATED_BODY()

public:
	/** Key for an object's property. Example of a key would be 'texture'. Max length is 64 chars. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	FString Key;

	/** Value for an object's property. Example of a value would be 'smooth'. Max length is 64 chars. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	FString Value;
};

/** Represents the featuers of a found object. */
USTRUCT(BlueprintType)
struct FFoundObjectResult
{
	GENERATED_BODY()

public:
	/** Unique ID for this object. Can be shared across network or across app runs to detect the same object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	FString ObjectUID;

	/** Type of the found object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	EFoundObjectType ObjectType;

	/** Position of the center of the found object in world coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	FVector ObjectPosition;

	/** Orientation of the found object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	FRotator ObjectOrientation;

	/** Dimensions of the found object (in Unreal units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	FVector ObjectDimensions;

	/** Labels describing this foudn obejct. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	TArray<FString> ObjectLabels;

	/** characteristics of this found object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	TArray<FFoundObjectProperty> ObjectProperties;

	/** Unique ID of another found object close to or related to the current one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	FString RelatedObjectID;
};

/** Creates requests to find objects and delegates their result. */
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API UFoundObjectComponent
	: public USceneComponent
{
	GENERATED_BODY()

public:
	UFoundObjectComponent();
	~UFoundObjectComponent();

	/**
	  Delegate used to convey the result of a found object query.
	  @param QuerySucceeded True if the found object query succeeded, false otherwise.
	  @param FoundObjects Array of found objects returned by the query.
	  @param QueryID Query this result is for.
	*/
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FFoundObjectResultDelegate, bool, QuerySucceeded, const TArray<FFoundObjectResult>&, FoundObjects, int32, QueryID);

	/** Unique ID of the object to look for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	FString QueryObjectID;

	/** Labels used to describe the object. Should be nouns. Examples would be 'chair, hermon miller, furniture'. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	TArray<FString> QueryLabels;

	/** Types of objects to look for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	TArray<EFoundObjectType> QueryTypes;

	/** Attributes to filter the object query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	TArray<FFoundObjectProperty> QueryProperties;

	/** The maximum number of objects that should be returned in the result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap", meta = (ClampMin = 1))
	int32 MaxResults;

	/** Bounding box for searching the objects in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FoundObject|MagicLeap")
	class UBoxComponent* SearchVolume;

	/**
	  Query for objects with the current values of the component members.
	  @param QueryID Can be used to identify the results.
	  @param ResultDelegate Delegate which will be called when the found object result is ready.
	  @returns True if the found object query was successfully placed, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "FoundObject|MagicLeap")
	bool SubmitQuery(int32& QueryID, const FFoundObjectResultDelegate& ResultDelegate);

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void FinishDestroy() override;

private:
	class FFoundObjectImpl *Impl;

#if WITH_EDITOR
private:
	void PrePIEEnded(bool bWasSimulatingInEditor);
#endif
};
