// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerStandaloneTypes.h"

#include "EditorActorFolders.h"


#define LOCTEXT_NAMESPACE "SceneOutlinerStandaloneTypes"

namespace SceneOutliner
{
	/** Parse a new path (including leaf-name) into this tree item. Does not do any notification */
	FName GetFolderLeafName(FName InPath)
	{
		FString PathString = InPath.ToString();
		int32 LeafIndex = 0;
		if (PathString.FindLastChar('/', LeafIndex))
		{
			return FName(*PathString.RightChop(LeafIndex + 1));
		}
		else
		{
			return InPath;
		}
	}

	FName MoveFolderTo(FName InPath, FName NewParent, UWorld& World)
	{
		FName LeafName = GetFolderLeafName(InPath);

		// Get unique name
		FName NewPath = FActorFolders::Get().GetFolderName(World, NewParent, LeafName);

		if (FActorFolders::Get().RenameFolderInWorld(World, InPath, NewPath))
		{
			return NewPath;
		}

		return FName();
	}

}	// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE
