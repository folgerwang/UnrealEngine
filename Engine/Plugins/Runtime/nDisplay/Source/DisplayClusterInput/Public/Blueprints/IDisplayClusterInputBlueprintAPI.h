// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DisplayClusterInputTypes.h"

#include "IDisplayClusterInputBlueprintAPI.generated.h"


UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class DISPLAYCLUSTERINPUT_API UDisplayClusterInputBlueprintAPI : public UInterface
{
	GENERATED_BODY()
};


/**
 * Blueprint API interface
 */
class DISPLAYCLUSTERINPUT_API IDisplayClusterInputBlueprintAPI
{
	GENERATED_BODY()

public:
	/**
	* Binds multiple device channels to multiple keys
	*
	* @return true if success
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind VRPN Channels"), Category = "DisplayClusterInput")
	virtual bool BindVrpnChannels(const FString& VrpnDeviceId, const TArray<struct FDisplayClusterInputBinding>& VrpnDeviceBinds) = 0;

	/**
	* Binds specified device channel to a key
	*
	* @return true if success
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind VRPN Channel"), Category = "DisplayClusterInput")
	virtual bool BindVrpnChannel(const FString& VrpnDeviceId, const int32 VrpnChannel, const FKey Target) = 0;

	/**
	* Create new bind from vrpn keyboard device key to UE4 target by user friendly target name
	*
	* @return True if success
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind VRPN Keyboard"), Category = "DisplayClusterInput")
	virtual bool BindVrpnKeyboard(const FString& VrpnDeviceId, const FKey VrpnKeyboardButtonName, const FKey Target) = 0;

	/*
	 * Sets VRPN keyboard reflection type
	 *
	 * @return true if success
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set VRPN Keyboard Reflection"), Category = "DisplayClusterInput")
	virtual bool SetVrpnKeyboardReflectionMode(const FString& VrpnDeviceId, EDisplayClusterInputKeyboardReflectMode ReflectMode) = 0;

	/**
	* Bind VRPN tracker to a hand controller
	*
	* @return true if success
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind VRPN Tracker"), Category = "DisplayClusterInput")
	virtual bool BindVrpnTracker(const FString& VrpnDeviceId, int32 VrpnChannel, EControllerHand Target) = 0;
};
