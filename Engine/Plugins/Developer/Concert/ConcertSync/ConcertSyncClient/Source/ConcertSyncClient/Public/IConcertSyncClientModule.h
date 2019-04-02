// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IConcertClientWorkspace;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnConcertClientWorkspaceStartupOrShutdown, const TSharedPtr<IConcertClientWorkspace>& /** InClientWorkspace */ );

/**
 * Interface for the Concert Sync Client module.
 */
class IConcertSyncClientModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IConcertSyncClientModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IConcertSyncClientModule >("ConcertSyncClient");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ConcertSyncClient");
	}

	/** Get the current session client workspace, if any. */
	virtual TSharedPtr<IConcertClientWorkspace> GetWorkspace() = 0;

	/** Get the delegate called on every workspace startup. */
	virtual FOnConcertClientWorkspaceStartupOrShutdown& OnWorkspaceStartup() = 0;

	/** Get the delegate called on every workspace shutdown. */
	virtual FOnConcertClientWorkspaceStartupOrShutdown& OnWorkspaceShutdown() = 0;

	/** Set whether presence is currently enabled and should be shown (unless hidden by other settings) */
	virtual void SetPresenceEnabled(const bool IsEnabled = true) = 0;

	/** Set presence visibility */
	virtual void SetPresenceVisibility(const FString& DisplayName, bool Visibility, bool PropagateToAll = false) = 0;

	/** Persist the session changes and prepare the files for source control submission. */
	virtual void PersistSessionChanges() = 0;

	/** Teleport to other presence. */
	virtual void JumpToPresence(const FGuid OtherEndpointId) = 0;

	/**
	 * Returns the path to the UWorld object opened in the editor of the specified client endpoint.
	 * The information may be unavailable if the client was disconnected, the information hasn't replicated yet
	 * or the code was not compiled as part of the UE Editor. The path returned can be the path of a play world (PIE/SIE)
	 * if the user is in PIE/SIE. It this case, the path will look like /Game/UEDPIE_10_FooMap.FooMap rather than /Game/FooMap.FooMap.
	 * @param EndpointId The end point of a clients connected to the session (local or remote).
	 * @return The path to the world being opened in the specified end point editor or an empty string if the information is not available.
	 */
	virtual FString GetPresenceWorldPath(const FGuid EndpointId) = 0;
};
