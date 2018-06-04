// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitFaceSupportModule.h"
#include "AppleARKitSystem.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "CoreGlobals.h"
#include "AppleARKitFaceSupportImpl.h"
#include "Misc/ConfigCacheIni.h"


IMPLEMENT_MODULE(FAppleARKitFaceSupportModule, AppleARKitFaceSupport);

DEFINE_LOG_CATEGORY(LogAppleARKitFace);

FAppleARKitFaceSupport* FaceSupportInstance = nullptr;

void FAppleARKitFaceSupportModule::StartupModule()
{
	ensureMsgf(FModuleManager::Get().LoadModule("AppleARKit"), TEXT("ARKitFaceSupport depends on the AppleARKit module."));

	FaceSupportInstance = new FAppleARKitFaceSupport();
	IModularFeatures::Get().RegisterModularFeature(FaceSupportInstance->GetModularFeatureName(), FaceSupportInstance);

	// LiveLink listener needs to be created here so that the editor can receive remote publishing events
#if PLATFORM_DESKTOP
	bool bEnableLiveLinkForFaceTracking = false;
	GConfig->GetBool(TEXT("/Script/AppleARKit.AppleARKitSettings"), TEXT("bEnableLiveLinkForFaceTracking"), bEnableLiveLinkForFaceTracking, GEngineIni);
	if (bEnableLiveLinkForFaceTracking)
	{
		FAppleARKitLiveLinkSourceFactory::CreateLiveLinkRemoteListener();
	}
#endif
}

void FAppleARKitFaceSupportModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(FaceSupportInstance->GetModularFeatureName(), FaceSupportInstance);
	delete FaceSupportInstance;
	FaceSupportInstance = nullptr;
}

