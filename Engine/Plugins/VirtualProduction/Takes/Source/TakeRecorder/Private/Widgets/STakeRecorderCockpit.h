// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"

enum class ECheckBoxState : uint8;

struct FFrameRate;
class ULevelSequence;
class UTakeMetaData;
class UTakeRecorder;
class FUICommandList;

/**
 * Cockpit UI for defining take meta-data.
 * Interacts with UTakeMetaData stored on the level sequence, if present, otherwise uses its own transient meta-data
 */
class STakeRecorderCockpit : public SCompoundWidget, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(STakeRecorderCockpit)
		: _LevelSequence(nullptr)
		{}

		SLATE_ATTRIBUTE(ULevelSequence*, LevelSequence)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	~STakeRecorderCockpit();

	UTakeMetaData* GetMetaData() const
	{
		return TakeMetaData;
	}

	bool Reviewing() const;

	bool Recording() const;

	TSharedRef<SWidget> MakeLockButton();

	bool CanStartRecording(FText* OutErrorText) const;

	void StartRecording();

	void StopRecording();

private:

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	EVisibility GetTakeWarningVisibility() const;
	FText GetTakeWarningText() const;

	EVisibility GetRecordErrorVisibility() const;
	FText GetRecordErrorText() const; 

	void UpdateRecordError();
	void UpdateTakeError();

	EVisibility GetCountdownVisibility() const;
	FText GetCountdownText() const;

	FText GetDurationText() const;
	void SetDurationText(const FText& InNewText, ETextCommit::Type);

	FText GetUserDescriptionText() const;
	void SetUserDescriptionText(const FText& InNewText, ETextCommit::Type);

	FText GetFrameRateText() const;
	FFrameRate GetFrameRate() const;
	bool IsFrameRateCompatible(FFrameRate InFrameRate) const;

	FText GetSlateText() const;
	void SetSlateText(const FText& InNewText, ETextCommit::Type InCommitType);

	FText GetTimestampText() const;

	int32 GetTakeNumber() const;

	FReply OnSetNextTakeNumber();

	void OnBeginSetTakeNumber();

	void SetTakeNumber(int32 InNewTakeNumber);

	void SetTakeNumber_FromCommit(int32 InNewTakeNumber, ETextCommit::Type InCommitType);

	void OnEndSetTakeNumber(int32 InFinalValue);

	FReply OnAddMarkedFrame();

	void OnToggleRecording(ECheckBoxState);

	FReply NewRecordingFromThis();

	ECheckBoxState IsRecording() const;

	bool CanRecord() const;

	bool IsLocked() const;

	bool EditingMetaData() const;

	void CacheMetaData();

	void OnAssetRegistryFilesLoaded();

	void OnRecordingInitialized(UTakeRecorder*);

	void OnRecordingFinished(UTakeRecorder*);

	void BindCommands();

	void OnToggleEditPreviousRecording(ECheckBoxState CheckState);

private:

	/** Take meta-data cached from the level sequence if it exists. Referenced by AddReferencedObjects. */
	UTakeMetaData* TakeMetaData;

	/** Transient take meta data owned by this widget and kept alive by AddReferencedObjects. Only used if none exists on the level sequence already. */
	UTakeMetaData* TransientTakeMetaData;

	/** The index of a pending transaction initiated by this widget, or INDEX_NONE if none is pending */
	int32 TransactionIndex;

	TAttribute<ULevelSequence*> LevelSequenceAttribute;

	/** Text that describes why the user cannot record with the current settings */
	FText RecordErrorText;

	/** Text that describes why the user cannot record with the current settings */
	FText TakeErrorText;

	/** Whether we should auto apply the next available take number when asset discovery has finished or not */
	bool bAutoApplyTakeNumber;

	FDelegateHandle OnAssetRegistryFilesLoadedHandle;
	FDelegateHandle OnRecordingInitializedHandle, OnRecordingFinishedHandle;

	TSharedPtr<FUICommandList> CommandList;
};
