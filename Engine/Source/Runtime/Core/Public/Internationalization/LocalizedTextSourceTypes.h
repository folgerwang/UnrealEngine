// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/EnumClassFlags.h"

/**
 * Categories of localized text.
 * @note This enum is mirrored in NoExportTypes.h for UHT.
 */
enum class ELocalizedTextSourceCategory : uint8
{
	Game,
	Engine,
	Editor,
};

/**
 * Load flags used in localization initialization.
 */
enum class ELocalizationLoadFlags : uint8
{
	/** Load no data */
	None = 0,

	/** Load native data */
	Native = 1<<0,

	/** Load editor localization data */
	Editor = 1<<1,

	/** Load game localization data */
	Game = 1<<2,

	/** Load engine localization data */
	Engine = 1<<3,

	/** Load additional (eg, plugin) localization data */
	Additional = 1<<4,

	/** Force localized game data to be loaded, even when running in the editor */
	ForceLocalizedGame = 1<<5,
};
ENUM_CLASS_FLAGS(ELocalizationLoadFlags);

/**
 * Pre-defined priorities for ILocalizedTextSource.
 */
struct ELocalizedTextSourcePriority
{
	enum Enum
	{
		Lowest = -1000,
		Low = -100,
		Normal = 0,
		High = 100,
		Highest = 1000,
	};
};
