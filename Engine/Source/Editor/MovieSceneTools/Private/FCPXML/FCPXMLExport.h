// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FCPXML/FCPXMLNode.h"
#include "MovieScene.h"
#include "MovieSceneTranslator.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"

/** The FFCPXMLExportVisitor class exports from Sequencer data into the FCP 7 XML structure.

    This class will eventually be used to merge the data with an existing XML structure 
	representing previously imported material. This is intended to preserve metadata 
	roundtrip between Sequencer and the FCP XML format.
	
	Currently the VisitNode functions are mostly empty. This is where the
	merge data functionality will be implemented. */

class FFCPXMLExportVisitor : public FFCPXMLNodeVisitor
{
public:
	/** Constructor */
	FFCPXMLExportVisitor(FString InSaveFilename, TSharedRef<FMovieSceneExportData> InExportData, TSharedRef<FMovieSceneTranslatorContext> InExportContext);
	/** Destructor */
	virtual ~FFCPXMLExportVisitor();

public:

	/** Called when visiting a FFCPXMLBasicNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLBasicNode> InBasicNode) override final;
	/** Called when visiting a FFCPXMLXmemlNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLXmemlNode> InXmemlNode) override final;
	/** Called when visiting a FFCPXMLSequenceNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLSequenceNode> InSequenceNode) override final;
	/** Called when visiting a FFCPXMLVideoNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLVideoNode> InVideoNode) override final;
	/** Called when visiting a FFCPXMLAudioNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLAudioNode> InAudioNode) override final;
	/** Called when visiting a FFCPXMLTrackNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLTrackNode> InTrackNode) override final;
	/** Called when visiting a FFCPXMLClipNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLClipNode> InClipNode) override final;
	/** Called when visiting a FFCPXMLClipItemNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLClipItemNode> InClipItemNode) override final;
	/** Called when visiting a FFCPXMLFileNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLFileNode> InFileNode) override final;

public:
	/** Creates project node. */
	bool ConstructProjectNode(TSharedRef<FFCPXMLNode> InParentNode);
	/** Creates master clip nodes. */
	bool ConstructMasterClipNodes(TSharedRef<FFCPXMLNode> InParentNode);
	/** Creates master clip node. */
	bool ConstructMasterClipNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, int32 InMasterClipId);
	/** Creates colorinfo node. */
	bool ConstructColorInfoNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData);
	/** Creates logginginfo node. */
	bool ConstructSectionLoggingInfoNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, const FString& InSectionName);
	/** Creates sequence node. */
	bool ConstructSequenceNode(TSharedRef<FFCPXMLNode> InParentNode);
	/** Creates video node. */
	bool ConstructVideoNode(TSharedRef<FFCPXMLNode> InParentNode);
	/** Creates audio node. */
	bool ConstructAudioNode(TSharedRef<FFCPXMLNode> InParentNode);
	/** Creates cinematic track node. */
	bool ConstructVideoTrackNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicTrackData> InCinematicTrackData);
	/** Creates audio track node. */
	bool ConstructAudioTrackNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportAudioTrackData> InAudioTrackData);
	/** Creates video clip item node. */
	bool ConstructVideoClipItemNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, bool bInMasterClip);
	/** Creates audio clip item node. */
	bool ConstructAudioClipItemNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportAudioSectionData> InAudioSectionData);
	/** Creates video file node. */
	bool ConstructVideoFileNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, uint32 Duration, bool bInMasterClip);
	/** Creates audio file node. */
	bool ConstructAudioFileNode(TSharedRef<FFCPXMLNode> InParentNode, const TSharedPtr<FMovieSceneExportAudioSectionData> InAudioSectionData);
	/** Creates sample characteristics node */
	bool ConstructSampleCharacteristicsNode(TSharedRef<FFCPXMLNode> InParentNode, int InWidth, int InHeight);
	/** Creates rate node. */
	bool ConstructRateNode(TSharedRef<FFCPXMLNode> InParentNode);
	/** Creates timecode node. */
	bool ConstructTimecodeNode(TSharedRef<FFCPXMLNode> InParentNode);
	/** Creates logginginfo elements. */
	void ConstructLoggingInfoElements(TSharedRef<FFCPXMLNode> InLoggingInfoNode, const UObject* InObject) ;
	/** Set logginginfo element value. */
	void SetLoggingInfoElementValue(TSharedPtr<FFCPXMLNode> InNode, const UObject* InObject, const FString& InElement) ;
	/** Get duration, start, end, in, out frames for a given shot section */
	bool GetSectionFrames(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, int32& OutDuration, int32& OutStartFrame, int32& OutEndFrame, int32& OutInFrame, int32& OutOutFrame);
	/** Get start and end frames for a given shot section */
	//bool GetSectionStartEnd(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, int32& OutStartFrame, int32& OutEndFrame);
	/** Get name of a given shot section */
	bool GetSectionName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString& OutSectionName);

private:

	/** Construct a master clip id name based on input name and id */
	bool CreateMasterClipIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, uint32 InId, FString& OutName);
	/** Get id name for given section and prefix */
	bool GetIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString InPrefix, FString &OutName);
	/** Get master clip id name for given section */
	bool GetMasterClipIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString& OutName);
	/** Get master clip's clipitem id name for given section */
	bool GetMasterClipItemIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString& OutName);
	/** Get master clip's file id name based on input name and id */
	bool GetMasterClipFileIdName(const TSharedPtr<FMovieSceneExportCinematicSectionData> InCinematicSectionData, FString& OutName);
	/** Get the default image resolution */
	void GetDefaultImageResolution(int32& OutWidth, int32& OutHeight);
	/** Return filename with path */
	FString GetFilePathName(const FString& InSectionName) const;

private:

	TSharedRef<FMovieSceneExportData> ExportData;
	TSharedRef<FMovieSceneTranslatorContext> ExportContext;
	FString SaveFilePath;
	TMap<FString, uint32> MasterClipIdMap;
	uint32 SequenceId;
};
