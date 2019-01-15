// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VPBookmarkBlueprintLibrary.h"
#include "VPBookmark.h"
#include "VPBookmarkModule.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectHash.h"


UVPBookmark* UVPBookmarkBlueprintLibrary::FindVPBookmark(const AActor* Actor)
{
	UVPBookmark* Result = nullptr;
	if (Actor)
	{
		if (UWorld* World = Actor->GetWorld())
		{
			if (AWorldSettings* WorldSettings = World->GetWorldSettings())
			{
				const TArray<UBookmarkBase*>& Bookmarks = WorldSettings->GetBookmarks();
				for (UBookmarkBase* Base : Bookmarks)
				{
					if (UVPBookmark* VPBookmark = Cast<UVPBookmark>(Base))
					{
						if (VPBookmark->OwnedActor.Get() == Actor)
						{
							Result = VPBookmark;
							break;
						}
					}
				}
			}
		}
	}
	return Result;
}


void UVPBookmarkBlueprintLibrary::GetAllVPBookmarkActors(const UObject* WorldContextObject, TArray<AActor*>& OutActors)
{
	OutActors.Reset();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World)
	{
		AWorldSettings* WorldSettings = World->GetWorldSettings();
		if (WorldSettings != nullptr)
		{
			const TArray<UBookmarkBase*>& Bookmarks = WorldSettings->GetBookmarks();
			OutActors.Reset(Bookmarks.Num());
			for (UBookmarkBase* Base : Bookmarks)
			{
				if (UVPBookmark* Bookmark = Cast<UVPBookmark>(Base))
				{
					if (AActor* Actor = Bookmark->OwnedActor.Get())
					{
						if (!Bookmark->IsPendingKill() && !Actor->IsPendingKill())
						{
							OutActors.Add(Actor);
						}
					}
				}
			}
		}
	}
}


void UVPBookmarkBlueprintLibrary::GetAllVPBookmark(const UObject* WorldContextObject, TArray<UVPBookmark*>& OutBookmarks)
{
	OutBookmarks.Reset();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World)
	{
		AWorldSettings* WorldSettings = World->GetWorldSettings();
		if (WorldSettings != nullptr)
		{
			const TArray<UBookmarkBase*>& Bookmarks = WorldSettings->GetBookmarks();
			OutBookmarks.Reset(Bookmarks.Num());
			for (UBookmarkBase* Base : Bookmarks)
			{
				if (UVPBookmark* Bookmark = Cast<UVPBookmark>(Base))
				{
					if (!Bookmark->IsPendingKill())
					{
						OutBookmarks.Add(Bookmark);
					}
				}
			}
		}
	}
}
