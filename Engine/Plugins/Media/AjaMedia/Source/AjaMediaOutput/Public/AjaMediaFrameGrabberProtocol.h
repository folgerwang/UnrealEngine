// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AjaMediaOutput.h"
#include "Protocols/FrameGrabberProtocol.h"
#include "UObject/StrongObjectPtr.h"

#include "AjaMediaFrameGrabberProtocol.generated.h"

class FAjaMediaViewportOutputImpl;

struct AJAMEDIAOUTPUT_API FAjaFrameGrabberProtocol : public IMovieSceneCaptureProtocol
{
public:
	FAjaFrameGrabberProtocol();

	/** ~FFrameGrabberProtocol implementation */
	virtual bool Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host) override;
	virtual void CaptureFrame(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host) override;
	virtual bool HasFinishedProcessing() const override;
	virtual void Finalize() override;
	virtual bool CanWriteToFile(const TCHAR* InFilename, bool bOverwriteExisting) const { return false; }
	/** ~End FFrameGrabberProtocol implementation */

private:
	TSharedPtr<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe> Implementation;
	TStrongObjectPtr<UAjaMediaOutput> MediaOutput;
	bool bInitialized;
};

/**
 * Protocol to use with the FrameGrabber 
 */
UCLASS()
class AJAMEDIAOUTPUT_API UAjaFrameGrabberProtocolSettings : public UFrameGrabberProtocolSettings
{
public:
	GENERATED_UCLASS_BODY()

	/** Aja Setting to use for the FrameGrabberProtocol */
	UPROPERTY(EditAnywhere, Category=AJA)
	UAjaMediaOutput* MediaOutput;

	/** States unused options for AJAFrameGrabberProtocolSettings */
	UPROPERTY(VisibleAnywhere, Transient, Category=AJA)
	FString Information;
};
