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

#include "MagicLeapARPinComponent.h"
#include "IMagicLeapPlugin.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "AppEventHandler.h"
#include "MagicLeapMath.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Kismet/KismetMathLibrary.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "MagicLeapSecureStorage.h"
#if PLATFORM_LUMIN
#include "Lumin/LuminAffinity.h"
#endif // PLATFORM_LUMIN

#if WITH_MLSDK
#include "ml_passable_world.h"
#include "ml_persistent_coordinate_frames.h"
// #include "ml_secure_storage.h"
#endif //WITH_MLSDK

#if WITH_MLSDK
// TODO: Don't rely on the size being same. Use an object abstraction similar to UARPin.
static_assert(sizeof(FGuid) == sizeof(MLCoordinateFrameUID), "Size of FGuid should be same as MLCoordinateFrameUID. TODO: Don't rely on the size being same. Use an object abstraction similar to UARPin.");
#endif //WITH_MLSDK

struct FPersistentData
{
#if WITH_MLSDK
	MLCoordinateFrameUID PinnedCFUID;
#endif //WITH_MLSDK
	FTransform ComponentWorldTransform;
	FTransform CFUIDTransform;

	FPersistentData()
	{}

#if WITH_MLSDK
	FPersistentData(const MLCoordinateFrameUID& InPinnedCFUID, const FTransform& InComponentWorldTransform, const FTransform& InCFUIDTransform)
	: ComponentWorldTransform(InComponentWorldTransform)
	, CFUIDTransform(InCFUIDTransform)
	{
		FMemory::Memcpy(PinnedCFUID, InPinnedCFUID);
	}
#endif //WITH_MLSDK

	FPersistentData(const FGuid& InPinnedCFUID, const FTransform& InComponentWorldTransform, const FTransform& InCFUIDTransform)
	: ComponentWorldTransform(InComponentWorldTransform)
	, CFUIDTransform(InCFUIDTransform)
	{
#if WITH_MLSDK
		FMemory::Memcpy(&PinnedCFUID, &InPinnedCFUID, sizeof(MLCoordinateFrameUID));
#endif //WITH_MLSDK
	}
};

#if WITH_MLSDK
EPassableWorldError MLToUnrealPassableWorldError(MLResult result)
{
	switch (result)
	{
		case MLResult_Ok: return EPassableWorldError::None;
		case MLPassableWorldResult_LowMapQuality: return EPassableWorldError::LowMapQuality;
		case MLPassableWorldResult_UnableToLocalize: return EPassableWorldError::UnableToLocalize;
		case MLPassableWorldResult_ServerUnavailable: return EPassableWorldError::Unavailable;
		case MLResult_PrivilegeDenied: return EPassableWorldError::PrivilegeDenied;
		case MLResult_InvalidParam: return EPassableWorldError::InvalidParam;
		case MLResult_UnspecifiedFailure: return EPassableWorldError::UnspecifiedFailure;
	}
	return EPassableWorldError::UnspecifiedFailure;
}
#endif //WITH_MLSDK

class FMagicLeapARPinInterface : public MagicLeap::IAppEventHandler
{
public:
	static TWeakPtr<FMagicLeapARPinInterface, ESPMode::ThreadSafe> Get()
	{
		if (!Instance.IsValid())
		{
			Instance = MakeShareable(new FMagicLeapARPinInterface());
		}

		// Keeping it separate from the constructor, so that async privs can be handled easily.
		// Every time a Get() request is made, the tracker creation is attempted, if not already created.
		Instance->CreateTracker();

		return Instance;
	}

	virtual ~FMagicLeapARPinInterface()
	{
	}

#if WITH_MLSDK
	MLHandle GetHandle() const
	{
		return Tracker;
	}
#endif //WITH_MLSDK

#if WITH_MLSDK
	EPassableWorldError GetClosestARPin(const FVector& SearchPoint, FGuid& PinID)
	{
		EPassableWorldError ErrorReturn = EPassableWorldError::Unavailable;
		MagicLeap::EPrivilegeState PrivilegeState = GetPrivilegeStatus(MLPrivilegeID_PwFoundObjRead, false);
		if (PrivilegeState == MagicLeap::EPrivilegeState::Granted)
		{
			if (IMagicLeapPlugin::Get().IsMagicLeapHMDValid())
			{
				const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
				const float WorldToMetersScale = AppFramework.GetWorldToMetersScale();
				FTransform PoseInverse = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld).Inverse();
				MLVec3f Target = MagicLeap::ToMLVector(PoseInverse.TransformPosition(SearchPoint), WorldToMetersScale);
				MLResult Result = MLPersistentCoordinateFrameGetClosest(GetHandle(), &Target, reinterpret_cast<MLCoordinateFrameUID*>(&PinID));
				UE_CLOG(MLResult_Ok != Result, LogMagicLeap, Error, TEXT("MLPersistentCoordinateFrameGetClosest failed with error %s"), UTF8_TO_TCHAR(MLPersistentCoordinateFrameGetResultString(Result)));
				ErrorReturn = MLToUnrealPassableWorldError(Result);
			}
		}
		else if (PrivilegeState == MagicLeap::EPrivilegeState::Pending)
		{
			ErrorReturn = EPassableWorldError::PrivilegeRequestPending;
		}
		else
		{
			ErrorReturn = EPassableWorldError::PrivilegeDenied;
		}

		return ErrorReturn;
	}

	EPassableWorldError GetAvailableARPins(int32 NumRequested, TArray<FGuid>& CoordinateFrames)
	{
		EPassableWorldError ErrorReturn = EPassableWorldError::Unavailable;
		MagicLeap::EPrivilegeState PrivilegeState = GetPrivilegeStatus(MLPrivilegeID_PwFoundObjRead, false);
		if (PrivilegeState == MagicLeap::EPrivilegeState::Granted)
		{
			if (NumRequested > 0)
			{
				CoordinateFrames.Reset();
				CoordinateFrames.AddZeroed(NumRequested);
				MLCoordinateFrameUID* ArrayDataPointer = reinterpret_cast<MLCoordinateFrameUID*>(CoordinateFrames.GetData());
				MLResult QueryResult = MLPersistentCoordinateFrameGetAll(GetHandle(), sizeof(MLCoordinateFrameUID) * NumRequested, &ArrayDataPointer);
				if (MLResult_Ok != QueryResult)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("MLPersistentCoordinateFrameGetAll failed with error %s"), UTF8_TO_TCHAR(MLPersistentCoordinateFrameGetResultString(QueryResult)));
				}
				ErrorReturn = MLToUnrealPassableWorldError(QueryResult);
			}
		}
		else if (PrivilegeState == MagicLeap::EPrivilegeState::Pending)
		{
			ErrorReturn = EPassableWorldError::PrivilegeRequestPending;
		}
		else
		{
			ErrorReturn = EPassableWorldError::PrivilegeDenied;
		}

		return ErrorReturn;
	}

	EPassableWorldError GetNumAvailableARPins(int32& Count)
	{
		EPassableWorldError ErrorReturn = EPassableWorldError::Unavailable;
		MagicLeap::EPrivilegeState PrivilegeState = GetPrivilegeStatus(MLPrivilegeID_PwFoundObjRead, false);
		if (PrivilegeState == MagicLeap::EPrivilegeState::Granted)
		{
			uint32 NumPersistentFrames = 0;
			MLResult Result = MLPersistentCoordinateFrameGetCount(GetHandle(), &NumPersistentFrames);
			if (MLResult_Ok == Result)
			{
				Count = static_cast<int32>(NumPersistentFrames);
			}
			else
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLPersistentCoordinateFrameGetCount failed with error %s"), UTF8_TO_TCHAR(MLPersistentCoordinateFrameGetResultString(Result)));
				Count = 0;
			}
			ErrorReturn = MLToUnrealPassableWorldError(Result);
		}
		else if (PrivilegeState == MagicLeap::EPrivilegeState::Pending)
		{
			ErrorReturn = EPassableWorldError::PrivilegeRequestPending;
		}
		else
		{
			ErrorReturn = EPassableWorldError::PrivilegeDenied;
		}

		return ErrorReturn;
	}
#endif //WITH_MLSDK

	virtual void OnAppPause() override
	{
#if WITH_MLSDK
		bWasTrackerValidOnPause = MLHandleIsValid(Tracker);
		// need to destroy the tracker here in case privileges are removed while the app is dormant.
		DestroyTracker();
#endif //WITH_MLSDK
	}

	virtual void OnAppResume() override
	{
		if (bWasTrackerValidOnPause)
		{
			CreateTracker();
		}
	}

	virtual void OnAppShutDown() override
	{
		DestroyTracker();
	}

private:
	FMagicLeapARPinInterface()
#if WITH_MLSDK
	: MagicLeap::IAppEventHandler({ MLPrivilegeID_PwFoundObjRead })
	, Tracker(ML_INVALID_HANDLE)
	, bWasTrackerValidOnPause(false)
#endif //WITH_MLSDK
	{}

	void CreateTracker()
	{
#if WITH_MLSDK
		if (!MLHandleIsValid(Tracker))
		{
			MagicLeap::EPrivilegeState PrivilegeState = GetPrivilegeStatus(MLPrivilegeID_PwFoundObjRead, false);
			if (PrivilegeState == MagicLeap::EPrivilegeState::Granted)
			{
				// TODO: add retires like in Image tracker if error is LowMapQuality, UnableToLocalize, ServerUnavailable or PrivilegeDenied.
				// Retrying for PrivilegeDenied would only make sense when MLPrivilege runtime api is functional.
				MLResult Result = MLPersistentCoordinateFrameTrackerCreate(&Tracker);
				UE_CLOG(Result != MLResult_Ok, LogMagicLeap, Error, TEXT("Failed to create persistent coordinate frame tracker with error %s."), UTF8_TO_TCHAR(MLPersistentCoordinateFrameGetResultString(Result)));
			}
			else if (PrivilegeState == MagicLeap::EPrivilegeState::Denied)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Failed to initialize persistent coordinate frame tracker due to lack of privilege!"));
			}
		}
#endif //WITH_MLSDK
	}

	void DestroyTracker()
	{
#if WITH_MLSDK
		if (MLHandleIsValid(Tracker))
		{
			MLResult Result = MLPersistentCoordinateFrameTrackerDestroy(Tracker);
			UE_CLOG(Result != MLResult_Ok, LogMagicLeap, Error, TEXT("Failed to destroy persistent coordinate frame tracker with error %s."), UTF8_TO_TCHAR(MLPersistentCoordinateFrameGetResultString(Result)));
			Tracker = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
	}

#if WITH_MLSDK
	MLHandle Tracker;
#endif //WITH_MLSDK
	bool bWasTrackerValidOnPause;

	static TSharedPtr<FMagicLeapARPinInterface, ESPMode::ThreadSafe> Instance;
};

TSharedPtr<FMagicLeapARPinInterface, ESPMode::ThreadSafe> FMagicLeapARPinInterface::Instance;


EPassableWorldError UMagicLeapARPinFunctionLibrary::GetNumAvailableARPins(int32& Count)
{
#if WITH_MLSDK
	return FMagicLeapARPinInterface::Get().Pin()->GetNumAvailableARPins(Count);
#else
	return EPassableWorldError::Unavailable;
#endif //WITH_MLSDK
}

EPassableWorldError UMagicLeapARPinFunctionLibrary::GetAvailableARPins(int32 NumRequested, TArray<FGuid>& Pins)
{
#if WITH_MLSDK
	if (NumRequested <= 0)
	{
		GetNumAvailableARPins(NumRequested);
	}

	if (NumRequested == 0)
	{
		// There are no coordinate frames to return, so this call did succeed without any errors, it just returned an array of size 0.
		Pins.Reset();
		return EPassableWorldError::None;
	}

	return FMagicLeapARPinInterface::Get().Pin()->GetAvailableARPins(NumRequested, Pins);
#else
	return EPassableWorldError::Unavailable;
#endif //WITH_MLSDK
}

EPassableWorldError UMagicLeapARPinFunctionLibrary::GetClosestARPin(const FVector& SearchPoint, FGuid& PinID)
{
#if WITH_MLSDK
	return FMagicLeapARPinInterface::Get().Pin()->GetClosestARPin(SearchPoint, PinID);
#else
	return EPassableWorldError::Unavailable;
#endif //WITH_MLSDK
}

bool UMagicLeapARPinFunctionLibrary::GetARPinPositionAndOrientation(const FGuid& PinID, FVector& Position, FRotator& Orientation, bool& PinFoundInEnvironment)
{
	if (!IMagicLeapPlugin::Get().IsMagicLeapHMDValid())
	{
		return false;
	}

	// Make sure we create the pcf tracker, request privilege etc so that the list of pins is populated by the time we call GetTransform on it.
	FMagicLeapARPinInterface::Get();

#if WITH_MLSDK
	const MLCoordinateFrameUID* MLCFUID = reinterpret_cast<const MLCoordinateFrameUID*>(&PinID);
	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	EFailReason FailReason = EFailReason::None;
	FTransform Pose = FTransform::Identity;
	if (AppFramework.GetTransform(*MLCFUID, Pose, FailReason))
	{
		FTransform TrackingToWorld = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld);
		Pose.AddToTranslation(TrackingToWorld.GetLocation());
		Pose.ConcatenateRotation(TrackingToWorld.Rotator().Quaternion());
		Position = Pose.GetLocation();
		Orientation = Pose.Rotator();
		PinFoundInEnvironment = true;
		return true;
	}
	PinFoundInEnvironment = (FailReason != EFailReason::PoseNotFound);
#endif //WITH_MLSDK

	return false;
}

UMagicLeapARPinComponent::UMagicLeapARPinComponent()
: AutoPinType(EAutoPinType::OnlyOnDataRestoration)
, bShouldPinActor(false)
, PinnedSceneComponent(nullptr)
, OldComponentWorldTransform(FTransform::Identity)
, OldCFUIDTransform(FTransform::Identity)
, NewComponentWorldTransform(FTransform::Identity)
, NewCFUIDTransform(FTransform::Identity)
, bPinned(false)
, bDataRestored(false)
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;
}

void UMagicLeapARPinComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ObjectUID.Len() == 0)
	{
		if (GetOwner() != nullptr)
		{
			ObjectUID = GetOwner()->GetName();
			UE_LOG(LogMagicLeap, Warning, TEXT("ObjectUID is empty. Using Owner actor's name instead. A non-empty unique ID is required to make the object persistent."));
		}
	}

	if (ObjectUID.Len() != 0)
	{
		FPersistentData Data;
		bool bSuccess = UMagicLeapSecureStorage::GetSecureBlob<FPersistentData>(ObjectUID, Data);
		if (bSuccess)
		{
#if WITH_MLSDK
			FMemory::Memcpy(&PinnedCFUID, &Data.PinnedCFUID, sizeof(MLCoordinateFrameUID));
#endif //WITH_MLSDK
			OldComponentWorldTransform = Data.ComponentWorldTransform;
			OldCFUIDTransform = Data.CFUIDTransform;
			bDataRestored = true;
		}
	}
	else
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("ObjectUID is empty. A non-empty unique ID is required to make the object persistent."));
	}

	if ((AutoPinType == EAutoPinType::Always) || (bDataRestored && AutoPinType == EAutoPinType::OnlyOnDataRestoration))
	{
		if (bShouldPinActor)
		{
			PinActor(GetOwner());
		}
		else
		{
			PinSceneComponent(this);
		}
	}
}

void UMagicLeapARPinComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!(IMagicLeapPlugin::Get().IsMagicLeapHMDValid()))
	{
		return;
	}

	bool bDidPin = false;

	if (PinnedSceneComponent != nullptr)
	{
		if (!bPinned)
		{
			if (bDataRestored)
			{
#if WITH_MLSDK
				FVector DummyPosition = FVector::ZeroVector;
				FRotator DummyOrientation = FRotator::ZeroRotator;
				// use MLSnapshotGetTransform's return value of PoseNotFound to determine of the given PinnedCFUID is available in the current environment.
				UMagicLeapARPinFunctionLibrary::GetARPinPositionAndOrientation(PinnedCFUID, DummyPosition, DummyOrientation, bPinned);
#endif //WITH_MLSDK
			}
			else
			{
				EPassableWorldError Error = UMagicLeapARPinFunctionLibrary::GetClosestARPin(PinnedSceneComponent->GetComponentLocation(), PinnedCFUID);
				if (Error == EPassableWorldError::None)
				{
					FVector PinWorldPosition = FVector::ZeroVector;
					FRotator PinWorldOrientation = FRotator::ZeroRotator;
					bool PinFoundInEnvironment = false;
					bool bCFUIDTransform = UMagicLeapARPinFunctionLibrary::GetARPinPositionAndOrientation(PinnedCFUID, PinWorldPosition, PinWorldOrientation, PinFoundInEnvironment);
					if (bCFUIDTransform)
					{
						bPinned = true;
						OldComponentWorldTransform = PinnedSceneComponent->GetComponentToWorld();
						OldCFUIDTransform = FTransform(PinWorldOrientation, PinWorldPosition);

						if (ObjectUID.Len() != 0)
						{
							FPersistentData PinData(PinnedCFUID, OldComponentWorldTransform, OldCFUIDTransform);
							UMagicLeapSecureStorage::PutSecureBlob<FPersistentData>(ObjectUID, &PinData);
						}

					}
				}
			}

			bDidPin = bPinned;
		}

		if (bPinned)
		{
			FVector PinWorldPosition = FVector::ZeroVector;
			FRotator PinWorldOrientation = FRotator::ZeroRotator;
			bool PinFoundInEnvironment = false;
			bool bCFUIDTransform = UMagicLeapARPinFunctionLibrary::GetARPinPositionAndOrientation(PinnedCFUID, PinWorldPosition, PinWorldOrientation, PinFoundInEnvironment);
			if (bCFUIDTransform)
			{
				NewCFUIDTransform = FTransform(PinWorldOrientation.Quaternion(), PinWorldPosition);

				const FMatrix NewComponentWorldTransformMatrix = OldComponentWorldTransform.ToMatrixNoScale() * (OldCFUIDTransform.ToMatrixNoScale().Inverse() * NewCFUIDTransform.ToMatrixNoScale());
				NewComponentWorldTransform = FTransform(NewComponentWorldTransformMatrix);

				PinnedSceneComponent->SetWorldLocationAndRotation(NewComponentWorldTransform.GetLocation(), NewComponentWorldTransform.Rotator());

				if (bDidPin)
				{
					OnPersistentEntityPinned.Broadcast(bDataRestored);
				}
			}
		}
	}
}

void UMagicLeapARPinComponent::FinishDestroy()
{
	if (ObjectUID.Len() != 0 && bPinned)
	{
		FPersistentData PinData(PinnedCFUID, NewComponentWorldTransform, NewCFUIDTransform);
		UMagicLeapSecureStorage::PutSecureBlob<FPersistentData>(ObjectUID, &PinData);
	}
	Super::FinishDestroy();
}

bool UMagicLeapARPinComponent::PinSceneComponent(USceneComponent* ComponentToPin)
{
	if (ComponentToPin != nullptr)
	{
		bPinned = (PinnedSceneComponent == ComponentToPin);
		PinnedSceneComponent = ComponentToPin;
	}
	else
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("nullptr passed to UMagicLeapARPinComponent::PinSceneComponent(). Use UMagicLeapARPinComponent::UnPin() if you no longer wish for this component to be persistent or want to move the component around."))
	}
	return (ComponentToPin != nullptr);
}

bool UMagicLeapARPinComponent::PinActor(AActor* ActorToPin)
{
	if (ActorToPin != nullptr)
	{
		return PinSceneComponent(ActorToPin->GetRootComponent());
	}
	return false;
}

void UMagicLeapARPinComponent::UnPin()
{
	PinnedSceneComponent = nullptr;
	bPinned = false;
	if (ObjectUID.Len() != 0)
	{
		UMagicLeapSecureStorage::DeleteSecureData(ObjectUID);
		bDataRestored = false;
	}
}

bool UMagicLeapARPinComponent::IsPinned() const
{
	return bPinned;
}

bool UMagicLeapARPinComponent::PinRestoredOrSynced() const
{
	return bDataRestored;
}

bool UMagicLeapARPinComponent::GetPinnedPinID(FGuid& PinID)
{
	if (bPinned)
	{
		PinID = PinnedCFUID;
	}
	return bPinned;
}
