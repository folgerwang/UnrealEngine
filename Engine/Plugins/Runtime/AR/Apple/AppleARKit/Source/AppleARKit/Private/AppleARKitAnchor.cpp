// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

// AppleARKit
#include "AppleARKitAnchor.h"
#include "AppleARKitModule.h"
#include "AppleARKitConversion.h"

// UE4
#include "Misc/ScopeLock.h"

FTransform UDEPRECATED_AppleARKitAnchor::GetTransform() const
{
	FScopeLock ScopeLock( &UpdateLock );

	return Transform;
}

#if SUPPORTS_ARKIT_1_0

void UDEPRECATED_AppleARKitAnchor::Update_DelegateThread( ARAnchor* Anchor )
{
	FScopeLock ScopeLock( &UpdateLock );

	// @todo arkit use World Settings WorldToMetersScale
	Transform = FAppleARKitConversion::ToFTransform( Anchor.transform );
}

#endif
