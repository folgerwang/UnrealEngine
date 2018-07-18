// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FoundObjectComponent.h"
#include "Components/BoxComponent.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "MagicLeapMath.h"
#include "Engine/Engine.h"
#include "Misc/Guid.h"
#include "Misc/CString.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

#if WITH_MLSDK
#include "ml_found_object.h"
#endif //WITH_MLSDK

class FFoundObjectImpl
{
public:
	FFoundObjectImpl()
#if WITH_MLSDK
		: Tracker(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	{};

public:
#if WITH_MLSDK
	MLHandle Tracker;
	TMap<uint32, UFoundObjectComponent::FFoundObjectResultDelegate> PendingQueries;
#endif //WITH_MLSDK

public:
	bool Create()
	{
#if WITH_MLSDK
		if (!MLHandleIsValid(Tracker))
		{
			Tracker = MLFoundObjectTrackerCreate();
			if (!MLHandleIsValid(Tracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error creating found object tracker."));
				return false;
			}
		}
#endif //WITH_MLSDK
		return true;
	}

	void Destroy()
	{
#if WITH_MLSDK
		if (MLHandleIsValid(Tracker))
		{
			bool bResult = MLFoundObjectTrackerDestroy(Tracker);
			if (!bResult)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error destroying found object tracker."));
			}
			Tracker = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
	}
};

#if WITH_MLSDK
MLFoundObjectType UnrealToMLFoundObjectType(EFoundObjectType FoundObjectType)
{
	switch (FoundObjectType)
	{
	case EFoundObjectType::None:
		return MLFoundObjectType_None;
	case EFoundObjectType::PersistentPoint:
		return MLFoundObjectType_FoundOrigin;
	case EFoundObjectType::Plane:
		return MLFoundObjectType_Plane;
	case EFoundObjectType::Generic:
		return MLFoundObjectType_Generic;
	default:
		UE_LOG(LogMagicLeap, Error, TEXT("Unhandled found object type %d"), static_cast<int32>(FoundObjectType));
	}
	return MLFoundObjectType_Ensure32Bits;
}

EFoundObjectType MLToUnrealFoundObjectType(MLFoundObjectType FoundObjectType)
{
	switch (FoundObjectType)
	{
	case MLFoundObjectType_None:
		return EFoundObjectType::None;
	case MLFoundObjectType_FoundOrigin:
		return EFoundObjectType::PersistentPoint;
	case MLFoundObjectType_Plane:
		return EFoundObjectType::Plane;
	case MLFoundObjectType_Generic:
		return EFoundObjectType::Generic;
	default:
		UE_LOG(LogMagicLeap, Error, TEXT("Unhandled found object type %d"), static_cast<int32>(FoundObjectType));
	}
	return EFoundObjectType::None;
}
#endif //WITH_MLSDK

#if WITH_MLSDK
FString MLUUIDToFString(const MLUUID& uuid)
{
	check(sizeof(MLUUID) == sizeof(FGuid));
	FGuid guid;
	FMemory::Memcpy(&guid, &uuid, sizeof(MLUUID));
	return guid.ToString(EGuidFormats::Digits);
}

bool FStringToMLUUID(const FString& strID, MLUUID& outUuid)
{
	check(sizeof(MLUUID) == sizeof(FGuid));
	FGuid guid;
	bool validGuid = FGuid::ParseExact(strID, EGuidFormats::Digits, guid);
	if (validGuid)
	{
		FMemory::Memcpy(&outUuid, &guid, sizeof(FGuid));
	}
	return validGuid;
}
#endif //WITH_MLSDK

UFoundObjectComponent::UFoundObjectComponent()
	: MaxResults(1)
	, Impl(new FFoundObjectImpl())
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;

	SearchVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SearchVolume"));
	SearchVolume->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	SearchVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SearchVolume->SetCanEverAffectNavigation(false);
	SearchVolume->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	SearchVolume->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	SearchVolume->SetGenerateOverlapEvents(false);

#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.AddUObject(this, &UFoundObjectComponent::PrePIEEnded);
	}
#endif
}

UFoundObjectComponent::~UFoundObjectComponent()
{
	delete Impl;
	Impl = nullptr;
}

bool UFoundObjectComponent::SubmitQuery(int32& QueryID, const FFoundObjectResultDelegate& ResultDelegate)
{
#if WITH_MLSDK
	if (!Impl->Create())
	{
		return false;
	}

	const FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFrameworkConst();
	const float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	const FTransform PoseInverse = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse();

	MLFoundObjectQueryFilter query;
	FMemory::Memset(&query, 0, sizeof(MLFoundObjectQueryFilter));

	FStringToMLUUID(QueryObjectID, query.id);

	TArray<const char*> labels;
	for (const FString& label : QueryLabels)
	{
		labels.Add(TCHAR_TO_ANSI(*label));
	}
	query.labels = labels.GetData();
	query.labels_count = labels.Num();

	TArray<MLFoundObjectType> types;
	for (const EFoundObjectType type : QueryTypes)
	{
		types.Add(UnrealToMLFoundObjectType(type));
	}
	query.types = types.GetData();
	query.types_count = types.Num();

	TArray<MLFoundObjectProperty> properties;
	properties.AddZeroed(QueryProperties.Num());
	int32 j = 0;
	for (const FFoundObjectProperty& property : QueryProperties)
	{
		FCStringAnsi::Strncpy(properties[j].key, TCHAR_TO_ANSI(*property.Key), MLFoundObject_MaxPropertiesKeySize);
		FCStringAnsi::Strncpy(properties[j].value, TCHAR_TO_ANSI(*property.Value), MLFoundObject_MaxPropertiesValueSize);
		++j;
	}
	query.properties = properties.GetData();
	query.properties_count = properties.Num();

	query.center = MagicLeap::ToMLVector(PoseInverse.TransformPosition(SearchVolume->GetComponentLocation()), WorldToMetersScale);

	query.max_distance = MagicLeap::ToMLVector(SearchVolume->GetScaledBoxExtent(), WorldToMetersScale);
	// MagicLeap::ToMLVector() causes the Z component to be negated.
	query.max_distance.x = FMath::Abs<float>(query.max_distance.x);
	query.max_distance.y = FMath::Abs<float>(query.max_distance.y);
	query.max_distance.z = FMath::Abs<float>(query.max_distance.z);

	query.max_results = static_cast<uint32>(MaxResults);

	uint32 queryID = 0;
	bool bQuerySucceeded = MLFoundObjectQuery(Impl->Tracker, &query, &queryID);
	if (!bQuerySucceeded)
	{
		UE_LOG(LogMagicLeap, Error, TEXT("Found objects query failed."));
		return false;
	}
	QueryID = static_cast<int32>(queryID);
	Impl->PendingQueries.Add(QueryID, ResultDelegate);

#endif //WITH_MLSDK
	return true;
}

void UFoundObjectComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_MLSDK
	if (!MLHandleIsValid(Impl->Tracker))
	{
		return;
	}

	const FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFrameworkConst();
	const float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

	FTransform PoseTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this);

	TArray<uint32> CompletedQueries;
	for (auto& pair : Impl->PendingQueries)
	{
		uint32 queryID = pair.Key;
		uint32 numResults = 0;
		bool bResult = MLFoundObjectGetResultCount(Impl->Tracker, queryID, &numResults);
		if (!bResult)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("Error retrieving number of found objects for query %d."), static_cast<int32>(queryID));
			continue;
		}

		TArray<FFoundObjectResult> results;
		for (uint32 i = 0; i < numResults; ++i)
		{
			MLFoundObject foundObject;
			bResult = MLFoundObjectGetResult(Impl->Tracker, queryID, i, &foundObject);
			if (!bResult)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error retrieving found object [%d]"), i);
				continue;
			}

			FFoundObjectResult foundObjectResult;
			foundObjectResult.ObjectUID = MLUUIDToFString(foundObject.id);
			foundObjectResult.ObjectType = MLToUnrealFoundObjectType(foundObject.type);

			for (uint32 labelItr = 0; labelItr < foundObject.label_count; ++labelItr)
			{
				char labelStr[MLFoundObject_MaxLabelSize];
				bResult = MLFoundObjectGetLabel(Impl->Tracker, foundObject.id, labelItr, MLFoundObject_MaxLabelSize, labelStr);
				if (!bResult)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("Error retreiving label [%d] for found object [%d]"), UTF8_TO_TCHAR(labelStr), i);
					continue;
				}
				foundObjectResult.ObjectLabels.Add(FString(ANSI_TO_TCHAR(labelStr)));
			}

			for (uint32 propItr = 0; propItr < foundObject.property_count; ++propItr)
			{
				MLFoundObjectProperty prop;
				bResult = MLFoundObjectGetProperty(Impl->Tracker, foundObject.id, static_cast<uint32>(propItr), &prop);
				if (!bResult)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("Error retreiving property [%d] for found object [%d]"), propItr, i);
					continue;
				}
				FFoundObjectProperty objectProp;
				objectProp.Key = FString(ANSI_TO_TCHAR(prop.key));
				objectProp.Value = FString(ANSI_TO_TCHAR(prop.value));
				foundObjectResult.ObjectProperties.Add(objectProp);
			}

			foundObjectResult.RelatedObjectID = MLUUIDToFString(foundObject.reference_point_id);

			FTransform objectTransform = FTransform(MagicLeap::ToFQuat(foundObject.rotation), MagicLeap::ToFVector(foundObject.position, WorldToMetersScale), FVector(1.0f, 1.0f, 1.0f));
			if (objectTransform.ContainsNaN())
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Found object %d transform contains NaN."), i);
				continue;
			}
			if (!objectTransform.GetRotation().IsNormalized())
			{
				FQuat rotation = objectTransform.GetRotation();
				rotation.Normalize();
				objectTransform.SetRotation(rotation);
			}
			objectTransform.AddToTranslation(PoseTransform.GetLocation());
			objectTransform.ConcatenateRotation(PoseTransform.Rotator().Quaternion());

			foundObjectResult.ObjectPosition = objectTransform.GetLocation();
			foundObjectResult.ObjectOrientation = objectTransform.Rotator();
			foundObjectResult.ObjectDimensions = MagicLeap::ToFVector(foundObject.size, WorldToMetersScale);
			foundObjectResult.ObjectDimensions.X = FMath::Abs<float>(foundObjectResult.ObjectDimensions.X);
		}

		CompletedQueries.Add(queryID);
		pair.Value.ExecuteIfBound(true, results, queryID);
	}

	for (uint32 completedQuery : CompletedQueries)
	{
		Impl->PendingQueries.Remove(completedQuery);
	}
#endif //WITH_MLSDK
}

void UFoundObjectComponent::FinishDestroy()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.RemoveAll(this);
	}
#endif
	Impl->Destroy();
	Super::FinishDestroy();
}

#if WITH_EDITOR
void UFoundObjectComponent::PrePIEEnded(bool bWasSimulatingInEditor)
{
	Impl->Destroy();
}
#endif
