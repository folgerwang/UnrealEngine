// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneEventSectionBase.h"
#include "Engine/Blueprint.h"

#if WITH_EDITORONLY_DATA

void UMovieSceneEventSectionBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);


	if (Ar.IsLoading())
	{
		if (UBlueprint* Blueprint = DirectorBlueprint.Get())
		{
			OnBlueprintCompiledHandle = Blueprint->OnCompiled().AddUObject(this, &UMovieSceneEventSectionBase::OnBlueprintRecompiled);
		}
	}
}

void UMovieSceneEventSectionBase::SetDirectorBlueprint(UBlueprint* InBlueprint)
{
	if (DirectorBlueprint != InBlueprint)
	{
		UBlueprint* ExistingBlueprint = DirectorBlueprint.Get();
		if (ExistingBlueprint)
		{
			ExistingBlueprint->OnCompiled().Remove(OnBlueprintCompiledHandle);
		}

		DirectorBlueprint = InBlueprint;
		if (InBlueprint)
		{
			OnBlueprintCompiledHandle = InBlueprint->OnCompiled().AddUObject(this, &UMovieSceneEventSectionBase::OnBlueprintRecompiled);
		}
	}
}

#endif		// WITH_EDITORONLY_DATA