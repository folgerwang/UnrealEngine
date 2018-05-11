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

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "MagicLeapPrivileges.generated.h"

// TODO: We need to get rid of any hand-mapping of these enums - rmobbs

/** Priviliges an app can request for from the system. */
UENUM(BlueprintType)
enum class EMagicLeapPrivilege : uint8
{
	Invalid,
	AddressBookRead,
	AddressBookWrite,
	AudioRecognizer,
	AudioSettings,
	BatteryInfo,
	CalendarRead,
	CalendarWrite,
	CameraCapture,
	DenseMap,
	EmailSend,
	Eyetrack,
	Headpose,
	InAppPurchase,
	Location,
	AudioCaptureMic,
	DrmCertificates,
	Occlusion,
	ScreenCapture,
	Internet,
	AudioCaptureMixed,
	IdentityRead,
	IdentityModify,
	BackgroundDownload,
	BackgroundUpload,
	MediaDrm,
	Media,
	MediaMetadata,
	PowerInfo,
	AudioCaptureVirtual,
	CalibrationRigModelRead,
	NetworkServer,
	LocalAreaNetwork,
	VoiceInput,
	ConnectBackgroundMusicService,
	RegisterBackgroundMusicService,
	NormalNotificationsUsage,
	MusicService,
	BackgroundLowLatencyLightwear
};

/**
 *  Class which provides functions to check and request the priviliges the app has at runtime.
 */
UCLASS(ClassGroup = MagicLeap, BlueprintType)
class MAGICLEAPPRIVILEGES_API UMagicLeapPrivileges : public UObject
{
	GENERATED_BODY()

public:
	UMagicLeapPrivileges();
	virtual ~UMagicLeapPrivileges();
	virtual void FinishDestroy() override;

	/**
	  Check whether the application has the specified privilege.
	  This does not solicit consent from the end-user.
	  @param Privilege The privilege to check.
	  @return True if the privilege is granted, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Privileges|MagicLeap")
	bool CheckPrivilege(EMagicLeapPrivilege Privilege);

	/**
	  Request the specified privilege.
	  This may possibly solicit consent from the end-user.
	  @param Privilege The privilege to request.
	  @return True if the privilege is granted, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Privileges|MagicLeap")
	bool RequestPrivilege(EMagicLeapPrivilege Privilege);

private:
	bool InitializePrivileges();

	bool bPrivilegeServiceStarted;
};
