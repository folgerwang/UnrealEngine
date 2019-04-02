// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IOpenColorIOEditorModule.h"

#include "AssetTypeActions_OpenColorIOConfiguration.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Modules/ModuleManager.h"
#include "OpenColorIOLibHandler.h"
#include "OpenColorIOColorSpaceConversionCustomization.h"
#include "OpenColorIOColorSpaceCustomization.h"
#include "OpenColorIOColorTransform.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "UObject/StrongObjectPtr.h"


DEFINE_LOG_CATEGORY(LogOpenColorIOEditor);

#define LOCTEXT_NAMESPACE "OpenColorIOEditorModule"

/**
 * Implements the OpenColorIOEditor module.
 */
class FOpenColorIOEditorModule : public IOpenColorIOEditorModule
{
public:

	virtual bool IsInitialized() const override { return FOpenColorIOLibHandler::IsInitialized(); }

	virtual void StartupModule() override
	{
		FOpenColorIOLibHandler::Initialize();

		FWorldDelegates::OnPreWorldInitialization.AddRaw(this, &FOpenColorIOEditorModule::OnWorldInit);

		// Register asset type actions for OpenColorIOConfiguration class
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		TSharedRef<IAssetTypeActions> OpenColorIOConfigurationAssetTypeAction = MakeShared<FAssetTypeActions_OpenColorIOConfiguration>();
		AssetTools.RegisterAssetTypeActions(OpenColorIOConfigurationAssetTypeAction);
		RegisteredAssetTypeActions.Add(OpenColorIOConfigurationAssetTypeAction);
		
		RegisterCustomizations();
		RegisterStyle();
	}

	virtual void ShutdownModule() override
	{
		UnregisterStyle();
		UnregisterCustomizations();

		// Unregister AssetTypeActions
		FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

		if (AssetToolsModule != nullptr)
		{
			IAssetTools& AssetTools = AssetToolsModule->Get();

			for (const TSharedRef<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
			{
				AssetTools.UnregisterAssetTypeActions(Action);
			}
		}

		CleanFeatureLevelDelegate();
		FWorldDelegates::OnPreWorldInitialization.RemoveAll(this);

		FOpenColorIOLibHandler::Shutdown();
	}

	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout(FOpenColorIOColorConversionSettings::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FOpenColorIOColorSpaceConversionCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout(FOpenColorIOColorSpace::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FOpenColorIOColorSpaceCustomization::MakeInstance));
	}

	void UnregisterCustomizations()
	{
		if (UObjectInitialized() == true)
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomPropertyTypeLayout(FOpenColorIOColorSpace::StaticStruct()->GetFName());
			PropertyModule.UnregisterCustomPropertyTypeLayout(FOpenColorIOColorConversionSettings::StaticStruct()->GetFName());
		}
	}

	void OnWorldInit(UWorld* InWorld, const UWorld::InitializationValues InInitializationValues)
	{
		if (InWorld && InWorld->WorldType == EWorldType::Editor)
		{
			CleanFeatureLevelDelegate();
			EditorWorld = MakeWeakObjectPtr(InWorld);

			FOnFeatureLevelChanged::FDelegate FeatureLevelChangedDelegate = FOnFeatureLevelChanged::FDelegate::CreateRaw(this, &FOpenColorIOEditorModule::OnLevelEditorFeatureLevelChanged);
			FeatureLevelChangedDelegateHandle = EditorWorld->AddOnFeatureLevelChangedHandler(FeatureLevelChangedDelegate);
		}
	}

	void OnLevelEditorFeatureLevelChanged(ERHIFeatureLevel::Type InFeatureLevel)
	{
		UOpenColorIOColorTransform::AllColorTransformsCacheResourceShadersForRendering();
	}

	void CleanFeatureLevelDelegate()
	{
		if (FeatureLevelChangedDelegateHandle.IsValid())
		{
			UWorld* RegisteredWorld = EditorWorld.Get();
			if (RegisteredWorld)
			{
				RegisteredWorld->RemoveOnFeatureLevelChangedHandler(FeatureLevelChangedDelegateHandle);
			}

			FeatureLevelChangedDelegateHandle.Reset();
		}
	}

private:

	void RegisterStyle()
	{
#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleInstance->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

		StyleInstance = MakeUnique<FSlateStyleSet>("OpenColorIOStyle");

		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OpenColorIO"));
		if (Plugin.IsValid())
		{
			StyleInstance->SetContentRoot(FPaths::Combine(Plugin->GetContentDir(), TEXT("Editor/Icons")));
		}

		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);

		StyleInstance->Set("ClassThumbnail.OpenColorIOConfiguration", new IMAGE_BRUSH("OpenColorIOConfigIcon_64x", Icon64x64));
		StyleInstance->Set("ClassIcon.OpenColorIOConfiguration", new IMAGE_BRUSH("OpenColorIOConfigIcon_20x", Icon20x20));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance.Get());

#undef IMAGE_BRUSH
	}

	void UnregisterStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance.Get());
		StyleInstance.Reset();
	}

private:

	TWeakObjectPtr<UWorld> EditorWorld;
	FDelegateHandle FeatureLevelChangedDelegateHandle;
	TUniquePtr<FSlateStyleSet> StyleInstance;
	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
};
	

IMPLEMENT_MODULE(FOpenColorIOEditorModule, OpenColorIOEditor);

#undef LOCTEXT_NAMESPACE
