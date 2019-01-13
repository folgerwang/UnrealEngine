// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterBlueprintLib.h"
#include "Blueprints/DisplayClusterBlueprintAPIImpl.h"
#include "UObject/Package.h"


UDisplayClusterBlueprintLib::UDisplayClusterBlueprintLib(class FObjectInitializer const & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDisplayClusterBlueprintLib::GetAPI(TScriptInterface<IDisplayClusterBlueprintAPI>& OutAPI)
{
	static UDisplayClusterBlueprintAPIImpl* Obj = NewObject<UDisplayClusterBlueprintAPIImpl>(GetTransientPackage(), NAME_None, RF_MarkAsRootSet);
	OutAPI = Obj;
}
