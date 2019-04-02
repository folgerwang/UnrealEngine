// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderSources.h"
#include "TakeRecorderSource.h"
#include "TakesCoreFwd.h"
#include "LevelSequence.h"
#include "TakeMetaData.h"
#include "AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "TakesUtils.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "MovieSceneTimeHelpers.h"
#include "ObjectTools.h"
#include "Engine/TimecodeProvider.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY(SubSequenceSerialization);

UTakeRecorderSources::UTakeRecorderSources(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, SourcesSerialNumber(0)
{
	// Ensure instances are always transactional
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SetFlags(RF_Transactional);
	}
}

UTakeRecorderSource* UTakeRecorderSources::AddSource(TSubclassOf<UTakeRecorderSource> InSourceType)
{
	UTakeRecorderSource* NewSource = nullptr;

	if (UClass* Class = InSourceType.Get())
	{
		NewSource = NewObject<UTakeRecorderSource>(this, Class, NAME_None, RF_Transactional);
		if (ensure(NewSource))
		{
			Sources.Add(NewSource);
			++SourcesSerialNumber;
		}
	}

	return NewSource;
}

void UTakeRecorderSources::RemoveSource(UTakeRecorderSource* InSource)
{
	Sources.Remove(InSource);

	// Remove the entry from the sub-sequence map as we won't be needing it anymore.
	SourceSubSequenceMap.Remove(InSource);

	++SourcesSerialNumber;
}

FDelegateHandle UTakeRecorderSources::BindSourcesChanged(const FSimpleDelegate& Handler)
{
	return OnSourcesChangedEvent.Add(Handler);
}

void UTakeRecorderSources::UnbindSourcesChanged(FDelegateHandle Handle)
{
	OnSourcesChangedEvent.Remove(Handle);
}

void UTakeRecorderSources::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property || PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTakeRecorderSources, Sources))
	{
		++SourcesSerialNumber;
	}
}

void UTakeRecorderSources::StartRecordingRecursive(TArray<UTakeRecorderSource*> InSources, ULevelSequence* InMasterSequence, const FTimecode& Timecode, FManifestSerializer* InManifestSerializer)
{
	TArray<UTakeRecorderSource*> NewSources;

	// Optionally create a folder in the Sequencer UI that will contain this source. We don't want sub-sequences to have folders
	// created for their sources as you would end up with a Subscene with one item in it hidden inside of a folder, so instead
	// only the master sequence gets folders created.
	const bool bCreateSequencerFolders = true;
	for (UTakeRecorderSource* Source : InSources)
	{
		if (Source->bEnabled)
		{
			ULevelSequence* TargetSequence = InMasterSequence;

			// The Sequencer Take system is built around swapping out sub-sequences. If they want to use this system, we create a sub-sequence
			// for the Source and tell it to write into this sub-sequence instead of the master sequence. We then keep track of which Source
			// is using which sub-sequence so that we can push the correct sequence for all points of the Source's recording lifecycle.
			if (bRecordSourcesToSubSequences && Source->SupportsSubscenes())
			{
				const FString& SubSequenceName = ObjectTools::SanitizeObjectName(Source->GetSubsceneName(InMasterSequence));

				TargetSequence = CreateSubSequenceForSource(InMasterSequence, SubSequenceName);
				TargetSequence->GetMovieScene()->TimecodeSource = Timecode;

				// If there's already a Subscene Track for our sub-sequence we need to remove that track before create a new one. No data is lost in this process as the
				// sequence that the subscene points to has been copied by CreateSubSequenceForSource so a new track pointed to the new subsequence includes all the old data.
				TOptional<int32> RowIndex;
				const FString SequenceName = FPaths::GetBaseFilename(TargetSequence->GetPathName());
				UMovieSceneSubTrack* SubsceneTrack = nullptr;

				for (UMovieSceneTrack* Track : InMasterSequence->GetMovieScene()->GetMasterTracks())
				{
					if (Track->IsA<UMovieSceneSubTrack>())
					{
						// Look through each section in the track to see if it has a sub-sequence that matches our new sequence.
						for (UMovieSceneSection* Section : Track->GetAllSections())
						{
							UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
							UMovieSceneSequence* SubSequence = SubSection->GetSequence();

							// Store the row index so we can re-inject the section at the same index to preserve the hierarchical evaluation order.
							if (FPaths::GetBaseFilename(SubSequence->GetPathName()) == SequenceName)
							{
								SubsceneTrack = CastChecked<UMovieSceneSubTrack>(Track);
								SubsceneTrack->RemoveSection(*Section);
								RowIndex = Section->GetRowIndex();
								break;
							}
						}
						
					}
				}

				// We need to add the new subsequence to the master sequence immediately so that it shows up in the UI and you can tell that things
				// are being recorded, otherwise they don't show up until recording stops and then it magically pops in.
				if (!SubsceneTrack)
				{
					SubsceneTrack = CastChecked<UMovieSceneSubTrack>(InMasterSequence->GetMovieScene()->AddMasterTrack(UMovieSceneSubTrack::StaticClass()));
				}
				
				// We create a new sub track for every Source so that we can name the Subtrack after the Source instead of just the sections within it.
				SubsceneTrack->SetDisplayName(FText::FromString(Source->GetSubsceneName(InMasterSequence)));
				SubsceneTrack->SetColorTint(Source->TrackTint);
					
				// When we create the Subscene Track we'll make sure a folder is created for it to sort into and add the new Subscene Track as a child of it.
				if (bCreateSequencerFolders)
				{
					UMovieSceneFolder* Folder = AddFolderForSource(Source, InMasterSequence->GetMovieScene());
					Folder->AddChildMasterTrack(SubsceneTrack);
				}
				
				// There isn't already a section for our new sub sequence, so we'll just append it to the end
				if (!RowIndex.IsSet())
				{
					RowIndex = SubsceneTrack->GetMaxRowIndex() + 1;
				}

				// We initialize the sequence to start at zero and be a 0 frame length section as there is no data in the sections yet.
				// We'll have to update these sections each frame as the recording progresses so they appear to get longer like normal
				// tracks do as we record into them.
				FFrameNumber RecordStartTime = FFrameNumber(0);
				UMovieSceneSubSection* NewSubSection = SubsceneTrack->AddSequence(TargetSequence, RecordStartTime, 0);

				NewSubSection->SetRowIndex(RowIndex.GetValue());
				SubsceneTrack->FixRowIndices();

				ActiveSubSections.Add(NewSubSection);
				if (InManifestSerializer)
				{
					FName SerializedType("SubSequence");
					FManifestProperty  ManifestProperty(SubSequenceName, SerializedType, FGuid());
					InManifestSerializer->WriteFrameData(InManifestSerializer->FramesWritten, ManifestProperty);

					FString AssetPath = InManifestSerializer->GetLocalCaptureDir();

					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					if (!PlatformFile.DirectoryExists(*AssetPath))
					{
						PlatformFile.CreateDirectory(*AssetPath);
					}

					AssetPath = AssetPath / SubSequenceName;
					if (!PlatformFile.DirectoryExists(*AssetPath))
					{
						PlatformFile.CreateDirectory(*AssetPath);
					}

					TSharedPtr<FManifestSerializer> NewManifestSerializer = MakeShared<FManifestSerializer>();
					CreatedManifestSerializers.Add(NewManifestSerializer);
					InManifestSerializer = NewManifestSerializer.Get();

					InManifestSerializer->SetLocalCaptureDir(AssetPath);

					FManifestFileHeader Header(SubSequenceName, SerializedType, FGuid());
					FText Error;
					FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *(SubSequenceName));

					if (!InManifestSerializer->OpenForWrite(FileName, Header, Error))
					{
						UE_LOG(SubSequenceSerialization, Warning, TEXT("Error Opening Sequence Sequencer File: Subject '%s' Error '%s'"), *(SubSequenceName), *(Error.ToString()));
					}
				}
			}

			// Update our mappings of which sources use which sub-sequence.
			SourceSubSequenceMap.FindOrAdd(Source) = TargetSequence;
			Source->TimecodeSource = Timecode;
			for (UTakeRecorderSource* NewlyAddedSource : Source->PreRecording(TargetSequence, InMasterSequence, InManifestSerializer))
			{
				// Add it to our classes list of sources 
				Sources.Add(NewlyAddedSource);

				// And then track it separately so we can recursively call PreRecording 
				NewSources.Add(NewlyAddedSource);
			}

			// We need to wait until PreRecording is called on a source before asking it to place itself in a folder
			// so that the Source has had a chance to create any required sections that will go in the folder.
			if (!bRecordSourcesToSubSequences && bCreateSequencerFolders)
			{
				UMovieSceneFolder* Folder = AddFolderForSource(Source, InMasterSequence->GetMovieScene());
				
				// Different sources can create different kinds of tracks so we allow each source to decide how it gets
				// represented inside the folder.
				Source->AddContentsToFolder(Folder);
			}
		}
	}

	if (NewSources.Num())
	{
		// We don't want to nestle sub-sequences recursively so we always pass the Master Sequence and not the sequence
		// created for a new source.
		StartRecordingRecursive(NewSources, InMasterSequence, Timecode,InManifestSerializer);
		SourcesSerialNumber++;

		bool bHasValidTimecodeSource;
		FQualifiedFrameTime QualifiedSequenceTime = GetCurrentRecordingFrameTime(Timecode, bHasValidTimecodeSource);
		for (auto NewSource : NewSources)
		{
			if (NewSource->bEnabled)
			{
				ULevelSequence* SourceSequence = SourceSubSequenceMap[NewSource];
				FFrameNumber FrameNumber = QualifiedSequenceTime.ConvertTo(SourceSequence->GetMovieScene()->GetTickResolution()).FloorToFrame();
				if (bHasValidTimecodeSource)
				{
					//Need to get difference of the source time from the start of recording and put into level frame rate.
					FFrameNumber SeqStartFrameTime = StartRecordingTimecodeSource.ToFrameNumber(FApp::GetTimecodeFrameRate());
					FFrameNumber StartFrameTime = Timecode.ToFrameNumber(FApp::GetTimecodeFrameRate());
					FFrameTime FrameTimeDiff;
					FrameTimeDiff.FrameNumber = StartFrameTime - SeqStartFrameTime;
					FrameTimeDiff = FFrameRate::TransformTime(FrameTimeDiff, FApp::GetTimecodeFrameRate(), TargetLevelSequenceTickResolution);
					FrameNumber = FrameTimeDiff.FrameNumber;
				}
				NewSource->StartRecording(Timecode, FrameNumber, SourceSubSequenceMap[NewSource]);
			}
		}
	}
}

void UTakeRecorderSources::StartRecordingSource(TArray<UTakeRecorderSource *> InSources,const FTimecode& CurrentTimecode)
{
	// This calls PreRecording recursively on every source so that all sources that get added by another source
	// have had PreRecording called.
	StartRecordingRecursive(InSources, CachedLevelSequence,CurrentTimecode, CachedManifestSerializer);

	bool bHasValidTimecodeSource;
	FQualifiedFrameTime QualifiedSequenceTime = GetCurrentRecordingFrameTime(CurrentTimecode, bHasValidTimecodeSource);
	for (auto Source : InSources)
	{
		if (Source->bEnabled)
		{
			ULevelSequence* SourceSequence = SourceSubSequenceMap[Source];
			FFrameNumber FrameNumber = QualifiedSequenceTime.ConvertTo(SourceSequence->GetMovieScene()->GetTickResolution()).FloorToFrame();
			Source->TimecodeSource = CurrentTimecode;
			if (bHasValidTimecodeSource)
			{
				//Need to get difference of the source time from the start of recording and put into level frame rate.
				FFrameNumber SeqStartFrameTime = StartRecordingTimecodeSource.ToFrameNumber(FApp::GetTimecodeFrameRate());
				FFrameNumber StartFrameTime = CurrentTimecode.ToFrameNumber(FApp::GetTimecodeFrameRate());
				FFrameTime FrameTimeDiff;
				FrameTimeDiff.FrameNumber = StartFrameTime - SeqStartFrameTime;
				FrameTimeDiff = FFrameRate::TransformTime(FrameTimeDiff, FApp::GetTimecodeFrameRate(), TargetLevelSequenceTickResolution);
				FrameNumber = FrameTimeDiff.FrameNumber;
			}
			Source->StartRecording(CurrentTimecode, FrameNumber, SourceSubSequenceMap[Source]);
		}
	}
}

void UTakeRecorderSources::StartRecording(class ULevelSequence* InSequence, FManifestSerializer* InManifestSerializer)
{
	// We want to cache the Serializer and Level Sequence in case more objects start recording mid-recording.
	// We want them to use the same logic flow as if initialized from scratch so that they properly sort into
	// sub-sequences, etc.
	CachedManifestSerializer = InManifestSerializer;
	CachedLevelSequence = InSequence;

	bIsRecording = true;
	TimeSinceRecordingStarted = 0.f;
	TargetLevelSequenceTickResolution = InSequence->GetMovieScene()->GetTickResolution();

	FTimecode TimecodeSource = FApp::GetTimecode();
	InSequence->GetMovieScene()->TimecodeSource = TimecodeSource;
	StartRecordingTimecodeSource = TimecodeSource;
	StartRecordingSource(Sources, TimecodeSource);
}

FFrameTime UTakeRecorderSources::TickRecording(class ULevelSequence* InSequence,float DeltaTime)
{
	FTimecode CurrentTimecode = FApp::GetTimecode();
	bool bHasValidTimecodeSource;
	FQualifiedFrameTime FrameTime = GetCurrentRecordingFrameTime(CurrentTimecode,bHasValidTimecodeSource);
	FQualifiedFrameTime SourceFrameTime(FrameTime);
	if (bHasValidTimecodeSource)
	{
		//We leave this ins timecode frame rate since the sources convert it later (cbb and faster to do it here, we actually do it below
		//for showiung the time
		FFrameNumber SeqStartFrameTime = StartRecordingTimecodeSource.ToFrameNumber(FApp::GetTimecodeFrameRate());
		SourceFrameTime.Time.FrameNumber = SourceFrameTime.Time.FrameNumber - SeqStartFrameTime;
	}

	for (auto Source : Sources)
	{
		if (Source->bEnabled)
		{
			Source->TickRecording(SourceFrameTime);
		}
	}

	//Time in seconds since recording started. Used when there is no Timecode Sync (e.g. in case it get's lost or dropped).
	TimeSinceRecordingStarted += DeltaTime;

	//We calculate and return the Current Frame Number based upon whether driven by timecode or engine tick.
	//We need to make sure this is TargetLevelSequenceTickResolution, but first do in Timecode rate space do
	//to precision issues with Timecode.
	FFrameTime CurrentFrameTimeSinceStart;
	if (bHasValidTimecodeSource)
	{
		FFrameNumber SeqStartFrameTime = StartRecordingTimecodeSource.ToFrameNumber(FApp::GetTimecodeFrameRate());
		FrameTime.Time.FrameNumber = FrameTime.Time.FrameNumber - SeqStartFrameTime;
		CurrentFrameTimeSinceStart = FrameTime.ConvertTo(TargetLevelSequenceTickResolution);

	}
	else
	{
		CurrentFrameTimeSinceStart = TargetLevelSequenceTickResolution.AsFrameTime(TimeSinceRecordingStarted);
	}

	// If we're recording into sub-sections we want to update their range every frame so they appear to
	// animate as their contents are filled. We can't check against the size of all sections (not all
	// source types have data in their sections until the end) and if you're partially re-recording
	// a track it would size to the existing content which would skip the animation as well.

	for (UMovieSceneSubSection* SubSection : ActiveSubSections)
	{
		// If this sub-section has a start frame we will use that as the first frame. This handles sub-sections that
		// are created part-way through a recording and have them show up with the correct timestep instead of 
		// snapping to be the full length (to the start) when they don't actually have any data there.
		FFrameNumber StartFrame = FFrameNumber(0);
		if (SubSection->HasStartFrame())
		{
			StartFrame = SubSection->GetInclusiveStartFrame();
		}

		// We're going to use the running time since recording started which is close enough for now until
		// we get to recording things that get destroyed and needing to stop updating the sub section...
		if (StartFrame < CurrentFrameTimeSinceStart.FrameNumber)
		{
			TRange<FFrameNumber> CurrentRange = TRange<FFrameNumber>::Exclusive(StartFrame, CurrentFrameTimeSinceStart.FrameNumber);
			SubSection->SetRange(CurrentRange);
		}
	}
	return CurrentFrameTimeSinceStart;
}

//If we have a valid timecode source the returned Qualified Time is the raw Timecode converted time.  Since we will need to
//convert that time to some relative value, based on start of the level sequence or start of the source we also pass if timecode was valid.
//If not valid then it's just TimeSinceRecordingStarted converted to a Qualified Time
FQualifiedFrameTime UTakeRecorderSources::GetCurrentRecordingFrameTime(const FTimecode& InTimecode, bool& bHasValidTimecodeSource) const
{
	FQualifiedFrameTime FrameTime;
	bHasValidTimecodeSource = false;

	if (GEngine)
	{
		const UTimecodeProvider* TimecodeProvider = GEngine->GetTimecodeProvider();

		// If their is a a Timecode Provider that is synchronized then we will sample the engine Timecode
		// to determine what frame the data should go on. If the engine is ticking faster than the given
		// Timecode framerate then there will be multiple frames submitted with the same qualified time
		// and the data sources will end up only storing the latest call on that frame.
		if (TimecodeProvider && TimecodeProvider->GetSynchronizationState() == ETimecodeProviderSynchronizationState::Synchronized)
		{

			const FFrameNumber QualifiedFrameNumber = TimecodeProvider->GetTimecode().ToFrameNumber(FApp::GetTimecodeFrameRate());
			FrameTime = FQualifiedFrameTime(FFrameTime(QualifiedFrameNumber), FApp::GetTimecodeFrameRate());

			bHasValidTimecodeSource = true;
		}
		else
		{
			UE_LOG(LogTakesCore, Error, TEXT("Attempted to sample timecode from custom Timecode Provider %s while provider was not synchronized! Falling back to engine clock for timecode source!"), TimecodeProvider != nullptr ? *TimecodeProvider->GetName() : TEXT(""));
			bHasValidTimecodeSource = false;
		}
	}

	if (!bHasValidTimecodeSource)
	{
		// If no Timecode Provider is specified (or it has an error) then we want to fall back to the normal Engine tickrate and capture
		//Use Level Sequence TickRate to make conversions cleaner later on.
		const FFrameNumber FrameNumber = TargetLevelSequenceTickResolution.AsFrameNumber(TimeSinceRecordingStarted);
		FrameTime = FQualifiedFrameTime(FFrameTime(FrameNumber), TargetLevelSequenceTickResolution);
	}	
	
	return FrameTime;
}

void UTakeRecorderSources::StopRecording(class ULevelSequence* InSequence, FTakeRecorderSourcesSettings TakeRecorderSourcesSettings)
{
	bIsRecording = false;
	TimeSinceRecordingStarted = 0.f;

	for (auto Source : Sources)
	{
		if (Source->bEnabled)
		{
			Source->StopRecording(SourceSubSequenceMap[Source]);
		}
	}

	TArray<UTakeRecorderSource*> SourcesToRemove;
	for (auto Source : Sources)
	{
		if (Source->bEnabled)
		{
			for (auto SourceToRemove : Source->PostRecording(SourceSubSequenceMap[Source], InSequence))
			{
				SourcesToRemove.Add(SourceToRemove);
			}
		}
	}

	if (SourcesToRemove.Num())
	{
		for (auto SourceToRemove : SourcesToRemove)
		{
			Sources.Remove(SourceToRemove);
		}
		++SourcesSerialNumber;
	}

	// Ensure each sub-section is as long as it should be. If we're recording into subsections and a user is doing a partial
	// re-record of the data within the sub section we can end up with the case where the new section is shorter than the original
	// data. We don't want to trim the data unnecessarily, and we've been updating the length of the section every frame of the recording
	// as we go (to show the 'animation' of it recording), but we need to restore it to the full length.
	for (UMovieSceneSubSection* SubSection : ActiveSubSections)
	{
		UMovieSceneSequence* SubSequence = SubSection->GetSequence();
		if (SubSequence)
		{
			// Expand the Play Range of the sub-section to encompass all sections within it.
			TakesUtils::ClampPlaybackRangeToEncompassAllSections(SubSequence->GetMovieScene());

			// Lock the sequence so that it can't be changed without implicitly unlocking it now
			SubSequence->GetMovieScene()->SetReadOnly(true);

			// Lock the meta data so it can't be changed without implicitly unlocking it now
			ULevelSequence* SequenceAsset = CastChecked<ULevelSequence>(SubSequence);
			UTakeMetaData* AssetMetaData = SequenceAsset->FindMetaData<UTakeMetaData>();
			check(AssetMetaData);
			AssetMetaData->Lock();

			SubSection->SetRange(SubSequence->GetMovieScene()->GetPlaybackRange());
		}
	}

	if (TakeRecorderSourcesSettings.bRemoveRedundantTracks)
	{
		RemoveRedundantTracks();
	}

	if (CreatedManifestSerializers.Num())
	{
		for (auto Serializer : CreatedManifestSerializers)
		{
			Serializer->Close();
		}
	}

	if (TakeRecorderSourcesSettings.bSaveRecordedAssets)
	{
		for (auto SourceSubSequence : SourceSubSequenceMap)
		{
			if (SourceSubSequence.Value)
			{
				TakesUtils::SaveAsset(SourceSubSequence.Value);
			}
		}
	}

	SourceSubSequenceMap.Empty();
	ActiveSubSections.Empty();
	CreatedManifestSerializers.Empty();
	CachedManifestSerializer = nullptr;
	CachedLevelSequence = nullptr;
}

ULevelSequence* UTakeRecorderSources::CreateSubSequenceForSource(ULevelSequence* InMasterSequence, const FString& SubSequenceName)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// We want to sanitize the object names because sometimes they come from names with spaces and other invalid characters in them.
	const FString& SequenceDirectory = FPaths::GetPath(InMasterSequence->GetPathName());
	const FString& SequenceName = FPaths::GetBaseFilename(InMasterSequence->GetPathName());

	// We need to check the Master Sequence to see if they already have a sub-sequence with this name so that we duplicate the right
	// sequence and re-use that, instead of just making a new blank sequence every time. This will help in cases where they've done a recording, 
	// modified a sub-sequence and want to record from that setup. Each source will individually remove any old data inside the Sub Sequence
	// so we don't have to worry about any data the user added via Sequencer unrelated to what they recorded.
	ULevelSequence* ExistingSubSequence = nullptr;
	UMovieSceneSubTrack* SubTrack = InMasterSequence->GetMovieScene()->FindMasterTrack<UMovieSceneSubTrack>();
	if (SubTrack)
	{
		// Look at each section in the track to see if it has the same name as our new SubSequence name.
		for (UMovieSceneSection* Section : SubTrack->GetAllSections())
		{
			UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
			if (FPaths::GetBaseFilename(SubSection->GetSequence()->GetPathName()) == SubSequenceName)
			{
				UE_LOG(LogTakesCore, Log, TEXT("Found existing sub-section for source %s, duplicating sub-section for recording into."), *SubSequenceName);
				ExistingSubSequence = CastChecked<ULevelSequence>(SubSection->GetSequence());
				break;
			}
		}
	}

	FString NewPath = FString::Printf(TEXT("%s/%s_Subscenes/%s"), *SequenceDirectory, *SequenceName, *SubSequenceName);
	
	ULevelSequence* OutAsset = nullptr;
	TakesUtils::CreateNewAssetPackage<ULevelSequence>(NewPath, OutAsset, nullptr, ExistingSubSequence);
	if (OutAsset)
	{

		OutAsset->Initialize();

		// We only set their tick resolution/display rate if we're creating the sub-scene from scratch. If we created it in the
		// past it will have the right resolution, but if the user modified it then we will preserve their desired resolution.
		if (!ExistingSubSequence)
		{
			OutAsset->GetMovieScene()->SetTickResolutionDirectly(InMasterSequence->GetMovieScene()->GetTickResolution());
			OutAsset->GetMovieScene()->SetDisplayRate(InMasterSequence->GetMovieScene()->GetDisplayRate());
		}

		UTakeMetaData* TakeMetaData = InMasterSequence->FindMetaData<UTakeMetaData>();
		if (TakeMetaData)
		{
			UTakeMetaData* OutTakeMetaData = OutAsset->CopyMetaData(TakeMetaData);

			// Tack on the sub sequence name so that it's unique from the master sequence
			OutTakeMetaData->SetSlate(TakeMetaData->GetSlate() + TEXT("_") + SubSequenceName);
		}

		OutAsset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(OutAsset);
	}

	return OutAsset;
}

UMovieSceneFolder* UTakeRecorderSources::AddFolderForSource(const UTakeRecorderSource* InSource, UMovieScene* InMovieScene)
{
	check(InSource);
	check(InMovieScene);

	// The TakeRecorderSources needs to create Sequencer UI folders to put each Source into so that Sources are not creating
	// their own folder structures inside of sub-sequences. This folder structure is designed to match the structure in
	// the Take Recorder UI, which is currently not customizable. If that becomes customizable this code should be updated
	// to ensure the created folder structure matches the one visible in the Take Recorder UI.

	// Currently we use the category that the Source is filed under as this is what the UI currently sorts by.
	const FName FolderName = FName(*InSource->GetClass()->GetMetaData(FName("Category")));

	// Search the Movie Scene for a folder with this name
	UMovieSceneFolder* FolderToUse = nullptr;
	for (UMovieSceneFolder* Folder : InMovieScene->GetRootFolders())
	{
		if (Folder->GetFolderName() == FolderName)
		{
			FolderToUse = Folder;
			break;
		}
	}

	// If we didn't find a folder with this name we're going to go ahead and create a new folder
	if (FolderToUse == nullptr)
	{
		FolderToUse = NewObject<UMovieSceneFolder>(InMovieScene, NAME_None, RF_Transactional);
		FolderToUse->SetFolderName(FolderName);
		InMovieScene->GetRootFolders().Add(FolderToUse);
	}

	// We want to expand these folders in the Sequencer UI (since these are visible as they record).
	InMovieScene->GetEditorData().ExpansionStates.FindOrAdd(FolderName.ToString()) = FMovieSceneExpansionState(true);

	return FolderToUse;
}

void UTakeRecorderSources::RemoveRedundantTracks()
{
	TArray<FGuid> ReferencedBindings;
	for (auto SourceSubSequence : SourceSubSequenceMap)
	{
		ULevelSequence* LevelSequence = SourceSubSequence.Value;
		if (!LevelSequence)
		{
			continue;
		}

		UMovieScene* MovieScene = LevelSequence->GetMovieScene();
		if (!MovieScene)
		{
			continue;
		}

		for (UMovieSceneSection* Section : MovieScene->GetAllSections())
		{
			Section->GetReferencedBindings(ReferencedBindings);
		}
	}


	for (auto SourceSubSequence : SourceSubSequenceMap)
	{
		ULevelSequence* LevelSequence = SourceSubSequence.Value;
		if (!LevelSequence)
		{
			continue;
		}

		UMovieScene* MovieScene = LevelSequence->GetMovieScene();
		if (!MovieScene)
		{
			continue;
		}

		TArray<FGuid> ParentBindings;
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
			if (Possessable)
			{
				ParentBindings.Add(Possessable->GetParent());
			}
		}
		
		TArray<FGuid> BindingsToRemove;
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			if (Binding.GetTracks().Num() == 0 && !ReferencedBindings.Contains(Binding.GetObjectGuid()) && !ParentBindings.Contains(Binding.GetObjectGuid()))
			{
				BindingsToRemove.Add(Binding.GetObjectGuid());
			}
		}

		if (BindingsToRemove.Num() == 0)
		{
			continue;
		}
	
		for (FGuid BindingToRemove : BindingsToRemove)
		{
			MovieScene->RemovePossessable(BindingToRemove);
		}

		UE_LOG(LogTakesCore, Log, TEXT("Removed %d unused object bindings in (%s)"), BindingsToRemove.Num(), *LevelSequence->GetName());
	}
}
