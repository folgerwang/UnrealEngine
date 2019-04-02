// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "Engine/DeveloperSettings.h"
#include "NiagaraSpawnShortcut.h"
#include "NiagaraEditorSettings.generated.h"

USTRUCT()
struct FNiagaraNewAssetDialogConfig
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SelectedOptionIndex;

	UPROPERTY()
	FVector2D WindowSize;

	FNiagaraNewAssetDialogConfig()
	{
		SelectedOptionIndex = 0;
		WindowSize = FVector2D(450, 600);
	}
};

UCLASS(config = Niagara, defaultconfig, meta=(DisplayName="Niagara"))
class UNiagaraEditorSettings : public UDeveloperSettings
{
public:
	GENERATED_UCLASS_BODY()

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath DefaultScript;

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath DefaultDynamicInputScript;

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath DefaultFunctionScript;

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FSoftObjectPath DefaultModuleScript;

	/** Shortcut key bindings that if held down while doing a mouse click, will spawn the specified type of Niagara node.*/
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	TArray<FNiagaraSpawnShortcut> GraphCreationShortcuts;

	/** Gets whether or not auto-compile is enabled in the editors. */
	bool GetAutoCompile() const;

	/** Sets whether or not auto-compile is enabled in the editors. */
	void SetAutoCompile(bool bInAutoCompile);

	/** Gets whether or not simulations should start playing automatically when the emitter or system editor is opened, or when the data is changed in the editor. */
	bool GetAutoPlay() const;

	/** Sets whether or not simulations should start playing automatically when the emitter or system editor is opened, or when the data is changed in the editor. */
	void SetAutoPlay(bool bInAutoPlay);

	/** Gets whether or not the simulation should reset when a value on the emitter or system is changed. */
	bool GetResetSimulationOnChange() const;

	/** Sets whether or not the simulation should reset when a value on the emitter or system is changed. */
	void SetResetSimulationOnChange(bool bInResetSimulationOnChange);

	/** Gets whether or not to rerun the simulation to the current time when making modifications while paused. */
	bool GetResimulateOnChangeWhilePaused() const;

	/** Sets whether or not to rerun the simulation to the current time when making modifications while paused. */
	void SetResimulateOnChangeWhilePaused(bool bInResimulateOnChangeWhilePaused);

	FNiagaraNewAssetDialogConfig GetNewAssetDailogConfig(FName InDialogConfigKey) const;

	void SetNewAssetDialogConfig(FName InDialogConfigKey, const FNiagaraNewAssetDialogConfig& InNewAssetDialogConfig);
	
	// Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;
	// END UDeveloperSettings Interface

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNiagaraEditorSettingsChanged, const FString&, const UNiagaraEditorSettings*);

	/** Gets a multicast delegate which is called whenever one of the parameters in this settings object changes. */
	static FOnNiagaraEditorSettingsChanged& OnSettingsChanged();

	const TMap<FString, FString>& GetHLSLKeywordReplacementsMap()const { return HLSLKeywordReplacements; }
protected:
	static FOnNiagaraEditorSettingsChanged SettingsChangedDelegate;

private:
	/** Whether or not auto-compile is enabled in the editors. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	bool bAutoCompile;

	/** Whether or not simulations should start playing automatically when the emitter or system editor is opened, or when the data is changed in the editor. */
	UPROPERTY(config, EditAnywhere, Category = SimulationOptions)
	bool bAutoPlay;

	/** Whether or not the simulation should reset when a value on the emitter or system is changed. */
	UPROPERTY(config, EditAnywhere, Category = SimulationOptions)
	bool bResetSimulationOnChange;

	/** Whether or not to rerun the simulation to the current time when making modifications while paused. */
	UPROPERTY(config, EditAnywhere, Category = SimulationOptions)
	bool bResimulateOnChangeWhilePaused;

	UPROPERTY(config)
	TMap<FName, FNiagaraNewAssetDialogConfig> NewAssetDialogConfigMap;

	UPROPERTY(config)
	TMap<FString, FString> HLSLKeywordReplacements;
};
