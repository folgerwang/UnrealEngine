// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Bookmarks/IBookmarkTypeActions.h"
#include "EditorViewportClient.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"

#include "Engine/BookMark.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"

class FBookMarkTypeActions : public IBookmarkTypeActions
{
public:

	virtual TSubclassOf<class UBookmarkBase> GetBookmarkClass() override
	{
		return UBookMark::StaticClass();
	}

	virtual void InitFromViewport(UBookmarkBase* InBookmark, FEditorViewportClient& InViewportClient) override
	{
		if (UBookMark* Bookmark = Cast<UBookMark>(InBookmark))
		{
			if (UWorld * const World = InViewportClient.GetWorld())
			{
				// Use the rotation from the first perspective viewport can find.
				FRotator Rotation(0, 0, 0);
				if (!InViewportClient.IsOrtho())
				{
					Rotation = InViewportClient.GetViewRotation();
				}

				Bookmark->Location = InViewportClient.GetViewLocation();
				Bookmark->Rotation = Rotation;

				// Keep a record of which levels were hidden so that we can restore these with the bookmark
				Bookmark->HiddenLevels.Empty();
				for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
				{
					if (StreamingLevel && !StreamingLevel->GetShouldBeVisibleInEditor())
					{
						Bookmark->HiddenLevels.Add(StreamingLevel->GetFullName());
					}
				}
			}
		}
	}

	virtual void JumpToBookmark(UBookmarkBase* InBookmark, const TSharedPtr<struct FBookmarkBaseJumpToSettings> InSettings, FEditorViewportClient& InViewportClient) override
	{
		if (UBookMark* Bookmark = Cast<UBookMark>(InBookmark))
		{
			// Set all level editing cameras to this bookmark
			for (int32 v = 0; v < GEditor->LevelViewportClients.Num(); v++)
			{
				GEditor->LevelViewportClients[v]->SetViewLocation(Bookmark->Location);
				if (!GEditor->LevelViewportClients[v]->IsOrtho())
				{
					GEditor->LevelViewportClients[v]->SetViewRotation(Bookmark->Rotation);
				}
				GEditor->LevelViewportClients[v]->Invalidate();
			}
		}
	}
};