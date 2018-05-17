// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEditorDataBase.h"
#include "NiagaraSystemEditorData.generated.h"

class UNiagaraStackEditorData;
class UNiagaraSystem;

/** Editor only folder data for emitters in a system. */
UCLASS()
class UNiagaraSystemEditorFolder : public UObject
{
	GENERATED_BODY()

public:
	const FName GetFolderName() const;

	void SetFolderName(FName InFolderName);

	const TArray<UNiagaraSystemEditorFolder*>& GetChildFolders() const;

	void AddChildFolder(UNiagaraSystemEditorFolder* ChildFolder);

	void RemoveChildFolder(UNiagaraSystemEditorFolder* ChildFolder);

	const TArray<FGuid>& GetChildEmitterHandleIds() const;

	void AddChildEmitterHandleId(FGuid ChildEmitterHandleId);

	void RemoveChildEmitterHandleId(FGuid ChildEmitterHandleId);

private:
	UPROPERTY()
	FName FolderName;

	UPROPERTY()
	TArray<UNiagaraSystemEditorFolder*> ChildFolders;

	UPROPERTY()
	TArray<FGuid> ChildEmitterHandleIds;
};

/** Editor only UI data for systems. */
UCLASS()
class UNiagaraSystemEditorData : public UNiagaraEditorDataBase
{
	GENERATED_BODY()

public:
	UNiagaraSystemEditorData(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoadFromOwner(UObject* InOwner) override;

	/** Gets the root folder for UI folders for emitters. */
	UNiagaraSystemEditorFolder& GetRootFolder() const;

	/** Gets the stack editor data for the system. */
	UNiagaraStackEditorData& GetStackEditorData() const;

	const FTransform& GetOwnerTransform() const {
		return OwnerTransform;
	}

	void SetOwnerTransform(const FTransform& InTransform)  {
		OwnerTransform = InTransform;
	}

	TRange<float> GetPlaybackRange() const;

	void SetPlaybackRange(TRange<float> InPlaybackRange);

private:
	void UpdatePlaybackRangeFromEmitters(UNiagaraSystem* OwnerSystem);

private:
	UPROPERTY(Instanced)
	UNiagaraSystemEditorFolder* RootFolder;

	UPROPERTY(Instanced)
	UNiagaraStackEditorData* StackEditorData;

	UPROPERTY()
	FTransform OwnerTransform;

	UPROPERTY()
	float PlaybackRangeMin;

	UPROPERTY()
	float PlaybackRangeMax;
};