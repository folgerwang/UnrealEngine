// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Internationalization/Text.h"
#include "Internationalization/TextKey.h"

/** Caches FText instances generated via the LOCTEXT macro to avoid repeated constructions */
class FTextCache
{
public:
	/** 
	 * Get the singleton instance of the text cache.
	 */
	static FTextCache& Get();

	/**
	 * Try and find an existing cached entry for the given data, or construct and cache a new entry if one cannot be found.
	 */
	FText FindOrCache(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey);

	/**
	 * Flush all the instances currently stored in this cache and free any allocated data.
	 */
	void Flush();

private:
	TMap<FTextId, FText> CachedText;
	FCriticalSection CachedTextCS;
};
