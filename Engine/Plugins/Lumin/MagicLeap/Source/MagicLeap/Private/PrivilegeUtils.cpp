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

#include "PrivilegeUtils.h"

//FString PrivilegeToString(EMagicLeapPrivilege PrivilegeID)
//{
//#define PRIV_TO_STR_CASE(x) case EMagicLeapPrivilege::x: { return UTF8_TO_TCHAR((#x)); }
//	switch (PrivilegeID)
//	{
//	PRIV_TO_STR_CASE(Invalid)
//	PRIV_TO_STR_CASE(AudioRecognizer)
//	PRIV_TO_STR_CASE(BatteryInfo)
//	PRIV_TO_STR_CASE(CameraCapture)
//	PRIV_TO_STR_CASE(WorldReconstruction)
//	PRIV_TO_STR_CASE(InAppPurchase)
//	PRIV_TO_STR_CASE(AudioCaptureMic)
//	PRIV_TO_STR_CASE(DrmCertificates)
//	PRIV_TO_STR_CASE(Occlusion)
//	PRIV_TO_STR_CASE(LowLatencyLightwear)
//	PRIV_TO_STR_CASE(Internet)
//	PRIV_TO_STR_CASE(IdentityRead)
//	PRIV_TO_STR_CASE(BackgroundDownload)
//	PRIV_TO_STR_CASE(BackgroundUpload)
//	PRIV_TO_STR_CASE(MediaDrm)
//	PRIV_TO_STR_CASE(Media)
//	PRIV_TO_STR_CASE(MediaMetadata)
//	PRIV_TO_STR_CASE(PowerInfo)
//	PRIV_TO_STR_CASE(LocalAreaNetwork)
//	PRIV_TO_STR_CASE(VoiceInput)
//	PRIV_TO_STR_CASE(Documents)
//	PRIV_TO_STR_CASE(ConnectBackgroundMusicService)
//	PRIV_TO_STR_CASE(RegisterBackgroundMusicService)
//	PRIV_TO_STR_CASE(PwFoundObjRead)
//	PRIV_TO_STR_CASE(NormalNotificationsUsage)
//	PRIV_TO_STR_CASE(MusicService)
//	PRIV_TO_STR_CASE(ControllerPose)
//	PRIV_TO_STR_CASE(ScreensProvider)
//	PRIV_TO_STR_CASE(GesturesSubscribe)
//	PRIV_TO_STR_CASE(GesturesConfig)
//	}
//
//	return UTF8_TO_TCHAR("Invalid");
//}