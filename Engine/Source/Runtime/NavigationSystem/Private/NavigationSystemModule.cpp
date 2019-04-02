// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NavigationSystemModule.h"
#include "EngineDefines.h"
#include "AI/NavigationSystemBase.h"
#include "Templates/SubclassOf.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "NavigationSystem"

DEFINE_LOG_CATEGORY_STATIC(LogNavSysModule, Log, All);

class FNavigationSystemModule : public INavSysModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	//virtual UNavigationSystemBase* CreateNavigationSystemInstance(UWorld& World) override;
	// End IModuleInterface
};

IMPLEMENT_MODULE(FNavigationSystemModule, NavigationSystem)

void FNavigationSystemModule::StartupModule()
{ 
	// mz@todo bind to all the delegates in FNavigationSystem

	// also, to 
}

void FNavigationSystemModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

//UNavigationSystemBase* FNavigationSystemModule::CreateNavigationSystemInstance(UWorld& World)
//{
//	UE_LOG(LogNavSysModule, Log, TEXT("Creating NavigationSystem for world %s"), *World.GetName());
//	
//	/*TSubclassOf<UNavigationSystemBase> NavSystemClass = LoadClass<UNavigationSystemBase>(NULL, *UNavigationSystemBase::GetNavigationSystemClassName().ToString(), NULL, LOAD_None, NULL);
//	return NewObject<UNavigationSystemBase>(&World, NavSystemClass);*/
//	return nullptr;
//}

#undef LOCTEXT_NAMESPACE
