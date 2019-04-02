// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Delegates/IDelegateInstance.h"
#include "UObject/GCObject.h"
#include "ITakeRecorderModule.h"

struct FLevelSequenceActionExtender;

class FExtender;
class FTakePresetActions;
class FSerializedRecorder;
class UTakeRecorderSources;
class USequencerSettings;

class FTakeRecorderModule : public ITakeRecorderModule, public FGCObject
{
public:
	FTakeRecorderModule();

	void PopulateSourcesMenu(TSharedRef<FExtender> InExtender, UTakeRecorderSources* InSources);

private:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual FDelegateHandle RegisterSourcesMenuExtension(const FOnExtendSourcesMenu& InExtension) override;
	virtual void UnregisterSourcesMenuExtension(FDelegateHandle Handle) override;
	virtual void RegisterSettingsObject(UObject* InSettingsObject) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:

	void RegisterDetailCustomizations();

	void UnregisterDetailCustomizations();

	void RegisterLevelEditorExtensions();

	void UnregisterLevelEditorExtensions();

	void RegisterAssetTools();

	void UnregisterAssetTools();

	void RegisterSettings();

	void UnregisterSettings();

	void RegisterSerializedRecorder();

	void UnregisterSerializedRecorder();

private:

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExtendSourcesMenuEvent, TSharedRef<FExtender>, UTakeRecorderSources*);

	FOnExtendSourcesMenuEvent SourcesMenuExtenderEvent;

	FDelegateHandle LevelEditorLayoutExtensionHandle;
	FDelegateHandle LevelEditorTabManagerChangedHandle;
	FDelegateHandle ModulesChangedHandle;

	/** Cached name of the project settings fo de-registration of details customizations on shutdown (after UObject destruction) */
	FName ProjectSettingsName;

	TSharedPtr<FTakePresetActions> TakePresetActions;
	TSharedPtr<FLevelSequenceActionExtender> LevelSequenceAssetActionExtender;
	TSharedPtr<FSerializedRecorder> SerializedRecorder;

	USequencerSettings* SequencerSettings;
};