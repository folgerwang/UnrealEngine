// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakePreset.h"
#include "LevelSequence.h"
#include "UObject/Package.h"

UTakePreset::UTakePreset(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{}

ULevelSequence* UTakePreset::GetOrCreateLevelSequence()
{
	if (!LevelSequence)
	{
		CreateLevelSequence();
	}
	return LevelSequence;
}

void UTakePreset::CreateLevelSequence()
{
	if (LevelSequence)
	{
		LevelSequence->Modify();

		FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), ULevelSequence::StaticClass(), "DEAD_TakePreset_LevelSequence");
		LevelSequence->Rename(*UniqueName.ToString());
		LevelSequence = nullptr;
	}

	// Copy the transient and transactional flags from the parent
	EObjectFlags SequenceFlags = GetFlags() & (RF_Transient | RF_Transactional);
	LevelSequence = NewObject<ULevelSequence>(this, GetFName(), SequenceFlags);
	LevelSequence->Initialize();

	LevelSequence->GetMovieScene()->SetPlaybackRange(TRange<FFrameNumber>(0, TNumericLimits<int32>::Max()-1));

	FMovieSceneEditorData& EditorData = LevelSequence->GetMovieScene()->GetEditorData();
	EditorData.ViewStart = -1.0;
	EditorData.ViewEnd   =  5.0;
	EditorData.WorkStart = -1.0;
	EditorData.WorkEnd   =  5.0;

	OnLevelSequenceChangedEvent.Broadcast();
}

void UTakePreset::CopyFrom(UTakePreset* TemplatePreset)
{
	Modify();

	if (TemplatePreset && TemplatePreset->LevelSequence)
	{
		CopyFrom(TemplatePreset->LevelSequence);
	}
	else
	{
		CreateLevelSequence();
	}
}

void UTakePreset::CopyFrom(ULevelSequence* TemplateLevelSequence)
{
	Modify();

	// Always call the sequence the same as the owning preset
	FName SequenceName = GetFName();

	if (TemplateLevelSequence)
	{
		// Rename the old one
		if (LevelSequence)
		{
			LevelSequence->Modify();

			FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), ULevelSequence::StaticClass(), "DEAD_TakePreset_LevelSequence");
			LevelSequence->Rename(*UniqueName.ToString());
		}

		EObjectFlags SequenceFlags = GetFlags() & (RF_Transient | RF_Transactional);

		LevelSequence = Cast<ULevelSequence>(StaticDuplicateObject(TemplateLevelSequence, this, SequenceName, SequenceFlags));
		LevelSequence->SetFlags(SequenceFlags);

		OnLevelSequenceChangedEvent.Broadcast();
	}
	else
	{
		CreateLevelSequence();
	}
}

FDelegateHandle UTakePreset::AddOnLevelSequenceChanged(const FSimpleDelegate& InHandler)
{
	return OnLevelSequenceChangedEvent.Add(InHandler);
}

void UTakePreset::RemoveOnLevelSequenceChanged(FDelegateHandle DelegateHandle)
{
	OnLevelSequenceChangedEvent.Remove(DelegateHandle);
}

void UTakePreset::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	if (TransactionEvent.GetChangedProperties().Contains(GET_MEMBER_NAME_CHECKED(UTakePreset, LevelSequence)))
	{
		OnLevelSequenceChangedEvent.Broadcast();
	}
}