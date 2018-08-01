// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Android/AndroidProperties.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformBase.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "AndroidTargetDevice.h"
#include "AndroidTargetPlatform.h"
#include "IAndroidTargetPlatformModule.h"

#define LOCTEXT_NAMESPACE "FAndroidTargetPlatformModule"

/**
 * Module for the Android target platform.
 */
class FAndroidTargetPlatformModule : public IAndroidTargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroidTargetPlatformModule( )
	{
		for (ITargetPlatform* TP : TargetPlatforms)
		{
			delete TP;
		}
		TargetPlatforms.Empty();
		MultiPlatforms.Empty();
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual TArray<ITargetPlatform*> GetTargetPlatforms() override
	{
		if (TargetPlatforms.Num() == 0)
		{
			for (int32 Type = 0; Type < 2; Type++)
			{
				bool bIsClient = Type == 1;
				SinglePlatforms.Add(new FAndroidTargetPlatform(bIsClient));
				SinglePlatforms.Add(new FAndroid_ASTCTargetPlatform(bIsClient));
				SinglePlatforms.Add(new FAndroid_ATCTargetPlatform(bIsClient));
				SinglePlatforms.Add(new FAndroid_DXTTargetPlatform(bIsClient));
				SinglePlatforms.Add(new FAndroid_ETC1TargetPlatform(bIsClient));
				SinglePlatforms.Add(new FAndroid_ETC1aTargetPlatform(bIsClient));
				SinglePlatforms.Add(new FAndroid_ETC2TargetPlatform(bIsClient));
				SinglePlatforms.Add(new FAndroid_PVRTCTargetPlatform(bIsClient));

				// thse are used in NotifyMultiSelectedFormatsChanged, so track in another array
				MultiPlatforms.Add(new FAndroid_MultiTargetPlatform(bIsClient));
			}

			// join the single and the multi into one
			TargetPlatforms.Append(SinglePlatforms);
			TargetPlatforms.Append(MultiPlatforms);

			// set up the multi platforms now that we have all the other platforms ready to go
			NotifyMultiSelectedFormatsChanged();
		}

		return TargetPlatforms;
	}


	virtual void NotifyMultiSelectedFormatsChanged() override
	{
		for (FAndroid_MultiTargetPlatform* TP : MultiPlatforms)
		{
			TP->LoadFormats(SinglePlatforms);
		}
		// @todo multi needs to be passed this event!
	}

	// End ITargetPlatformModule interface

public:
	// Begin IModuleInterface interface
	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
	// End IModuleInterface interface

private:

	/** Holds the target platforms. */
	TArray<ITargetPlatform*> TargetPlatforms;
	TArray<FAndroidTargetPlatform*> SinglePlatforms;
	TArray<FAndroid_MultiTargetPlatform*> MultiPlatforms;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroidTargetPlatformModule, AndroidTargetPlatform);
