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

#include "MagicLeapMusicService.h"
#include "IMagicLeapMusicServicePlugin.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "MagicLeapPluginUtil.h"

#if WITH_MLSDK
#include "ml_music_service.h"
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY_STATIC(LogMusicService, Display, All);

class FMagicLeapMusicServicePlugin : public IMagicLeapMusicServicePlugin
{
public:
	virtual void StartupModule() override
	{
		IModuleInterface::StartupModule();
		APISetup.Startup();
#if WITH_MLSDK
		APISetup.LoadDLL(TEXT("ml_musicservice"));
#endif //WITH_MLSDK
	}

	virtual void ShutdownModule() override
	{
		APISetup.Shutdown();
		IModuleInterface::ShutdownModule();
	}

private:
	FMagicLeapAPISetup APISetup;
};

IMPLEMENT_MODULE(FMagicLeapMusicServicePlugin, MagicLeapMusicService);

//////////////////////////////////////////////////////////////////////////

#if WITH_MLSDK
MLMusicServiceShuffleState MapEnum(EMagicLeapMusicServiceShuffleState SourceEnum)
{
	switch (SourceEnum)
	{
		case EMagicLeapMusicServiceShuffleState::On:
			return MLMusicServiceShuffleState_On;
		case EMagicLeapMusicServiceShuffleState::Off:
			return MLMusicServiceShuffleState_Off;
		default:
			break;
	}
	return MLMusicServiceShuffleState_Unknown;
}

MLMusicServiceRepeatState MapEnum(EMagicLeapMusicServiceRepeatState SourceEnum)
{
	switch (SourceEnum)
	{
		case EMagicLeapMusicServiceRepeatState::Off:
			return MLMusicServiceRepeatState_Off;
		case EMagicLeapMusicServiceRepeatState::Song:
			return MLMusicServiceRepeatState_Song;
		case EMagicLeapMusicServiceRepeatState::Album:
			return MLMusicServiceRepeatState_Album;
		default:
			break;
	}
	return MLMusicServiceRepeatState_Unknown;
}

EMagicLeapMusicServiceStatus MapEnum(MLMusicServiceStatus SourceEnum)
{
	switch (SourceEnum)
	{
		case MLMusicServiceStatus_ContextChanged:
			return EMagicLeapMusicServiceStatus::ContextChanged;
		case MLMusicServiceStatus_Created:
			return EMagicLeapMusicServiceStatus::Created;
		case MLMusicServiceStatus_LoggedIn:
			return EMagicLeapMusicServiceStatus::LoggedIn;
		case MLMusicServiceStatus_LoggedOut:
			return EMagicLeapMusicServiceStatus::LoggedOut;
		case MLMusicServiceStatus_NextTrack:
			return EMagicLeapMusicServiceStatus::NextTrack;
		case MLMusicServiceStatus_PrevTrack:
			return EMagicLeapMusicServiceStatus::PrevTrack;
		case MLMusicServiceStatus_TrackChanged:
			return EMagicLeapMusicServiceStatus::TrackChanged;
		default:
			break;
	}
	return EMagicLeapMusicServiceStatus::Unknown;
}

EMagicLeapMusicServiceError MapEnum(MLMusicServiceError SourceEnum)
{
	switch (SourceEnum)
	{
		case MLMusicServiceError_None:
			return EMagicLeapMusicServiceError::None;
		case MLMusicServiceError_Connectivity:
			return EMagicLeapMusicServiceError::Connectivity;
		case MLMusicServiceError_Timeout:
			return EMagicLeapMusicServiceError::Timeout;
		case MLMusicServiceError_GeneralPlayback:
			return EMagicLeapMusicServiceError::GeneralPlayback;
		case MLMusicServiceError_Privilege:
			return EMagicLeapMusicServiceError::Privilege;
		case MLMusicServiceError_ServiceSpecific:
			return EMagicLeapMusicServiceError::ServiceSpecific;
		case MLMusicServiceError_NoMemory:
			return EMagicLeapMusicServiceError::NoMemory;
		default:
			break;
	}
	return EMagicLeapMusicServiceError::Unspecified;
}

EMagicLeapMusicServicePlaybackState MapEnum(MLMusicServicePlaybackState SourceEnum)
{
	switch (SourceEnum)
	{
		case MLMusicServicePlaybackState_Playing:
			return EMagicLeapMusicServicePlaybackState::Playing;
		case MLMusicServicePlaybackState_Paused:
			return EMagicLeapMusicServicePlaybackState::Paused;
		case MLMusicServicePlaybackState_Stopped:
			return EMagicLeapMusicServicePlaybackState::Stopped;
		case MLMusicServicePlaybackState_Error:
			return EMagicLeapMusicServicePlaybackState::Error;
		default:
			break;
	}
	return EMagicLeapMusicServicePlaybackState::Unknown;
}

EMagicLeapMusicServiceRepeatState MapEnum(MLMusicServiceRepeatState SourceEnum)
{
	switch (SourceEnum)
	{
		case MLMusicServiceRepeatState_Off:
			return EMagicLeapMusicServiceRepeatState::Off;
		case MLMusicServiceRepeatState_Song:
			return EMagicLeapMusicServiceRepeatState::Song;
		case MLMusicServiceRepeatState_Album:
			return EMagicLeapMusicServiceRepeatState::Album;
		default:
			break;
	}
	return EMagicLeapMusicServiceRepeatState::Unknown;
}

EMagicLeapMusicServiceShuffleState MapEnum(MLMusicServiceShuffleState SourceEnum)
{
	switch (SourceEnum)
	{
		case MLMusicServiceShuffleState_On:
			return EMagicLeapMusicServiceShuffleState::On;
		case MLMusicServiceShuffleState_Off:
			return EMagicLeapMusicServiceShuffleState::Off;
		default:
			break;
	}
	return EMagicLeapMusicServiceShuffleState::Unknown;
}
#endif //WITH_MLSDK

bool UMagicLeapMusicService::Connect(const FString& ProviderName)
{
#if WITH_MLSDK
	return (MLMusicServiceConnect(TCHAR_TO_ANSI(*ProviderName)) == MLResult_Ok);
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::Disconnect()
{
#if WITH_MLSDK
	return MLMusicServiceDisconnect() == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::SetAuthorizationString(const FString& AuthorizationString)
{
#if WITH_MLSDK
	return MLMusicServiceSetAuthString(TCHAR_TO_ANSI(*AuthorizationString)) == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::SetUrl(const FString& requestedUrl)
{
#if WITH_MLSDK
	return (MLMusicServiceSetURL(TCHAR_TO_ANSI(*requestedUrl)) == MLResult_Ok);
#else
	return false;
#endif //WITH_MLSDK

}

bool UMagicLeapMusicService::SetUrlList(const TArray<FString>& Playlist)
{
#if WITH_MLSDK
	TArray<const char *> ptrs;
	for (int i = 0; i < Playlist.Num(); ++ i) {
		ptrs.Add(TCHAR_TO_ANSI(*Playlist[i]));
	}
	return MLMusicServiceSetPlayList(&ptrs[0], ptrs.Num()) == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::Start()
{
#if WITH_MLSDK
	return MLMusicServiceStart() == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::Stop()
{
#if WITH_MLSDK
	return MLMusicServiceStop() == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::Pause()
{
#if WITH_MLSDK
	return MLMusicServicePause() == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::Resume()
{
#if WITH_MLSDK
	return MLMusicServiceResume() == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::Seek(int32 SeekMs)
{
#if WITH_MLSDK
	return MLMusicServiceSeek(static_cast<uint32>(SeekMs)) == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::Next()
{
#if WITH_MLSDK
	return MLMusicServiceNext() == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::Previous()
{
#if WITH_MLSDK
	return MLMusicServicePrevious() == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::SetShuffle(EMagicLeapMusicServiceShuffleState ShuffleState)
{
#if WITH_MLSDK
	return MLMusicServiceSetShuffle(MapEnum(ShuffleState)) == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::SetRepeat(EMagicLeapMusicServiceRepeatState RepeatState)
{
#if WITH_MLSDK
	return MLMusicServiceSetRepeat(MapEnum(RepeatState)) == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::SetVolume(float Volume)
{
#if WITH_MLSDK
	return MLMusicServiceSetVolume(Volume) == MLResult_Ok;
#else
	return false;
#endif //WITH_MLSDK
}

bool UMagicLeapMusicService::GetTrackLength(int32& TrackLength)
{
#if WITH_MLSDK
	uint32 NativeTrackLength;
	if (MLMusicServiceGetTrackLength(&NativeTrackLength) == MLResult_Ok)
	{
		TrackLength = static_cast<int32>(NativeTrackLength);
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapMusicService::GetCurrentPosition(int32& Position)
{
#if WITH_MLSDK
	uint32 NativePosition;
	if (MLMusicServiceGetCurrentPosition(&NativePosition) == MLResult_Ok)
	{
		Position = static_cast<int32>(NativePosition);
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapMusicService::GetStatus(EMagicLeapMusicServiceStatus& CurrentStatus)
{
#if WITH_MLSDK
	MLMusicServiceStatus NativeCurrentStatus = MLMusicServiceStatus_Unknown;
	if (MLMusicServiceGetStatus(&NativeCurrentStatus) == MLResult_Ok)
	{
		CurrentStatus = MapEnum(NativeCurrentStatus);
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapMusicService::GetLastError(EMagicLeapMusicServiceError& LastError)
{
#if WITH_MLSDK
	MLMusicServiceError NativeLastError = MLMusicServiceError_None;
	int32 ErrorCode;
	if (MLMusicServiceGetError(&NativeLastError, &ErrorCode) == MLResult_Ok)
	{
		LastError = MapEnum(NativeLastError);
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapMusicService::GetPlaybackState(EMagicLeapMusicServicePlaybackState& PlaybackState)
{
#if WITH_MLSDK
	MLMusicServicePlaybackState NativePlaybackState = MLMusicServicePlaybackState_Unknown;
	if (MLMusicServiceGetPlaybackState(&NativePlaybackState) == MLResult_Ok)
	{
		PlaybackState = MapEnum(NativePlaybackState);
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapMusicService::GetRepeatState(EMagicLeapMusicServiceRepeatState& RepeatState)
{
#if WITH_MLSDK
	MLMusicServiceRepeatState NativeRepeatState = MLMusicServiceRepeatState_Unknown;
	if (MLMusicServiceGetRepeatState(&NativeRepeatState) == MLResult_Ok)
	{
		RepeatState = MapEnum(NativeRepeatState);
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapMusicService::GetShuffleState(EMagicLeapMusicServiceShuffleState& ShuffleState)
{
#if WITH_MLSDK
	MLMusicServiceShuffleState NativeShuffleState = MLMusicServiceShuffleState_Unknown;
	if (MLMusicServiceGetShuffleState(&NativeShuffleState) == MLResult_Ok)
	{
		ShuffleState = MapEnum(NativeShuffleState);
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

bool UMagicLeapMusicService::GetMetadata(FMagicLeapMusicServiceMetadata& MetaData)
{
#if WITH_MLSDK
	MLMusicServiceMetadata NativeMetaData;
	if (MLMusicServiceGetMetadata(&NativeMetaData) == MLResult_Ok)
	{
		MetaData.AlbumTitle = NativeMetaData.album_title;
		MetaData.AlbumInfoName = NativeMetaData.album_info_name;
		MetaData.AlbumInfoUrl = NativeMetaData.album_info_url;
		MetaData.AlbumInfoCoverUrl = NativeMetaData.album_info_cover_url;
		MetaData.ArtistInfoName = NativeMetaData.artist_info_name;
		MetaData.ArtistInfoUrl = NativeMetaData.artist_info_url;
		MetaData.Length = NativeMetaData.length;
		MetaData.Position = NativeMetaData.position;
		(void)MLMusicServiceReleaseMetadata(&NativeMetaData);
		return true;
	}

#endif //WITH_MLSDK
	return false;
}
