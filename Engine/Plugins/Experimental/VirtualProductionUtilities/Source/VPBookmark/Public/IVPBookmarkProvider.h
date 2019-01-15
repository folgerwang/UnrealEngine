// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IVPBookmarkProvider.generated.h"


class UVPBookmark;


UINTERFACE(BlueprintType)
class VPBOOKMARK_API UVPBookmarkProvider : public UInterface
{
	GENERATED_BODY()
};


class VPBOOKMARK_API IVPBookmarkProvider : public IInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, CallInEditor, Category = "Bookmarks")
	void OnBookmarkActivation(UVPBookmark* Bookmark, bool bActivate);

	UFUNCTION(BlueprintNativeEvent, CallInEditor, Category = "Bookmarks")
	void OnBookmarkChanged(UVPBookmark* Bookmark);
};
