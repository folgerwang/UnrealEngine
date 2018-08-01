// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
