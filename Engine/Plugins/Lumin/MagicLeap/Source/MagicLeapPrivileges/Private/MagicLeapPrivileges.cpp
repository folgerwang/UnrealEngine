// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#include "MagicLeapPrivileges.h"
#include "IMagicLeapPrivilegesPlugin.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "MagicLeapPluginUtil.h"
#include "MagicLeapSDKDetection.h"

#if WITH_MLSDK
#include "ml_privileges.h"
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapPrivileges, Display, All);

class FMagicLeapPrivilegesPlugin : public IMagicLeapPrivilegesPlugin
{
public:
	void StartupModule() override
	{
		IModuleInterface::StartupModule();
		APISetup.Startup();
#if WITH_MLSDK
		APISetup.LoadDLL(TEXT("ml_privileges"));
#endif
	}

	void ShutdownModule() override
	{
		APISetup.Shutdown();
		IModuleInterface::ShutdownModule();
	}

private:
	FMagicLeapAPISetup APISetup;
};

IMPLEMENT_MODULE(FMagicLeapPrivilegesPlugin, MagicLeapPrivileges);

//////////////////////////////////////////////////////////////////////////

#if WITH_MLSDK

MLPrivilegeID UnrealToMLPrivilege(EMagicLeapPrivilege Privilege)
{
// TODO: We need to get rid of any hand-mapping of these enums. In the meantime,
// the macro is to make it easier to keep it in step with the header - rmobbs
#define PRIVCASE(x) case EMagicLeapPrivilege::x: { return MLPrivilegeID_##x; }
	switch(Privilege)
	{
		PRIVCASE(AddressBookRead)
		PRIVCASE(AddressBookWrite)
		PRIVCASE(AudioRecognizer)
		PRIVCASE(AudioSettings)
		PRIVCASE(BatteryInfo)
		PRIVCASE(CalendarRead)
		PRIVCASE(CalendarWrite)
		PRIVCASE(CameraCapture)
		PRIVCASE(DenseMap)
		PRIVCASE(EmailSend)
		PRIVCASE(Eyetrack)
		PRIVCASE(Headpose)
		PRIVCASE(InAppPurchase)
		PRIVCASE(Location)
		PRIVCASE(AudioCaptureMic)
		PRIVCASE(DrmCertificates)
		PRIVCASE(Occlusion)
		PRIVCASE(ScreenCapture)
		PRIVCASE(Internet)
		PRIVCASE(AudioCaptureMixed)
		PRIVCASE(IdentityRead)
		PRIVCASE(IdentityModify)
		PRIVCASE(BackgroundDownload)
		PRIVCASE(BackgroundUpload)
		PRIVCASE(MediaDrm)
		PRIVCASE(Media)
		PRIVCASE(MediaMetadata)
		PRIVCASE(PowerInfo)
		PRIVCASE(AudioCaptureVirtual)
		PRIVCASE(CalibrationRigModelRead)
		PRIVCASE(NetworkServer)
		PRIVCASE(LocalAreaNetwork)
		PRIVCASE(VoiceInput)
		PRIVCASE(ConnectBackgroundMusicService)
		PRIVCASE(RegisterBackgroundMusicService)
		PRIVCASE(NormalNotificationsUsage)
		PRIVCASE(MusicService)
		PRIVCASE(BackgroundLowLatencyLightwear)
		default:
			UE_LOG(LogMagicLeapPrivileges, Error, TEXT("Unmapped privilege %d"), static_cast<int32>(Privilege));
			return MLPrivilegeID_Invalid;
	}
}
#endif //WITH_MLSDK

UMagicLeapPrivileges::UMagicLeapPrivileges()
: bPrivilegeServiceStarted(false)
{
#if !PLATFORM_MAC
	ML_FUNCTION_WRAPPER(bPrivilegeServiceStarted = MLPrivilegesInit());
	if (!bPrivilegeServiceStarted)
	{
#if PLATFORM_LUMIN
		// Setting log level to Error caused cook error.
		UE_LOG(LogMagicLeapPrivileges, Warning, TEXT("Error initializing privilege service."));
#endif
	}
#endif
}

UMagicLeapPrivileges::~UMagicLeapPrivileges()
{
}

void UMagicLeapPrivileges::FinishDestroy()
{
	if (bPrivilegeServiceStarted)
	{
#if WITH_MLSDK
		MLPrivilegesDestroy();
#endif //WITH_MLSDK
		bPrivilegeServiceStarted = false;
	}

	Super::FinishDestroy();
}

bool UMagicLeapPrivileges::CheckPrivilege(EMagicLeapPrivilege Privilege)
{
#if WITH_MLSDK
	return InitializePrivileges() ? MLPrivilegesCheckPrivilege(UnrealToMLPrivilege(Privilege)) : false;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapPrivileges::RequestPrivilege(EMagicLeapPrivilege Privilege)
{
#if WITH_MLSDK
	return InitializePrivileges() ? MLPrivilegesRequestPrivilege(UnrealToMLPrivilege(Privilege)) : false;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapPrivileges::InitializePrivileges()
{
#if WITH_MLSDK
	if (!bPrivilegeServiceStarted)
	{
		bPrivilegeServiceStarted = MLPrivilegesInit();
		if (!bPrivilegeServiceStarted)
		{
			UE_LOG(LogMagicLeapPrivileges, Warning, TEXT("Error initializing privilege service."));
		}
	}
#endif //WITH_MLSDK
	return bPrivilegeServiceStarted;
}
