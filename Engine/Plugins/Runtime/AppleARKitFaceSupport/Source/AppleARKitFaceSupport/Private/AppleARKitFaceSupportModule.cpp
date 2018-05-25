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

class APPLEARKITFACESUPPORT_API FAppleARKitFaceSupportFactory :
	public IAppleARKitFaceSupportFactory
{
public:
	static void CreateInstance()
	{
		Factory = new FAppleARKitFaceSupportFactory();
		IModularFeatures::Get().RegisterModularFeature(Factory->GetModularFeatureName(), Factory);
	}

	static void DestroyInstance()
	{
		IModularFeatures::Get().UnregisterModularFeature(Factory->GetModularFeatureName(), Factory);
		delete Factory;
		Factory = nullptr;
	}

private:
	FAppleARKitFaceSupportFactory() :
		FaceARSupport(nullptr)
	{
	}

	virtual ~FAppleARKitFaceSupportFactory()
	{
		delete FaceARSupport;
	}

	virtual IAppleARKitFaceSupport* CreateFaceSupport()
	{
		if (FaceARSupport == nullptr)
		{
			FaceARSupport = new FAppleARKitFaceSupport();
		}
		return FaceARSupport;
	};

	static FAppleARKitFaceSupportFactory* Factory;
	IAppleARKitFaceSupport* FaceARSupport;
};

FAppleARKitFaceSupportFactory* FAppleARKitFaceSupportFactory::Factory = nullptr;

void FAppleARKitFaceSupportModule::StartupModule()
{
	ensureMsgf(FModuleManager::Get().LoadModule("AppleARKit"), TEXT("ARKitFaceSupport depends on the AppleARKit module."));

	FAppleARKitFaceSupportFactory::CreateInstance();

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
	FAppleARKitFaceSupportFactory::DestroyInstance();
}

