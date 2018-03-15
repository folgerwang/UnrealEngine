// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IKeyArea.h"
#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SNullWidget.h"
#include "ISequencerChannelInterface.h"
#include "SequencerClipboardReconciler.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "CurveModel.h"

IKeyArea::IKeyArea(
	uint32                               InChannelTypeID,
	UMovieSceneSection&                  InSection,
	TMovieSceneChannelHandle<void>       InChannel,
	const FMovieSceneChannelEditorData&  InCommonEditorData,
	TMovieSceneChannelHandle<const void> InSpecializedData
)
	: ChannelTypeID(InChannelTypeID)
	, WeakOwningSection(&InSection)
	, Channel(InChannel)
	, SpecializedData(InSpecializedData)
	, Color(InCommonEditorData.Color)
	, ChannelName(InCommonEditorData.Name)
	, DisplayText(InCommonEditorData.DisplayText)
{
	UMovieScenePropertyTrack* PropertyTrack = InSection.GetTypedOuter<UMovieScenePropertyTrack>();
	if (PropertyTrack && !PropertyTrack->GetPropertyPath().IsEmpty())
	{
		PropertyBindings = FTrackInstancePropertyBindings(PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath());
	}
}

void* IKeyArea::GetChannelPtr() const
{
	return Channel.Get();
}

const void* IKeyArea::GetSpecializedEditorData() const
{
	return SpecializedData.Get();
}

UMovieSceneSection* IKeyArea::GetOwningSection() const
{
	return WeakOwningSection.Get();
}

FName IKeyArea::GetName() const
{
	return ChannelName;
}

void IKeyArea::SetName(FName InName)
{
	ChannelName = InName;
}

ISequencerChannelInterface* IKeyArea::FindChannelInterface() const
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	ISequencerChannelInterface* ChannelInterface = SequencerModule.FindChannelInterface(ChannelTypeID);
	ensureMsgf(ChannelInterface, TEXT("No channel interface found for type ID 0x%08x. Did you forget to call ISequencerModule::RegisterChannelInterface<ChannelType>()?"), ChannelTypeID);
	return ChannelInterface;
}

bool IKeyArea::HasAnyKeys() const
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();
	void *const RawChannelPtr = GetChannelPtr();

	return ChannelInterface && RawChannelPtr && ChannelInterface->HasAnyKeys_Raw(MakeArrayView(&RawChannelPtr, 1));
}

FKeyHandle IKeyArea::AddOrUpdateKey(FFrameNumber Time, const FGuid& ObjectBindingID, ISequencer& InSequencer)
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();
	void* RawChannelPtr = GetChannelPtr();

	// The specialized data may be null, but is passed to the interface regardless
	const void* RawSpecializedData = GetSpecializedEditorData();

	if (ChannelInterface && RawChannelPtr)
	{
		FTrackInstancePropertyBindings* BindingsPtr = PropertyBindings.IsSet() ? &PropertyBindings.GetValue() : nullptr;
		return ChannelInterface->AddOrUpdateKey_Raw(RawChannelPtr, RawSpecializedData, Time, InSequencer, ObjectBindingID, BindingsPtr);
	}

	return FKeyHandle();
}

FKeyHandle IKeyArea::DuplicateKey(FKeyHandle InKeyHandle) const
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();

	FKeyHandle NewHandle = FKeyHandle::Invalid();

	void* RawChannelPtr = GetChannelPtr();
	if (ChannelInterface && RawChannelPtr)
	{
		ChannelInterface->DuplicateKeys_Raw(RawChannelPtr, TArrayView<const FKeyHandle>(&InKeyHandle, 1), TArrayView<FKeyHandle>(&NewHandle, 1));
	}
	return NewHandle;
}

void IKeyArea::SetKeyTimes(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<const FFrameNumber> InKeyTimes) const
{
	check(InKeyHandles.Num() == InKeyTimes.Num());

	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();
	void* RawChannelPtr = GetChannelPtr();

	if (ChannelInterface && RawChannelPtr)
	{
		ChannelInterface->SetKeyTimes_Raw(RawChannelPtr, InKeyHandles, InKeyTimes);
	}
}

void IKeyArea::GetKeyTimes(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FFrameNumber> OutTimes) const
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();

	void* RawChannelPtr = GetChannelPtr();
	if (ChannelInterface && RawChannelPtr)
	{
		ChannelInterface->GetKeyTimes_Raw(RawChannelPtr, InKeyHandles, OutTimes);
	}
}

void IKeyArea::GetKeyInfo(TArray<FKeyHandle>* OutHandles, TArray<FFrameNumber>* OutTimes, const TRange<FFrameNumber>& WithinRange) const
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();

	void* RawChannelPtr = GetChannelPtr();
	if (ChannelInterface && RawChannelPtr)
	{
		ChannelInterface->GetKeys_Raw(RawChannelPtr, WithinRange, OutTimes, OutHandles);
	}
}

TSharedPtr<FStructOnScope> IKeyArea::GetKeyStruct(FKeyHandle KeyHandle) const
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();

	if (ChannelInterface && Channel.Get())
	{
		return ChannelInterface->GetKeyStruct_Raw(Channel, KeyHandle);
	}
	return nullptr;
}

void IKeyArea::DrawKeys(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams)
{
	check(InKeyHandles.Num() == OutKeyDrawParams.Num());

	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();

	void* RawChannelPtr = GetChannelPtr();
	if (ChannelInterface && RawChannelPtr)
	{
		return ChannelInterface->DrawKeys_Raw(RawChannelPtr, InKeyHandles, OutKeyDrawParams);
	}
}

bool IKeyArea::CanCreateKeyEditor() const
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();
	void* RawChannelPtr = GetChannelPtr();
	return ChannelInterface && RawChannelPtr && ChannelInterface->CanCreateKeyEditor_Raw(RawChannelPtr);
}

TSharedRef<SWidget> IKeyArea::CreateKeyEditor(TWeakPtr<ISequencer> Sequencer, const FGuid& ObjectBindingID)
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();
	void* RawChannelPtr = GetChannelPtr();
	UMovieSceneSection* OwningSection = GetOwningSection();

	// The specialized data may be null, but is passed to the interface regardless
	const void* RawSpecializedData = GetSpecializedEditorData();

	TWeakPtr<FTrackInstancePropertyBindings> PropertyBindingsPtr;
	if (PropertyBindings.IsSet())
	{
		PropertyBindingsPtr = TSharedPtr<FTrackInstancePropertyBindings>(AsShared(), &PropertyBindings.GetValue());
	}

	if (ChannelInterface && RawChannelPtr && OwningSection)
	{
		return ChannelInterface->CreateKeyEditor_Raw(RawChannelPtr, RawSpecializedData, OwningSection, ObjectBindingID, PropertyBindingsPtr, Sequencer);
	}
	return SNullWidget::NullWidget;
}

void IKeyArea::CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, TArrayView<const FKeyHandle> KeyMask) const
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();
	void* RawChannelPtr = GetChannelPtr();
	UMovieSceneSection* OwningSection = GetOwningSection();
	if (ChannelInterface && RawChannelPtr && OwningSection)
	{
		ChannelInterface->CopyKeys_Raw(RawChannelPtr, OwningSection, ChannelName, ClipboardBuilder, KeyMask);
	}
}

void IKeyArea::PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment)
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();
	void* RawChannelPtr = GetChannelPtr();
	UMovieSceneSection* OwningSection = GetOwningSection();
	if (ChannelInterface && RawChannelPtr && OwningSection)
	{
		TArray<FKeyHandle> PastedKeys;
		ChannelInterface->PasteKeys_Raw(RawChannelPtr, OwningSection, KeyTrack, SrcEnvironment, DstEnvironment, PastedKeys);

		for (FKeyHandle KeyHandle : PastedKeys)
		{
			DstEnvironment.ReportPastedKey(KeyHandle, *this);
		}
	}
}

TUniquePtr<FCurveModel> IKeyArea::CreateCurveEditorModel(TSharedRef<ISequencer> InSequencer) const
{
	ISequencerChannelInterface* ChannelInterface = FindChannelInterface();
	void* RawChannelPtr = GetChannelPtr();
	UMovieSceneSection* OwningSection = GetOwningSection();
	if (ChannelInterface && RawChannelPtr && OwningSection)
	{
		TUniquePtr<FCurveModel> CurveModel = ChannelInterface->CreateCurveEditorModel_Raw(OwningSection->GetChannelProxy().MakeHandle(RawChannelPtr), OwningSection, InSequencer);
		if (CurveModel.IsValid())
		{
			CurveModel->SetDisplayName(DisplayText);
			if (Color.IsSet())
			{
				CurveModel->SetColor(Color.GetValue());
			}
		}
		return CurveModel;
	}

	return nullptr;
}
