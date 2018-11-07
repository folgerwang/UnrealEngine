// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if WITH_MLSDK
#include "ml_privileges.h"
#endif //WITH_MLSDK

namespace MagicLeap
{
	enum EPrivilegeState
	{
		NotYetRequested,
		Pending,
		Granted,
		Denied
	};

	struct FRequiredPrivilege
	{
#if WITH_MLSDK
		FRequiredPrivilege(MLPrivilegeID InPrivilegeID)
		: PrivilegeID(InPrivilegeID)
		, PrivilegeRequest(nullptr)
		, State(NotYetRequested)
		{}

		MLPrivilegeID PrivilegeID;
		MLPrivilegesAsyncRequest* PrivilegeRequest;
#endif // WITH_MLSDK
		EPrivilegeState State;
	};

	/**
	* Provides an interface between the AppFramework and any system that needs to be
	* notified of application events (such as pause/resume).
	*/
	class MAGICLEAP_API IAppEventHandler
	{
	public:
#if WITH_MLSDK
		/** 
			Adds the IAppEventHandler instance to the application's list of IAppEventHandler instances.
			Populates a RequiredPrivileges list based on the privilge ids passed via InRequiredPrivileges;
			@param InRequiredPrivileges The list of privilge ids required by the calling system.
		*/
		IAppEventHandler(TArray<MLPrivilegeID> InRequiredPrivileges);
#endif // WITH_MLSDK

		IAppEventHandler();

		/** Removes the IAppEventHandler instance from the application's list of IAppEventHandler instances.*/
		virtual ~IAppEventHandler();

		/**
			Can be overridden by inheriting class that needs to destroy certain api interfaces before the perception stack is
			closed down.
		*/
		virtual void OnAppShutDown() {}

		/**
			Use to check status of privilege requests.
		*/
		virtual void OnAppTick();

		/**
			Can be overridden by inheriting class in order to pause its system.
		*/
		virtual void OnAppPause() {}

		/**
			Can be overridden by inheriting class in order to resume its system.
		*/
		virtual void OnAppResume();

#if WITH_MLSDK
		/**
			Returns the status of the specified privilege.
			@param PrivilegeID The privilege id to be queried.
			@param bBlocking Flags whether or not to use the blocking query internally.
		*/
		EPrivilegeState GetPrivilegeStatus(MLPrivilegeID PrivilegeID, bool bBlocking = true);
#endif //WITH_MLSDK

		/**
			Pushes this object onto a worker thread so that it's blocking destructor can be called without locking up the update thread.
			@note This should only be called by objects that have a blocking destructor and are no longer referenced.
		*/
		void AsyncDestroy();

	protected:
#if WITH_MLSDK
		TMap<MLPrivilegeID, FRequiredPrivilege> RequiredPrivileges;
#endif // WITH_MLSDK
		bool bAllPrivilegesInSync;
		bool bWasSystemEnabledOnPause;
		FCriticalSection CriticalSection;
	};
} // MagicLeap
