// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"


class SEditableTextBox;
struct FSlateBrush;
struct FTimecodeSynchronizerActiveTimecodedInputSource;
class UMaterial;
class UTimecodeSynchronizer;
class UTexture;
class UMaterialExpressionTextureSample;

class STimecodeSynchronizerSourceViewport
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STimecodeSynchronizerSourceViewport) { }
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	STimecodeSynchronizerSourceViewport();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InTimecodeSynchronization The Asset currently active.
	 * @param InAttachedSource The Source to display.
	 * @param InTexture The source texture to render if any
	 */
	void Construct(const FArguments& InArgs, UTimecodeSynchronizer* InTimecodeSynchronization, int32 InAttachedSourceIndex, bool InTimecodedSource, UTexture* InTexture);

private:

	/** Callback for getting the overlay text of Source minimum Timecode buffered. */
	FText HandleIntervalMinTimecodeText() const;

	/** Callback for getting the overlay text of Source maximum Timecode buffered. */
	FText HandleIntervalMaxTimecodeText() const;
	
	/** Callback for getting the text of the Current Synchronized Timecode. */
	FText HandleCurrentTimecodeText() const;
	
	/** Callback setuping the text if the source is the master source. */
	FText HandleIsSourceMasterText() const;

	/** Get the attached Input source in TimecodeSynchronization.GetActiveSources() at the AttachedSourceIndex index  */
	const FTimecodeSynchronizerActiveTimecodedInputSource* GetAttachedSource() const;

private:
	class FInternalReferenceCollector : public FGCObject
	{
	public:
		FInternalReferenceCollector(STimecodeSynchronizerSourceViewport* InObject)
			: Object(InObject)
		{
		}

		//~ FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& InCollector) override
		{
			InCollector.AddReferencedObject(Object->TimecodeSynchronization);
			InCollector.AddReferencedObject(Object->Material);
			InCollector.AddReferencedObject(Object->TextureSampler);
		}

	private:
		STimecodeSynchronizerSourceViewport* Object;
	};
	friend FInternalReferenceCollector;

	/** Collector to keep the UObject alive. */
	FInternalReferenceCollector Collector;

	/** Media Source name text box. */
	TSharedPtr<SEditableTextBox> SourceTextBox;

	/** Current TimecodeSynchronization being used. */
	UTimecodeSynchronizer* TimecodeSynchronization;

	/** Attached Input source index in either TimecodeSynchronization.GetSynchronizedSources() or GetNonSynchronizedSources(). */
	int32 AttachedSourceIndex;

	/** Whether or not this source is used for synchronization. */
	bool bIsSynchronizedSource;
	
	/** The material that wraps the video texture for display in an SImage. */
	UMaterial* Material;

	/** The Slate brush that renders the material. */
	TSharedPtr<FSlateBrush> MaterialBrush;

	/** The video texture sampler in the wrapper material. */
	UMaterialExpressionTextureSample* TextureSampler;
};