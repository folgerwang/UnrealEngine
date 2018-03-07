// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IMixedRealityFrameworkModule.h"
#include "Modules/ModuleManager.h" // for IMPLEMENT_MODULE()
#include "Engine/Engine.h"
#include "MixedRealityConfigurationSaveGame.h" // for SaveSlotName/UserIndex
#include "Kismet/GameplayStatics.h" // for DoesSaveGameExist()
#include "MixedRealityCaptureActor.h"
#include "MixedRealityCaptureComponent.h"
#include "UObject/UObjectIterator.h"
#include "MotionDelayBuffer.h" // for SetEnabled()
#include "UObject/UObjectGlobals.h" // for FCoreUObjectDelegates::PostLoadMapWithWorld

#if WITH_EDITOR
#include "Editor.h" // for FEditorDelegates::PostPIEStarted
#endif

class FMixedRealityFrameworkModule : public IMixedRealityFrameworkModule
{
public:
	FMixedRealityFrameworkModule();

public:
	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnWorldCreated(UWorld* NewWorld);
#if WITH_EDITOR
	void OnPieWorldCreated(bool bIsSimulating);
#endif

private:
	FDelegateHandle WorldEventBinding;
	FString TargetConfigName;
	int32 TargetConfigIndex;
};

FMixedRealityFrameworkModule::FMixedRealityFrameworkModule()
	: TargetConfigIndex(0)
{}

void FMixedRealityFrameworkModule::StartupModule()
{
	const UMixedRealityConfigurationSaveGame* DefaultSaveData = GetDefault<UMixedRealityConfigurationSaveGame>();
	TargetConfigName  = DefaultSaveData->SaveSlotName;
	TargetConfigIndex = DefaultSaveData->UserIndex;

	{
		WorldEventBinding = GEngine->OnWorldAdded().AddRaw(this, &FMixedRealityFrameworkModule::OnWorldCreated);
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FMixedRealityFrameworkModule::OnWorldCreated);

#if WITH_EDITOR
		FEditorDelegates::PostPIEStarted.AddRaw(this, &FMixedRealityFrameworkModule::OnPieWorldCreated);
#endif
	}

	FMotionDelayService::SetEnabled(true);
}

void FMixedRealityFrameworkModule::ShutdownModule()
{
#if WITH_EDITOR
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
#endif

	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	if (GEngine)
	{
		GEngine->OnWorldAdded().Remove(WorldEventBinding);
	}
}

void FMixedRealityFrameworkModule::OnWorldCreated(UWorld* NewWorld)
{
#if WITH_EDITORONLY_DATA
	const bool bIsGameInst = !IsRunningCommandlet() && NewWorld->IsGameWorld();
	if (bIsGameInst)
#endif 
	{
		const bool bHasMRConfigFile = UGameplayStatics::DoesSaveGameExist(TargetConfigName, TargetConfigIndex);
		if (bHasMRConfigFile)
		{
			UMixedRealityCaptureComponent* MRCaptureComponent = nullptr;
			for (TObjectIterator<UMixedRealityCaptureComponent> ObjIt; ObjIt; ++ObjIt)
			{
				if (ObjIt->GetWorld() == NewWorld)
				{
					MRCaptureComponent = *ObjIt;
				}
			}

			if (MRCaptureComponent == nullptr)
			{
				AMixedRealityCaptureActor* MRActor = NewWorld->SpawnActor<AMixedRealityCaptureActor>();
				MRCaptureComponent = MRActor->CaptureComponent;
			}

			MRCaptureComponent->LoadConfiguration(TargetConfigName, TargetConfigIndex);
		}
	}
}

#if WITH_EDITOR
void FMixedRealityFrameworkModule::OnPieWorldCreated(bool bIsSimulating)
{
	UWorld* PieWorld = GEditor->GetPIEWorldContext()->World();
	if (!bIsSimulating && PieWorld)
	{
		OnWorldCreated(PieWorld);
	}
}
#endif // WITH_EDITOR

IMPLEMENT_MODULE(FMixedRealityFrameworkModule, MixedRealityFramework);
