// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Widgets/STimecodeSynchronizerSourceViewer.h"



#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MediaPlayer.h"
#include "MediaPlayerTimeSynchronizationSource.h"
#include "MediaTexture.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/STimecodeSynchronizerSourceViewport.h"


/* STimecodeSynchronizerSourceViewer structors
 *****************************************************************************/

STimecodeSynchronizerSourceViewer::STimecodeSynchronizerSourceViewer()
	: TimecodeSynchronizer(nullptr)
	, ViewportVerticalBox(nullptr)
{

}

STimecodeSynchronizerSourceViewer::~STimecodeSynchronizerSourceViewer()
{
	if (TimecodeSynchronizer.IsValid())
	{
		TimecodeSynchronizer->OnSynchronizationEvent().RemoveAll(this);
	}
}

/* STimecodeSynchronizerSourceViewer implementation
 *****************************************************************************/

void STimecodeSynchronizerSourceViewer::Construct(const FArguments& InArgs, UTimecodeSynchronizer& InTimecodeSynchronization)
{
	TimecodeSynchronizer.Reset(&InTimecodeSynchronization);
	TimecodeSynchronizer->OnSynchronizationEvent().AddSP(this, &STimecodeSynchronizerSourceViewer::HandleSynchronizationEvent);

	//Create the Box that will hold a Widget for each source.
	ViewportVerticalBox = SNew(SVerticalBox);

	// The viewer will be populated later on event.
	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			ViewportVerticalBox.ToSharedRef()
		]
	];

	PopulateActiveSources();
}

void STimecodeSynchronizerSourceViewer::PopulateActiveSources()
{
	ViewportVerticalBox->ClearChildren();
	
	if (TimecodeSynchronizer.IsValid())
	{
		const TArray<FTimecodeSynchronizerActiveTimecodedInputSource>& TimecodedSources = TimecodeSynchronizer->GetTimecodedSources();
		for (int32 Index = 0; Index < TimecodedSources.Num(); ++Index)
		{
			const FTimecodeSynchronizerActiveTimecodedInputSource& Source = TimecodedSources[Index];
			if (Source.InputSource)
			{
				UMediaPlayerTimeSynchronizationSource* MediaPlayerSource = Cast<UMediaPlayerTimeSynchronizationSource>(Source.InputSource);
				UMediaTexture* TextureArg = MediaPlayerSource ? MediaPlayerSource->MediaTexture : nullptr;
							 
				//Add a Viewport Widget for each active Source
				ViewportVerticalBox->AddSlot()
				.Padding(1.0f, 1.0f, 1.0f, 1.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GreenBrush"))
					.Padding(0.0f)
					[
						//Source area
						SNew(STimecodeSynchronizerSourceViewport, TimecodeSynchronizer.Get(), Index, true, TextureArg)
					]
				];
			}
		}

		const TArray<FTimecodeSynchronizerActiveTimecodedInputSource>& SynchronizationSources = TimecodeSynchronizer->GetSynchronizationSources();
		for (int32 Index = 0; Index < SynchronizationSources.Num(); ++Index)
		{
			const FTimecodeSynchronizerActiveTimecodedInputSource& Source = SynchronizationSources[Index];
			if (Source.InputSource)
			{
				UMediaPlayerTimeSynchronizationSource* MediaPlayerSource = Cast<UMediaPlayerTimeSynchronizationSource>(Source.InputSource);
				UMediaTexture* TextureArg = MediaPlayerSource ? MediaPlayerSource->MediaTexture : nullptr;
							 
				//Add a Viewport Widget for each active Source
				ViewportVerticalBox->AddSlot()
				.Padding(1.0f, 1.0f, 1.0f, 1.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GreenBrush"))
					.Padding(0.0f)
					[
						//Source area
						SNew(STimecodeSynchronizerSourceViewport, TimecodeSynchronizer.Get(), Index, false, TextureArg)
					]
				];
			}
		}
	}
}

void STimecodeSynchronizerSourceViewer::HandleSynchronizationEvent(ETimecodeSynchronizationEvent Event)
{
	if (Event == ETimecodeSynchronizationEvent::SynchronizationStarted)
	{
		PopulateActiveSources();
	}
}


