// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertSessionHandler.h"
#include "ConcertPresenceEvents.h"
#include "ConcertMessages.h"
#include "ConcertClientPresenceActor.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"

enum class EMapChangeType : uint8;

#if WITH_EDITOR

// These "PresenceMode" classes are used to send avatar-specific presence events and cache 
// avatar-related state for the current client. 
// 
// Adding a new presence avatar type requires the following:
//
//  1) Add a presence mode class inherited from FConcertClientBasePresenceMode to 
//     send events and cache state, if needed, for the current client.
//  2) Add a presence actor class to handle events and display the avatar for
//     remote clients.
//  3) Register and unregister event handlers in FConcertClientPresenceManager.

class IConcertClientSession;
struct FConcertClientInfo;

class FConcertClientBasePresenceMode
{
public:
	explicit FConcertClientBasePresenceMode(class FConcertClientPresenceManager* InManager)
		: LastHeadTransform(FTransform::Identity)
		, ParentManager(InManager) {}
	virtual ~FConcertClientBasePresenceMode() {}

	/** Factory method to create mode based on the avatar class */
	static TUniquePtr<FConcertClientBasePresenceMode> CreatePresenceMode(const UClass* AvatarActorClass, class FConcertClientPresenceManager* InManager);

	/** Send events for this presence mode */
	virtual void SendEvents(IConcertClientSession& Session);

protected:

	/** Set event update index on an event, used for out-of-order event handling */
	virtual void SetUpdateIndex(IConcertClientSession& Session, const FName& InEventName, FConcertClientPresenceEventBase& OutEvent);

	/** Get the current head transformation */
	FTransform GetHeadTransform();

	class FConcertClientPresenceManager* GetManager() const
	{
		return ParentManager;
	}

	/** Last head transform returned. */
	FTransform LastHeadTransform;

	/** Parent manager */
	class FConcertClientPresenceManager* ParentManager;

};

class FConcertClientDesktopPresenceMode : public FConcertClientBasePresenceMode
{
public:
	explicit FConcertClientDesktopPresenceMode(class FConcertClientPresenceManager* InManager)
		: FConcertClientBasePresenceMode(InManager) {}
	virtual ~FConcertClientDesktopPresenceMode() {}

	/** Send events for this presence mode */
	virtual void SendEvents(IConcertClientSession& Session) override;

protected:
	/** Cached desktop cursor location to avoid resending changes when mouse did not move */
	FIntPoint CachedDesktopCursorLocation;
};

class FConcertClientVRPresenceMode : public FConcertClientBasePresenceMode
{
public:
	explicit FConcertClientVRPresenceMode(class FConcertClientPresenceManager* InManager)
		: FConcertClientBasePresenceMode(InManager)
		, LastRoomTransform(FTransform::Identity) {}
	virtual ~FConcertClientVRPresenceMode() {}

	/** Send events for this presence mode */
	virtual void SendEvents(IConcertClientSession& Session) override;

protected:
	/** Get the current room transformation */
	FTransform GetRoomTransform();

	/** Last room transform returned. */
	FTransform LastRoomTransform;
};



#endif
