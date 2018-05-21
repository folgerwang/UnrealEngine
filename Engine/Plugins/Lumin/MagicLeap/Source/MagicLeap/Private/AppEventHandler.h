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
