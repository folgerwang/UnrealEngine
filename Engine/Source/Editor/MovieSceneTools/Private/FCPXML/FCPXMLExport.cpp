// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FCPXML/FCPXMLExport.h"
#include "MovieScene.h"
#include "LevelSequence.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "MovieSceneTimeHelpers.h"
#include "AssetRegistryModule.h"
#include "UObject/MetaData.h"
#include "Logging/TokenizedMessage.h"

#define LOCTEXT_NAMESPACE "FCPXMLExporter"

DEFINE_LOG_CATEGORY_STATIC(LogFCPXMLExporter, Log, All);

FFCPXMLExportVisitor::FFCPXMLExportVisitor(FString InSaveFilename, TSharedRef<FMovieSceneExportData> InExportData, TSharedRef<FMovieSceneTranslatorContext> InExportContext) :
	  FFCPXMLNodeVisitor()
	, ExportData(InExportData)
	, ExportContext(InExportContext)
	, SaveFilePath(InSaveFilename)
{
	SaveFilePath = FPaths::GetPath(InSaveFilename);
	if (FPaths::IsRelative(SaveFilePath))
	{
		SaveFilePath = FPaths::ConvertRelativePathToFull(SaveFilePath);
	}

	SequenceId = 0;
}

FFCPXMLExportVisitor::~FFCPXMLExportVisitor() {}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLBasicNode> InBasicNode)
{
	return InBasicNode->VisitChildren(*this);
}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLXmemlNode> InXmemlNode)
{
	float EditTime = 0.f;

	// Construct the FCP 7 XML structure from Sequencer class objects
	if (InXmemlNode->GetChildCount() == 0)
	{
		if (!ConstructProjectNode(InXmemlNode)) 
		{
			return false;
		}
	}

	/** @todo - MERGE METADATA

	   Merging the newly exported XML structure with pre-existing
	   XML metadata will be implemented here. The traversal would proceed
	   through the new XML structure, referring back to the metadata 
	   XML structure to incorporate any missing or desired attributes or
	   elements. Alternatively, the traversal might be invoked directly on
	   the metadata referring back to and modifying the new XML structure. 

	*/

	return InXmemlNode->VisitChildren(*this);
}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLSequenceNode> InSequenceNode)
{
	return InSequenceNode->VisitChildren(*this);
}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLVideoNode> InVideoNode)
{
	return InVideoNode->VisitChildren(*this);
}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLAudioNode> InAudioNode)
{
	return InAudioNode->VisitChildren(*this);
}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLTrackNode> InTrackNode)
{
	return InTrackNode->VisitChildren(*this);
}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLClipNode> InClipNode)
{
	return InClipNode->VisitChildren(*this);
}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLClipItemNode> InClipItemNode)
{
	return InClipItemNode->VisitChildren(*this);
}

bool FFCPXMLExportVisitor::VisitNode(TSharedRef<FFCPXMLFileNode> InFileNode)
{
	return InFileNode->VisitChildren(*this);
}


/** Creates project node. */
bool FFCPXMLExportVisitor::ConstructProjectNode(TSharedRef<FFCPXMLNode> InParentNode)
{
	TSharedRef<FFCPXMLNode> ProjectNode = InParentNode->CreateChildNode(TEXT("project"));

	TSharedRef<FFCPXMLNode> NameNode = ProjectNode->CreateChildNode(TEXT("name"));
	NameNode->SetContent(ExportData->MovieSceneData->Name + TEXT("Project"));

	TSharedRef<FFCPXMLNode> ChildrenNode = ProjectNode->CreateChildNode(TEXT("children"));

	if (!ConstructMasterClipNodes(ChildrenNode))
	{
		return false;
	}

	if (!ConstructSequenceNode(ChildrenNode))
	{
		return false;
	}

	return true;
}

/** Creates master clip node. */
bool FFCPXMLExportVisitor::ConstructMasterClipNodes(TSharedRef<FFCPXMLNode> InParentNode)
{
	if (!ExportData->IsExportDataValid() || !ExportData->MovieSceneData.IsValid() || !ExportData->MovieSceneData->CinematicMasterTrack.IsValid())
	{
		return false;
	}

	int32 MasterClipId = 0;

	TSharedPtr<FMovieSceneExportCinematicMasterTrackData> CinematicMasterTrackData = ExportData->MovieSceneData->CinematicMasterTrack;
	if (!CinematicMasterTrackData.IsValid())
	{
		return false;
	}

	for (TSharedPtr<FMovieSceneExportCinematicSectionData> CinematicSection : CinematicMasterTrackData->CinematicSections)
	{
		if (!CinematicSection.IsValid())
		{
			continue;
		}

		// skip sections outside of playback range
		if (CinematicSection->bWithinPlaybackRange == false)
		{
			CinematicSection->bEnabled = false;
			continue;
		}

		// skip sections if media file does not exist
		FString FilePathName = GetFilePathName(CinematicSection->ShotFilename);
		if (!FPaths::FileExists(FilePathName))
		{
			// add warning message and skip this section
			ExportContext->AddMessage(
				EMessageSeverity::Warning,
				FText::Format(LOCTEXT("SkippingSection", "Warning: Skipping section {0}, media file does not exist: {1}."), FText::FromString(CinematicSection->ShotDisplayName), FText::FromString(FilePathName)));
			CinematicSection->bEnabled = false;
			continue;
		}


		if (!ConstructMasterClipNode(InParentNode, CinematicSection, ++MasterClipId))
		{
			return false;
		}
	}
	return true;
}

bool FFCPXMLExportVisitor::ConstructMasterClipNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, int32 InMasterClipId)
{
	if (!InCinematicSectionData.IsValid())
	{
		return false;
	}

	int32 Duration{ 0 };
	int32 StartFrame{ 0 };
	int32 EndFrame{ 0 };
	int32 InFrame{ 0 };
	int32 OutFrame{ 0 };
	FString SectionName(TEXT(""));
	GetSectionFrames(InCinematicSectionData, Duration, StartFrame, EndFrame, InFrame, OutFrame);
	GetSectionName(InCinematicSectionData, SectionName);

	/** Construct a master clip id name based on the cinematic section and id */
	FString MasterClipName(TEXT(""));
	if (!CreateMasterClipIdName(InCinematicSectionData, InMasterClipId, MasterClipName))
	{
		return false;
	}

	TSharedRef<FFCPXMLNode> ClipNode = InParentNode->CreateChildNode(TEXT("clip"));
	ClipNode->AddAttribute(TEXT("id"), MasterClipName);

	// @todo add to file's masterclip and refidmap HERE

	ClipNode->CreateChildNode(TEXT("masterclipid"))->SetContent(MasterClipName);
	ClipNode->CreateChildNode(TEXT("ismasterclip"))->SetContent(true);
	ClipNode->CreateChildNode(TEXT("duration"))->SetContent(Duration);

	if (!ConstructRateNode(ClipNode))
	{
		return false;
	}

	ClipNode->CreateChildNode(TEXT("in"))->SetContent(InFrame);
	ClipNode->CreateChildNode(TEXT("out"))->SetContent(OutFrame);
	ClipNode->CreateChildNode(TEXT("name"))->SetContent(SectionName);
	TSharedRef<FFCPXMLNode> MediaNode = ClipNode->CreateChildNode(TEXT("media"));
	TSharedRef<FFCPXMLNode> VideoNode = MediaNode->CreateChildNode(TEXT("video"));
	TSharedRef<FFCPXMLNode> TrackNode = VideoNode->CreateChildNode(TEXT("track"));

	if (!ConstructVideoClipItemNode(TrackNode, InCinematicSectionData, true))
	{
		return false;
	}

	if (!ConstructSectionLoggingInfoNode(ClipNode, InCinematicSectionData, SectionName))
	{
		return false;
	}

	if (!ConstructColorInfoNode(ClipNode, InCinematicSectionData))
	{
		return false;
	}

	return true;
}

/** Creates logginginfo node. */
bool FFCPXMLExportVisitor::ConstructSectionLoggingInfoNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, const FString& InSectionName)
{
	if (InCinematicSectionData->CinematicShotSection == nullptr)
	{
		return false;
	}

	TSharedRef<FFCPXMLNode> LoggingInfoNode = InParentNode->CreateChildNode(TEXT("logginginfo"));
	ConstructLoggingInfoElements(LoggingInfoNode, InCinematicSectionData->CinematicShotSection);

	TSharedPtr<FFCPXMLNode> LogNoteNode = LoggingInfoNode->GetChildNode(TEXT("lognote"), ENodeInherit::NoInherit, ENodeReference::NoReferences);
	if (!LogNoteNode.IsValid())
	{
		LogNoteNode = LoggingInfoNode->CreateChildNode(TEXT("lognote"));
	}
	LogNoteNode->SetContent(FFCPXMLClipNode::GetMetadataSectionName(InSectionName));

	return true;
}

/** Creates logginginfo elements. */
void FFCPXMLExportVisitor::ConstructLoggingInfoElements(TSharedRef<FFCPXMLNode> InLoggingInfoNode, const UObject* InObject)
{
	TSharedRef<FFCPXMLNode> DescriptionNode = InLoggingInfoNode->CreateChildNode(TEXT("description"));
	TSharedRef<FFCPXMLNode> SceneNode = InLoggingInfoNode->CreateChildNode(TEXT("scene"));
	TSharedRef<FFCPXMLNode> ShotTakeNode = InLoggingInfoNode->CreateChildNode(TEXT("shottake"));
	TSharedRef<FFCPXMLNode> GoodNode = InLoggingInfoNode->CreateChildNode(TEXT("good"));
	TSharedRef<FFCPXMLNode> OriginalVideoNode = InLoggingInfoNode->CreateChildNode(TEXT("originalvideofilename"));
	TSharedRef<FFCPXMLNode> OriginalAudioNode = InLoggingInfoNode->CreateChildNode(TEXT("originalaudiofilename"));

	if (InObject != nullptr)
	{
		SetLoggingInfoElementValue(DescriptionNode, InObject, TEXT("description"));
		SetLoggingInfoElementValue(SceneNode, InObject, TEXT("scene"));
		SetLoggingInfoElementValue(ShotTakeNode, InObject, TEXT("shottake"));
		SetLoggingInfoElementValue(GoodNode, InObject, TEXT("good"));
		SetLoggingInfoElementValue(OriginalVideoNode, InObject, TEXT("originalvideofilename"));
		SetLoggingInfoElementValue(OriginalAudioNode, InObject, TEXT("originalaudiofilename"));
	}
}

/** Set logginginfo element value. */
void FFCPXMLExportVisitor::SetLoggingInfoElementValue(TSharedPtr<FFCPXMLNode> InNode, const UObject* InObject, const FString& InElement)
{
	if (InNode.IsValid() && InObject != nullptr)
	{
		UPackage* Package = InObject->GetOutermost();
		check(Package);

		UMetaData* MetaData = Package->GetMetaData();
		check(MetaData);

		if (MetaData->HasValue(InObject, *InElement))
		{
			FString Value = MetaData->GetValue(InObject, *InElement);
			InNode->SetContent(Value);
		}
	}	
}

/** Creates colorinfo node. */
bool FFCPXMLExportVisitor::ConstructColorInfoNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData)
{
	TSharedPtr<FFCPXMLNode> ColorInfoNode = InParentNode->CreateChildNode(TEXT("colorinfo"));
	ColorInfoNode->CreateChildNode(TEXT("lut"));
	ColorInfoNode->CreateChildNode(TEXT("lut1"));
	ColorInfoNode->CreateChildNode(TEXT("asc_sop"));
	ColorInfoNode->CreateChildNode(TEXT("asc_sat"));
	ColorInfoNode->CreateChildNode(TEXT("lut2"));

	return true;
}

bool FFCPXMLExportVisitor::ConstructSequenceNode(TSharedRef<FFCPXMLNode> InParentNode)
{
	if (!ExportData->IsExportDataValid() || !ExportData->MovieSceneData.IsValid()) 
	{ 
		return false; 
	}

	TSharedRef<FFCPXMLNode> SequenceNode = InParentNode->CreateChildNode("sequence");

	// attributes
	SequenceNode->AddAttribute(TEXT("id"), FString::Printf(TEXT("sequence%d"), ++SequenceId));

	// required elements
	TSharedRef<FFCPXMLNode> DurationNode = SequenceNode->CreateChildNode(TEXT("duration"));
	DurationNode->SetContent(ExportData->MovieSceneData->Duration);

	if (!ConstructRateNode(SequenceNode)) 
	{ 
		return false; 
	}

	TSharedRef<FFCPXMLNode> NameNode = SequenceNode->CreateChildNode(TEXT("name"));
	NameNode->SetContent(ExportData->MovieSceneData->Name);

	TSharedRef<FFCPXMLNode> MediaNode = SequenceNode->CreateChildNode(TEXT("media"));

	if (!ConstructVideoNode(MediaNode)) 
	{ 
		return false; 
	}

	if (!ConstructAudioNode(MediaNode))
	{
		return false;
	}

	if (!ConstructTimecodeNode(SequenceNode))
	{
		return false;
	}

	return true;
}

bool FFCPXMLExportVisitor::ConstructVideoNode(TSharedRef<FFCPXMLNode> InParentNode)
{
	if (!ExportData->IsExportDataValid() || !ExportData->MovieSceneData.IsValid() || !ExportData->MovieSceneData->CinematicMasterTrack.IsValid())
	{ 
		return false; 
	}

	TSharedPtr<FMovieSceneExportCinematicMasterTrackData> CinematicMasterTrackData = ExportData->MovieSceneData->CinematicMasterTrack;
	if (!CinematicMasterTrackData.IsValid())
	{
		return false;
	}

	TSharedRef<FFCPXMLNode> VideoNode = InParentNode->CreateChildNode(TEXT("video"));

	TSharedRef<FFCPXMLNode> FormatNode = VideoNode->CreateChildNode(TEXT("format"));
	int32 Width{ 0 };
	int32 Height{ 0 };
	GetDefaultImageResolution(Width, Height);
	if (!ConstructSampleCharacteristicsNode(FormatNode, Width, Height))
	{
		return false;
	}
 
	for (TSharedPtr<FMovieSceneExportCinematicTrackData> CinematicTrack : CinematicMasterTrackData->CinematicTracks)
	{
		if (!ConstructVideoTrackNode(VideoNode, CinematicTrack)) 
		{ 
			return false;
		}
	}

	return true;
}

bool FFCPXMLExportVisitor::ConstructAudioNode(TSharedRef<FFCPXMLNode> InParentNode)
{
	if (!ExportData->IsExportDataValid())
	{ 
		return false; 
	}

	TSharedRef<FFCPXMLNode> AudioNode = InParentNode->CreateChildNode(TEXT("audio"));

	//TSharedRef<FFCPXMLNode> FormatNode = InVideoNode->CreateChildNode(TEXT("format"));

	for (TSharedPtr<FMovieSceneExportAudioTrackData> AudioTrack : ExportData->MovieSceneData->AudioTracks)
	{
		if (!ConstructAudioTrackNode(AudioNode, AudioTrack))
		{
			return false;
		}
	}

	return true;
}

bool FFCPXMLExportVisitor::ConstructVideoTrackNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicTrackData> InCinematicTrackData)
{
	if (!ExportData->IsExportDataValid() || !InCinematicTrackData.IsValid())
	{
		return false;
	}

	TSharedRef<FFCPXMLNode> TrackNode = InParentNode->CreateChildNode(TEXT("track"));

	for (TSharedPtr<FMovieSceneExportCinematicSectionData> CinematicSection : InCinematicTrackData->CinematicSections)
	{
		// skip disabled sections
		if (!CinematicSection.IsValid() || !CinematicSection->bEnabled)
		{
			continue;
		}
		
		if (!ConstructVideoClipItemNode(TrackNode, CinematicSection, false))
		{
			return false;
		}
	}

	TSharedRef<FFCPXMLNode> EnabledNode = TrackNode->CreateChildNode(TEXT("enabled"));
	EnabledNode->SetContent(true);

	TSharedRef<FFCPXMLNode> LockedNode = TrackNode->CreateChildNode(TEXT("locked"));
	LockedNode->SetContent(false);

	return true;
}

bool FFCPXMLExportVisitor::ConstructAudioTrackNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportAudioTrackData> InAudioTrackData)
{
	if (!ExportData->IsExportDataValid() || !InAudioTrackData.IsValid())
	{
		return false;
	}

	TSharedRef<FFCPXMLNode> TrackNode = InParentNode->CreateChildNode(TEXT("track"));

	for (TSharedPtr<FMovieSceneExportAudioSectionData> AudioSection : InAudioTrackData->AudioSections)
	{
		if (!ConstructAudioClipItemNode(TrackNode, AudioSection))
		{
			return false;
		}
	}

	TSharedRef<FFCPXMLNode> EnabledNode = TrackNode->CreateChildNode(TEXT("enabled"));
	EnabledNode->SetContent(true);

	TSharedRef<FFCPXMLNode> LockedNode = TrackNode->CreateChildNode(TEXT("locked"));
	LockedNode->SetContent(false);

	return true;
}

bool FFCPXMLExportVisitor::ConstructVideoClipItemNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, bool bInMasterClip)
{
	if (!ExportData->IsExportDataValid() || !InCinematicSectionData.IsValid())
	{
		return false;
	}

	TSharedRef<FFCPXMLNode> ClipItemNode = InParentNode->CreateChildNode(TEXT("clipitem"));

	int32 Duration{ 0 };
	int32 In{ 0 };
	int32 Out{ 0 };
	int32 Start{ 0 };
	int32 End{ 0 };
	FString SectionName(TEXT(""));

	GetSectionFrames(InCinematicSectionData, Duration, Start, End, In, Out);
	GetSectionName(InCinematicSectionData, SectionName);

	FString ClipItemIdName = TEXT("");
	if (!GetMasterClipItemIdName(InCinematicSectionData, ClipItemIdName))
	{
		return false;
	}

	FString MasterClipIdName = TEXT("");
	if (!GetMasterClipIdName(InCinematicSectionData, MasterClipIdName))
	{
		return false;
	}

	// attributes
	ClipItemNode->AddAttribute(TEXT("id"), ClipItemIdName);

	// elements
	ClipItemNode->CreateChildNode(TEXT("masterclipid"))->SetContent(MasterClipIdName);
	ClipItemNode->CreateChildNode(TEXT("ismasterclip"))->SetContent(bInMasterClip);
	ClipItemNode->CreateChildNode(TEXT("name"))->SetContent(SectionName);
	ClipItemNode->CreateChildNode(TEXT("enabled"))->SetContent(true);
	ClipItemNode->CreateChildNode(TEXT("duration"))->SetContent(Duration);

	if (!ConstructRateNode(ClipItemNode))
	{
		return false;
	}

	if (!bInMasterClip)
	{
		ClipItemNode->CreateChildNode(TEXT("start"))->SetContent(Start);
		ClipItemNode->CreateChildNode(TEXT("end"))->SetContent(End);
	}

	ClipItemNode->CreateChildNode(TEXT("in"))->SetContent(In);
	ClipItemNode->CreateChildNode(TEXT("out"))->SetContent(Out);

	if (bInMasterClip)
	{
		ClipItemNode->CreateChildNode(TEXT("anamorphic"))->SetContent(false);
		ClipItemNode->CreateChildNode(TEXT("pixelaspectratio"))->SetContent(TEXT("square"));
		ClipItemNode->CreateChildNode(TEXT("fielddominance"))->SetContent(TEXT("lower"));
	}

	if (!ConstructVideoFileNode(ClipItemNode, InCinematicSectionData, Duration, bInMasterClip))
	{
		return false;
	}

	return true;
}

bool FFCPXMLExportVisitor::ConstructAudioClipItemNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportAudioSectionData> InAudioSectionData)
{
	if (!ExportData->IsExportDataValid() || !InAudioSectionData.IsValid())
	{
		return false;
	}

	// @todo - audio

	return true;
}

bool FFCPXMLExportVisitor::ConstructVideoFileNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, uint32 Duration, bool bInMasterClip)
{
	if (!InCinematicSectionData.IsValid())
	{
		return false;
	}

	FString FileIdName = TEXT("");
	if (!GetMasterClipFileIdName(InCinematicSectionData, FileIdName))
	{
		return false;
	}

	// attributes
	TSharedRef<FFCPXMLNode> FileNode = InParentNode->CreateChildNode(TEXT("file"));
	FileNode->AddAttribute(TEXT("id"), FileIdName);

	if (bInMasterClip)
	{
		FString FilePathName = SaveFilePath + TEXT("/") + InCinematicSectionData->ShotFilename;
		FString FilePathUrl = FString(TEXT("file://localhost/")) + FilePathName.Replace(TEXT(" "), TEXT("%20")).Replace(TEXT(":"), TEXT("%3a"));

		// required elements
		TSharedRef<FFCPXMLNode> NameNode = FileNode->CreateChildNode(TEXT("name"));
		NameNode->SetContent(InCinematicSectionData->ShotFilename);

		TSharedRef<FFCPXMLNode> PathUrlNode = FileNode->CreateChildNode(TEXT("pathurl"));
		PathUrlNode->SetContent(FilePathUrl);

		if (!ConstructRateNode(FileNode))
		{
			return false;
		}

		TSharedRef<FFCPXMLNode> DurationNode = FileNode->CreateChildNode(TEXT("duration"));
		DurationNode->SetContent(static_cast<int32>(Duration));

		if (!ConstructTimecodeNode(FileNode))
		{
			return false;
		}

		TSharedRef<FFCPXMLNode> MediaNode = FileNode->CreateChildNode(TEXT("media"));
		TSharedRef<FFCPXMLNode> VideoNode = MediaNode->CreateChildNode(TEXT("video"));

		int32 Width{ 0 };
		int32 Height{ 0 };
		GetDefaultImageResolution(Width, Height);
		if (!ConstructSampleCharacteristicsNode(VideoNode, Width, Height))
		{
			return false;
		}
	}

	return true;
}

bool FFCPXMLExportVisitor::ConstructAudioFileNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportAudioSectionData> InAudioSectionData)
{
	// @todo - audio

	return true;
}

bool FFCPXMLExportVisitor::ConstructSampleCharacteristicsNode(TSharedRef<FFCPXMLNode> InParentNode, int InWidth, int InHeight)
{
	TSharedRef<FFCPXMLNode> SampleCharacteristicsNode = InParentNode->CreateChildNode(TEXT("samplecharacteristics"));

	if (!ConstructRateNode(SampleCharacteristicsNode))
	{
		return false;
	}

	SampleCharacteristicsNode->CreateChildNode(TEXT("width"))->SetContent(InWidth);
	SampleCharacteristicsNode->CreateChildNode(TEXT("height"))->SetContent(InHeight);
	SampleCharacteristicsNode->CreateChildNode(TEXT("anamorphic"))->SetContent(false);
	SampleCharacteristicsNode->CreateChildNode(TEXT("pixelaspectratio"))->SetContent(TEXT("square"));
	SampleCharacteristicsNode->CreateChildNode(TEXT("fielddominance"))->SetContent(TEXT("lower"));

	return true;
}

bool FFCPXMLExportVisitor::ConstructRateNode(TSharedRef<FFCPXMLNode> InParentNode)
{
	TSharedRef<FFCPXMLNode> RateNode = InParentNode->CreateChildNode(TEXT("rate"));

	TSharedRef<FFCPXMLNode> TimebaseNode = RateNode->CreateChildNode(TEXT("timebase"));
	TimebaseNode->SetContent(static_cast<int32>(ExportData->GetNearestWholeFrameRate()));

	TSharedRef<FFCPXMLNode> NTSCNode = RateNode->CreateChildNode(TEXT("ntsc"));
	NTSCNode->SetContent(ExportData->GetFrameRateIsNTSC());

	return true;
}

bool FFCPXMLExportVisitor::ConstructTimecodeNode(TSharedRef<FFCPXMLNode> InParentNode)
{
	TSharedRef<FFCPXMLNode> TimecodeNode = InParentNode->CreateChildNode(TEXT("timecode"));

	if (!ConstructRateNode(TimecodeNode))
	{
		return false;
	}

	TimecodeNode->CreateChildNode(TEXT("string"))->SetContent(TEXT("00:00:00:00"));
	TimecodeNode->CreateChildNode(TEXT("frame"))->SetContent(0);
	TimecodeNode->CreateChildNode(TEXT("displayformat"))->SetContent(TEXT("NDF"));

	TSharedRef<FFCPXMLNode> ReelNode = TimecodeNode->CreateChildNode(TEXT("reel"));
	ReelNode->CreateChildNode(TEXT("name"))->SetContent(TEXT(""));

	return true;
}

/** Get duration, in and out frames for a given shot section */
bool FFCPXMLExportVisitor::GetSectionFrames(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, int32& OutDuration, int32& OutStartFrame, int32&OutEndFrame, int32& OutInFrame, int32& OutOutFrame)
{
	if (!InCinematicSectionData.IsValid() || !ExportData->MovieSceneData.IsValid())
	{
		return false;
	}

	int32 Handles = ExportData->GetHandleFrames().FloorToFrame().Value;
	OutStartFrame = InCinematicSectionData->StartFrame.Value;
	OutEndFrame = InCinematicSectionData->EndFrame.Value;
	OutDuration = OutEndFrame - OutStartFrame;
	OutInFrame = Handles;
	OutOutFrame = Handles + OutDuration;

	return true;
}

/** Get name of a given shot section */
bool FFCPXMLExportVisitor::GetSectionName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString& OutSectionName)
{
	if (!InCinematicSectionData.IsValid())
	{
		return false;
	}
	OutSectionName = InCinematicSectionData->ShotDisplayName;
	return true;
}

bool FFCPXMLExportVisitor::CreateMasterClipIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, uint32 InId, FString& OutName)
{
	if (!InCinematicSectionData.IsValid())
	{
		return false;
	}
	FString Name(TEXT(""));
	GetSectionName(InCinematicSectionData, Name);
	MasterClipIdMap.Add(Name, InId);
	OutName = FString::Printf(TEXT("masterclip%d_"), InId) + Name;
	return true;
}

bool FFCPXMLExportVisitor::GetIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString InPrefix, FString &OutName)
{
	if (!InCinematicSectionData.IsValid())
	{
		return false;
	}
	FString Name(TEXT(""));
	GetSectionName(InCinematicSectionData, Name);
	uint32* Id = MasterClipIdMap.Find(Name);
	if (Id == nullptr)
	{
		return false;
	}
	OutName = InPrefix + FString::Printf(TEXT("%d_"), *Id) + Name;
	return true;
}

bool FFCPXMLExportVisitor::GetMasterClipIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString& OutName)
{
	return GetIdName(InCinematicSectionData, FString(TEXT("masterclip")), OutName);
}

bool FFCPXMLExportVisitor::GetMasterClipItemIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString& OutName)
{
	return GetIdName(InCinematicSectionData, FString(TEXT("clipitem")), OutName);
}

bool FFCPXMLExportVisitor::GetMasterClipFileIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString& OutName)
{
	return GetIdName(InCinematicSectionData, FString(TEXT("file")), OutName);
}

void FFCPXMLExportVisitor::GetDefaultImageResolution(int32& OutWidth, int32& OutHeight)
{
	OutWidth = 1280;
	OutHeight = 720;
}

FString FFCPXMLExportVisitor::GetFilePathName(const FString& InSectionName) const
{
	return (SaveFilePath + TEXT("/") + InSectionName);
}

#undef LOCTEXT_NAMESPACE
