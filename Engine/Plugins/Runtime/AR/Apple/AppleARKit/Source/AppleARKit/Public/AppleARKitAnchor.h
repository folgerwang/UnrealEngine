// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AppleARKitAvailability.h"

// ARKit
#if SUPPORTS_ARKIT_1_0
	#import <ARKit/ARKit.h>
#endif

// UE4
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"

// AppleARKit
#include "AppleARKitAnchor.generated.h"

UCLASS( BlueprintType, Deprecated )
class APPLEARKIT_API UDEPRECATED_AppleARKitAnchor : public UObject
{
	GENERATED_BODY()

public: 

	/**
	 * Unique identifier of the anchor.
	 */
	UPROPERTY()
	FGuid Identifier;

	/**
	 * The transformation matrix that defines the anchor's rotation, translation and scale.
	 *
	 * NOTE: This does not have Session::BaseTransform applied due to thread safety issues. You'll
	 * need to apply this yourself in the game thread.
	 *
	 * @todo arkit Fix this ^
	 */
	UFUNCTION( BlueprintPure, Category = "AppleARKit|Anchor" )
	FTransform GetTransform() const;

#if SUPPORTS_ARKIT_1_0

	virtual void Update_DelegateThread( ARAnchor* Anchor );

#endif

protected:

	// Thread safe update lock
	mutable FCriticalSection UpdateLock;

	/**
	 * The transformation matrix that defines the anchor's rotation, translation and scale in world coordinates.
	 */
	FTransform Transform;
};
