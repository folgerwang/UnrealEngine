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
#include "UObject/ObjectMacros.h"
#include "MagicLeapMusicServiceTypes.generated.h"

/** List of possible errors when calling the music service functions. */
UENUM(BlueprintType)
enum class EMagicLeapMusicServiceError : uint8
{
	None			UMETA(DisplayName = "None"),
	Connectivity	UMETA(DisplayName = "Connectivity"),
	Timeout			UMETA(DisplayName = "Timeout"),
	GeneralPlayback	UMETA(DisplayName = "General Playback"),
	Privilege		UMETA(DisplayName = "Privilege"),
	ServiceSpecific	UMETA(DisplayName = "Service Specific"),
	NoMemory		UMETA(DisplayName = "No Memory"),
	Unspecified		UMETA(DisplayName = "Unspecified"),
};

/** List of possible status of the music service. */
UENUM(BlueprintType)
enum class EMagicLeapMusicServiceStatus : uint8
{
	ContextChanged	UMETA(DisplayName = "Context Changed"),
	Created			UMETA(DisplayName = "Created"),
	LoggedIn		UMETA(DisplayName = "Logged In"),
	LoggedOut		UMETA(DisplayName = "Logged Out"),
	NextTrack		UMETA(DisplayName = "Next Track"),
	PrevTrack		UMETA(DisplayName = "Prev Track"),
	TrackChanged	UMETA(DisplayName = "Track Changed"),
	Unknown			UMETA(DisplayName = "Unknown"),
};

/** List of MusicService playback options. */
UENUM(BlueprintType)
enum class EMagicLeapMusicServicePlaybackOption : uint8
{
	Playback		UMETA(DisplayName = "Playback"),
	Repeat			UMETA(DisplayName = "Repeat"),
	Shuffle			UMETA(DisplayName = "Shuffle"),
	Unknown			UMETA(DisplayName = "Unknown"),
};

/** List of MusicService playback states. */
UENUM(BlueprintType)
enum class EMagicLeapMusicServicePlaybackState : uint8
{
	Playing 		UMETA(DisplayName = "Playing"),
	Paused			UMETA(DisplayName = "Paused"),
	Stopped			UMETA(DisplayName = "Stopped"),
	Error			UMETA(DisplayName = "Error"),
	Unknown			UMETA(DisplayName = "Unknown"),
};

/** List of MusicService repeat settings. */
UENUM(BlueprintType)
enum class EMagicLeapMusicServiceRepeatState : uint8
{
	Off				UMETA(DisplayName = "Off"),
	Song			UMETA(DisplayName = "Song"),
	Album			UMETA(DisplayName = "Album"),
	Unknown			UMETA(DisplayName = "Unknown"),
};

/** List of MusicService shuffle settings. */
UENUM(BlueprintType)
enum class EMagicLeapMusicServiceShuffleState : uint8
{
	On				UMETA(DisplayName = "On"),
	Off				UMETA(DisplayName = "Off"),
	Unknown			UMETA(DisplayName = "Unknown"),
};

/** Track metadata. */
USTRUCT(BlueprintType)
struct FMagicLeapMusicServiceMetadata
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	FString AlbumTitle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	FString AlbumInfoName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	FString AlbumInfoUrl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	FString AlbumInfoCoverUrl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	FString ArtistInfoName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	FString ArtistInfoUrl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	int32 Length;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	int32 Position;

	FMagicLeapMusicServiceMetadata() :
	Length(0),
	Position(0)
	{
	}
};

