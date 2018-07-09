// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AppleARKitFaceSupport.h"
#include "AppleARKitLiveLinkSourceFactory.h"
#include "AppleARKitConversion.h"


class APPLEARKITFACESUPPORT_API FAppleARKitFaceSupport :
	public IAppleARKitFaceSupport,
	public TSharedFromThis<FAppleARKitFaceSupport, ESPMode::ThreadSafe>
{
public:
	FAppleARKitFaceSupport();
	virtual ~FAppleARKitFaceSupport();

	void Init();
	void Shutdown();

private:
#if SUPPORTS_ARKIT_1_0
	// ~IAppleARKitFaceSupport
	virtual TArray<TSharedPtr<FAppleARKitAnchorData>> MakeAnchorData(NSArray<ARAnchor*>* NewAnchors, double Timestamp, uint32 FrameNumber, const FRotator& AdjustBy) override;
	virtual ARConfiguration* ToARConfiguration(UARSessionConfig* InSessionConfig) override;
	virtual void PublishLiveLinkData(TSharedPtr<FAppleARKitAnchorData> Anchor, double Timestamp, uint32 FrameNumber) override;
	virtual bool DoesSupportFaceAR() override;
	// ~IAppleARKitFaceSupport
#endif

	/** If requested, publishes face ar updates to LiveLink for the animation system to use */
	TSharedPtr<ILiveLinkSourceARKit> LiveLinkSource;
	/** Copied from the UARSessionConfig project settings object */
	FName FaceTrackingLiveLinkSubjectName;
};

