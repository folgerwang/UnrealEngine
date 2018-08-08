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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapMusicServiceTypes.h"
#include "MagicLeapMusicService.generated.h"

/**
Function library for the Magic Leap Music Service API.
*/
UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPMUSICSERVICE_API UMagicLeapMusicService : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	Connects to the specified music service.
	@param   ProviderName String containing name of provider.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Connect(const FString& ProviderName);

	/**
	Disconnects from the current music service.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Disconnect();

	/**
	Sets the authorization string for the current music service.
	@param   AuthorizationString Authorization data.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool SetAuthorizationString(const FString& AuthorizationString);

	/**
	Plays a specified URL on the currently connected music service.
	@param   RequestedUrl URL specifying music resource.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool SetUrl(const FString& RequestedUrl);

	/**
	Plays the specified URL list on the currently connected music service.
	@param   Playlist Array of URLs specifying music resources.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool SetUrlList(const TArray<FString>& Playlist);

	/**
	Starts the service with the current data set.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Start();

	/**
	Stops the service.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Stop();

	/**
	Pauses the service.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Pause();

	/**
	Resumes the service.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Resume();

	/**
	Seeks to the specified (absolute/relative?) position of the current track.
	@param   SeekMs The position (in milliseconds) to seek.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Seek(int32 SeekMs);

	/**
	Advances to the next track in the active playlist.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Next();

	/**
	Rewinds to the previous track in the active playlist.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool Previous();

	/**
	Sets the shuffle state.
	@param   ShuffleState The desired shuffle state.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool SetShuffle(EMagicLeapMusicServiceShuffleState ShuffleState);

	/**
	Sets the repeat state.
	@param   RepeatState The desired repeat state.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool SetRepeat(EMagicLeapMusicServiceRepeatState RepeatState);

	/**
	Sets the volume.
	@param   Volume The desired volume.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool SetVolume(float Volume);

	/**
	Gets the length of the current track.
	@param   TrackLength Length of the current track.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool GetTrackLength(int32& TrackLength);

	/**
	Gets the current playback position of the current track.
	@param   Position Current position of the current track.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool GetCurrentPosition(int32& Position);

	/**
	Gets the current status of the music service.
	@param   Status Current status of the music service.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool GetStatus(EMagicLeapMusicServiceStatus& Status);

	/**
	Gets the last error code. Call this function if another API fails.
	@param   LastError Last error.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool GetLastError(EMagicLeapMusicServiceError& LastError);

	/**
	Gets the current playback state.
	@param   PlaybackState Current playback state.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool GetPlaybackState(EMagicLeapMusicServicePlaybackState& PlaybackState);

	/**
	Gets the current repeat state.
	@param   RepeatState Current repeat state.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool GetRepeatState(EMagicLeapMusicServiceRepeatState& RepeatState);

	/**
	Gets the current shuffle state.
	@param   ShuffleState Current shuffle state.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool GetShuffleState(EMagicLeapMusicServiceShuffleState& ShuffleState);

	/**
	Gets the metadata from the current track.
	@param   MetaData Current track's metadata.
	@return  True if the operation was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MusicService|MagicLeap")
		static bool GetMetadata(FMagicLeapMusicServiceMetadata& MetaData);
};
