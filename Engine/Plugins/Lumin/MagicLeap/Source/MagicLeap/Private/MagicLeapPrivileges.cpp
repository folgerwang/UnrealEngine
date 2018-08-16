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
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "MagicLeapPluginUtil.h"
#include "MagicLeapSDKDetection.h"
#include "AppFramework.h"

#if WITH_MLSDK
#include "ml_privileges.h"
#endif //WITH_MLSDK

//////////////////////////////////////////////////////////////////////////

#if WITH_MLSDK

MLPrivilegeID UnrealToMLPrivilege(EMagicLeapPrivilege Privilege)
{
// TODO: We need to get rid of any hand-mapping of these enums. In the meantime,
// the macro is to make it easier to keep it in step with the header - rmobbs
#define PRIVCASE(x) case EMagicLeapPrivilege::x: { return MLPrivilegeID_##x; }
	switch(Privilege)
	{
		PRIVCASE(AudioRecognizer)
		PRIVCASE(BatteryInfo)
		PRIVCASE(CameraCapture)
		PRIVCASE(WorldReconstruction)
		PRIVCASE(InAppPurchase)
		PRIVCASE(AudioCaptureMic)
		PRIVCASE(DrmCertificates)
		PRIVCASE(Occlusion)
		PRIVCASE(LowLatencyLightwear)
		PRIVCASE(Internet)
		PRIVCASE(IdentityRead)
		PRIVCASE(BackgroundDownload)
		PRIVCASE(BackgroundUpload)
		PRIVCASE(MediaDrm)
		PRIVCASE(Media)
		PRIVCASE(MediaMetadata)
		PRIVCASE(PowerInfo)
		PRIVCASE(LocalAreaNetwork)
		PRIVCASE(VoiceInput)
		PRIVCASE(Documents)
		PRIVCASE(ConnectBackgroundMusicService)
		PRIVCASE(RegisterBackgroundMusicService)
		PRIVCASE(PwFoundObjRead)
		PRIVCASE(NormalNotificationsUsage)
		PRIVCASE(MusicService)
		PRIVCASE(ControllerPose)
		PRIVCASE(ScreensProvider)
		PRIVCASE(GesturesSubscribe)
		PRIVCASE(GesturesConfig)
	default:
		UE_LOG(LogMagicLeap, Error, TEXT("Unmapped privilege %d"), static_cast<int32>(Privilege));
		return MLPrivilegeID_Invalid;
	}
}
#endif //WITH_MLSDK

UMagicLeapPrivileges::UMagicLeapPrivileges()
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;
}

UMagicLeapPrivileges::~UMagicLeapPrivileges()
{
}

bool UMagicLeapPrivileges::CheckPrivilege(EMagicLeapPrivilege Privilege)
{
#if WITH_MLSDK
	MLResult Result = MLPrivilegesCheckPrivilege(UnrealToMLPrivilege(Privilege));

	UE_LOG(LogMagicLeap, Warning, TEXT("UMagicLeapPrivileges::CheckPrivilege got result "
		"%u %s for privilege #%d"), static_cast<uint32>(Result), 
		UTF8_TO_TCHAR(MLPrivilegesGetResultString(Result)), 
		static_cast<int32>(UnrealToMLPrivilege(Privilege)));

	if (MLPrivilegesResult_Granted == Result)
	{
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapPrivileges::RequestPrivilege(EMagicLeapPrivilege Privilege)
{
#if WITH_MLSDK
	MLResult Result = MLPrivilegesRequestPrivilege(UnrealToMLPrivilege(Privilege));

	UE_LOG(LogMagicLeap, Warning, TEXT("UMagicLeapPrivileges::RequestPrivilege got result "
		"%u %s for privilege #%d"), static_cast<uint32>(Result), 
		UTF8_TO_TCHAR(MLPrivilegesGetResultString(Result)), 
		static_cast<int32>(UnrealToMLPrivilege(Privilege)));

	if (MLPrivilegesResult_Granted == Result)
	{
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapPrivileges::RequestPrivilegeAsync(EMagicLeapPrivilege Privilege, const FPrivilegeRequestDelegate& ResultDelegate)
{
#if WITH_MLSDK
	MLPrivilegesAsyncRequest* AsyncPrivilegeRequest;

	auto Result = MLPrivilegesRequestPrivilegeAsync(UnrealToMLPrivilege(Privilege), &AsyncPrivilegeRequest);

	UE_LOG(LogMagicLeap, Warning, TEXT("UMagicLeapPrivileges::RequestPrivilegeAsync got result "
		"%u %s for privilege #%d"), static_cast<uint32>(Result), 
		UTF8_TO_TCHAR(MLPrivilegesGetResultString(Result)), 
		static_cast<int32>(UnrealToMLPrivilege(Privilege)));

	if (MLResult_Ok != Result)
	{
		UE_LOG(LogMagicLeap, Error, TEXT("UMagicLeapPrivileges::RequestPrivilegeAsync "
			"failure %u %s for privilege #%d"), static_cast<uint32>(Result), 
			UTF8_TO_TCHAR(MLPrivilegesGetResultString(Result)),
			static_cast<int32>(UnrealToMLPrivilege(Privilege)));
		return false;
	}

	// Store the request
	PendingAsyncRequests.Add({ Privilege, AsyncPrivilegeRequest, ResultDelegate });
	return true;
#else
	return false;
#endif //WITH_MLSDK
}

void UMagicLeapPrivileges::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if WITH_MLSDK
	auto CopyPendingAsyncRequests(PendingAsyncRequests);
	
	PendingAsyncRequests.Empty();

	for (const auto& PendingAsyncRequest : CopyPendingAsyncRequests)
	{
		// Re-add pending ones
		MLResult Result = MLPrivilegesRequestPrivilegeTryGet(PendingAsyncRequest.Request);
		if (MLResult_Pending == Result)
		{
			PendingAsyncRequests.Add(PendingAsyncRequest);
			continue;
		}

		UE_LOG(LogMagicLeap, Warning, TEXT("UMagicLeapPrivileges::"
			"TickComponent has result %u %s for privilege request %d"), 
			static_cast<uint32>(Result), UTF8_TO_TCHAR(MLPrivilegesGetResultString(Result)), 
			static_cast<int32>(PendingAsyncRequest.Privilege));

		// Dispatch. Granted gets true, everything else gets false.
		if (MLPrivilegesResult_Granted == Result)
		{
			PendingAsyncRequest.Delegate.ExecuteIfBound(PendingAsyncRequest.Privilege, true);
		}
		else
		{
			PendingAsyncRequest.Delegate.ExecuteIfBound(PendingAsyncRequest.Privilege, false);
		}
	}
#endif
}
