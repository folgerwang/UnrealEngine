// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterBlueprintLib.h"
#include "DisplayClusterBlueprintAPIImpl.h"


UDisplayClusterBlueprintLib::UDisplayClusterBlueprintLib(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{

}

void UDisplayClusterBlueprintLib::GetAPI(TScriptInterface<IDisplayClusterBlueprintAPI>& OutAPI)
{
	static TUniquePtr<UDisplayClusterBlueprintAPIImpl> Obj(NewObject<UDisplayClusterBlueprintAPIImpl>());
	OutAPI = Obj.Get();
}
