// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterInputBlueprintAPIImpl.h"

#include "IDisplayClusterInputModule.h"
#include "Misc/DisplayClusterInputLog.h"
#include "Misc/DisplayClusterInputHelpers.h"
#include "UObject/Package.h"


bool UDisplayClusterInputBlueprintAPIImpl::BindVrpnChannels(const FString& VrpnDeviceId, const TArray<struct FDisplayClusterInputBinding>& VrpnDeviceBinds)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputBP);

	for (const auto& It : VrpnDeviceBinds)
	{
		BindVrpnChannel(VrpnDeviceId, It.VrpnChannel, It.Target);
	}

	return true;
}

bool UDisplayClusterInputBlueprintAPIImpl::BindVrpnChannel(const FString& VrpnDeviceId, const int32 VrpnChannel, const FKey Target)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputBP);

	if(!Target.IsValid() || VrpnChannel < 0 || VrpnDeviceId.IsEmpty())
	{
		UE_LOG(LogDisplayClusterInputBP, Error, TEXT("Can't bind %s:%d to %s"), *VrpnDeviceId, VrpnChannel, *Target.ToString());
		return false;
	}

	return IDisplayClusterInputModule::Get().BindVrpnChannel(VrpnDeviceId, VrpnChannel, Target.GetDisplayName().ToString());
}

bool UDisplayClusterInputBlueprintAPIImpl::BindVrpnKeyboard(const FString& VrpnDeviceId, const FKey Key, const FKey Target)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputBP);

	if (!Key.IsValid() || !Target.IsValid() || VrpnDeviceId.IsEmpty())
	{
		UE_LOG(LogDisplayClusterInputBP, Error, TEXT("Can't bind %s#%s to %s"), *VrpnDeviceId, *Key.GetDisplayName().ToString(), *Target.GetDisplayName().ToString());
		return false;
	}

	int32 VrpnChannel;
	if (!DisplayClusterInputHelpers::KeyNameToVrpnScancode(Key.GetDisplayName().ToString(), VrpnChannel))
	{
		UE_LOG(LogDisplayClusterInputBP, Error, TEXT("Couldn't map key name %s to VRPN scancode"), *Key.GetDisplayName().ToString());
		return false;
	}

	return IDisplayClusterInputModule::Get().BindVrpnChannel(VrpnDeviceId, VrpnChannel, Target.GetDisplayName().ToString());
}

bool UDisplayClusterInputBlueprintAPIImpl::SetVrpnKeyboardReflectionMode(const FString& VrpnDeviceId, EDisplayClusterInputKeyboardReflectMode ReflectMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputBP);

	return IDisplayClusterInputModule::Get().SetVrpnKeyboardReflectionMode(VrpnDeviceId, ReflectMode);
}

bool UDisplayClusterInputBlueprintAPIImpl::BindVrpnTracker(const FString& VrpnDeviceId, int32 VrpnChannel, EControllerHand Target)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputBP);

	const FString ObjName(TEXT("EControllerHand"));
	const UEnum* const EnumPtr = FindObject<UEnum>(ANY_PACKAGE, *ObjName, true);
	if (!EnumPtr)
	{
		UE_LOG(LogDisplayClusterInputBP, Error, TEXT("Couldn't find %s object"), *ObjName);
		return false;
	}

	FName TargetName = EnumPtr->GetNameByValue((int64)Target);
	return IDisplayClusterInputModule::Get().BindVrpnChannel(VrpnDeviceId, VrpnChannel, TargetName.ToString());
}
