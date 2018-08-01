// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace MagicLeap
{
	/**
	* Provides an interface between the AppFramework and any system that needs to be
	* notified of application events (such as pause/resume).
	*/
	class MAGICLEAP_API IAppEventHandler
	{
	public:
		/** Adds the IAppEventHandler instance to the application's list of IAppEventHandler instances.*/
		IAppEventHandler();

		/** Removes the IAppEventHandler instance from the application's list of IAppEventHandler instances.*/
		virtual ~IAppEventHandler();

		/**
			Can be overridden by inheriting class that needs to perform certain initializations after the app is finished
			booting up.  AppFramework remains agnostic of the subsystem being initialized.
		*/
		virtual void OnAppStartup() {}

		/**
			Can be overridden by inheriting class that needs to destroy certain api interfaces before the perception stack is
			closed down.
		*/
		virtual void OnAppShutDown() {}

		/**
			Can be overridden by inheriting class in order to pause its system.  AppFramework
			remains agnostic of the subsystem being paused.
		*/
		virtual void OnAppPause() {}

		/**
			Can be overridden by inheriting class in order to resume its system.  AppFramework
			remains agnostic of the subsystem being resumed.
		*/
		virtual void OnAppResume() {}

		/**
			Pushes this object onto a worker thread so that it's blocking destructor can be called without locking up the update thread.
			@note This should only be called by objects that have an asynchronous destructor and are no longer referenced.
		*/
		void AsyncDestroy();

	protected:
		bool bWasSystemEnabledOnPause;
	};
} // MagicLeap
