// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PixelStreamingCommon.h"
#include "IInputDeviceModule.h"

/**
* The public interface to this module
*/
class IPixelStreamingPlugin : public IInputDeviceModule
{
public:

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IPixelStreamingPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IPixelStreamingPlugin >("PixelStreaming");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("PixelStreaming");
	}

	/**
	 * Returns a reference to the input device. The lifetime of this reference
	 * is that of the underlying shared pointer.
	 * @return A reference to the input device.
	 */
	virtual class FPixelStreamingInputDevice& GetInputDevice() = 0;
	
	/**
	 * Add any client config JSON to the given object which relates to
	 * configuring the input system for the pixel streaming on the browser.
	 * @param JsonObject - The JSON object to add fields to.
	 */
	virtual void AddClientConfig(TSharedRef<class FJsonObject>& JsonObject) = 0;

	/**
	 * Send a data response back to the browser where we are sending video. This
	 * could be used as a response to a UI interaction, for example.
	 * @param Descriptor - A generic descriptor string.
	 */
	virtual void SendResponse(const FString& Descriptor) = 0;
};

