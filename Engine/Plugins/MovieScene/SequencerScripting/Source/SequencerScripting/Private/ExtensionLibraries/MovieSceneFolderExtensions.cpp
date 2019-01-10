// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExtensionLibraries/MovieSceneFolderExtensions.h"
#include "MovieSceneFolder.h"
#include "MovieSceneSequence.h"


FName UMovieSceneFolderExtensions::GetFolderName(UMovieSceneFolder* Folder)
{
	if (Folder)
	{
		return Folder->GetFolderName();
	}

	return FName();
}

bool UMovieSceneFolderExtensions::SetFolderName(UMovieSceneFolder* Folder, FName InFolderName)
{
	if (Folder)
	{
		Folder->SetFolderName(InFolderName);
		return true;
	}

	return false;
}

FColor UMovieSceneFolderExtensions::GetFolderColor(UMovieSceneFolder* Folder)
{
#if WITH_EDITORONLY_DATA
	if (Folder)
	{
		return Folder->GetFolderColor();
	}
#endif //WITH_EDITORONLY_DATA
	return FColor();
}

bool UMovieSceneFolderExtensions::SetFolderColor(UMovieSceneFolder* Folder, FColor InFolderColor)
{
#if WITH_EDITORONLY_DATA
	if (Folder)
	{
		Folder->SetFolderColor(InFolderColor);
		return true;
	}
#endif //WITH_EDITORONLY_DATA

	return false;
}

TArray<UMovieSceneFolder*> UMovieSceneFolderExtensions::GetChildFolders(UMovieSceneFolder* Folder)
{
	TArray<UMovieSceneFolder*> Result;

	if (Folder)
	{
		Result = Folder->GetChildFolders();
	}

	return Result;
}

bool UMovieSceneFolderExtensions::AddChildFolder(UMovieSceneFolder* TargetFolder, UMovieSceneFolder* FolderToAdd)
{
	if (TargetFolder && FolderToAdd)
	{
		TargetFolder->AddChildFolder(FolderToAdd);
		return true;
	}

	return false;
}

bool UMovieSceneFolderExtensions::RemoveChildFolder(UMovieSceneFolder* TargetFolder, UMovieSceneFolder* FolderToRemove)
{
	if (TargetFolder && FolderToRemove)
	{
		TargetFolder->RemoveChildFolder(FolderToRemove);
		return true;
	}

	return false;
}

TArray<UMovieSceneTrack*> UMovieSceneFolderExtensions::GetChildMasterTracks(UMovieSceneFolder* Folder)
{
	TArray<UMovieSceneTrack*> Result;

	if (Folder)
	{
		Result = Folder->GetChildMasterTracks();
	}

	return Result;
}

bool UMovieSceneFolderExtensions::AddChildMasterTrack(UMovieSceneFolder* Folder, UMovieSceneTrack* InMasterTrack)
{
	if (Folder && InMasterTrack)
	{
		Folder->AddChildMasterTrack(InMasterTrack);
		return true;
	}

	return false;
}

bool UMovieSceneFolderExtensions::RemoveChildMasterTrack(UMovieSceneFolder* Folder, UMovieSceneTrack* InMasterTrack)
{
	if (Folder && InMasterTrack)
	{
		Folder->RemoveChildMasterTrack(InMasterTrack);
		return true;
	}

	return false;
}

TArray<FSequencerBindingProxy> UMovieSceneFolderExtensions::GetChildObjectBindings(UMovieSceneFolder* Folder)
{
	TArray<FSequencerBindingProxy> Result;

	if (Folder)
	{
		// Attempt to get the sequence reference from the folder
		UMovieScene* MovieScene = Cast<UMovieScene>(Folder->GetOuter());
		UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(MovieScene->GetOuter());

		for (FGuid ID : Folder->GetChildObjectBindings())
		{
			Result.Add(FSequencerBindingProxy(ID, Sequence));
		}
	}

	return Result;
}

bool UMovieSceneFolderExtensions::AddChildObjectBinding(UMovieSceneFolder* Folder, FSequencerBindingProxy InObjectBinding)
{
	if (Folder && InObjectBinding.BindingID.IsValid())
	{
		Folder->AddChildObjectBinding(InObjectBinding.BindingID);
		return true;
	}

	return false;
}

bool UMovieSceneFolderExtensions::RemoveChildObjectBinding(UMovieSceneFolder* Folder, const FSequencerBindingProxy InObjectBinding)
{
	if (Folder && InObjectBinding.BindingID.IsValid())
	{
		Folder->RemoveChildObjectBinding(InObjectBinding.BindingID);
		return true;
	}

	return false;
}
