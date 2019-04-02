// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkPropertyTrackEditor.h"
#include "MovieScene/MovieSceneLiveLinkSection.h"
#include "MovieScene/MovieSceneLiveLinkBufferData.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/QualifiedFrameTime.h"
#include "EditorStyleSet.h" //for add track stuff mz todo remove maybe
#include "Features/IModularFeatures.h"
#include "Styling/SlateIconFinder.h"
#include "ISequencerSection.h"
//mz for serialization
#include "DesktopPlatformModule.h"
#include "Misc/App.h"
#include "LiveLinkComponent.h"

/**
* An implementation of live link property sections.
*/
class FLiveLinkSection : public FSequencerSection
{
public:

	/**
	* Creates a new Live Link section.
	*
	* @param InSection The section object which is being displayed and edited.
	* @param InSequencer The sequencer which is controlling this section.
	*/
	FLiveLinkSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
		: FSequencerSection(InSection), WeakSequencer(InSequencer)
	{
	}

public:
	
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding) override;
	virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;
protected:

	/** The sequencer which is controlling this section. */
	TWeakPtr<ISequencer> WeakSequencer;

};


#define LOCTEXT_NAMESPACE "FLiveLinkSection"

//MZ todo will fill out possible for mask support
void FLiveLinkSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding)
{

	UMovieSceneLiveLinkSection* LiveLinkSection = CastChecked<UMovieSceneLiveLinkSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();

	auto MakeUIAction = [=](int32 Index)
	{
		return FUIAction(
			FExecuteAction::CreateLambda([=]
		{
			FScopedTransaction Transaction(LOCTEXT("SetLiveLinkActiveChannelsTransaction", "Set Live LinkActive Channels"));
			LiveLinkSection->Modify();
			TArray<bool> ChannelMask = LiveLinkSection->ChannelMask;
			ChannelMask[Index] = !ChannelMask[Index];
			LiveLinkSection->SetMask(ChannelMask);

			SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
		}
			),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([=]
		{
			TArray<bool> ChannelMask = LiveLinkSection->ChannelMask;
			if (ChannelMask[Index])
			{
				return ECheckBoxState::Checked;
			}
			else 
			{
				return ECheckBoxState::Unchecked;
			}
		})
		);
	};

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("LiveLinkChannelsText", "Active Live Link Channels"));
	{
		FMovieSceneChannelProxy& ChannelProxy = LiveLinkSection->GetChannelProxy();
		for (const FMovieSceneChannelEntry& Entry : LiveLinkSection->GetChannelProxy().GetAllEntries())
		{
			const FName ChannelTypeName = Entry.GetChannelTypeName();
			TArrayView<FMovieSceneChannel* const>        Channels = Entry.GetChannels();
			TArrayView<const FMovieSceneChannelMetaData> AllMetaData = Entry.GetMetaData();

			for (int32 Index = 0; Index < Channels.Num(); ++Index)
			{
				const FMovieSceneChannelMetaData& MetaData = AllMetaData[Index];
				FText Name = FText::FromName(MetaData.Name);

				FText Text = FText::Format(LOCTEXT("LiveLinkChannelEnable", "{0}"), Name);
				FText TooltipText = FText::Format(LOCTEXT("LiveLinkChannelEnableTooltip", "Toggle {0}"), Name);
				MenuBuilder.AddMenuEntry(
					Text, TooltipText,
					FSlateIcon(), MakeUIAction(Index), NAME_None, EUserInterfaceActionType::ToggleButton);
			}
		}
	}
	MenuBuilder.EndSection();
}
	

bool FLiveLinkSection::RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePaths)
{
	if (KeyAreaNamePaths.Num() > 0)
	{
		UMovieSceneLiveLinkSection* LiveLinkSection = CastChecked<UMovieSceneLiveLinkSection>(WeakSection.Get());
		TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
		const FScopedTransaction Transaction(LOCTEXT("DeleteLiveLinkChannel", "Delete Live Link channel"));
		if (LiveLinkSection->TryModify())
		{
			TArray<bool> ChannelMask = LiveLinkSection->ChannelMask;
			int32 PathNameIndex = 0;
			FName KeyAreaName = KeyAreaNamePaths[PathNameIndex];
			for (const FMovieSceneChannelEntry& Entry : LiveLinkSection->GetChannelProxy().GetAllEntries())
			{
				const FName ChannelTypeName = Entry.GetChannelTypeName();
				TArrayView<FMovieSceneChannel* const>        Channels = Entry.GetChannels();
				TArrayView<const FMovieSceneChannelMetaData> AllMetaData = Entry.GetMetaData();
				for (int32 Index = 0; Index < Channels.Num(); ++Index)
				{
					const FMovieSceneChannelMetaData& MetaData = AllMetaData[Index];
					if (MetaData.Name == KeyAreaName)
					{
						ChannelMask[Index] = false;
						if (++PathNameIndex == KeyAreaNamePaths.Num())
						{
							break;
						}
						else
						{
							KeyAreaName = KeyAreaNamePaths[PathNameIndex];
						}
					}
				}
			}
			LiveLinkSection->SetMask(ChannelMask);
			SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
			return true;
		}
	}
	return false;
}
#undef LOCTEXT_NAMESPACE


#define LOCTEXT_NAMESPACE "LiveLinkPropertyTrackEditor"


TSharedRef<ISequencerTrackEditor> FLiveLinkPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FLiveLinkPropertyTrackEditor(InSequencer));
}

void FLiveLinkPropertyTrackEditor::LoadLiveLinkData(TSharedRef<ISequencer> InSequencer,  UMovieSceneLiveLinkTrack* LiveLinkTrack)
{
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		FString FileTypeDescription = TEXT("");
		FString DialogTitle = TEXT("OpenLiveLinkFile");
		FString InOpenDirectory = Serializer.GetLocalCaptureDir();
		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			DialogTitle,
			InOpenDirectory,
			TEXT(""),
			FileTypeDescription,
			EFileDialogFlags::None,
			OpenFilenames
		);
	}

	if (!bOpen || !OpenFilenames.Num())
	{
		return;
	}

	LiveLinkTrack->RemoveAllAnimationData();
	UMovieSceneLiveLinkTrack *MovieSceneTrack = LiveLinkTrack;
	for (const FString& FileName : OpenFilenames)
	{

		bool bFileExists = Serializer.DoesFileExist(FileName);
		if (bFileExists)
		{
			FText Error;
			FLiveLinkFileHeader Header;

			if (Serializer.OpenForRead(FileName, Header, Error))
			{
				Serializer.GetDataRanges([this, FileName, MovieSceneTrack, Header](uint64 InMinFrameId, uint64 InMaxFrameId)
				{
					auto OnReadComplete = [this,MovieSceneTrack, Header]()
					{
						TArray<FLiveLinkSerializedFrame> &InFrames =  Serializer.ResultData;
						if (InFrames.Num() > 0)
						{
							FName SubjectName;
							FLiveLinkFrameData FrameData;
							FLiveLinkRefSkeleton RefSkeleton;
							TWeakObjectPtr<class UMovieSceneLiveLinkSection> MovieSceneSection;
							MovieSceneSection = Cast<UMovieSceneLiveLinkSection>(MovieSceneTrack->CreateNewSection());
							MovieSceneTrack->AddSection(*MovieSceneSection);

							MovieSceneSection->SetSubjectName(Header.SubjectName);
							FFrameRate   TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();

							/*
							FFrameRate   TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
							FFrameNumber CurrentFrame = (Time * TickResolution).FloorToFrame();
							MovieSceneSection->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));
							*/
							MovieSceneSection->TimecodeSource = FMovieSceneTimecodeSource(FApp::GetTimecode());

							int32 NumChannels = MovieSceneSection->CreateChannelProxy(Header.RefSkeleton, Header.CurveNames);
							if (NumChannels > 0)
							{
								TArray<FFrameNumber> Times;
								Times.Reserve(InFrames.Num());
								TArray<FLiveLinkTransformKeys> LinkTransformKeysArray;
								TArray<FLiveLinkCurveKeys>  LinkCurveKeysArray;
								bool InFirst = true;
								for (const FLiveLinkSerializedFrame& SerializedFrame : InFrames)
								{
									const FLiveLinkFrame &Frame = SerializedFrame.Frame;

									int32 TransformIndex = 0;
									int32 CurveIndex = 0;
									//FQualifiedFrameTime QualifiedFrameTime = LiveLinkClient->GetQualifiedFrameTimeAtIndex(SubjectName.GetValue(), FrameIndex++);
									double Second = Frame.WorldTime.Time - Header.SecondsDiff;
									FFrameNumber FrameNumber = (Second * TickResolution).FloorToFrame();
									Times.Add(FrameNumber);
									MovieSceneSection->ExpandToFrame(FrameNumber);

									if (InFirst)
									{
										LinkTransformKeysArray.SetNum(Frame.Transforms.Num());
										for (FLiveLinkTransformKeys& TransformKeys : LinkTransformKeysArray)
										{
											TransformKeys.Reserve(InFrames.Num());
										}
										LinkCurveKeysArray.SetNum(Frame.Curves.Num());
										for (FLiveLinkCurveKeys& CurveKeys : LinkCurveKeysArray)
										{
											CurveKeys.Reserve(InFrames.Num());
										}
										InFirst = false;
									}

									for (const FTransform& Transform : Frame.Transforms)
									{
										LinkTransformKeysArray[TransformIndex++].Add(Transform);
									}
									for (const FOptionalCurveElement& Curve : Frame.Curves)
									{
										if (Curve.IsValid())
										{
											LinkCurveKeysArray[CurveIndex].Add(Curve.Value, FrameNumber);
										}
										CurveIndex += 1;
									}

								}

								TArray<FMovieSceneFloatChannel>& FloatChannels = MovieSceneSection->GetFloatChannels();
								int32 ChannelIndex = 0;
								for (FLiveLinkTransformKeys& TransformKeys : LinkTransformKeysArray)
								{
									TransformKeys.AddToFloatChannels(ChannelIndex, FloatChannels, Times);
									ChannelIndex += 9;
								}
								for (FLiveLinkCurveKeys CurveKeys : LinkCurveKeysArray)
								{
									CurveKeys.AddToFloatChannels(ChannelIndex, FloatChannels);
									ChannelIndex += 1;
								}

							}
						}
						Serializer.Close();
						GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

					}; //callback

				
					Serializer.ReadFramesAtFrameRange(InMinFrameId, InMaxFrameId, OnReadComplete);
				

				});
			}
		}

	}

	InSequencer.Get().NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

}



//MZ todo will fillout possible for mask support
void FLiveLinkPropertyTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{

	//mz hack to load in saved live link data

	UMovieSceneLiveLinkTrack* LiveLinkTrack = Cast<UMovieSceneLiveLinkTrack>(Track);
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "LoadLiveLinkTrack", "Load Live Link Track"),
		NSLOCTEXT("Sequencer", "LoadLiveLinkTrackTooltip", "Test to load in saved live link data"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FLiveLinkPropertyTrackEditor::LoadLiveLinkData, GetSequencer().ToSharedRef(),LiveLinkTrack),
			FCanExecuteAction::CreateLambda([=]()->bool { return true; })));


}

/* ISequencerTrackEditor interface
*****************************************************************************/

void FLiveLinkPropertyTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddLiveLinkTrack", "Live Link Track"),
		LOCTEXT("AddLiveLinkTrackTooltip", "Adds a new track that exposes Live Link Sources."),
		FSlateIconFinder::FindIconForClass(ULiveLinkComponent::StaticClass()),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FLiveLinkPropertyTrackEditor::HandleAddLiveLinkTrackMenuEntryExecute),
			FCanExecuteAction::CreateRaw(this, &FLiveLinkPropertyTrackEditor::HandleAddLiveLinkTrackMenuEntryCanExecute)
		)
	);
}

TSharedRef<ISequencerSection> FLiveLinkPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShared<FLiveLinkSection>(SectionObject, GetSequencer());
}

bool FLiveLinkPropertyTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("LevelSequence"));
}

bool FLiveLinkPropertyTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneLiveLinkTrack::StaticClass());
}

const FSlateBrush* FLiveLinkPropertyTrackEditor::GetIconBrush() const
{
	return FSlateIconFinder::FindIconForClass(ULiveLinkComponent::StaticClass()).GetIcon();
}


/* FLiveLinkTrackEditor callbacks
*****************************************************************************/

void FLiveLinkPropertyTrackEditor::HandleAddLiveLinkTrackMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	UMovieSceneTrack* Track = FocusedMovieScene->FindMasterTrack<UMovieSceneLiveLinkTrack>();
	if (!Track)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddLiveLinkTrack_Transaction", "Add Live Link Track"));
		FocusedMovieScene->Modify();
		UMovieSceneLiveLinkTrack* NewTrack = FocusedMovieScene->AddMasterTrack<UMovieSceneLiveLinkTrack>();
		ensure(NewTrack);

		NewTrack->SetDisplayName(LOCTEXT("LiveLinkTrackName", "Live Link"));

		if (GetSequencer().IsValid())
		{
			GetSequencer()->OnAddTrack(NewTrack);
		}


		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

bool FLiveLinkPropertyTrackEditor::HandleAddLiveLinkTrackMenuEntryCanExecute() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	return ((FocusedMovieScene != nullptr) && (FocusedMovieScene->FindMasterTrack<UMovieSceneLiveLinkTrack>() == nullptr));
}




#undef LOCTEXT_NAMESPACE