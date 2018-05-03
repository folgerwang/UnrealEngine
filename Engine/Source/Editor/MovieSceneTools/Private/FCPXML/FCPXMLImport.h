// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FCPXML/FCPXMLNode.h"
#include "MovieSceneTranslator.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"

/** FCP XML node visitor classes. */

/** The FFCPXMLImportVisitor class imports from the FCP 7 XML structure into Sequencer classes. */

class FFCPXMLImportVisitor : public FFCPXMLNodeVisitor
{
public:
	/** Constructor */
	FFCPXMLImportVisitor(TSharedRef<FMovieSceneImportData> InImportData, TSharedRef<FMovieSceneTranslatorContext> InImportContext);
	/** Destructor */
	~FFCPXMLImportVisitor();

public:
	/** Called when visiting a FFCPXMLBasicNode during visitor traversal. */
	virtual bool VisitNode(TSharedRef<FFCPXMLBasicNode> InBasicNode) override final;
	/** Called when visiting a FFCPXMLMemlNode during visitor traversal. */
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

private:
	/** Add entry to master clip section name map */
	bool AddMasterClipSectionName(const FString& InMasterClipIdName, const FString& InSequencerSectionName);
	/** Query master clip section name map */
	bool GetMasterClipSectionName(const FString& InMasterClipIdName, FString& OutSectionName) const;
	/** Add entry to metadata map */
	bool AddSectionMetaData(const FString& InSequencerSectionName, TSharedPtr<FFCPXMLNode> InLoggingInfoNode);
	/** Import clip metadata to sequencer section */
	bool ImportSectionMetaData(const FString& InSequencerSectionName, const UMovieSceneCinematicShotSection* InSection);

private:
	TSharedRef<FMovieSceneImportData> ImportData;
	TSharedRef<FMovieSceneTranslatorContext> ImportContext;

	/** Map of masterclip names to sequencer section names. */
	TMap<FString, FString> MasterClipSectionNameMap;

	/** Map of sequencer section name to logging info */
	TMap<FString, TSharedPtr<FFCPXMLNode>> SectionMetaDataMap;

	bool bInSequenceNode;
	bool bInVideoNode;
	bool bInAudioNode;
	bool bInVideoTrackNode;
	bool bInAudioTrackNode;
	int32 MaxVideoTrackRowIndex;
	int32 CurrVideoTrackRowIndex;
	int32 MaxAudioTrackIndex;
	int32 CurrAudioTrackIndex;
};
