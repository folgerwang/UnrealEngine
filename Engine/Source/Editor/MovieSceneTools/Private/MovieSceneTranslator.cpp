// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTranslator.h"
#include "MovieScene.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "AssetData.h"
#include "LevelSequence.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "MovieSceneTimeHelpers.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "MovieSceneTranslator"

FMovieSceneExportData::FMovieSceneExportData(const UMovieScene* InMovieScene, FFrameRate InFrameRate, int32 InHandleFrames, FString InSaveFilename, TSharedPtr<FMovieSceneTranslatorContext> InContext)
{
	if (InMovieScene == nullptr)
	{
		bExportDataIsValid = false;
		return;
	}

	ExportContext = InContext;
	if (ExportContext.IsValid())
	{
		ExportContext->Init();
	}

	FrameRate = InFrameRate;
	HandleFrames = ConvertFrameTime(FFrameTime(InHandleFrames), InMovieScene->GetTickResolution(), FrameRate);
	SaveFilename = InSaveFilename;

	bExportDataIsValid = ConstructData(InMovieScene);
}

FMovieSceneExportData::FMovieSceneExportData()
{
	bExportDataIsValid = false;
}

FMovieSceneExportData::~FMovieSceneExportData()
{
}

bool FMovieSceneExportData::IsExportDataValid() const
{
	return bExportDataIsValid;
}

bool FMovieSceneExportData::ConstructData(const UMovieScene* InMovieScene)
{
	if (InMovieScene == nullptr) 
	{ 
		return false; 
	}

	SaveFilenamePath = FPaths::GetPath(SaveFilename);
	if (FPaths::IsRelative(SaveFilenamePath))
	{
		SaveFilenamePath = FPaths::ConvertRelativePathToFull(SaveFilenamePath);
	}

	return ConstructMovieSceneData(InMovieScene);
}

bool FMovieSceneExportData::ConstructMovieSceneData(const UMovieScene* InMovieScene)
{
	if (InMovieScene == nullptr)
	{
		return false;
	}

	MovieSceneData = MakeShared<FMovieSceneExportMovieSceneData>();

	FFrameRate Resolution = InMovieScene->GetTickResolution();

	TRange<FFrameNumber> PlaybackRange = InMovieScene->GetPlaybackRange();

	if (PlaybackRange.HasLowerBound())
	{
		MovieSceneData->PlaybackRangeStartFrame = ConvertFrameTime(PlaybackRange.GetLowerBoundValue(), Resolution, FrameRate).CeilToFrame();
	}
	else
	{
		UE_LOG(LogMovieScene, Error, TEXT("Invalid condition: Movie scene playback range has infinite lower bound."));
		return false;
	}

	if (PlaybackRange.HasUpperBound())
	{
		MovieSceneData->PlaybackRangeEndFrame = ConvertFrameTime(PlaybackRange.GetUpperBoundValue(), Resolution, FrameRate).CeilToFrame();
	}
	else
	{
		UE_LOG(LogMovieScene, Error, TEXT("Invalid condition: Movie scene playback range has infinite upper bound."));
		return false;
	}

	MovieSceneData->Name = InMovieScene->GetOuter()->GetName();
	MovieSceneData->Path = InMovieScene->GetOuter()->GetPathName();
	MovieSceneData->Resolution = Resolution;
	MovieSceneData->Duration = ConvertFrameTime(MovieScene::DiscreteSize(PlaybackRange), Resolution, FrameRate).FrameNumber.Value;

	bool bFoundCinematicMasterTrack = false;

	const TArray<UMovieSceneTrack*> MasterTracks = InMovieScene->GetMasterTracks();
	for (UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		if (!bFoundCinematicMasterTrack && MasterTrack->IsA(UMovieSceneCinematicShotTrack::StaticClass()))
		{
			const UMovieSceneCinematicShotTrack* CinematicTrack = Cast<UMovieSceneCinematicShotTrack>(MasterTrack);
			if (!ConstructCinematicMasterTrackData(InMovieScene, CinematicTrack))
			{ 
				return false; 
			}
			bFoundCinematicMasterTrack = true;
		}
		else if (MasterTrack->IsA(UMovieSceneAudioTrack::StaticClass()))
		{
			const UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(MasterTrack);
			if (!ConstructAudioTrackData(InMovieScene, MovieSceneData, AudioTrack))
			{ 
				return false; 
			}
		}
	}

	return true;
}

bool FMovieSceneExportData::ConstructCinematicMasterTrackData(const UMovieScene* InMovieScene, const UMovieSceneCinematicShotTrack* InCinematicMasterTrack)
{
	if (InMovieScene == nullptr || !MovieSceneData.IsValid())
	{
		return false;
	}

	TSharedPtr<FMovieSceneExportCinematicMasterTrackData> MasterTrackData = MakeShared<FMovieSceneExportCinematicMasterTrackData>();
	MovieSceneData->CinematicMasterTrack = MasterTrackData;

	// Construct sections & create track row index array
	TArray<int32> CinematicTrackRowIndices;

	for (UMovieSceneSection* Section : InCinematicMasterTrack->GetAllSections())
	{
		const UMovieSceneCinematicShotSection* CinematicSection = Cast<UMovieSceneCinematicShotSection>(Section);

		if (CinematicSection != nullptr && CinematicSection->GetSequence() != nullptr)
		{
			if (!ConstructCinematicSectionData(InMovieScene, MasterTrackData, CinematicSection))
			{
				return false;
			}

			int32 RowIndex = CinematicSection->GetRowIndex();
			if (RowIndex >= 0)
			{
				CinematicTrackRowIndices.AddUnique(RowIndex);
			}
		}
	}

	// Construct tracks and point to sections
	CinematicTrackRowIndices.Sort();

	for (int32 CinematicTrackRowIndex : CinematicTrackRowIndices)
	{	
		if (!ConstructCinematicTrackData(InMovieScene, MasterTrackData, CinematicTrackRowIndex))
		{
			return false;
		}		
	}

	return true;
}

bool FMovieSceneExportData::ConstructCinematicTrackData(const UMovieScene* InMovieScene, TSharedPtr<FMovieSceneExportCinematicMasterTrackData> InCinematicMasterTrackData, int32 RowIndex)
{
	if (InMovieScene == nullptr || !InCinematicMasterTrackData.IsValid() || !MovieSceneData.IsValid() || !MovieSceneData->CinematicMasterTrack.IsValid())
	{
		return false;
	}

	TSharedPtr<FMovieSceneExportCinematicTrackData> TrackData = MakeShared<FMovieSceneExportCinematicTrackData>(RowIndex);
	MovieSceneData->CinematicMasterTrack->CinematicTracks.Add(TrackData);

	for (TSharedPtr<FMovieSceneExportCinematicSectionData> Section : InCinematicMasterTrackData->CinematicSections)
	{
		if (Section.IsValid() && Section->RowIndex == RowIndex)
		{
			TrackData->CinematicSections.Add(Section);
		}
	}

	return true;
}

bool FMovieSceneExportData::ConstructAudioTrackData(const UMovieScene* InMovieScene, TSharedPtr<FMovieSceneExportMovieSceneData> InMovieSceneData, const UMovieSceneAudioTrack* InAudioTrack)
{
	if (InMovieScene == nullptr || InAudioTrack == nullptr || !InMovieSceneData.IsValid())
	{
		return false;
	}

	TSharedPtr<FMovieSceneExportAudioTrackData> TrackData = MakeShared<FMovieSceneExportAudioTrackData>();
	InMovieSceneData->AudioTracks.Add(TrackData);

	for (UMovieSceneSection* ShotSection : InAudioTrack->GetAllSections())
	{
		const UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(ShotSection);

		if (!ConstructAudioSectionData(InMovieScene, TrackData, AudioSection))
		{
			return false;
		}
	}

	return true;
}

bool FMovieSceneExportData::ConstructCinematicSectionData(const UMovieScene* InMovieScene, TSharedPtr<FMovieSceneExportCinematicMasterTrackData> InMasterTrackData, const UMovieSceneCinematicShotSection* InCinematicSection)
{
	if (InMovieScene == nullptr || !InMasterTrackData.IsValid() || InCinematicSection == nullptr)
	{
		return false;
	}

	TSharedPtr<FMovieSceneExportCinematicSectionData> SectionData = MakeShared<FMovieSceneExportCinematicSectionData>();
	InMasterTrackData->CinematicSections.Add(SectionData);

	SectionData->ShotDisplayName = InCinematicSection->GetShotDisplayName();
	SectionData->ShotFilename = SectionData->ShotDisplayName + TEXT(".avi");
	SectionData->CinematicShotSection = InCinematicSection;

	ConstructSectionData(InMovieScene, SectionData, InCinematicSection, EMovieSceneTranslatorSectionType::Cinematic, SectionData->ShotDisplayName);

	return true;
}

bool FMovieSceneExportData::ConstructAudioSectionData(const UMovieScene* InMovieScene, TSharedPtr<FMovieSceneExportAudioTrackData> InTrackData, const UMovieSceneAudioSection* InAudioSection)
{
	if (InMovieScene == nullptr || !InTrackData.IsValid() || InAudioSection == nullptr)
	{
		return false;
	}

	TSharedPtr<FMovieSceneExportAudioSectionData> SectionData = MakeShared<FMovieSceneExportAudioSectionData>();
	InTrackData->AudioSections.Add(SectionData);

	ConstructSectionData(InMovieScene, SectionData, InAudioSection, EMovieSceneTranslatorSectionType::Audio, TEXT(""));

	return true;
}

bool FMovieSceneExportData::ConstructSectionData(const UMovieScene* InMovieScene, TSharedPtr<FMovieSceneExportSectionData> InSectionData, const UMovieSceneSection* InSection, EMovieSceneTranslatorSectionType InSectionType, const FString& InSectionDisplayName)
{
	if (InMovieScene == nullptr || !MovieSceneData.IsValid() || InSection == nullptr || !InSectionData.IsValid())
	{
		return false;
	}

	InSectionData->RowIndex = InSection->GetRowIndex();

	if (InSection->HasStartFrame())
	{
		FFrameTime InclusiveStartFrame = InSection->GetInclusiveStartFrame();
		FFrameTime ConvertedStartFrame = ConvertFrameTime(InclusiveStartFrame, MovieSceneData->Resolution, FrameRate);
		InSectionData->StartFrame = ConvertedStartFrame.CeilToFrame();

		if (ExportContext.IsValid() && InSectionType == EMovieSceneTranslatorSectionType::Cinematic && ConvertedStartFrame.GetSubFrame() > 0.0f)
		{
			ExportContext->AddMessage(EMessageSeverity::Warning,
				FText::Format(LOCTEXT("SectionStartNotDivisByDisplayRate", "Section '{0}' starts on tick {1} which is not evenly divisible by the display rate {2}. Enable snapping and adjust the start or edit the section properties to ensure it lands evenly on a whole frame."),
					FText::FromString(InSectionDisplayName),
					FText::AsNumber(InclusiveStartFrame.CeilToFrame().Value),
					FText::FromString(FString::SanitizeFloat(FrameRate.AsDecimal()))));
		}
	}
	else
	{
		InSectionData->StartFrame = FFrameNumber(0);
		if (ExportContext.IsValid() && InSectionType == EMovieSceneTranslatorSectionType::Cinematic)
		{
			ExportContext->AddMessage(EMessageSeverity::Warning, FText::Format(LOCTEXT("SectionHasNoStartFrame", "Section '{0}' has no start frame. Start frame will default to 0."), FText::FromString(InSectionDisplayName)));
		}
	}

	if (InSection->HasEndFrame())
	{
		FFrameTime ExclusiveEndFrame = InSection->GetExclusiveEndFrame();
		FFrameTime ConvertedEndFrame = ConvertFrameTime(ExclusiveEndFrame, MovieSceneData->Resolution, FrameRate);
		InSectionData->EndFrame = ConvertedEndFrame.CeilToFrame();

		if (ExportContext.IsValid() && InSectionType == EMovieSceneTranslatorSectionType::Cinematic && ConvertedEndFrame.GetSubFrame() > 0.0f)
		{
			ExportContext->AddMessage(EMessageSeverity::Warning,
				FText::Format(LOCTEXT("SectionEndNotDivisByDisplayRate", "Section '{0}' ends on tick {1} which is not evenly divisible by the display rate {2}. Enable snapping and adjust the end or edit the section properties to ensure it lands evenly on a whole frame."),
					FText::FromString(InSectionDisplayName),
					FText::FromString(FString::FromInt(ExclusiveEndFrame.CeilToFrame().Value)), 
					FText::FromString(FString::SanitizeFloat(FrameRate.AsDecimal()))));
		}
	}
	else
	{
		InSectionData->EndFrame = MovieSceneData->PlaybackRangeEndFrame;
		if (ExportContext.IsValid() && InSectionType == EMovieSceneTranslatorSectionType::Cinematic)
		{
			ExportContext->AddMessage(EMessageSeverity::Warning, FText::Format(LOCTEXT("SectionHasNoEndFrame", "Section '{0}' has no end frame. End frame will default to playback range end."), FText::FromString(InSectionDisplayName)));
		}
	}

	// @todo handle intersection with playback range
	TRange<FFrameNumber> PlaybackRange = InMovieScene->GetPlaybackRange();
	TRange<FFrameNumber> EditRange = InSection->GetRange();
	TRange<FFrameNumber> Intersection = TRange<FFrameNumber>::Intersection(PlaybackRange, EditRange);
	InSectionData->bWithinPlaybackRange = EditRange.Overlaps(PlaybackRange);
	InSectionData->bEnabled = true;

	return true;
}

FString FMovieSceneExportData::GetFilename() const
{
	return SaveFilename;
}

FString FMovieSceneExportData::GetFilenamePath() const
{
	return SaveFilenamePath;
}

FFrameRate FMovieSceneExportData::GetFrameRate() const
{
	return FrameRate;
}

uint32 FMovieSceneExportData::GetNearestWholeFrameRate() const
{
	if (GetFrameRateIsNTSC())
	{
		double Rate = FrameRate.AsDecimal();
		return static_cast<int32>(FMath::FloorToDouble(Rate + 0.5));
	}
	return static_cast<uint32>(FrameRate.AsDecimal());
}

bool FMovieSceneExportData::GetFrameRateIsNTSC() const
{
	float FractionalPart = FMath::Frac(FrameRate.AsDecimal());
	return (!FMath::IsNearlyZero(FractionalPart));
}

FFrameTime FMovieSceneExportData::GetHandleFrames() const
{
	return HandleFrames;
}

FMovieSceneImportData::FMovieSceneImportData(UMovieScene* InMovieScene, TSharedPtr<FMovieSceneTranslatorContext> InContext)
{
	if (InMovieScene == nullptr)
	{
		return;
	}

	ImportContext = InContext;

	MovieSceneData = ConstructMovieSceneData(InMovieScene);
}

FMovieSceneImportData::FMovieSceneImportData() : MovieSceneData(nullptr)
{
}

FMovieSceneImportData::~FMovieSceneImportData()
{
}

bool FMovieSceneImportData::IsImportDataValid() const
{
	return MovieSceneData.IsValid();
}

TSharedPtr<FMovieSceneImportCinematicSectionData> FMovieSceneImportData::FindCinematicSection(const FString& InName) 
{
	TSharedPtr<FMovieSceneImportCinematicMasterTrackData> MasterTrackData = GetCinematicMasterTrackData(false);
	if (!MasterTrackData.IsValid())
	{
		return nullptr;
	}

	for (TSharedPtr<FMovieSceneImportCinematicSectionData> CinematicSectionData : MasterTrackData->CinematicSectionsData)
	{
		UMovieSceneCinematicShotSection* CinematicShotSection = CinematicSectionData->CinematicSection;
		if (CinematicShotSection != nullptr)
		{
			UMovieSceneSequence* ShotSequence = CinematicShotSection->GetSequence();

			if (ShotSequence != nullptr && ShotSequence->GetName() == InName)
			{
				return CinematicSectionData;
			}
		}
	}
	return nullptr;
}

/** Create cinematic section */
TSharedPtr<FMovieSceneImportCinematicSectionData> FMovieSceneImportData::CreateCinematicSection(FString InName, int32 InRow, FFrameRate InFrameRate, FFrameNumber InStartFrame, FFrameNumber InEndFrame, FFrameNumber InStartOffsetFrame)
{
	if (!MovieSceneData.IsValid() || MovieSceneData->MovieScene == nullptr)
	{ 
		return nullptr;
	}

	TSharedPtr<FMovieSceneImportCinematicMasterTrackData> MasterTrackData = GetCinematicMasterTrackData(true);
	if (!MasterTrackData.IsValid())
	{
		return nullptr;
	}

	UMovieSceneCinematicShotTrack* MasterTrack = MasterTrackData->CinematicMasterTrack;
	if (MasterTrack == nullptr)
	{
		return nullptr;
	}

	UMovieSceneSequence* SequenceToAdd = nullptr;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	TArray<FAssetData> AssetDataArray;
	AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), AssetDataArray);

	for (FAssetData AssetData : AssetDataArray)
	{
		if (AssetData.AssetName == *InName)
		{
			SequenceToAdd = Cast<ULevelSequence>(AssetData.GetAsset());
			break;
		}
	}

	if (SequenceToAdd == nullptr)
	{
		return nullptr;
	}

	// both FCP XML and Sequencer have inclusive start frame, exclusive end frame
	FFrameRate Resolution = MovieSceneData->MovieScene->GetTickResolution();
	FFrameNumber StartFrame = ConvertFrameTime(InStartFrame, InFrameRate, Resolution).RoundToFrame();
	FFrameNumber StartOffsetFrame = ConvertFrameTime(InStartOffsetFrame, InFrameRate, Resolution).RoundToFrame();
	FFrameNumber EndFrame = ConvertFrameTime(InEndFrame, InFrameRate, Resolution).RoundToFrame();
	int32 Duration = (EndFrame - StartFrame).Value;
	
	MasterTrack->Modify();
	UMovieSceneCinematicShotSection* Section = Cast<UMovieSceneCinematicShotSection>(MasterTrack->AddSequence(SequenceToAdd, StartFrame, Duration));
	if (Section == nullptr)
	{
		return nullptr;
	}
	Section->Modify();
	Section->SetRowIndex(InRow);
	Section->Parameters.SetStartFrameOffset(StartOffsetFrame.Value);
	Section->SetRange(TRange<FFrameNumber>(StartFrame, EndFrame));

	TSharedPtr<FMovieSceneImportCinematicSectionData> SectionData = ConstructCinematicSectionData(Section);
	MasterTrackData->CinematicSectionsData.Add(SectionData);

	return SectionData;
}

bool FMovieSceneImportData::SetCinematicSection(TSharedPtr<FMovieSceneImportCinematicSectionData> InSection, int32 InRow, FFrameRate InFrameRate, FFrameNumber InStartFrame, FFrameNumber InEndFrame, FFrameNumber InStartOffsetFrame)
{
	if (!InSection.IsValid() || InSection->CinematicSection == nullptr)
	{
		return false;
	}

	FFrameRate Resolution = MovieSceneData->MovieScene->GetTickResolution();
	FFrameNumber StartFrame = ConvertFrameTime(InStartFrame, InFrameRate, Resolution).GetFrame();
	FFrameNumber StartOffsetFrame = ConvertFrameTime(InStartOffsetFrame, InFrameRate, Resolution).GetFrame();
	FFrameNumber EndFrame = ConvertFrameTime(InEndFrame, InFrameRate, Resolution).GetFrame();

	InSection->CinematicSection->Modify();
	InSection->CinematicSection->Parameters.SetStartFrameOffset(StartOffsetFrame.Value);
	InSection->CinematicSection->SetRange(TRange<FFrameNumber>(StartFrame, EndFrame));
	if (InRow != InSection->CinematicSection->GetRowIndex())
	{
		InSection->CinematicSection->SetRowIndex(InRow);
	}

	return true;
}

TSharedPtr<FMovieSceneImportAudioSectionData> FMovieSceneImportData::FindAudioSection(FString InName) const
{
	// @todo

	return nullptr;
}


TSharedPtr<FMovieSceneImportCinematicMasterTrackData> FMovieSceneImportData::GetCinematicMasterTrackData(bool CreateTrackIfNull) 
{
	if (!MovieSceneData.IsValid())
	{
		return nullptr;
	}
	if (!MovieSceneData->CinematicMasterTrackData.IsValid() && CreateTrackIfNull)
	{
		UMovieSceneCinematicShotTrack* CinematicMasterTrack = MovieSceneData->MovieScene->AddMasterTrack<UMovieSceneCinematicShotTrack>();
		MovieSceneData->CinematicMasterTrackData = ConstructCinematicMasterTrackData(CinematicMasterTrack);
	}
	return MovieSceneData->CinematicMasterTrackData;
}

TSharedPtr<FMovieSceneImportMovieSceneData> FMovieSceneImportData::ConstructMovieSceneData(UMovieScene* InMovieScene)
{
	if (InMovieScene == nullptr)
	{
		return nullptr;
	}

	MovieSceneData = MakeShared<FMovieSceneImportMovieSceneData>();
	MovieSceneData->MovieScene = InMovieScene;

	// Get cinematic master track
	UMovieSceneCinematicShotTrack* CinematicMasterTrack = InMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (CinematicMasterTrack != nullptr)
	{
		MovieSceneData->CinematicMasterTrackData = ConstructCinematicMasterTrackData(CinematicMasterTrack);
		if (!MovieSceneData->CinematicMasterTrackData.IsValid())
		{
			return nullptr;
		}
	}

	// Get audio tracks
	const TArray<UMovieSceneTrack*> MasterTracks = InMovieScene->GetMasterTracks();
	for (UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		if (MasterTrack->IsA(UMovieSceneAudioTrack::StaticClass()))
		{

			UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(MasterTrack);
			TSharedPtr<FMovieSceneImportAudioTrackData> AudioTrackData = ConstructAudioTrackData(AudioTrack);
			if (!AudioTrackData.IsValid())
			{
				return nullptr;
			}
			MovieSceneData->AudioTracksData.Add(AudioTrackData);
		}
	}

	return MovieSceneData;
}


TSharedPtr<FMovieSceneImportCinematicMasterTrackData> FMovieSceneImportData::ConstructCinematicMasterTrackData(UMovieSceneCinematicShotTrack* InCinematicMasterTrack)
{
	if (!MovieSceneData.IsValid() || InCinematicMasterTrack == nullptr)
	{
		return nullptr;
	}

	TSharedPtr<FMovieSceneImportCinematicMasterTrackData> TrackData = MakeShared<FMovieSceneImportCinematicMasterTrackData>();
	TrackData->CinematicMasterTrack = InCinematicMasterTrack;

	for (UMovieSceneSection* ShotSection : InCinematicMasterTrack->GetAllSections())
	{
		UMovieSceneCinematicShotSection* CinematicSection = Cast<UMovieSceneCinematicShotSection>(ShotSection);

		if (CinematicSection != nullptr && CinematicSection->GetSequence() != nullptr)
		{
			TSharedPtr<FMovieSceneImportCinematicSectionData> CinematicSectionData = ConstructCinematicSectionData(CinematicSection);
			if (!CinematicSectionData.IsValid())
			{
				return nullptr;
			}
			TrackData->CinematicSectionsData.Add(CinematicSectionData);
		}
	}

	return TrackData;
}

TSharedPtr<FMovieSceneImportAudioTrackData> FMovieSceneImportData::ConstructAudioTrackData(UMovieSceneAudioTrack* InAudioTrack)
{
	if (!MovieSceneData.IsValid() || InAudioTrack == nullptr)
	{
		return nullptr;
	}

	TSharedPtr<FMovieSceneImportAudioTrackData> TrackData = MakeShared<FMovieSceneImportAudioTrackData>();
	TrackData->AudioTrack = InAudioTrack;

	for (UMovieSceneSection* ShotSection : InAudioTrack->GetAllSections())
	{
		UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(ShotSection);

		TSharedPtr<FMovieSceneImportAudioSectionData> AudioSectionData = ConstructAudioSectionData(AudioSection);
		if (!AudioSectionData.IsValid())
		{
			return nullptr;
		}
		TrackData->AudioSectionsData.Add(AudioSectionData);
	}

	return TrackData;
}

TSharedPtr<FMovieSceneImportCinematicSectionData> FMovieSceneImportData::ConstructCinematicSectionData(UMovieSceneCinematicShotSection* InCinematicSection)
{
	if (!MovieSceneData.IsValid() || MovieSceneData->MovieScene == nullptr || InCinematicSection == nullptr)
	{
		return nullptr;
	}	

	TSharedPtr<FMovieSceneImportCinematicSectionData> SectionData = MakeShared<FMovieSceneImportCinematicSectionData>();
	SectionData->CinematicSection = InCinematicSection;
	
	return SectionData;
}

TSharedPtr<FMovieSceneImportAudioSectionData> FMovieSceneImportData::ConstructAudioSectionData(UMovieSceneAudioSection* InAudioSection)
{
	if (InAudioSection == nullptr)
	{
		return nullptr;
	}

	TSharedPtr<FMovieSceneImportAudioSectionData> SectionData = MakeShared<FMovieSceneImportAudioSectionData>();
	SectionData->AudioSection = InAudioSection;

	return SectionData;
}


void FMovieSceneTranslatorContext::Init()
{
	ClearMessages();
}

void FMovieSceneTranslatorContext::AddMessage(EMessageSeverity::Type InMessageSeverity, FText InMessage)
{
	Messages.Add(FTokenizedMessage::Create(InMessageSeverity, InMessage));
}

void FMovieSceneTranslatorContext::ClearMessages()
{
	Messages.Empty();
}

bool FMovieSceneTranslatorContext::ContainsMessageType(EMessageSeverity::Type InMessageSeverity) const
{
	for (const TSharedRef<FTokenizedMessage>& Message : Messages)
	{
		if (Message->GetSeverity() == InMessageSeverity)
		{
			return true;
		}
	}
	return false;
}

const TArray<TSharedRef<FTokenizedMessage>>& FMovieSceneTranslatorContext::GetMessages() const
{
	return Messages;
}

#undef LOCTEXT_NAMESPACE
