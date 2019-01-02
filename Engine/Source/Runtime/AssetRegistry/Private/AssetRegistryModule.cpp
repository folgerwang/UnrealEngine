// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "AssetRegistryModule.h"
#include "AssetRegistry.h"
#include "AssetRegistryConsoleCommands.h"

IMPLEMENT_MODULE( FAssetRegistryModule, AssetRegistry );

void FAssetRegistryModule::StartupModule()
{
	LLM_SCOPE(ELLMTag::AssetRegistry);

	AssetRegistry = MakeWeakObjectPtr(const_cast<UAssetRegistryImpl*>(GetDefault<UAssetRegistryImpl>()));
	ConsoleCommands = new FAssetRegistryConsoleCommands(*this);
}


void FAssetRegistryModule::ShutdownModule()
{
	AssetRegistry = nullptr;

	if ( ConsoleCommands )
	{
		delete ConsoleCommands;
		ConsoleCommands = NULL;
	}
}

IAssetRegistry& FAssetRegistryModule::Get() const
{
	UAssetRegistryImpl* AssetRegistryPtr = AssetRegistry.Get();
	check(AssetRegistryPtr);
	return *AssetRegistryPtr;
}

