// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextCache.h"
#include "Misc/ScopeLock.h"

FTextCache& FTextCache::Get()
{
	static FTextCache Instance;
	return Instance;
}

FText FTextCache::FindOrCache(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey)
{
	const FTextId TextId(InNamespace, InKey);

	// First try and find a cached instance
	{
		FScopeLock Lock(&CachedTextCS);
	
		const FText* FoundText = CachedText.Find(TextId);
		if (FoundText)
		{
			const FString* FoundTextLiteral = FTextInspector::GetSourceString(*FoundText);
			if (FoundTextLiteral && FCString::Strcmp(**FoundTextLiteral, InTextLiteral) == 0)
			{
				return *FoundText;
			}
		}
	}

	// Not currently cached, make a new instance...
	FText NewText = FText(InTextLiteral, TextId.GetNamespace(), TextId.GetKey(), ETextFlag::Immutable);

	// ... and add it to the cache
	{
		FScopeLock Lock(&CachedTextCS);

		CachedText.Emplace(TextId, NewText);
	}

	return NewText;
}

void FTextCache::Flush()
{
	FScopeLock Lock(&CachedTextCS);

	CachedText.Empty();
}
