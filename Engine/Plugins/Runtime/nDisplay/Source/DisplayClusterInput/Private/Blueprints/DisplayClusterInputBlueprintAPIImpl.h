// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

#include "Blueprints/IDisplayClusterInputBlueprintAPI.h"
#include "DisplayClusterInputBlueprintAPIImpl.generated.h"


/**
 * Blueprint API interface implementation
 */
UCLASS()
class DISPLAYCLUSTERINPUT_API UDisplayClusterInputBlueprintAPIImpl
	: public UObject
	, public IDisplayClusterInputBlueprintAPI
{
	GENERATED_BODY()

public:
	/**
	* Binds multiple device channels to multiple keys
	*
	* @return true if success
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind VRPN Device"), Category = "DisplayClusterInput")
	virtual bool BindVrpnChannels(const FString& VrpnDeviceId, const TArray<struct FDisplayClusterInputBinding>& VrpnDeviceBinds) override;

	/**
	* Binds specified device channel to a key
	*
	* @return true if success
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind VRPN Channel"), Category = "DisplayClusterInput")
	virtual bool BindVrpnChannel(const FString& VrpnDeviceId, const int32 VrpnChannel, const FKey Target) override;

	/**
	* Create new bind from vrpn keyboard device key to UE4 target by user friendly target name
	*
	* @return True if success
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind VRPN Keyboard"), Category = "DisplayClusterInput")
	virtual bool BindVrpnKeyboard(const FString& VrpnDeviceId, const FKey VrpnKeyboardButtonName, const FKey Target) override;

	/*
	 * Sets VRPN keyboard reflection type
	 *
	 * @return true if success
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Reflect VRPN Keyboard"), Category = "DisplayClusterInput")
	virtual bool SetVrpnKeyboardReflectionMode(const FString& VrpnDeviceId, EDisplayClusterInputKeyboardReflectMode ReflectMode) override;

	/**
	* Bind VRPN tracker to a hand controller
	*
	* @return true if success
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind VRPN Tracker"), Category = "DisplayClusterInput")
	virtual bool BindVrpnTracker(const FString& VrpnDeviceId, int32 VrpnChannel, EControllerHand Target) override;
};
