// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FCPXML/FCPXMLImport.h"
#include "MovieScene.h"
#include "MovieSceneTranslator.h"
#include "LevelSequence.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "AssetRegistryModule.h"
#include "UObject/MetaData.h"

#define LOCTEXT_NAMESPACE "FCPXMLImporter"
// DEFINE_LOG_CATEGORY_STATIC(FCPXMLImporter, Log, All);

#define INHERIT true
#define NO_INHERIT false

FFCPXMLImportVisitor::FFCPXMLImportVisitor(TSharedRef<FMovieSceneImportData> InImportData, TSharedRef<FMovieSceneTranslatorContext> InImportContext) :
	FFCPXMLNodeVisitor()
	, ImportData(InImportData)
	, ImportContext(InImportContext)
	, bInSequenceNode(false)
	, bInVideoNode(false)
	, bInAudioNode(false)
	, bInVideoTrackNode(false)
	, bInAudioTrackNode(false)
	, CurrVideoTrackRowIndex(0)
	, CurrAudioTrackIndex(0)
{
}
	
FFCPXMLImportVisitor::~FFCPXMLImportVisitor() {}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLBasicNode> InBasicNode)
{
	return InBasicNode->VisitChildren(*this);
}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLXmemlNode> InXmemlNode)
{
	return InXmemlNode->VisitChildren(*this);
}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLSequenceNode> InSequenceNode)
{ 
	bool bPrev = bInSequenceNode;
	bInSequenceNode = true;

	// Sequences can be referenced so flag to visit reference node children
	bool bSuccess = InSequenceNode->VisitChildren(*this, true);
	
	bInSequenceNode = false;

	return bSuccess;
}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLVideoNode> InVideoNode)
{
	/*
	if (!ImportData->GetCinematicMasterTrackData(true).IsValid())
	{
		return false;
	}
	*/
	bool bPrev = bInVideoNode;
	bInVideoNode = true;
	int32 PrevVideoTrackRowIndex = CurrVideoTrackRowIndex;
	CurrVideoTrackRowIndex = 0;

	bool bSuccess = InVideoNode->VisitChildren(*this);
	
	bInVideoNode = bPrev;
	CurrVideoTrackRowIndex = PrevVideoTrackRowIndex;

	return bSuccess;
}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLAudioNode> InAudioNode)
{
	// @todo
	CurrAudioTrackIndex = 0;
	return InAudioNode->VisitChildren(*this);
}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLTrackNode> InTrackNode)
{
	bool bPrevInTrackNode = false;
	int32 PrevTrackIndex = 0;

	if (bInSequenceNode && bInVideoNode)
	{
		bPrevInTrackNode = bInVideoTrackNode;
		bInVideoTrackNode = true;
	}
	else if (bInSequenceNode && bInAudioNode)
	{
		bPrevInTrackNode = bInAudioTrackNode;
		bInAudioTrackNode = true;
	}
	
	bool bSuccess = InTrackNode->VisitChildren(*this);

	if (bInSequenceNode && bInVideoNode)
	{
		bInVideoTrackNode = bPrevInTrackNode;
		CurrVideoTrackRowIndex++;
	}
	else if (bInSequenceNode && bInAudioNode)
	{
		bInAudioTrackNode = bPrevInTrackNode;
		CurrAudioTrackIndex++;
	}

	return bSuccess;
}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLClipNode> InClipNode)
{
	// masterclip
	bool bIsMasterClip = false;
	FString MasterClipName(TEXT(""));
	FString MetadataSectionName(TEXT(""));

	if (InClipNode->GetChildValue<bool>("ismasterclip", bIsMasterClip, ENodeInherit::NoInherit) && bIsMasterClip)
	{
		bool bHasMasterClipName = InClipNode->GetChildValue<FString>("masterclipid", MasterClipName, ENodeInherit::NoInherit);

		TSharedPtr<FFCPXMLNode> LoggingInfoNode = InClipNode->GetChildNode("logginginfo", ENodeInherit::NoInherit, ENodeReference::NoReferences);
		if (LoggingInfoNode.IsValid())
		{
			bool bHasLoggingShotTrack = LoggingInfoNode->GetChildValue<FString>("lognote", MetadataSectionName, ENodeInherit::NoInherit, ENodeReference::NoReferences);

			if (!MetadataSectionName.IsEmpty())
			{
				if (bHasMasterClipName)
				{
					FString CinematicSectionName;
					if (FFCPXMLClipNode::GetSequencerSectionName(MetadataSectionName, CinematicSectionName))
					{
						AddMasterClipSectionName(MasterClipName, CinematicSectionName);
						AddSectionMetaData(CinematicSectionName, LoggingInfoNode);
					}
				}
			}
		}
	}

	// Clips can be referenced so flag to visit reference node children
	return InClipNode->VisitChildren(*this, true);
}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLClipItemNode> InClipItemNode)
{
	if (bInSequenceNode && bInVideoTrackNode)
	{
		// Sequencer section name 
		FString CinematicSectionName(TEXT(""));
		bool bHasCinematicSectionName = false;

		// masterclip
		FString MasterClipIdName(TEXT(""));
		if (InClipItemNode->GetChildValue<FString>("masterclipid", MasterClipIdName, ENodeInherit::NoInherit))
		{
			bHasCinematicSectionName = GetMasterClipSectionName(MasterClipIdName, CinematicSectionName);
		}

		// Frame rate 
		int32 FrameRateTimebase = 0;
		if (!InClipItemNode->GetChildSubValue<int32>("rate", "timebase", FrameRateTimebase, ENodeInherit::CheckInherit))
		{
			return false;
		}

		bool bFrameRateIsNTSC = false;
		if (!InClipItemNode->GetChildSubValue<bool>("rate", "ntsc", bFrameRateIsNTSC, ENodeInherit::CheckInherit))
		{
			bFrameRateIsNTSC = false;
		}

		// Shot name
		FString ShotName;
		if (!InClipItemNode->GetChildValue("name", ShotName, ENodeInherit::NoInherit) || ShotName.IsEmpty())
		{
			return false;
		}

		if (!bHasCinematicSectionName)
		{
			CinematicSectionName = ShotName;
		}

		// Start frame 
		int32 StartVal = 0;
		if (!InClipItemNode->GetChildValue("start", StartVal, ENodeInherit::NoInherit))
		{
			return false;
		}
		FFrameNumber StartFrame = StartVal;

		// End frame 
		int32 EndVal = 0;
		if (!InClipItemNode->GetChildValue("end", EndVal, ENodeInherit::NoInherit))
		{
			return false;
		}

		// In frame 
		int32 InVal = 0;
		bool bInDefault = false;
		if (!InClipItemNode->GetChildValue("in", InVal, ENodeInherit::NoInherit))
		{
			bInDefault = true;
		}

		// Out frame 
		int32 OutVal = 0;
		bool bOutDefault = false;
		if (!InClipItemNode->GetChildValue("out", OutVal, ENodeInherit::NoInherit))
		{
			bOutDefault = true;
		}

		uint32 Numerator = static_cast<uint32>(FrameRateTimebase);
		FFrameRate FrameRate(Numerator, 1);
		FFrameNumber StartOffset = bInDefault ? 0 : InVal;
		FFrameNumber Start = StartVal;
		FFrameNumber End = EndVal;

		// @todo handle xml subclips?

		TSharedPtr<FMovieSceneImportCinematicSectionData> SectionData = ImportData->FindCinematicSection(CinematicSectionName);
		if (!SectionData.IsValid())
		{
			SectionData = ImportData->CreateCinematicSection(CinematicSectionName, CurrVideoTrackRowIndex, FrameRate, Start, End, StartOffset);
			if (!SectionData.IsValid())
			{
				return false;
			}
		}
		else
		{
			if (!ImportData->SetCinematicSection(SectionData, CurrVideoTrackRowIndex, FrameRate, Start, End, StartOffset))
			{
				return false;
			}
		}

		ImportSectionMetaData(CinematicSectionName, SectionData->CinematicSection);
	}

	// Clip items can be referenced so flag to visit reference node children
	return InClipItemNode->VisitChildren(*this, true);
}

bool FFCPXMLImportVisitor::VisitNode(TSharedRef<FFCPXMLFileNode> InFileNode)
{
	// Files can be referenced so flag to visit reference node children
	return InFileNode->VisitChildren(*this, true);
}

bool FFCPXMLImportVisitor::AddMasterClipSectionName(const FString& InMasterClipIdName, const FString& InSequencerSectionName)
{
	if (MasterClipSectionNameMap.Contains(InMasterClipIdName))
	{
		return false;
	}

	MasterClipSectionNameMap.Add(InMasterClipIdName, InSequencerSectionName);
	return true;
}

bool FFCPXMLImportVisitor::GetMasterClipSectionName(const FString& InMasterClipIdName, FString& OutSequencerSectionName) const
{
	const FString* Name = MasterClipSectionNameMap.Find(InMasterClipIdName);
	if (Name == nullptr)
	{
		return false;
	}

	OutSequencerSectionName = *Name;
	return true;
}


/** Add metadata entry */
bool FFCPXMLImportVisitor::AddSectionMetaData(const FString& InSequencerSectionName, TSharedPtr<FFCPXMLNode> InLoggingInfoNode)
{
	if (SectionMetaDataMap.Contains(InSequencerSectionName))
	{
		return false;
	}

	SectionMetaDataMap.Add(InSequencerSectionName, InLoggingInfoNode);
	return true;
}

void SetMetaDataValue(TSharedPtr<FFCPXMLNode> InNode, const FString& InElement, UMetaData* InMetaData, const UMovieSceneCinematicShotSection* InSection)
{
	if (!InNode.IsValid() || InMetaData == nullptr || InSection == nullptr)
	{
		return;
	}

	FString Value;
	InNode->GetChildValue(InElement, Value, ENodeInherit::NoInherit, ENodeReference::NoReferences);
	if (!Value.IsEmpty())
	{
		InMetaData->SetValue(InSection, *InElement, *Value);
	}
}

/** Store clip metadata */
bool FFCPXMLImportVisitor::ImportSectionMetaData(const FString& InSequencerSectionName, const UMovieSceneCinematicShotSection* InSection)
{
	if (InSequencerSectionName.IsEmpty() || InSection == nullptr)
	{
		return false;
	}

	TSharedPtr<FFCPXMLNode>* LoggingInfoNode = SectionMetaDataMap.Find(InSequencerSectionName);
	if (LoggingInfoNode != nullptr && (*LoggingInfoNode).IsValid())
	{
		UPackage* Package = InSection->GetOutermost();
		check(Package);

		UMetaData* MetaData = Package->GetMetaData();
		check(MetaData);

		SetMetaDataValue(*LoggingInfoNode, TEXT("description"), MetaData, InSection);
		SetMetaDataValue(*LoggingInfoNode, TEXT("scene"), MetaData, InSection);
		SetMetaDataValue(*LoggingInfoNode, TEXT("shottake"), MetaData, InSection);
		SetMetaDataValue(*LoggingInfoNode, TEXT("good"), MetaData, InSection);
		SetMetaDataValue(*LoggingInfoNode, TEXT("originalvideofilename"), MetaData, InSection);
		SetMetaDataValue(*LoggingInfoNode, TEXT("originalaudiofilename"), MetaData, InSection);
/*
		FString Value;
		(*LoggingInfoNode)->GetChildValue(TEXT("description"), Value, ENodeInherit::NoInherit, ENodeReference::NoReferences);
		if (!Value.IsEmpty())
		{
			MetaData->SetValue(InSection, TEXT("description"), *Value);
		}
		*/
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
