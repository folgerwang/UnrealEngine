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
#include "UObject/NoExportTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapARPinComponent.generated.h"

class AActor;

/** List of possible error values for MagicLeapARPin fucntions. */
UENUM(BlueprintType)
enum class EPassableWorldError : uint8
{
	/** No error. */
	None,
	/** Map quality too low for content persistence. Continue building the map. */
	LowMapQuality,
	/** Currently unable to localize into any map. Continue building the map. */
	UnableToLocalize,
	/** AR Pin is not available at this time. */
	Unavailable,
	/** Privileges not met. Add 'PwFoundObjRead' privilege to app manifest and request it at runtime. */
	PrivilegeDenied,
	/** Invalid function parameter. */
	InvalidParam,
	/** Unspecified error. */
	UnspecifiedFailure,
	/** Privilege has been requested but not yet granted by the user. */
	PrivilegeRequestPending
};

/** Modes for automatically pinning content to real-world. */
UENUM(BlueprintType)
enum class EAutoPinType : uint8
{
	/** 
	 * Pin this component / owner actor automatically only if it was pinned in a previous run of the app or replicated over network.
	 * App needs to call PinSceneComponent() or PinActor() to pin for the very first time.
	 */
	OnlyOnDataRestoration,
	/** Always pin this component / owner actor automatically, without having to call PinSceneComponent() or PinActor() explicitely. */
	Always,
	/** Never pin this component / owner actor automatically. App will control pinning and unpinning itself. */
	Never
};

/** Direct API interface for the Magic Leap Persistent AR Pin tracker system. */
UCLASS(ClassGroup = MagicLeap)
class MAGICLEAP_API UMagicLeapARPinFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	* Returns the count of currently available AR Pins.
	* @param Count Output param for number of currently available AR Pins. Valid only if return value is EPassableWorldError::None.
	* @return Error code representing specific success or failure cases.
	*/
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	static EPassableWorldError GetNumAvailableARPins(int32& Count);

	/**
	* Returns all the AR Pins currently available.
	* @param NumRequested Max number of AR Pins to query. Pass in a negative integer to get all available Pins.
	* @param Pins Output array containing IDs of the found Pins. Valid only if return value is EPassableWorldError::None.
	* @return Error code representing specific success or failure cases.
	*/
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	static EPassableWorldError GetAvailableARPins(int32 NumRequested, TArray<FGuid>& Pins);

	/**
	* Returns the Pin closest to the target point passed in.
	* @param SearchPoint Position, in world space, to search the closest Pin to.
	* @param PinID Output param for the ID of the closest Pin. Valid only if return value is EPassableWorldError::None.
	* @return Error code representing specific success or failure cases.
	*/
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	static EPassableWorldError GetClosestARPin(const FVector& SearchPoint, FGuid& PinID);

	/**
	* Returns the world position & orientation of the requested Pin.
	* @param PinID ID of the Pin to get the position and orientation for.
	* @param Position Output param for the world position of the Pin. Valid only if return value is true.
	* @param Orientation Output param for the world orientation of the Pin. Valid only if return value is true.
	* @param PinFoundInEnvironment Output param for indicating ig the requested Pin was found user's current environment or not.
	* @return true if the PinID was valid and the position & orientation were successfully retrieved.
	*/
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	static bool GetARPinPositionAndOrientation(const FGuid& PinID, FVector& Position, FRotator& Orientation, bool& PinFoundInEnvironment);
};

/** Component to make content persist at locations in the real world. */
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API UMagicLeapARPinComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UMagicLeapARPinComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void FinishDestroy() override;

	/**
	 * Pin given SceneComponent to the closest AR Pin in real-world.
	 * OnPersistentEntityPinned event will be fired when a suitable AR Pin is found for this component.
	 * The component's transform will then be locked. App needs to call UnPin() if it wants to move the component again.
	 * @param ComponentToPin SceneComponent to pin to the world. Pass in 'this' component if app is using 'OnlyOnDataRestoration' or 'Always' AutoPinType.
	 * @return true if the component was accepted to be pinned, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	bool PinSceneComponent(USceneComponent* ComponentToPin);

	/**
	 * Pin given Actor to the closest AR Pin in real-world.
	 * OnPersistentEntityPinned event will be fired when a suitable AR Pin is found for this Actor.
	 * The Actor's transform will then be locked. App needs to call UnPin() if it wants to move the Actor again.
	 * @param ActorToPin Actor to pin to the world. Pass in this component's owner if app is using 'OnlyOnDataRestoration' or 'Always' AutoPinType.
	 * @return true if the Actor was accepted to be pinned, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	bool PinActor(AActor* ActorToPin);

	/**
	 * Detach or un-pin the currently pinned entity (component or actor) from the real-world.
	 * Call this if you want to change the transform of a pinned entity.
	 * Note that if you still want your content to persist, you will have to call PinSceneComponent() or PinActor() before EndPlay().
	 */
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	void UnPin();

	/**
	 * True if an entity (component or actor) is currently pinned by this component.
	 * If true, the entity's transform will be locked. App needs to call UnPin() if it wants to move it again.
	 * If false, and you still want your content to persist, you will have to call PinSceneComponent() or PinActor() before EndPlay().
	 * @return True if an entity (component or actor) is currently pinned by this component.
	 */
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	bool IsPinned() const;

	/**
	 * True if the AR Pin for the unique ID ObjectUID was restored from the app's local storage or was repliated over network.
	 * Implies if content was already pinned earlier. Does not imply if that restored Pin is available in the current environment.
	 * @return True if the Pin data was restored from local storage or network.
	 */
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	bool PinRestoredOrSynced() const;

	/**
	 * Get the ID of the Pin the entity (component or actor) is currently pinned to.
	 * @param PinID Output param for the ID of the Pin.
	 * @return True if an entity is currently pinned by this component and the output param is successfully populated.
	 */
	UFUNCTION(BlueprintCallable, Category = "ContentPersistence|MagicLeap")
	bool GetPinnedPinID(FGuid& PinID);

public:
	/**
	 * Unique ID for this component to save the meta data for the Pin and make content persistent.
	 * This name has to be unique across all instances of the MagicLeapARPinComponent class.
	 * If empty, the name of the owner actor will be used.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ContentPersistence|MagicLeap")
	FString ObjectUID;

	/** Mode for automatically pinning this component or it's owner actor to real-world. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ContentPersistence|MagicLeap")
	EAutoPinType AutoPinType;

	/** Pin this component's owner actor instead of just the component itself. Relevant only when using 'OnlyOnDataRestoration' or 'Always' as AutoPinType. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ContentPersistence|MagicLeap")
	bool bShouldPinActor;

	/**
	 * Delegate used to notify the instigating blueprint that an entity (component or actor) has been successfuly pinned to the real-world.
	 * Indicates that the transform of the pinned entity is now locked. App needs to call UnPin() if it wants to move the entity again.
	 * @param bRestoredOrSynced True if the entity was pinned as a result of Pin data being restored from local storage or replicatred over network, false if pinned by an explicit PinSceneComponent() or PinActor() call from the app.
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPersistentEntityPinned, bool, bRestoredOrSynced);

	/** Fired when an entity is successfully pinned by this component. */
	UPROPERTY(BlueprintAssignable)
	FPersistentEntityPinned OnPersistentEntityPinned;

private:
	UPROPERTY()
	FGuid PinnedCFUID;

	UPROPERTY()
	USceneComponent* PinnedSceneComponent;

	FTransform OldComponentWorldTransform;
	FTransform OldCFUIDTransform;
	FTransform NewComponentWorldTransform;
	FTransform NewCFUIDTransform;

	bool bPinned;
	bool bDataRestored;

	class FMagicLeapARPinTrackerImpl *Impl;
};
