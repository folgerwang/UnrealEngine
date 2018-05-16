// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemEditorData.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterEditorData.h"

const FName UNiagaraSystemEditorFolder::GetFolderName() const
{
	return FolderName;
}

void UNiagaraSystemEditorFolder::SetFolderName(FName InFolderName)
{
	FolderName = InFolderName;
}

const TArray<UNiagaraSystemEditorFolder*>& UNiagaraSystemEditorFolder::GetChildFolders() const
{
	return ChildFolders;
}

void UNiagaraSystemEditorFolder::AddChildFolder(UNiagaraSystemEditorFolder* ChildFolder)
{
	Modify();
	ChildFolders.Add(ChildFolder);
}

void UNiagaraSystemEditorFolder::RemoveChildFolder(UNiagaraSystemEditorFolder* ChildFolder)
{
	Modify();
	ChildFolders.Remove(ChildFolder);
}

const TArray<FGuid>& UNiagaraSystemEditorFolder::GetChildEmitterHandleIds() const
{
	return ChildEmitterHandleIds;
}

void UNiagaraSystemEditorFolder::AddChildEmitterHandleId(FGuid ChildEmitterHandleId)
{
	Modify();
	ChildEmitterHandleIds.Add(ChildEmitterHandleId);
}

void UNiagaraSystemEditorFolder::RemoveChildEmitterHandleId(FGuid ChildEmitterHandleId)
{
	Modify();
	ChildEmitterHandleIds.Remove(ChildEmitterHandleId);
}

UNiagaraSystemEditorData::UNiagaraSystemEditorData(const FObjectInitializer& ObjectInitializer)
{
	RootFolder = ObjectInitializer.CreateDefaultSubobject<UNiagaraSystemEditorFolder>(this, TEXT("RootFolder"));
	StackEditorData = ObjectInitializer.CreateDefaultSubobject<UNiagaraStackEditorData>(this, TEXT("StackEditorData"));
	OwnerTransform.SetLocation(FVector(0.0f, 0.0f, 100.0f));
	PlaybackRangeMin = 0;
	PlaybackRangeMax = 10;
}

void UNiagaraSystemEditorData::PostLoadFromOwner(UObject* InOwner)
{
	UNiagaraSystem* OwnerSystem = CastChecked<UNiagaraSystem>(InOwner);

	if (RootFolder == nullptr)
	{
		RootFolder = NewObject<UNiagaraSystemEditorFolder>(this, TEXT("RootFolder"), RF_Transactional);
	}
	if (StackEditorData == nullptr)
	{
		StackEditorData = NewObject<UNiagaraStackEditorData>(this, TEXT("StackEditorData"), RF_Transactional);
	}

	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);

	if (NiagaraVer < FNiagaraCustomVersion::PlaybackRangeStoredOnSystem)
	{
		UpdatePlaybackRangeFromEmitters(OwnerSystem);
	}
}

UNiagaraSystemEditorFolder& UNiagaraSystemEditorData::GetRootFolder() const
{
	return *RootFolder;
}

UNiagaraStackEditorData& UNiagaraSystemEditorData::GetStackEditorData() const
{
	return *StackEditorData;
}

TRange<float> UNiagaraSystemEditorData::GetPlaybackRange() const
{
	return TRange<float>(PlaybackRangeMin, PlaybackRangeMax);
}

void UNiagaraSystemEditorData::SetPlaybackRange(TRange<float> InPlaybackRange)
{
	PlaybackRangeMin = InPlaybackRange.GetLowerBoundValue();
	PlaybackRangeMax = InPlaybackRange.GetUpperBoundValue();
}

void UNiagaraSystemEditorData::UpdatePlaybackRangeFromEmitters(UNiagaraSystem* OwnerSystem)
{
	if (OwnerSystem->GetEmitterHandles().Num() > 0)
	{
		float EmitterPlaybackRangeMin = TNumericLimits<float>::Max();
		float EmitterPlaybackRangeMax = TNumericLimits<float>::Lowest();

		for (const FNiagaraEmitterHandle& EmitterHandle : OwnerSystem->GetEmitterHandles())
		{
			UNiagaraEmitterEditorData* EmitterEditorData = Cast<UNiagaraEmitterEditorData>(EmitterHandle.GetInstance()->EditorData);
			if (EmitterEditorData != nullptr)
			{
				EmitterPlaybackRangeMin = FMath::Min(PlaybackRangeMin, EmitterEditorData->GetPlaybackRange().GetLowerBoundValue());
				EmitterPlaybackRangeMax = FMath::Max(PlaybackRangeMax, EmitterEditorData->GetPlaybackRange().GetUpperBoundValue());
			}
		}

		PlaybackRangeMin = EmitterPlaybackRangeMin;
		PlaybackRangeMax = EmitterPlaybackRangeMax;
	}
}