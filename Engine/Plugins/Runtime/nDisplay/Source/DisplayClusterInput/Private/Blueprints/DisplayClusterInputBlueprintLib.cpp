// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterInputBlueprintLib.h"
#include "Blueprints/DisplayClusterInputBlueprintAPIImpl.h"
#include "UObject/Package.h"


UDisplayClusterInputBlueprintLib::UDisplayClusterInputBlueprintLib(class FObjectInitializer const & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDisplayClusterInputBlueprintLib::GetAPI(TScriptInterface<IDisplayClusterInputBlueprintAPI>& OutAPI)
{
	static UDisplayClusterInputBlueprintAPIImpl* Obj = NewObject<UDisplayClusterInputBlueprintAPIImpl>(GetTransientPackage(), NAME_None, RF_MarkAsRootSet);
	OutAPI = Obj;
}
