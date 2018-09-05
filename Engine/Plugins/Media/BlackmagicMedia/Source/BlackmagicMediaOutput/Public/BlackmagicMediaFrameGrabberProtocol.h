// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlackmagicMediaOutput.h"
#include "Protocols/FrameGrabberProtocol.h"
#include "UObject/StrongObjectPtr.h"

#include "BlackmagicMediaFrameGrabberProtocol.generated.h"

class FBlackmagicMediaViewportOutputImpl;

UCLASS(meta=(DisplayName="Blackmagic Output", CommandLineID="BlackmagicOutput"))
class BLACKMAGICMEDIAOUTPUT_API UBlackmagicFrameGrabberProtocol : public UMovieSceneImageCaptureProtocolBase
{
public:
	GENERATED_BODY()

	UBlackmagicFrameGrabberProtocol(const FObjectInitializer& ObjInit);

	/** ~UMovieSceneCaptureProtocolBase implementation */
	virtual bool StartCaptureImpl() override;
	virtual void CaptureFrameImpl(const FFrameMetrics& FrameMetrics) override;
	virtual bool HasFinishedProcessingImpl() const override;
	virtual void FinalizeImpl() override;
	virtual bool CanWriteToFileImpl(const TCHAR* InFilename, bool bOverwriteExisting) const { return false; }
	/** ~End UMovieSceneCaptureProtocolBase implementation */

public:

	/** Setting to use for the FrameGrabberProtocol */
	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category=Blackmagic, meta=(AllowedClasses=BlackmagicMediaOutput))
	FSoftObjectPath MediaOutput;

	/** States unused options for BlackmagicFrameGrabberProtocolSettings */
	UPROPERTY(VisibleAnywhere, Transient, Category=Blackmagic)
	FString Information;

private:

	/** Transient media output pointer to keep the media output alive while this protocol is in use */
	UPROPERTY(transient)
	UBlackmagicMediaOutput* TransientMediaOutputPtr;

	TSharedPtr<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe> Implementation;
};
