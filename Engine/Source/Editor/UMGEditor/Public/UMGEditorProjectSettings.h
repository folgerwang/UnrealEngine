// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "UMGEditorProjectSettings.generated.h"

class UWidgetCompilerRule;
class UUserWidget;
class UWidgetBlueprint;
class UPanelWidget;

USTRUCT()
struct FDebugResolution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Resolution)
	int32 Width;

	UPROPERTY(EditAnywhere, Category = Resolution)
	int32 Height;

	UPROPERTY(EditAnywhere, Category=Resolution)
	FString Description;

	UPROPERTY(EditAnywhere, Category = Resolution)
	FLinearColor Color;
};

/** Controls the level of support you want to have for widget property binding. */
UENUM()
enum class EPropertyBindingPermissionLevel : uint8
{
	/** Freely allow the use of property binding. */
	Allow,
	/**
	 * Prevent any new property binding, will still allow you to edit widgets with property binding, but
	 * the buttons will be missing on all existing widgets that don't have bindings.
	 */
	Prevent,
	/**
	 * Prevent any new property binding, and warn when compiling any existing bindings.
	 */
	PreventAndWarn,
	/**
	* Prevent any new property binding, and error when compiling any existing bindings.
	*/
	PreventAndError
};

USTRUCT()
struct FWidgetCompilerOptions
{
	GENERATED_BODY()

public:

	/**
	 * As a precaution, the slow construction widget tree is cooked in case some non-fast construct widget
	 * needs it.  If your project does not need the slow path at all, then disable this, so that you can re-coop
	 * that memory.
	 */
	UPROPERTY(EditAnywhere, Category = Compiler)
	bool bCookSlowConstructionWidgetTree = true;

	/**
	 * By default all widgets can be dynamically created.  By disabling this by default you require widgets
	 * to opt into it, which saves memory, because a template doesn't need to be constructed for it.
	 */
	UPROPERTY(EditAnywhere, Category = Compiler)
	bool bWidgetSupportsDynamicCreation = true;

	/**
	 * If you disable this, these widgets these compiler options apply to will not be allowed to implement Tick.
	 */
	UPROPERTY(EditAnywhere, Category = Compiler)
	bool bAllowBlueprintTick = true;

	/**
	 * If you disable this, these widgets these compiler options apply to will not be allowed to implement Paint.
	 */
	UPROPERTY(EditAnywhere, Category = Compiler)
	bool bAllowBlueprintPaint = true;

	/**
	 * Controls if you allow property bindings in widgets.  They can have a large performance impact if used.
	 */
	UPROPERTY(EditAnywhere, Category = Compiler)
	EPropertyBindingPermissionLevel PropertyBindingRule = EPropertyBindingPermissionLevel::Allow;

	/**
	 * Custom rules.
	 */
	UPROPERTY(EditAnywhere, Category = Compiler)
	TArray<TSoftClassPtr<UWidgetCompilerRule>> Rules;

	//TODO Allow limiting delay nodes?
	//TODO Allow limiting 'size of blueprint'
	//TODO Allow preventing blueprint inheritance
};


/**  */
USTRUCT()
struct FDirectoryWidgetCompilerOptions
{
	GENERATED_BODY()

public:

	/** The directory to limit the rules effects to. */
	UPROPERTY(EditAnywhere, Category = Compiler, meta = (ContentDir))
	FDirectoryPath Directory;

	/** These widgets are ignored, and they will use the next most applicable directory to determine their rules. */
	UPROPERTY(EditAnywhere, Category = Compiler)
	TArray<TSoftObjectPtr<UWidgetBlueprint>> IgnoredWidgets;

	/** The directory specific compiler options for these widgets. */
	UPROPERTY(EditAnywhere, Category = Compiler)
	FWidgetCompilerOptions Options;
};

/**
 * Implements the settings for the UMG Editor Project Settings
 */
UCLASS(config=Editor, defaultconfig)
class UMGEDITOR_API UUMGEditorProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUMGEditorProjectSettings();

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	// Begin UObject Interface
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
	// End UObject Interface
#endif

protected:

	UPROPERTY(EditAnywhere, config, Category = Compiler)
	FWidgetCompilerOptions DefaultCompilerOptions;

	UPROPERTY(EditAnywhere, config, Category = Compiler)
	TArray<FDirectoryWidgetCompilerOptions> DirectoryCompilerOptions;

public:

	UPROPERTY(EditAnywhere, config, Category="Class Filtering")
	bool bShowWidgetsFromEngineContent;

	UPROPERTY(EditAnywhere, config, Category="Class Filtering")
	bool bShowWidgetsFromDeveloperContent;

	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category="Class Filtering")
	TArray<FString> CategoriesToHide;

	UPROPERTY(EditAnywhere, config, Category = "Class Filtering", meta = (MetaClass = "Widget"))
	TArray<FSoftClassPath> WidgetClassesToHide;

public:

	/** The panel widget to place at the root of all newly constructed widget blueprints. Can be empty. */
	UPROPERTY(EditAnywhere, config, Category = Designer)
	TSubclassOf<UPanelWidget> DefaultRootWidget;

	UPROPERTY(EditAnywhere, config, Category=Designer)
	TArray<FDebugResolution> DebugResolutions;

	bool CompilerOption_SupportsDynamicCreation(const class UWidgetBlueprint* WidgetBlueprint) const;
	bool CompilerOption_CookSlowConstructionWidgetTree(const class UWidgetBlueprint* WidgetBlueprint) const;
	bool CompilerOption_AllowBlueprintTick(const class UWidgetBlueprint* WidgetBlueprint) const;
	bool CompilerOption_AllowBlueprintPaint(const class UWidgetBlueprint* WidgetBlueprint) const;
	EPropertyBindingPermissionLevel CompilerOption_PropertyBindingRule(const class UWidgetBlueprint* WidgetBlueprint) const;
	TArray<UWidgetCompilerRule*> CompilerOption_Rules(const class UWidgetBlueprint* WidgetBlueprint) const;

private:
	template<typename ReturnType, typename T>
	ReturnType GetFirstCompilerOption(const class UWidgetBlueprint* WidgetBlueprint, T FWidgetCompilerOptions::* OptionMember, ReturnType bDefaultValue) const
	{
		ReturnType bValue = bDefaultValue;
		GetCompilerOptionsForWidget(WidgetBlueprint, [&bValue, OptionMember](const FWidgetCompilerOptions& Options) {
			bValue = Options.*OptionMember;
			return true;
		});
		return bValue;
	}

	void GetCompilerOptionsForWidget(const class UWidgetBlueprint* WidgetBlueprint, TFunctionRef<bool(const FWidgetCompilerOptions&)> Operator) const;

protected:
	virtual void PerformUpgradeStepForVersion(int32 ForVersion);

	UPROPERTY(config)
	int32 Version;

	/** This one is unsaved, we compare it on post init to see if the save matches real */
	int32 CurrentVersion;

private:
	UPROPERTY(config)
	bool bCookSlowConstructionWidgetTree_DEPRECATED;

	UPROPERTY(config)
	bool bWidgetSupportsDynamicCreation_DEPRECATED;
};
