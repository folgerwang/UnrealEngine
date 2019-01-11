// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IOpenColorIOEditorModule.h"

#include "Engine/World.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Modules/ModuleManager.h"
#include "OpenColorIOColorTransform.h"
#include "OpenColorIOLibHandler.h"
#include "OpenColorIOColorSpaceConversionCustomization.h"
#include "OpenColorIOColorSpaceCustomization.h"
#include "PropertyEditorModule.h"
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

		RegisterCustomizations();
	}

	virtual void ShutdownModule() override
	{
		UnregisterCustomizations();

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
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FOpenColorIOColorSpace::StaticStruct()->GetFName());
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

	TWeakObjectPtr<UWorld> EditorWorld;
	FDelegateHandle FeatureLevelChangedDelegateHandle;
};
	


IMPLEMENT_MODULE(FOpenColorIOEditorModule, OpenColorIOEditor);

#undef LOCTEXT_NAMESPACE
