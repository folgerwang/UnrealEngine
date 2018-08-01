// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlutilityLevelEditorExtensions.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "ActorActionUtility.h"
#include "EditorUtilityBlueprint.h"
#include "LevelEditor.h"
#include "BlueprintEditorModule.h"
#include "BlutilityMenuExtensions.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "BlutilityLevelEditorExtensions"

FDelegateHandle LevelViewportExtenderHandle;

class FBlutilityLevelEditorExtensions_Impl
{
public:
	static TSharedRef<FExtender> OnExtendLevelEditorActorContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> SelectedActors)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		// Run thru the assets to determine if any meet our criteria
		TArray<UGlobalEditorUtilityBase*> SupportedUtils;
		if (SelectedActors.Num() > 0)
		{
			// Check blueprint utils (we need to load them to query their validity against these actors)
			TArray<FAssetData> UtilAssets;
			FBlutilityMenuExtensions::GetBlutilityClasses(UtilAssets, UActorActionUtility::StaticClass()->GetFName());
			if (UtilAssets.Num() > 0)
			{
				for (AActor* Actor : SelectedActors)
				{			
					if(Actor)
					{
						for(FAssetData& UtilAsset : UtilAssets)
						{
							if(UEditorUtilityBlueprint* Blueprint = Cast<UEditorUtilityBlueprint>(UtilAsset.GetAsset()))
							{
								if(UClass* BPClass = Blueprint->GeneratedClass.Get())
								{
									if(UActorActionUtility* DefaultObject = Cast<UActorActionUtility>(BPClass->GetDefaultObject()))
									{
										UClass* SupportedClass = DefaultObject->GetSupportedClass();
										if(SupportedClass == nullptr || (SupportedClass && Actor->GetClass()->IsChildOf(SupportedClass)))
										{
											SupportedUtils.AddUnique(DefaultObject);
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (SupportedUtils.Num() > 0)
		{
			// Add asset actions extender
			Extender->AddMenuExtension(
				"ActorControl",
				EExtensionHook::After,
				CommandList,
				FMenuExtensionDelegate::CreateStatic(&FBlutilityMenuExtensions::CreateBlutilityActionsMenu, SupportedUtils));
		}

		return Extender;
	}
};

void FBlutilityLevelEditorExtensions::InstallHooks()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&FBlutilityLevelEditorExtensions_Impl::OnExtendLevelEditorActorContextMenu));
	LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void FBlutilityLevelEditorExtensions::RemoveHooks()
{
	if (LevelViewportExtenderHandle.IsValid())
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors DelegateType;
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll([=](const DelegateType& In) { return In.GetHandle() == LevelViewportExtenderHandle; });
		}
	}
}

#undef LOCTEXT_NAMESPACE
