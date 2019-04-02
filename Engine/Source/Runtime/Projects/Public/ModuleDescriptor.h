// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonWriter.h"

class FJsonObject;
enum class EModuleLoadResult;

/**
 * Phase at which this module should be loaded during startup.
 */
namespace ELoadingPhase
{
	enum Type
	{
		/** As soon as possible - in other words, uplugin files are loadable from a pak file (as well as right after PlatformFile is set up in case pak files aren't used) Used for plugins needed to read files (compression formats, etc) */
		EarliestPossible,

		/** Loaded before the engine is fully initialized, immediately after the config system has been initialized.  Necessary only for very low-level hooks */
		PostConfigInit,

		/** Loaded before coreUObject for setting up manual loading screens, used for our chunk patching system */
		PreEarlyLoadingScreen,

		/** Loaded before the engine is fully initialized for modules that need to hook into the loading screen before it triggers */
		PreLoadingScreen,

		/** Right before the default phase */
		PreDefault,

		/** Loaded at the default loading point during startup (during engine init, after game modules are loaded.) */
		Default,

		/** Right after the default phase */
		PostDefault,

		/** After the engine has been initialized */
		PostEngineInit,

		/** Do not automatically load this module */
		None,

		// NOTE: If you add a new value, make sure to update the ToString() method below!
		Max
	};

	/**
	 * Converts a string to a ELoadingPhase::Type value
	 *
	 * @param	The string to convert to a value
	 * @return	The corresponding value, or 'Max' if the string is not valid.
	 */
	PROJECTS_API ELoadingPhase::Type FromString( const TCHAR *Text );

	/**
	 * Returns the name of a module load phase.
	 *
	 * @param	The value to convert to a string
	 * @return	The string representation of this enum value
	 */
	PROJECTS_API const TCHAR* ToString( const ELoadingPhase::Type Value );
};

/**
 * Environment that can load a module.
 */
namespace EHostType
{
	enum Type
	{
		Runtime,
		RuntimeNoCommandlet,
		RuntimeAndProgram,
		CookedOnly,
		Developer,
		Editor,
		EditorNoCommandlet,
		Program,		//!< Program-only plugin type
		ServerOnly,
		ClientOnly,
		// NOTE: If you add a new value, make sure to update the ToString() method below!

		Max
	};

	/**
	 * Converts a string to a EHostType::Type value
	 *
	 * @param	The string to convert to a value
	 * @return	The corresponding value, or 'Max' if the string is not valid.
	 */
	PROJECTS_API EHostType::Type FromString( const TCHAR *Text );

	/**
	 * Converts an EHostType::Type value to a string literal
	 *
	 * @param	The value to convert to a string
	 * @return	The string representation of this enum value
	 */
	PROJECTS_API const TCHAR* ToString( const EHostType::Type Value );
};

/**
 * Description of a loadable module.
 */
struct PROJECTS_API FModuleDescriptor
{
	/** Name of this module */
	FName Name;

	/** Usage type of module */
	EHostType::Type Type;

	/** When should the module be loaded during the startup sequence?  This is sort of an advanced setting. */
	ELoadingPhase::Type LoadingPhase;

	/** List of allowed platforms */
	TArray<FString> WhitelistPlatforms;

	/** List of disallowed platforms */
	TArray<FString> BlacklistPlatforms;

	/** List of allowed targets */
	TArray<FString> WhitelistTargets;

	/** List of disallowed targets */
	TArray<FString> BlacklistTargets;

	/** List of allowed target configurations */
	TArray<FString> WhitelistTargetConfigurations;

	/** List of disallowed target configurations */
	TArray<FString> BlacklistTargetConfigurations;

	/** List of allowed programs */
	TArray<FString> WhitelistPrograms;

	/** List of disallowed programs */
	TArray<FString> BlacklistPrograms;

	/** List of additional dependencies for building this module. */
	TArray<FString> AdditionalDependencies;

	/** Normal constructor */
	FModuleDescriptor(const FName InName = NAME_None, EHostType::Type InType = EHostType::Runtime, ELoadingPhase::Type InLoadingPhase = ELoadingPhase::Default);

	/** Reads a descriptor from the given JSON object */
	bool Read(const FJsonObject& Object, FText& OutFailReason);

	/** Reads an array of modules from the given JSON object */
	static bool ReadArray(const FJsonObject& Object, const TCHAR* Name, TArray<FModuleDescriptor>& OutModules, FText& OutFailReason);

	/** Writes a descriptor to JSON */
	void Write(TJsonWriter<>& Writer) const;

	/** Writes an array of modules to JSON */
	static void WriteArray(TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FModuleDescriptor>& Modules);

	/** Tests whether the module should be built for the current engine configuration */
	bool IsCompiledInCurrentConfiguration() const;

	/** Tests whether the module should be loaded for the current engine configuration */
	bool IsLoadedInCurrentConfiguration() const;

	/** Loads all the modules for a given loading phase. Returns a map of module names to load errors */
	static void LoadModulesForPhase(ELoadingPhase::Type LoadingPhase, const TArray<FModuleDescriptor>& Modules, TMap<FName, EModuleLoadResult>& ModuleLoadErrors);

#if !IS_MONOLITHIC
	/** Checks that all modules are compatible with the current engine version. Returns false and appends a list of names to OutIncompatibleFiles if not. */
	static bool CheckModuleCompatibility(const TArray<FModuleDescriptor>& Modules, TArray<FString>& OutIncompatibleFiles);
#endif
};

/** Context information used when validating that source code is being placed in the correct place for a given module */
struct FModuleContextInfo
{
	/** Path to the Source folder of the module */
	FString ModuleSourcePath;

	/** Name of the module */
	FString ModuleName;

	/** Type of this module, eg, Runtime, Editor, etc */
	EHostType::Type ModuleType;
};
