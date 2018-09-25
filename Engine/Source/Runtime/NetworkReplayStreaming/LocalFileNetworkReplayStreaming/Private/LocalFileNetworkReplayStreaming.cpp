// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LocalFileNetworkReplayStreaming.h"
#include "Misc/NetworkVersion.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Async/Async.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/CoreOnline.h"
#include "Serialization/LargeMemoryReader.h"

DEFINE_LOG_CATEGORY_STATIC(LogLocalFileReplay, Log, All);

DECLARE_STATS_GROUP(TEXT("LocalReplay"), STATGROUP_LocalReplay, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("Local replay compress time"), STAT_LocalReplay_CompressTime, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay decompress time"), STAT_LocalReplay_DecompressTime, STATGROUP_LocalReplay);

DECLARE_CYCLE_STAT(TEXT("Local replay read info"), STAT_LocalReplay_ReadReplayInfo, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay write info"), STAT_LocalReplay_WriteReplayInfo, STATGROUP_LocalReplay);

DECLARE_CYCLE_STAT(TEXT("Local replay rename"), STAT_LocalReplay_Rename, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay rename friendly"), STAT_LocalReplay_RenameFriendly, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay enumerate"), STAT_LocalReplay_Enumerate, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay delete"), STAT_LocalReplay_Delete, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay automatic name"), STAT_LocalReplay_AutomaticName, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay start recording"), STAT_LocalReplay_StartRecording, STATGROUP_LocalReplay);

DECLARE_CYCLE_STAT(TEXT("Local replay read checkpoint"), STAT_LocalReplay_ReadCheckpoint, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay read stream"), STAT_LocalReplay_ReadStream, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay read header"), STAT_LocalReplay_ReadHeader, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay read event"), STAT_LocalReplay_ReadEvent, STATGROUP_LocalReplay);

DECLARE_CYCLE_STAT(TEXT("Local replay flush checkpoint"), STAT_LocalReplay_FlushCheckpoint, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay flush stream"), STAT_LocalReplay_FlushStream, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay flush header"), STAT_LocalReplay_FlushHeader, STATGROUP_LocalReplay);
DECLARE_CYCLE_STAT(TEXT("Local replay flush event"), STAT_LocalReplay_FlushEvent, STATGROUP_LocalReplay);

namespace LocalFileReplay
{
	enum ELocalFileVersionHistory : uint32
	{
		HISTORY_INITIAL							= 0,
		HISTORY_FIXEDSIZE_FRIENDLY_NAME			= 1,
		HISTORY_COMPRESSION						= 2,
		HISTORY_RECORDED_TIMESTAMP				= 3,
		HISTORY_STREAM_CHUNK_TIMES				= 4,
		HISTORY_FRIENDLY_NAME_ENCODING			= 5,

		// -----<new versions can be added before this line>-------------------------------------------------
		HISTORY_PLUS_ONE,
		HISTORY_LATEST 							= HISTORY_PLUS_ONE - 1
	};

	const uint32 FileMagic = 0x1CA2E27F;
	const uint32 MaxFriendlyNameLen = 256;

	TAutoConsoleVariable<int32> CVarMaxCacheSize(TEXT("localReplay.MaxCacheSize"), 1024 * 1024 * 10, TEXT(""));
	TAutoConsoleVariable<int32> CVarMaxBufferedStreamChunks(TEXT("localReplay.MaxBufferedStreamChunks"), 5, TEXT(""));
	TAutoConsoleVariable<int32> CVarAllowLiveStreamDelete(TEXT("localReplay.AllowLiveStreamDelete"), 1, TEXT(""));
	TAutoConsoleVariable<float> CVarChunkUploadDelayInSeconds(TEXT("localReplay.ChunkUploadDelayInSeconds"), 20.0f, TEXT(""));
};

FLocalFileNetworkReplayStreamer::FLocalFileSerializationInfo::FLocalFileSerializationInfo() 
	: FileVersion(LocalFileReplay::HISTORY_LATEST)
{
}

void FLocalFileStreamFArchive::Serialize(void* V, int64 Length) 
{
	if (IsLoading())
	{
		if ((Pos + Length) > Buffer.Num())
		{
			ArIsError = true;
			return;
		}

		FMemory::Memcpy(V, Buffer.GetData() + Pos, Length);

		Pos += Length;
	}
	else
	{
		check(Pos <= Buffer.Num());

		const int32 SpaceNeeded = Length - (Buffer.Num() - Pos);

		if (SpaceNeeded > 0)
		{
			Buffer.AddZeroed(SpaceNeeded);
		}

		FMemory::Memcpy(Buffer.GetData() + Pos, V, Length);

		Pos += Length;
	}
}

int64 FLocalFileStreamFArchive::Tell() 
{
	return Pos;
}

int64 FLocalFileStreamFArchive::TotalSize()
{
	return Buffer.Num();
}

void FLocalFileStreamFArchive::Seek(int64 InPos) 
{
	check(InPos <= Buffer.Num());

	Pos = InPos;
}

bool FLocalFileStreamFArchive::AtEnd() 
{
	return Pos >= Buffer.Num() && bAtEndOfReplay;
}

FLocalFileNetworkReplayStreamer::FLocalFileNetworkReplayStreamer() 
	: StreamTimeRange(0, 0)
	, StreamDataOffset(0)
	, StreamChunkIndex(0)
	, LastChunkTime(0)
	, LastRefreshTime(0)
	, bStopStreamingCalled(false)
	, HighPriorityEndTime(0)
	, LastGotoTimeInMS(-1)
	, StreamerState(EStreamerState::Idle)
	, StreamerLastError(ENetworkReplayError::None)
	, DemoSavePath(GetDefaultDemoSavePath())
	, bCacheFileReadsInMemory(false)
{
}

FLocalFileNetworkReplayStreamer::FLocalFileNetworkReplayStreamer(const FString& InDemoSavePath) 
	: StreamTimeRange(0, 0)
	, StreamDataOffset(0)
	, StreamChunkIndex(0)
	, LastChunkTime(0)
	, LastRefreshTime(0)
	, bStopStreamingCalled(false)
	, HighPriorityEndTime(0)
	, LastGotoTimeInMS(-1)
	, StreamerState(EStreamerState::Idle)
	, StreamerLastError(ENetworkReplayError::None)
	, DemoSavePath(InDemoSavePath.EndsWith(TEXT("/")) ? InDemoSavePath : InDemoSavePath + FString("/"))
	, bCacheFileReadsInMemory(false)
{
}

FLocalFileNetworkReplayStreamer::~FLocalFileNetworkReplayStreamer()
{

}

bool FLocalFileNetworkReplayStreamer::ReadReplayInfo(const FString& StreamName, FLocalFileReplayInfo& Info) const
{
	SCOPE_CYCLE_COUNTER(STAT_LocalReplay_ReadReplayInfo);

	TSharedPtr<FArchive> LocalFileAr = CreateLocalFileReader(GetDemoFullFilename(StreamName));
	if (LocalFileAr.IsValid())
	{
		return ReadReplayInfo(*LocalFileAr.Get(), Info);
	}

	return false;
}

bool FLocalFileNetworkReplayStreamer::ReadReplayInfo(FArchive& Archive, FLocalFileReplayInfo& Info) const
{
	FLocalFileSerializationInfo DefaultSerializationInfo;
	return ReadReplayInfo(Archive, Info, DefaultSerializationInfo);
}

bool FLocalFileNetworkReplayStreamer::ReadReplayInfo(FArchive& Archive, FLocalFileReplayInfo& Info, FLocalFileSerializationInfo& SerializationInfo) const
{
	// reset the info before reading
	Info = FLocalFileReplayInfo();

	if (Archive.TotalSize() != 0)
	{
		uint32 MagicNumber;
		Archive << MagicNumber;

		uint32 FileVersion;
		Archive << FileVersion;

		if (MagicNumber == LocalFileReplay::FileMagic)
		{
			SerializationInfo.FileVersion = FileVersion;

			// read summary info
			Archive << Info.LengthInMS;
			Archive << Info.NetworkVersion;
			Archive << Info.Changelist;

			FString FriendlyName;
			Archive << FriendlyName;

			SerializationInfo.FileFriendlyName = FriendlyName;

			if (FileVersion >= LocalFileReplay::HISTORY_FIXEDSIZE_FRIENDLY_NAME)
			{
				// trim whitespace since this may have been padded
				Info.FriendlyName = FriendlyName.TrimEnd();
			}
			else
			{
				// Note, don't touch the FriendlyName if this is an older replay.
				// Users can adjust the name as necessary using GetMaxFriendlyNameSize.
				UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::ReadReplayInfoInternal - Loading an old replay, friendly name length **must not** be changed."));
			}

			uint32 IsLive;
			Archive << IsLive;

			Info.bIsLive = (IsLive != 0);

			if (FileVersion >= LocalFileReplay::HISTORY_RECORDED_TIMESTAMP)
			{
				Archive << Info.Timestamp;
			}

			if (FileVersion >= LocalFileReplay::HISTORY_COMPRESSION)
			{
				uint32 Compressed;
				Archive << Compressed;

				Info.bCompressed = (Compressed != 0);
			}

			int64 TotalSize = Archive.TotalSize();

			// now look for all chunks
			while (!Archive.AtEnd())
			{
				int64 TypeOffset = Archive.Tell();

				ELocalFileChunkType ChunkType;
				Archive << ChunkType;

				int32 Idx = Info.Chunks.AddDefaulted();
				FLocalFileChunkInfo& Chunk = Info.Chunks[Idx];
				Chunk.ChunkType = ChunkType;

				Archive << Chunk.SizeInBytes;

				Chunk.TypeOffset = TypeOffset;
				Chunk.DataOffset = Archive.Tell();

				if ((Chunk.SizeInBytes < 0) || ((Chunk.DataOffset + Chunk.SizeInBytes) > TotalSize))
				{
					UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Invalid chunk size: %lld"), Chunk.SizeInBytes);
					Archive.SetError();
					return false;
				}

				switch(ChunkType)
				{
				case ELocalFileChunkType::Header:
				{
					if (Info.HeaderChunkIndex == INDEX_NONE)
					{
						Info.HeaderChunkIndex = Idx;
					}
					else
					{
						UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Found multiple header chunks"));
						Archive.SetError();
						return false;
					}
				}
				break;
				case ELocalFileChunkType::Checkpoint:
				{
					int32 CheckpointIdx = Info.Checkpoints.AddDefaulted();
					FLocalFileEventInfo& Checkpoint = Info.Checkpoints[CheckpointIdx];

					Checkpoint.ChunkIndex = Idx;
					
					Archive << Checkpoint.Id;
					Archive << Checkpoint.Group;
					Archive << Checkpoint.Metadata;
					Archive << Checkpoint.Time1;
					Archive << Checkpoint.Time2;

					Archive << Checkpoint.SizeInBytes;

					Checkpoint.EventDataOffset = Archive.Tell();
					
					if ((Checkpoint.SizeInBytes < 0) || ((Checkpoint.EventDataOffset + Checkpoint.SizeInBytes) > TotalSize))
					{
						UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Invalid checkpoint size: %lld"), Checkpoint.SizeInBytes);
						Archive.SetError();
						return false;
					}

					if (Info.bCompressed)
					{
						int32 DecompressedSize = GetDecompressedSize(Archive);
						if (DecompressedSize < 0)
						{
							UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Invalid decompressed checkpoint size: %d"), DecompressedSize);
							Archive.SetError();
							return false;
						}
					}
				}
				break;
				case ELocalFileChunkType::ReplayData:
				{
					int32 DataIdx = Info.DataChunks.AddDefaulted();
					FLocalFileReplayDataInfo& DataChunk = Info.DataChunks[DataIdx];
					DataChunk.ChunkIndex = Idx;
					DataChunk.StreamOffset = Info.TotalDataSizeInBytes;

					if (FileVersion >= LocalFileReplay::HISTORY_STREAM_CHUNK_TIMES)
					{
						Archive << DataChunk.Time1;
						Archive << DataChunk.Time2;
						Archive << DataChunk.SizeInBytes;
					}
					else
					{
						DataChunk.SizeInBytes = Chunk.SizeInBytes;
					}

					DataChunk.ReplayDataOffset = Archive.Tell();

					if ((DataChunk.SizeInBytes < 0) || ((DataChunk.ReplayDataOffset + DataChunk.SizeInBytes) > TotalSize))
					{
						UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Invalid stream chunk size: %lld"), DataChunk.SizeInBytes);
						Archive.SetError();
						return false;
					}

					if (Info.bCompressed)
					{
						int32 DecompressedSize = GetDecompressedSize(Archive);
						if (DecompressedSize < 0)
						{
							UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Invalid decompressed replay data size: %d"), DecompressedSize);
							Archive.SetError();
							return false;
						}
							
						Info.TotalDataSizeInBytes += DecompressedSize;
					}
					else
					{
						Info.TotalDataSizeInBytes += DataChunk.SizeInBytes;
					}
				}
				break;
				case ELocalFileChunkType::Event:
				{
					int32 EventIdx = Info.Events.AddDefaulted();
					FLocalFileEventInfo& Event = Info.Events[EventIdx];
					Event.ChunkIndex = Idx;

					Archive << Event.Id;
					Archive << Event.Group;
					Archive << Event.Metadata;
					Archive << Event.Time1;
					Archive << Event.Time2;

					Archive << Event.SizeInBytes;

					Event.EventDataOffset = Archive.Tell();

					if ((Event.SizeInBytes < 0) || ((Event.EventDataOffset + Event.SizeInBytes) > TotalSize))
					{
						UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Invalid event size: %lld"), Event.SizeInBytes);
						Archive.SetError();
						return false;
					}
				}
				break;
				case ELocalFileChunkType::Unknown:
					UE_LOG(LogLocalFileReplay, Verbose, TEXT("ReadReplayInfo: Skipping unknown (cleared) chunk"));
					break;
				default:
					UE_LOG(LogLocalFileReplay, Warning, TEXT("ReadReplayInfo: Unhandled file chunk type: %d"), (uint32)ChunkType);
					break;
				}

				if (Archive.IsError())
				{
					UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Archive error after parsing chunk"));
					return false;
				}

				Archive.Seek(Chunk.DataOffset + Chunk.SizeInBytes);
			}
		}

		if (FileVersion < LocalFileReplay::HISTORY_STREAM_CHUNK_TIMES)
		{
			for(int i=0; i < Info.DataChunks.Num(); ++i)
			{
				const int32 CheckpointStartIdx = i - 1;

				if (Info.Checkpoints.IsValidIndex(CheckpointStartIdx))
				{
					Info.DataChunks[i].Time1 = Info.Checkpoints[CheckpointStartIdx].Time1;
				}
				else
				{
					Info.DataChunks[i].Time1 = 0;
				}					

				if (Info.Checkpoints.IsValidIndex(i))
				{
					Info.DataChunks[i].Time2 = Info.Checkpoints[i].Time1;
				}
				else
				{
					Info.DataChunks[i].Time2 = Info.LengthInMS;
				}
			}
		}

		// check for overlapping data chunk times
		for(const FLocalFileReplayDataInfo& DataInfo : Info.DataChunks)
		{
			TInterval<uint32> Range1(DataInfo.Time1, DataInfo.Time2);

			for(const FLocalFileReplayDataInfo& DataInfoCompare : Info.DataChunks)
			{
				if (DataInfo.ChunkIndex != DataInfoCompare.ChunkIndex)
				{
					const TInterval<uint32> Range2(DataInfoCompare.Time1, DataInfoCompare.Time2);
					const TInterval<uint32> Overlap = Intersect(Range1, Range2);

					if (Overlap.IsValid() && Overlap.Size() > 0)
					{
						UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Found overlapping data chunks"));
						Archive.SetError();
						return false;
					}
				}
			}
		}

		// checkpoints should be unique
		TSet<FString> CheckpointIds;

		for (const FLocalFileEventInfo& Checkpoint : Info.Checkpoints)
		{
			if (CheckpointIds.Contains(Checkpoint.Id))
			{
				UE_LOG(LogLocalFileReplay, Error, TEXT("ReadReplayInfo: Found duplicate checkpoint id: %s"), *Checkpoint.Id);
				Archive.SetError();
				return false;
			}

			CheckpointIds.Add(Checkpoint.Id);
		}

		Info.bIsValid = Info.Chunks.IsValidIndex(Info.HeaderChunkIndex);

		return Info.bIsValid && !Archive.IsError();
	}

	return false;
}

bool FLocalFileNetworkReplayStreamer::WriteReplayInfo(const FString& StreamName, const FLocalFileReplayInfo& InReplayInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_LocalReplay_WriteReplayInfo);

	// Update metadata with latest info
	TSharedPtr<FArchive> ReplayInfoFileAr = CreateLocalFileWriter(GetDemoFullFilename(StreamName));
	if (ReplayInfoFileAr.IsValid())
	{
		return WriteReplayInfo(*ReplayInfoFileAr.Get(), InReplayInfo);
	}

	return false;
}

bool FLocalFileNetworkReplayStreamer::WriteReplayInfo(FArchive& Archive, const FLocalFileReplayInfo& InReplayInfo)
{
	FLocalFileSerializationInfo DefaultSerializationInfo;
	return WriteReplayInfo(Archive, InReplayInfo, DefaultSerializationInfo);
}

bool FLocalFileNetworkReplayStreamer::WriteReplayInfo(FArchive& Archive, const FLocalFileReplayInfo& InReplayInfo, FLocalFileSerializationInfo& SerializationInfo)
{
	if (SerializationInfo.FileVersion < LocalFileReplay::HISTORY_FIXEDSIZE_FRIENDLY_NAME)
	{
		UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkRepalyStreamer::WriteReplayInfo: Unable to safely rewrite old replay info"));
		return false;
	}

	Archive.Seek(0);

	uint32 MagicNumber = LocalFileReplay::FileMagic;
	Archive << MagicNumber;

	Archive << SerializationInfo.FileVersion;

	Archive << const_cast<int32&>(InReplayInfo.LengthInMS);
	Archive << const_cast<uint32&>(InReplayInfo.NetworkVersion);
	Archive << const_cast<uint32&>(InReplayInfo.Changelist);

	FString FixedSizeName;
	FixupFriendlyNameLength(InReplayInfo.FriendlyName, FixedSizeName);

	if (SerializationInfo.FileVersion < LocalFileReplay::HISTORY_FRIENDLY_NAME_ENCODING)
	{
		// if the new name contains non-ANSI characters and the old does not, serializing would corrupt the file
		if (!FCString::IsPureAnsi(*FixedSizeName) && FCString::IsPureAnsi(*SerializationInfo.FileFriendlyName))
		{
			UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkRepalyStreamer::WriteReplayInfo: Forcing friendly name to ANSI to avoid corrupting file"));
			
			FString ConvertedName = TCHAR_TO_ANSI(*FixedSizeName);
			Archive << ConvertedName;
		}
		// otherwise if the old name has non-ANSI character, force unicode
		else if (!FCString::IsPureAnsi(*SerializationInfo.FileFriendlyName))
		{
			bool bForceUnicode = Archive.IsForcingUnicode();
			Archive.SetForceUnicode(true);
			Archive << FixedSizeName;
			Archive.SetForceUnicode(bForceUnicode);
		}
		else // both are ANSI, just write the string
		{
			Archive << FixedSizeName;
		}
	}
	else
	{
		// force unicode so the size will actually be fixed
		bool bForceUnicode = Archive.IsForcingUnicode();
		Archive.SetForceUnicode(true);
		Archive << FixedSizeName;
		Archive.SetForceUnicode(bForceUnicode);
	}

	uint32 IsLive = InReplayInfo.bIsLive ? 1 : 0;
	Archive << IsLive;

	// It's possible we're updating an older replay (e.g., for a rename)
	// Therefore, we can't write out any data that the replay wouldn't have had.
	if (SerializationInfo.FileVersion >= LocalFileReplay::HISTORY_RECORDED_TIMESTAMP)
	{
		Archive << const_cast<FDateTime&>(InReplayInfo.Timestamp);
	}

	if (SerializationInfo.FileVersion >= LocalFileReplay::HISTORY_COMPRESSION)
	{
		uint32 Compressed = SupportsCompression() ? 1 : 0;
		Archive << Compressed;
	}

	return !Archive.IsError();
}

void FLocalFileNetworkReplayStreamer::FixupFriendlyNameLength(const FString& UnfixedName, FString& FixedName) const
{
	const uint32 DesiredLength = GetMaxFriendlyNameSize();
	const uint32 NameLen = UnfixedName.Len();
	if (NameLen < DesiredLength)
	{
		FixedName = UnfixedName.RightPad(DesiredLength);
	}
	else
	{
		FixedName = UnfixedName.Left(DesiredLength);
	}
}

void FLocalFileNetworkReplayStreamer::StartStreaming(const FString& CustomName, const FString& FriendlyName, const TArray<int32>& UserIndices, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FStartStreamingCallback& Delegate)
{
	StartStreaming_Internal(CustomName, FriendlyName, UserIndices, bRecord, ReplayVersion, Delegate);
}

void FLocalFileNetworkReplayStreamer::StartStreaming(const FString& CustomName, const FString& FriendlyName, const TArray<FString>& UserStrings, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FStartStreamingCallback& Delegate)
{
	TArray<int32> UserIndices;
	GetUserIndicesFromUserStrings(UserStrings, UserIndices);
	StartStreaming_Internal(CustomName, FriendlyName, UserIndices, bRecord, ReplayVersion, Delegate);
}

void FLocalFileNetworkReplayStreamer::StartStreaming_Internal(const FString& CustomName, const FString& FriendlyName, const TArray<int32>& UserIndices, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FStartStreamingCallback& Delegate)
{
	FStartStreamingResult Result;
	Result.bRecording = bRecord;

	if (IsStreaming())
	{
		UE_LOG( LogLocalFileReplay, Warning, TEXT( "FLocalFileNetworkReplayStreamer::StartStreaming. IsStreaming == true." ) );
		Delegate.ExecuteIfBound(Result);
		return;
	}

	if (IsFileRequestInProgress())
	{
		UE_LOG( LogLocalFileReplay, Warning, TEXT( "FLocalFileNetworkReplayStreamer::StartStreaming. IsFileRequestInProgress == true." ) );
		Delegate.ExecuteIfBound(Result);
		return;
	}

	FString FinalDemoName = CustomName;

	if (CustomName.IsEmpty())
	{
		if (bRecord)
		{
			// If we're recording and the caller didn't provide a name, generate one automatically
			FinalDemoName = GetAutomaticDemoName();
		}
		else
		{
			// Can't play a replay if the user didn't provide a name!
			Result.Result = EStreamingOperationResult::ReplayNotFound;
			Delegate.ExecuteIfBound(Result);
			return;
		}
	}

	// Setup the archives
	StreamAr.SetIsLoading(!bRecord);
	StreamAr.SetIsSaving(!StreamAr.IsLoading());
	StreamAr.bAtEndOfReplay = false;

	HeaderAr.SetIsLoading(StreamAr.IsLoading());
	HeaderAr.SetIsSaving(StreamAr.IsSaving());

	CheckpointAr.SetIsLoading(StreamAr.IsLoading());
	CheckpointAr.SetIsSaving(StreamAr.IsSaving());

	CurrentReplayInfo.LengthInMS = 0;

	StreamTimeRange = TInterval<uint32>(0, 0);

	StreamDataOffset = 0;
	StreamChunkIndex = 0;

	LastChunkTime = FPlatformTime::Seconds();

	const FString FullDemoFilename = GetDemoFullFilename(FinalDemoName);

	CurrentStreamName = FinalDemoName;

	if (!bRecord)
	{
		// We are playing
		StreamerState = EStreamerState::Playback;

		// Add the request to start loading
		AddDelegateFileRequestToQueue<FStartStreamingResult>(EQueuedLocalFileRequestType::StartPlayback, 
			[this, bRecord, FullDemoFilename](TLocalFileRequestCommonData<FStartStreamingResult>& RequestData)
			{
				RequestData.DelegateResult.bRecording = bRecord;
				
				if (!FPaths::FileExists(FullDemoFilename))
				{
					RequestData.DelegateResult.Result = EStreamingOperationResult::ReplayNotFound;
				}
				else
				{
					// Load metadata if it exists
					ReadReplayInfo(CurrentStreamName, RequestData.ReplayInfo);
				}
			},
			[this, bRecord, Delegate](TLocalFileRequestCommonData<FStartStreamingResult>& RequestData)
			{
				if (RequestData.DelegateResult.Result == EStreamingOperationResult::ReplayNotFound)
				{
					Delegate.ExecuteIfBound(RequestData.DelegateResult);
				}
				else
				{
					CurrentReplayInfo = RequestData.ReplayInfo;

					if (!CurrentReplayInfo.bIsValid)
					{
						RequestData.DelegateResult.Result = EStreamingOperationResult::ReplayCorrupt;

						Delegate.ExecuteIfBound(RequestData.DelegateResult);
					}
					else
					{
						DownloadHeader(FDownloadHeaderCallback());

						AddDelegateFileRequestToQueue<FStartStreamingCallback, FStartStreamingResult>(EQueuedLocalFileRequestType::StartPlayback, Delegate,
							[this, bRecord](TLocalFileRequestCommonData<FStartStreamingResult>& PlaybackRequestData)
							{
								PlaybackRequestData.DelegateResult.bRecording = bRecord;	

								if (CurrentReplayInfo.bIsValid)
								{
									PlaybackRequestData.DelegateResult.Result = EStreamingOperationResult::Success;
								}
							});
					}
				}
			});
	}
	else
	{
		// We are recording
		StreamerState = EStreamerState::Recording;

		AddDelegateFileRequestToQueue<FStartStreamingResult>(EQueuedLocalFileRequestType::StartRecording,
			[this, bRecord, FullDemoFilename, ReplayVersion, FriendlyName, FinalDemoName](TLocalFileRequestCommonData<FStartStreamingResult>& RequestData)
			{
				SCOPE_CYCLE_COUNTER(STAT_LocalReplay_StartRecording);

				RequestData.DelegateResult.bRecording = bRecord;

				FLocalFileReplayInfo ExistingInfo;
				if (ReadReplayInfo(FinalDemoName, ExistingInfo))
				{
					if (ExistingInfo.bIsLive)
					{
						UE_LOG(LogLocalFileReplay, Warning, TEXT("StartStreaming is overwriting an existing live replay file."));
					}
				}

				// Delete any existing demo with this name
				IFileManager::Get().Delete(*FullDemoFilename);

				RequestData.ReplayInfo.NetworkVersion = ReplayVersion.NetworkVersion;
				RequestData.ReplayInfo.Changelist = ReplayVersion.Changelist;
				RequestData.ReplayInfo.FriendlyName = FriendlyName;
				RequestData.ReplayInfo.bIsLive = true;
				RequestData.ReplayInfo.Timestamp = FDateTime::Now();

				WriteReplayInfo(CurrentStreamName, RequestData.ReplayInfo);

				RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
			},
			[this, Delegate](TLocalFileRequestCommonData<FStartStreamingResult>& RequestData)
			{
				CurrentReplayInfo = RequestData.ReplayInfo;

				Delegate.ExecuteIfBound(RequestData.DelegateResult);
			});

		RefreshHeader();
	}
}

void FLocalFileNetworkReplayStreamer::CancelStreamingRequests()
{
	// Cancel any active request
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->CancelRequest();
		ActiveRequest = nullptr;
	}

	// Empty the request queue
	QueuedRequests.Empty();

	StreamerState = EStreamerState::Idle;
	bStopStreamingCalled = false;
}

void FLocalFileNetworkReplayStreamer::SetLastError( const ENetworkReplayError::Type InLastError )
{
	CancelStreamingRequests();
	StreamerLastError = InLastError;
}

ENetworkReplayError::Type FLocalFileNetworkReplayStreamer::GetLastError() const
{
	return StreamerLastError;
}

void FLocalFileNetworkReplayStreamer::StopStreaming()
{
	if (IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::StartPlayback) || IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::StartRecording))
	{
		UE_LOG( LogLocalFileReplay, Warning, TEXT( "FLocalFileNetworkReplayStreamer::StopStreaming. Called while existing StartStreaming request wasn't finished" ) );
		CancelStreamingRequests();
		check( !IsStreaming() );
		return;
	}

	if (!IsStreaming())
	{
		UE_LOG( LogLocalFileReplay, Warning, TEXT( "FLocalFileNetworkReplayStreamer::StopStreaming. Not currently streaming." ) );
		check( bStopStreamingCalled == false );
		return;
	}

	if (bStopStreamingCalled)
	{
		UE_LOG( LogLocalFileReplay, Warning, TEXT( "FLocalFileNetworkReplayStreamer::StopStreaming. Already called" ) );
		return;
	}

	bStopStreamingCalled = true;

	if (StreamerState == EStreamerState::Recording)
	{
		// Flush any final pending stream
		int32 TotalLengthInMS = CurrentReplayInfo.LengthInMS;

		FlushStream(TotalLengthInMS);

		AddGenericRequestToQueue<FLocalFileReplayInfo>(EQueuedLocalFileRequestType::StopRecording,
			[this, TotalLengthInMS](FLocalFileReplayInfo& ReplayInfo)
			{
				if (ReadReplayInfo(CurrentStreamName, ReplayInfo))
				{
					ReplayInfo.bIsLive = false;
					ReplayInfo.LengthInMS = TotalLengthInMS;

					WriteReplayInfo(CurrentStreamName, ReplayInfo);
				}
			},
			[this](FLocalFileReplayInfo& ReplayInfo)
			{
				CurrentReplayInfo = ReplayInfo;
			});
	}

	// Finally, add the stop streaming request, which should put things in the right state after the above requests are done
	AddSimpleRequestToQueue(EQueuedLocalFileRequestType::StopStreaming,
		[]()
		{
			UE_LOG(LogLocalFileReplay, Verbose, TEXT("FLocalFileNetworkReplayStreamer::StopStreaming"));
		},
		[this]()
		{
			bStopStreamingCalled = false;
			StreamAr.SetIsLoading(false);
			StreamAr.SetIsSaving(false);
			StreamAr.Buffer.Empty();
			StreamAr.Pos = 0;
			StreamDataOffset = 0;
			StreamChunkIndex = 0;
			CurrentStreamName.Empty();
			StreamerState = EStreamerState::Idle;
		});
}

FArchive* FLocalFileNetworkReplayStreamer::GetHeaderArchive()
{
	return &HeaderAr;
}

FArchive* FLocalFileNetworkReplayStreamer::GetStreamingArchive()
{
	return &StreamAr;
}

void FLocalFileNetworkReplayStreamer::UpdateTotalDemoTime(uint32 TimeInMS)
{
	check(StreamerState == EStreamerState::Recording);

	CurrentReplayInfo.LengthInMS = TimeInMS;
}

bool FLocalFileNetworkReplayStreamer::IsDataAvailable() const
{
	if (GetLastError() != ENetworkReplayError::None)
	{
		return false;
	}

	if (IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::ReadingCheckpoint))
	{
		return false;
	}

	if (HighPriorityEndTime > 0)
	{
		// If we are waiting for a high priority portion of the stream, pretend like we don't have any data so that game code waits for the entire portion
		// of the high priority stream to load.
		// We do this because we assume the game wants to race through this high priority portion of the stream in a single frame
		return false;
	}

	// If we are loading, and we have more data
	if (StreamAr.IsLoading() && StreamAr.Pos < StreamAr.Buffer.Num() && CurrentReplayInfo.DataChunks.Num() > 0)
	{
		return true;
	}

	return false;
}

bool FLocalFileNetworkReplayStreamer::IsLive() const
{
	return CurrentReplayInfo.bIsLive;
}

bool FLocalFileNetworkReplayStreamer::IsNamedStreamLive(const FString& StreamName) const
{
	check(!IsInGameThread());

	FLocalFileReplayInfo Info;
	return ReadReplayInfo(StreamName, Info) && Info.bIsLive;
}

void FLocalFileNetworkReplayStreamer::DeleteFinishedStream(const FString& StreamName, const int32 UserIndex, const FDeleteFinishedStreamCallback& Delegate)
{
	DeleteFinishedStream_Internal(StreamName, UserIndex, Delegate);
}

void FLocalFileNetworkReplayStreamer::DeleteFinishedStream(const FString& StreamName, const FDeleteFinishedStreamCallback& Delegate)
{
	DeleteFinishedStream_Internal(StreamName, INDEX_NONE, Delegate);
}

void FLocalFileNetworkReplayStreamer::DeleteFinishedStream_Internal(const FString& StreamName, const int32 UserIndex, const FDeleteFinishedStreamCallback& Delegate)
{
	AddDelegateFileRequestToQueue<FDeleteFinishedStreamCallback, FDeleteFinishedStreamResult>(EQueuedLocalFileRequestType::DeletingFinishedStream, Delegate,
		[this, StreamName](TLocalFileRequestCommonData<FDeleteFinishedStreamResult>& RequestData)
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_Delete);

			const bool bIsLive = IsNamedStreamLive(StreamName);

			if (LocalFileReplay::CVarAllowLiveStreamDelete.GetValueOnAnyThread() || !bIsLive)
			{
				UE_CLOG(bIsLive, LogLocalFileReplay, Warning, TEXT("Deleting network replay stream %s that is currently live!"), *StreamName);

				const FString FullDemoFilename = GetDemoFullFilename(StreamName);
			
				if (!FPaths::FileExists(FullDemoFilename))
				{
					RequestData.DelegateResult.Result = EStreamingOperationResult::ReplayNotFound;
				}
				else if (IFileManager::Get().Delete(*FullDemoFilename))
				{
					RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
				}
			}
			else
			{
				UE_LOG(LogLocalFileReplay, Warning, TEXT("Can't delete network replay stream %s because it is live!"), *StreamName);
			}
		});
}

void FLocalFileNetworkReplayStreamer::EnumerateStreams(const FNetworkReplayVersion& ReplayVersion, const int32 UserIndex, const FString& MetaString, const TArray< FString >& ExtraParms, const FEnumerateStreamsCallback& Delegate)
{
	EnumerateStreams_Internal(ReplayVersion, UserIndex, MetaString, ExtraParms, Delegate);
}

void FLocalFileNetworkReplayStreamer::EnumerateStreams(const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const TArray< FString >& ExtraParms, const FEnumerateStreamsCallback& Delegate)
{
	EnumerateStreams_Internal(ReplayVersion, GetUserIndexFromUserString(UserString), MetaString, ExtraParms, Delegate);
}

void FLocalFileNetworkReplayStreamer::EnumerateStreams(const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const FEnumerateStreamsCallback& Delegate)
{
	EnumerateStreams_Internal(ReplayVersion, GetUserIndexFromUserString(UserString), MetaString, TArray< FString >(), Delegate);
}

void FLocalFileNetworkReplayStreamer::EnumerateStreams_Internal(const FNetworkReplayVersion& ReplayVersion, const int32 UserIndex, const FString& MetaString, const TArray< FString >& ExtraParms, const FEnumerateStreamsCallback& Delegate)
{
	AddDelegateFileRequestToQueue<FEnumerateStreamsCallback, FEnumerateStreamsResult>(EQueuedLocalFileRequestType::EnumeratingStreams, Delegate,
		[this, ReplayVersion](TLocalFileRequestCommonData<FEnumerateStreamsResult>& RequestData)
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_Enumerate);

			const FString WildCardPath = GetDemoPath() + TEXT("*.replay");

			TArray<FString> ReplayFileNames;
			IFileManager::Get().FindFiles(ReplayFileNames, *WildCardPath, true, false);

			for (const FString& ReplayFileName : ReplayFileNames)
			{
				// Read stored info for this replay
				FLocalFileReplayInfo StoredReplayInfo;
				if (!ReadReplayInfo(FPaths::GetBaseFilename(ReplayFileName), StoredReplayInfo))
				{
					continue;
				}

				// Check version. NetworkVersion and changelist of 0 will ignore version check.
				const bool bNetworkVersionMatches = ReplayVersion.NetworkVersion == StoredReplayInfo.NetworkVersion;
				const bool bChangelistMatches = ReplayVersion.Changelist == StoredReplayInfo.Changelist;

				const bool bNetworkVersionPasses = ReplayVersion.NetworkVersion == 0 || bNetworkVersionMatches;
				const bool bChangelistPasses = ReplayVersion.Changelist == 0 || bChangelistMatches;

				if (bNetworkVersionPasses && bChangelistPasses)
				{
					FNetworkReplayStreamInfo Info;

					Info.Name = FPaths::GetBaseFilename(ReplayFileName);
					Info.bIsLive = StoredReplayInfo.bIsLive;
					Info.Changelist = StoredReplayInfo.Changelist; 
					Info.LengthInMS = StoredReplayInfo.LengthInMS;
					Info.FriendlyName = StoredReplayInfo.FriendlyName;
					Info.SizeInBytes = StoredReplayInfo.TotalDataSizeInBytes;
					Info.Timestamp = StoredReplayInfo.Timestamp;

					// If we don't have a valid timestamp, assume it's the file's timestamp.
					if (Info.Timestamp == FDateTime::MinValue())
					{
						Info.Timestamp = IFileManager::Get().GetTimeStamp(*GetDemoFullFilename(Info.Name));
					}

					RequestData.DelegateResult.FoundStreams.Add(Info);
				}
			}

			RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
		});
}

void FLocalFileNetworkReplayStreamer::EnumerateRecentStreams(const FNetworkReplayVersion& ReplayVersion, const int32 UserIndex, const FEnumerateStreamsCallback& Delegate)
{
	EnumerateRecentStreams_Internal(ReplayVersion, UserIndex, Delegate);
}

void FLocalFileNetworkReplayStreamer::EnumerateRecentStreams(const FNetworkReplayVersion& ReplayVersion, const FString& RecentViewer, const FEnumerateStreamsCallback& Delegate)
{
	EnumerateRecentStreams_Internal(ReplayVersion, GetUserIndexFromUserString(RecentViewer), Delegate);
}

void FLocalFileNetworkReplayStreamer::EnumerateRecentStreams_Internal(const FNetworkReplayVersion& ReplayVersion, const int32 UserIndex, const FEnumerateStreamsCallback& Delegate)
{
	UE_LOG(LogLocalFileReplay, Log, TEXT("FLocalFileNetworkReplayStreamer::EnumerateRecentStreams is currently unsupported."));
	FEnumerateStreamsResult Result;
	Result.Result = EStreamingOperationResult::Unsupported;
	Delegate.ExecuteIfBound(Result);
}

void FLocalFileNetworkReplayStreamer::AddUserToReplay(const FString& UserString)
{
	UE_LOG(LogLocalFileReplay, Log, TEXT("FLocalFileNetworkReplayStreamer::AddUserToReplay is currently unsupported."));
}

void FLocalFileNetworkReplayStreamer::AddEvent(const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data)
{
	if (StreamerState != EStreamerState::Recording)
	{
		UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::AddEvent. Not recording."));
		return;
	}

	AddOrUpdateEvent(TEXT(""), TimeInMS, Group, Meta, Data);
}

void FLocalFileNetworkReplayStreamer::AddOrUpdateEvent(const FString& Name, const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data)
{
	if (StreamerState != EStreamerState::Recording)
	{
		UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::AddOrUpdateEvent. Not recording."));
		return;
	}

	UE_LOG(LogLocalFileReplay, Verbose, TEXT("FLocalFileNetworkReplayStreamer::AddOrUpdateEvent. Size: %i"), Data.Num());


	FString EventName = Name;

	// if name is empty, assign one
	if (EventName.IsEmpty())
	{
		EventName = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	// prefix with stream name to be consistent with http streamer
	EventName = CurrentStreamName + TEXT("_") + EventName;

	AddGenericRequestToQueue<FLocalFileReplayInfo>(EQueuedLocalFileRequestType::UpdatingEvent,
		[this, EventName, Group, TimeInMS, Meta, Data](FLocalFileReplayInfo& ReplayInfo)
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_FlushEvent);

			if (ReadReplayInfo(CurrentStreamName, ReplayInfo))
			{
				TSharedPtr<FArchive> LocalFileAr = CreateLocalFileWriter(GetDemoFullFilename(CurrentStreamName));
				if (LocalFileAr.IsValid())
				{
					int32 EventIndex = INDEX_NONE;

					// see if this event already exists
					for (int32 i=0; i < ReplayInfo.Events.Num(); ++i)
					{
						if (ReplayInfo.Events[i].Id == EventName)
						{
							EventIndex = i;
							break;
						}
					}

					// serialize event to temporary location
					FArrayWriter Writer;

					ELocalFileChunkType ChunkType = ELocalFileChunkType::Event;
					Writer << ChunkType;

					int64 SavedPos = Writer.Tell();

					int32 PlaceholderSize = 0;
					Writer << PlaceholderSize;

					int64 MetadataPos = Writer.Tell();

					FString TempName = EventName;
					Writer << TempName;

					FString Value = Group;
					Writer << Value;

					Value = Meta;
					Writer << Value;

					uint32 Time1 = TimeInMS;
					Writer << Time1;

					uint32 Time2 = TimeInMS;
					Writer << Time2;

					int32 EventSize = Data.Num();
					Writer << EventSize;

					Writer.Serialize((void*)Data.GetData(), Data.Num());

					int32 ChunkSize = Writer.Tell() - MetadataPos;

					Writer.Seek(SavedPos);
					Writer << ChunkSize;

					if (EventIndex == INDEX_NONE)
					{
						// append new event chunk
						LocalFileAr->Seek(LocalFileAr->TotalSize());
					}
					else 
					{
						if (ChunkSize > ReplayInfo.Chunks[ReplayInfo.Events[EventIndex].ChunkIndex].SizeInBytes)
						{
							LocalFileAr->Seek(ReplayInfo.Chunks[ReplayInfo.Events[EventIndex].ChunkIndex].TypeOffset);

							// clear chunk type so it will be skipped later
							ChunkType = ELocalFileChunkType::Unknown;
							*LocalFileAr << ChunkType;

							LocalFileAr->Seek(LocalFileAr->TotalSize());
						}
						else
						{
							LocalFileAr->Seek(ReplayInfo.Chunks[ReplayInfo.Events[EventIndex].ChunkIndex].TypeOffset);
						}
					}

					LocalFileAr->Serialize(Writer.GetData(), Writer.TotalSize());

					LocalFileAr = nullptr;
				}

				ReadReplayInfo(CurrentStreamName, ReplayInfo);
			}
		},
		[this](FLocalFileReplayInfo& ReplayInfo)
		{
			if (ReplayInfo.bIsValid)
			{
				const int32 TotalLengthInMS = CurrentReplayInfo.LengthInMS;
				CurrentReplayInfo = ReplayInfo;
				CurrentReplayInfo.LengthInMS = TotalLengthInMS;
			}
		});
}

void FLocalFileNetworkReplayStreamer::EnumerateEvents(const FString& ReplayName, const FString& Group, const int32 UserIndex, const FEnumerateEventsCallback& Delegate)
{
	EnumerateEvents_Internal(ReplayName, Group, UserIndex, Delegate);
}

void FLocalFileNetworkReplayStreamer::EnumerateEvents(const FString& ReplayName, const FString& Group, const FEnumerateEventsCallback& Delegate)
{
	EnumerateEvents_Internal(ReplayName, Group, INDEX_NONE, Delegate);
}

void FLocalFileNetworkReplayStreamer::EnumerateEvents(const FString& Group, const FEnumerateEventsCallback& Delegate)
{
	EnumerateEvents_Internal(CurrentStreamName, Group, INDEX_NONE, Delegate);
}

void FLocalFileNetworkReplayStreamer::EnumerateEvents_Internal(const FString& ReplayName, const FString& Group, const int32 UserIndex, const FEnumerateEventsCallback& Delegate)
{
	AddDelegateFileRequestToQueue<FEnumerateEventsCallback, FEnumerateEventsResult>(EQueuedLocalFileRequestType::EnumeratingEvents, Delegate,
		[this, ReplayName, Group](TLocalFileRequestCommonData<FEnumerateEventsResult>& RequestData)
		{
			if (!FPaths::FileExists(GetDemoFullFilename(ReplayName)))
			{
				RequestData.DelegateResult.Result = EStreamingOperationResult::ReplayNotFound;
			}
			else
			{
				// Read stored info for this replay
				FLocalFileReplayInfo StoredReplayInfo;
				if (ReadReplayInfo(ReplayName, StoredReplayInfo))
				{
					for (const FLocalFileEventInfo& EventInfo : StoredReplayInfo.Events)
					{
						if (Group.IsEmpty() || (EventInfo.Group == Group))
						{
							int Idx = RequestData.DelegateResult.ReplayEventList.ReplayEvents.AddDefaulted();

							FReplayEventListItem& Event = RequestData.DelegateResult.ReplayEventList.ReplayEvents[Idx];
							Event.ID = EventInfo.Id;
							Event.Group = EventInfo.Group;
							Event.Metadata = EventInfo.Metadata;
							Event.Time1 = EventInfo.Time1;
							Event.Time2 = EventInfo.Time2;
						}
					}

					RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
				}
			}
		});
}

void FLocalFileNetworkReplayStreamer::RequestEventData(const FString& ReplayName, const FString& EventID, const int32 UserIndex, const FRequestEventDataCallback& Delegate)
{
	RequestEventData_Internal(ReplayName, EventID, UserIndex, Delegate);
}

void FLocalFileNetworkReplayStreamer::RequestEventData(const FString& ReplayName, const FString& EventID, const FRequestEventDataCallback& Delegate)
{
	RequestEventData_Internal(ReplayName, EventID, INDEX_NONE, Delegate);
}

void FLocalFileNetworkReplayStreamer::RequestEventData(const FString& EventID, const FRequestEventDataCallback& Delegate)
{
	// Assume current stream
	FString StreamName = CurrentStreamName;

	// But look for name prefix, http streamer expects to pull details from arbitrary streams
	int32 Idx = INDEX_NONE;
	if (EventID.FindChar(TEXT('_'), Idx))
	{
		StreamName = EventID.Left(Idx);
	}

	RequestEventData_Internal(StreamName, EventID, INDEX_NONE, Delegate);
}

void FLocalFileNetworkReplayStreamer::RequestEventData_Internal(const FString& ReplayName, const FString& EventID, const int32 UserIndex, const FRequestEventDataCallback& RequestEventDataComplete)
{
	AddDelegateFileRequestToQueue<FRequestEventDataCallback, FRequestEventDataResult>(EQueuedLocalFileRequestType::RequestingEvent, RequestEventDataComplete,
		[this, ReplayName, EventID](TLocalFileRequestCommonData<FRequestEventDataResult>& RequestData)
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_ReadEvent);

			const FString FullDemoFilename = GetDemoFullFilename(ReplayName);
			if (!FPaths::FileExists(FullDemoFilename))
			{
				RequestData.DelegateResult.Result = EStreamingOperationResult::ReplayNotFound;
			}
			else
			{
				// Read stored info for this replay
				FLocalFileReplayInfo StoredReplayInfo;
				if (ReadReplayInfo(ReplayName, StoredReplayInfo))
				{
					TSharedPtr<FArchive> LocalFileAr = CreateLocalFileReader(FullDemoFilename);
					if (LocalFileAr.IsValid())
					{
						for (const FLocalFileEventInfo& EventInfo : StoredReplayInfo.Events)
						{
							if (EventInfo.Id == EventID)
							{
								LocalFileAr->Seek(EventInfo.EventDataOffset);

								RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
								RequestData.DelegateResult.ReplayEventListItem.AddUninitialized(EventInfo.SizeInBytes);

								LocalFileAr->Serialize(RequestData.DelegateResult.ReplayEventListItem.GetData(), RequestData.DelegateResult.ReplayEventListItem.Num());

								break;
							}
						}

						LocalFileAr = nullptr;
					}
				}
			}
		});
}

void FLocalFileNetworkReplayStreamer::SearchEvents(const FString& EventGroup, const FSearchEventsCallback& Delegate)
{
	UE_LOG(LogLocalFileReplay, Log, TEXT("FLocalFileNetworkReplayStreamer::SearchEvents is currently unsupported."));
	FSearchEventsResult Result;
	Result.Result = EStreamingOperationResult::Unsupported;
	Delegate.ExecuteIfBound(Result);
}

void FLocalFileNetworkReplayStreamer::KeepReplay(const FString& ReplayName, const bool bKeep, const int32 UserIndex, const FKeepReplayCallback& Delegate)
{
	KeepReplay_Internal(ReplayName, bKeep, UserIndex, Delegate);
}

void FLocalFileNetworkReplayStreamer::KeepReplay(const FString& ReplayName, const bool bKeep, const FKeepReplayCallback& Delegate)
{
	KeepReplay_Internal(ReplayName, bKeep, INDEX_NONE, Delegate);
}

void FLocalFileNetworkReplayStreamer::KeepReplay_Internal(const FString& ReplayName, const bool bKeep, const int32 UserIndex, const FKeepReplayCallback& Delegate)
{
	AddDelegateFileRequestToQueue<FKeepReplayCallback, FKeepReplayResult>(EQueuedLocalFileRequestType::KeepReplay, Delegate,
		[this, ReplayName](TLocalFileRequestCommonData<FKeepReplayResult>& RequestData)
		{
			// Replays are kept during streaming so there's no need to explicitly save them.
			// However, sanity check that what was passed in still exists.
			if (!FPaths::FileExists(GetDemoFullFilename(ReplayName)))
			{
				RequestData.DelegateResult.Result = EStreamingOperationResult::ReplayNotFound;
			}
			else
			{
				RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
				RequestData.DelegateResult.NewReplayName = ReplayName;
			}
		});
}

void FLocalFileNetworkReplayStreamer::RenameReplayFriendlyName(const FString& ReplayName, const FString& NewFriendlyName, const int32 UserIndex, const FRenameReplayCallback& Delegate)
{
	RenameReplayFriendlyName_Internal(ReplayName, NewFriendlyName, UserIndex, Delegate);
}

void FLocalFileNetworkReplayStreamer::RenameReplayFriendlyName(const FString& ReplayName, const FString& NewFriendlyName, const FRenameReplayCallback& Delegate)
{
	RenameReplayFriendlyName_Internal(ReplayName, NewFriendlyName, INDEX_NONE, Delegate);
}

void FLocalFileNetworkReplayStreamer::RenameReplayFriendlyName_Internal(const FString& ReplayName, const FString& NewFriendlyName, const int32 UserIndex, const FRenameReplayCallback& Delegate)
{
	AddDelegateFileRequestToQueue<FRenameReplayCallback, FRenameReplayResult>(EQueuedLocalFileRequestType::RenameReplayFriendlyName, Delegate,
		[this, ReplayName, NewFriendlyName](TLocalFileRequestCommonData<FRenameReplayResult>& RequestData)
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_RenameFriendly);

			const FString& FullReplayName = GetDemoFullFilename(ReplayName);
			if (!FPaths::FileExists(FullReplayName))
			{
				UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::RenameReplayFriendlyName: Replay does not exist %s"), *ReplayName);
				RequestData.DelegateResult.Result = EStreamingOperationResult::ReplayNotFound;
				return;
			}

			FLocalFileSerializationInfo SerializationInfo;
			FLocalFileReplayInfo TempReplayInfo;

			// Do this inside a scope, to make sure the file archive is closed before continuing.
			{
				TSharedPtr<FArchive> ReadAr = CreateLocalFileReader(FullReplayName);
				if (!ReadAr.IsValid() || ReadAr->TotalSize() <= 0 || !ReadReplayInfo(*ReadAr.Get(), TempReplayInfo, SerializationInfo))
				{
					UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::RenameReplayFriendlyName: Failed to read replay info %s"), *ReplayName);
					return;
				}

				if (SerializationInfo.FileVersion < LocalFileReplay::HISTORY_FIXEDSIZE_FRIENDLY_NAME)
				{
					UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::RenameReplayFriendlyName: Replay too old to rename safely %s"), *ReplayName);
					return;
				}
			}

			TempReplayInfo.FriendlyName = NewFriendlyName;

			// Do this inside a scope, to make sure the file archive is closed before continuing.
			{
				TSharedPtr<FArchive> WriteAr = CreateLocalFileWriter(FullReplayName);
				if (!WriteAr.IsValid() || WriteAr->TotalSize() <= 0 || !WriteReplayInfo(*WriteAr.Get(), TempReplayInfo, SerializationInfo))
				{
					UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::RenameReplayFriendlyName: Failed to write replay info %s"), *ReplayName);
					return;
				}
			}

			RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
		});
}

void FLocalFileNetworkReplayStreamer::RenameReplay(const FString& ReplayName, const FString& NewName, const int32 UserIndex, const FRenameReplayCallback& Delegate)
{
	RenameReplay_Internal(ReplayName, NewName, UserIndex, Delegate);
}

void FLocalFileNetworkReplayStreamer::RenameReplay(const FString& ReplayName, const FString& NewName, const FRenameReplayCallback& Delegate)
{
	RenameReplay_Internal(ReplayName, NewName, INDEX_NONE, Delegate);
}

void FLocalFileNetworkReplayStreamer::RenameReplay_Internal(const FString& ReplayName, const FString& NewName, const int32 UserIndex, const FRenameReplayCallback& Delegate)
{
	AddDelegateFileRequestToQueue<FRenameReplayCallback, FRenameReplayResult>(EQueuedLocalFileRequestType::RenameReplay, Delegate,
		[this, ReplayName, NewName](TLocalFileRequestCommonData<FRenameReplayResult>& RequestData)
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_Rename);

			const FString& FullReplayName = GetDemoFullFilename(ReplayName);
			if (!FPaths::FileExists(FullReplayName))
			{
				UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::RenameReplay: Replay does not exist (old %s new %s)"), *ReplayName, *NewName);
				RequestData.DelegateResult.Result = EStreamingOperationResult::ReplayNotFound;
				return;
			}

			const FString& NewReplayName = GetDemoFullFilename(NewName);

			FString NewReplayBaseName = FPaths::GetBaseFilename(NewReplayName);
			NewReplayBaseName.RemoveFromEnd(".replay");

			// Sanity check to make sure the input name isn't changing directories.
			if (NewName != NewReplayBaseName)
			{
				UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::RenameReplay: Path separator characters present in replay (old %s new %s)"), *ReplayName, *NewName);
				return;
			}

			if (!IFileManager::Get().Move(*NewReplayName, *FullReplayName, /* bReplace= */ false))
			{
				UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::RenameReplay: Failed to rename replay (old %s new %s)"), *ReplayName, *NewName);
				return;
			}

			RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
		});
}

FArchive* FLocalFileNetworkReplayStreamer::GetCheckpointArchive()
{
	return &CheckpointAr;
}

void FLocalFileNetworkReplayStreamer::FlushStream(const uint32 TimeInMS)
{
	check( StreamAr.IsSaving() );

	if (CurrentStreamName.IsEmpty() || IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::WriteHeader))
	{
		// If we haven't uploaded the header, or we are not recording, we don't need to flush
		UE_LOG( LogLocalFileReplay, Warning, TEXT( "FLocalFileNetworkReplayStreamer::FlushStream. Waiting on header upload." ) );
		return;
	}

	if (StreamAr.Buffer.Num() == 0)
	{
		// Nothing to flush
		return;
	}

	StreamTimeRange.Max = TimeInMS;

	const uint32 StreamChunkStartMS = StreamTimeRange.Min;
	const uint32 StreamChunkEndMS = StreamTimeRange.Max;

	StreamTimeRange.Min = StreamTimeRange.Max;

	// Save any newly streamed data to disk
	UE_LOG(LogLocalFileReplay, Log, TEXT("FLocalFileNetworkReplayStreamer::FlushStream. StreamChunkIndex: %i, Size: %i"), StreamChunkIndex, StreamAr.Buffer.Num());

	AddGenericRequestToQueue<FLocalFileReplayInfo>(EQueuedLocalFileRequestType::WritingStream, 
		[this, StreamChunkStartMS, StreamChunkEndMS, StreamData=MoveTemp(StreamAr.Buffer)](FLocalFileReplayInfo& ReplayInfo) mutable
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_FlushStream);

			TSharedPtr<FArchive> LocalFileAr = CreateLocalFileWriter(GetDemoFullFilename(CurrentStreamName));
			if (LocalFileAr.IsValid())
			{
				LocalFileAr->Seek(LocalFileAr->TotalSize());

				TArray<uint8> FinalData;

				if (SupportsCompression())
				{
					SCOPE_CYCLE_COUNTER(STAT_LocalReplay_CompressTime);

					if (!CompressBuffer(StreamData, FinalData))
					{
						UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::FlushStream - CompressBuffer failed"));
						SetLastError( ENetworkReplayError::ServiceUnavailable );
					}
				}
				else
				{
					FinalData = MoveTemp(StreamData);
				}

				// flush chunk to disk
				if (FinalData.Num() > 0)
				{
					ELocalFileChunkType ChunkType = ELocalFileChunkType::ReplayData;
					*LocalFileAr << ChunkType;

					int64 SavedPos = LocalFileAr->Tell();

					int32 PlaceholderSize = 0;
					*LocalFileAr << PlaceholderSize;

					int64 MetadataPos = LocalFileAr->Tell();

					uint32 Time1 = StreamChunkStartMS;
					*LocalFileAr << Time1;

					uint32 Time2 = StreamChunkEndMS;
					*LocalFileAr << Time2;

					int32 DataSize = FinalData.Num();
					*LocalFileAr << DataSize;

					LocalFileAr->Serialize((void*)FinalData.GetData(), FinalData.Num());

					int32 ChunkSize = LocalFileAr->Tell() - MetadataPos;

					LocalFileAr->Seek(SavedPos);
					*LocalFileAr << ChunkSize;
				}
			
				LocalFileAr = nullptr;
			}

			ReadReplayInfo(CurrentStreamName, ReplayInfo);
		},
		[this](FLocalFileReplayInfo& ReplayInfo)
		{
			if (ReplayInfo.bIsValid)
			{
				const int32 TotalLengthInMS = CurrentReplayInfo.LengthInMS;
				CurrentReplayInfo = ReplayInfo;
				CurrentReplayInfo.LengthInMS = TotalLengthInMS;
			}
		});

	StreamAr.Buffer.Empty();
	StreamAr.Pos = 0;

	// Keep track of the time range we have in our buffer, so we can accurately upload that each time we submit a chunk
	StreamTimeRange.Min = StreamTimeRange.Max;

	StreamChunkIndex++;

	LastChunkTime = FPlatformTime::Seconds();
}

void FLocalFileNetworkReplayStreamer::FlushCheckpoint(const uint32 TimeInMS)
{
	if (CheckpointAr.Buffer.Num() == 0)
	{
		UE_LOG( LogLocalFileReplay, Warning, TEXT( "FLocalFileNetworkReplayStreamer::FlushCheckpoint. Checkpoint is empty." ) );
		return;
	}

	// Flush any existing stream, we need checkpoints to line up with the next chunk
	FlushStream(TimeInMS);

	// Flush the checkpoint
	FlushCheckpointInternal(TimeInMS);
}

void FLocalFileNetworkReplayStreamer::FlushCheckpointInternal(const uint32 TimeInMS)
{
	if (CurrentStreamName.IsEmpty() || StreamerState != EStreamerState::Recording || CheckpointAr.Buffer.Num() == 0)
	{
		// If there is no active session, or we are not recording, we don't need to flush
		CheckpointAr.Buffer.Empty();
		CheckpointAr.Pos = 0;
		return;
	}

	const int32 TotalLengthInMS = CurrentReplayInfo.LengthInMS;
	const uint32 CheckpointTimeInMS = StreamTimeRange.Max;

	AddGenericRequestToQueue<FLocalFileReplayInfo>(EQueuedLocalFileRequestType::WritingCheckpoint, 
		[this, CheckpointTimeInMS, TotalLengthInMS, CheckpointData=MoveTemp(CheckpointAr.Buffer)](FLocalFileReplayInfo& ReplayInfo) mutable
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_FlushCheckpoint);

			if (ReadReplayInfo(CurrentStreamName, ReplayInfo))
			{
				int32 DataChunkIndex = ReplayInfo.DataChunks.Num();
				int32 CheckpointIndex = ReplayInfo.Checkpoints.Num();

				TSharedPtr<FArchive> LocalFileAr = CreateLocalFileWriter(GetDemoFullFilename(CurrentStreamName));
				if (LocalFileAr.IsValid())
				{
					LocalFileAr->Seek(LocalFileAr->TotalSize());

					TArray<uint8> FinalData;

					if (SupportsCompression())
					{
						SCOPE_CYCLE_COUNTER(STAT_LocalReplay_CompressTime);

						if (!CompressBuffer(CheckpointData, FinalData))
						{
							UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::FlushStream - CompressBuffer failed"));
							SetLastError( ENetworkReplayError::ServiceUnavailable );
						}
					}
					else
					{
						FinalData = MoveTemp(CheckpointData);
					}

					// flush checkpoint
					if (FinalData.Num() > 0)
					{
						ELocalFileChunkType ChunkType = ELocalFileChunkType::Checkpoint;
						*LocalFileAr << ChunkType;

						int64 SavedPos = LocalFileAr->Tell();

						int32 PlaceholderSize = 0;
						*LocalFileAr << PlaceholderSize;

						int64 MetadataPos = LocalFileAr->Tell();

						FString Id = FString::Printf(TEXT("checkpoint%ld"), CheckpointIndex);
						*LocalFileAr << Id;

						FString Group = TEXT("checkpoint");
						*LocalFileAr << Group;

						FString Metadata = FString::Printf(TEXT("%ld"), DataChunkIndex);
						*LocalFileAr << Metadata;

						uint32 Time1 = CheckpointTimeInMS;
						*LocalFileAr << Time1;

						uint32 Time2 = CheckpointTimeInMS;
						*LocalFileAr << Time2;

						int32 CheckpointSize = FinalData.Num();
						*LocalFileAr << CheckpointSize;

						LocalFileAr->Serialize((void*)FinalData.GetData(), FinalData.Num());

						int32 ChunkSize = LocalFileAr->Tell() - MetadataPos;

						LocalFileAr->Seek(SavedPos);
						*LocalFileAr << ChunkSize;
					}

					LocalFileAr = nullptr;
				}

				if (ReadReplayInfo(CurrentStreamName, ReplayInfo))
				{
					ReplayInfo.LengthInMS = TotalLengthInMS;

					WriteReplayInfo(CurrentStreamName, ReplayInfo);
				}
			}
		},
		[this](FLocalFileReplayInfo& ReplayInfo)
		{
			if (ReplayInfo.bIsValid)
			{
				int32 CurrentTotalLengthInMS = CurrentReplayInfo.LengthInMS;
				CurrentReplayInfo = ReplayInfo;
				CurrentReplayInfo.LengthInMS = CurrentTotalLengthInMS;
			}
		});

	CheckpointAr.Buffer.Empty();
	CheckpointAr.Pos = 0;	
}

void FLocalFileNetworkReplayStreamer::GotoCheckpointIndex(const int32 CheckpointIndex, const FGotoCallback& Delegate)
{
	if (IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::ReadingCheckpoint))
	{
		// If we're currently going to a checkpoint now, ignore this request
		UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::GotoCheckpointIndex. Busy processing another checkpoint."));
		Delegate.ExecuteIfBound(FGotoResult());
		return;
	}

	if (CheckpointIndex == INDEX_NONE)
	{
		AddSimpleRequestToQueue(EQueuedLocalFileRequestType::ReadingCheckpoint, 
			[]()
			{
				UE_LOG(LogLocalFileReplay, Verbose, TEXT("FLocalFileNetworkReplayStreamer::GotoCheckpointIndex"));
			},
			[this, Delegate]()
			{
				// Make sure to reset the checkpoint archive (this is how we signify that the engine should start from the beginning of the stream (we don't need a checkpoint for that))
				CheckpointAr.Buffer.Empty();
				CheckpointAr.Pos = 0;

				if (!IsDataAvailableForTimeRange(0, LastGotoTimeInMS))
				{
					// Completely reset our stream (we're going to start loading from the start of the checkpoint)
					StreamAr.Buffer.Empty();

					StreamDataOffset = 0;

					// Reset our stream range
					StreamTimeRange = TInterval<uint32>(0, 0);

					// Reset chunk index
					StreamChunkIndex = 0;

					LastChunkTime = 0;		// Force the next chunk to start loading immediately in case LastGotoTimeInMS is 0 (which would effectively disable high priority mode immediately)

					SetHighPriorityTimeRange(0, LastGotoTimeInMS);
				}

				StreamAr.Pos = 0;
				StreamAr.bAtEndOfReplay	= false;

				FGotoResult Result;
				Result.ExtraTimeMS = LastGotoTimeInMS;
				Result.Result = EStreamingOperationResult::Success;

				Delegate.ExecuteIfBound( Result );

				LastGotoTimeInMS = -1;
			});
		
		return;
	}

	if (!CurrentReplayInfo.Checkpoints.IsValidIndex(CheckpointIndex))
	{
		UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::GotoCheckpointIndex. Invalid checkpoint index."));
		Delegate.ExecuteIfBound(FGotoResult());
		return;
	}

	AddCachedFileRequestToQueue<FGotoResult>(EQueuedLocalFileRequestType::ReadingCheckpoint, CurrentReplayInfo.Checkpoints[CheckpointIndex].ChunkIndex,
		[this, CheckpointIndex](TLocalFileRequestCommonData<FGotoResult>& RequestData)
		{
			// If we get here after StopStreaming was called, then assume this operation should be cancelled
			// A more correct fix would be to actually cancel this in-flight request when StopStreaming is called
			// But for now, this is a safe change, and can co-exist with the more proper fix
			if (bStopStreamingCalled)
			{
				return;
			}

			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_ReadCheckpoint);

			RequestData.DataBuffer.Empty();

			const FString FullDemoFilename = GetDemoFullFilename(CurrentStreamName);

			TSharedPtr<FArchive> LocalFileAr = CreateLocalFileReader(FullDemoFilename);
			if (LocalFileAr.IsValid())
			{
				if (ReadReplayInfo(*LocalFileAr, RequestData.ReplayInfo))
				{
					LocalFileAr->Seek(RequestData.ReplayInfo.Checkpoints[CheckpointIndex].EventDataOffset);

					RequestData.DataBuffer.AddUninitialized(RequestData.ReplayInfo.Checkpoints[CheckpointIndex].SizeInBytes);

					LocalFileAr->Serialize(RequestData.DataBuffer.GetData(), RequestData.DataBuffer.Num());

					// Get the checkpoint data
					if (RequestData.ReplayInfo.bCompressed)
					{
						if (SupportsCompression())
						{
							SCOPE_CYCLE_COUNTER(STAT_LocalReplay_DecompressTime);

							TArray<uint8> UncompressedData;

							if (!DecompressBuffer(RequestData.DataBuffer, UncompressedData))
							{
								UE_LOG(LogLocalFileReplay, Error, TEXT("FLocalFileNetworkReplayStreamer::GotoCheckpointIndex. DecompressBuffer FAILED."));
								RequestData.DataBuffer.Empty();
								return;
							}

							RequestData.DataBuffer = MoveTemp(UncompressedData);
						}
						else
						{
							UE_LOG(LogLocalFileReplay, Error, TEXT("FLocalFileNetworkReplayStreamer::GotoCheckpointIndex. Compressed checkpoint but streamer does not support compression."));
							RequestData.DataBuffer.Empty();
							return;
						}
					}
				}

				LocalFileAr = nullptr;
			}
		},
		[this, CheckpointIndex, Delegate](TLocalFileRequestCommonData<FGotoResult>& RequestData)
		{
			if (bStopStreamingCalled)
			{
				Delegate.ExecuteIfBound(RequestData.DelegateResult);
				LastGotoTimeInMS = -1;
				return;
			}

			if (RequestData.DataBuffer.Num() == 0)
			{
				UE_LOG( LogLocalFileReplay, Warning, TEXT( "FLocalFileNetworkReplayStreamer::GotoCheckpointIndex. Checkpoint empty." ) );
				Delegate.ExecuteIfBound(RequestData.DelegateResult);
				LastGotoTimeInMS = -1;
				return;
			}

			AddRequestToCache(CurrentReplayInfo.Checkpoints[CheckpointIndex].ChunkIndex, RequestData.DataBuffer);

			CheckpointAr.Buffer = MoveTemp(RequestData.DataBuffer);
			CheckpointAr.Pos = 0;

			int32 DataChunkIndex = FCString::Atoi(*CurrentReplayInfo.Checkpoints[CheckpointIndex].Metadata);

			if (CurrentReplayInfo.DataChunks.IsValidIndex(DataChunkIndex))
			{
				bool bIsDataAvailableForTimeRange = IsDataAvailableForTimeRange(CurrentReplayInfo.Checkpoints[CheckpointIndex].Time1, LastGotoTimeInMS);

				if (!bIsDataAvailableForTimeRange)
				{
					// Completely reset our stream (we're going to start loading from the start of the checkpoint)
					StreamAr.Buffer.Empty();
					StreamAr.Pos = 0;
					StreamAr.bAtEndOfReplay = false;

					// Reset any time we were waiting on in the past
					HighPriorityEndTime = 0;

					StreamDataOffset = CurrentReplayInfo.DataChunks[DataChunkIndex].StreamOffset;

					// Reset our stream range
					StreamTimeRange = TInterval<uint32>(0, 0);

					// Set the next chunk to be right after this checkpoint (which was stored in the metadata)
					StreamChunkIndex = DataChunkIndex;

					LastChunkTime = 0;		// Force the next chunk to start loading immediately in case LastGotoTimeInMS is 0 (which would effectively disable high priority mode immediately)
				}
				else
				{
					// set stream position back to the correct location
					StreamAr.Pos = CurrentReplayInfo.DataChunks[DataChunkIndex].StreamOffset - StreamDataOffset;
					check(StreamAr.Pos >= 0 && StreamAr.Pos <= StreamAr.Buffer.Num());
					StreamAr.bAtEndOfReplay = false;
				}
			}
			else if (LastGotoTimeInMS >= 0)
			{
				UE_LOG( LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::GotoCheckpointIndex. Clamped to checkpoint: %i"), LastGotoTimeInMS );

				// If we want to fast forward past the end of a stream, clamp to the checkpoint
				StreamTimeRange = TInterval<uint32>(CurrentReplayInfo.Checkpoints[CheckpointIndex].Time1, CurrentReplayInfo.Checkpoints[CheckpointIndex].Time1);
				LastGotoTimeInMS = -1;
			}

			if ( LastGotoTimeInMS >= 0 )
			{
				// If we are fine scrubbing, make sure to wait on the part of the stream that is needed to do this in one frame
				SetHighPriorityTimeRange(CurrentReplayInfo.Checkpoints[CheckpointIndex].Time1, LastGotoTimeInMS);

				// Subtract off starting time so we pass in the leftover to the engine to fast forward through for the fine scrubbing part
				LastGotoTimeInMS -= CurrentReplayInfo.Checkpoints[CheckpointIndex].Time1;
			}

			// Notify game code of success
			RequestData.DelegateResult.Result = EStreamingOperationResult::Success;
			RequestData.DelegateResult.ExtraTimeMS = LastGotoTimeInMS;

			Delegate.ExecuteIfBound(RequestData.DelegateResult);

			UE_LOG(LogLocalFileReplay, Verbose, TEXT("FLocalFileNetworkReplayStreamer::GotoCheckpointIndex. SUCCESS. StreamChunkIndex: %i"), StreamChunkIndex);

			// Reset things
			LastGotoTimeInMS		= -1;
		});
}

void FLocalFileNetworkReplayStreamer::GotoTimeInMS(const uint32 TimeInMS, const FGotoCallback& Delegate)
{
	if (IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::ReadingCheckpoint) || LastGotoTimeInMS != -1)
	{
		// If we're processing requests, be on the safe side and cancel the scrub
		// FIXME: We can cancel the in-flight requests as well
		UE_LOG(LogLocalFileReplay, Log, TEXT("FLocalFileNetworkReplayStreamer::GotoTimeInMS. Busy processing pending requests."));
		Delegate.ExecuteIfBound( FGotoResult() );
		return;
	}

	UE_LOG(LogLocalFileReplay, Verbose, TEXT("FLocalFileNetworkReplayStreamer::GotoTimeInMS. TimeInMS: %i"), (int)TimeInMS );

	check(LastGotoTimeInMS == -1);

	int32 CheckpointIndex = -1;

	LastGotoTimeInMS = FMath::Min( TimeInMS, (uint32)CurrentReplayInfo.LengthInMS );

	if (CurrentReplayInfo.Checkpoints.Num() > 0 && TimeInMS >= CurrentReplayInfo.Checkpoints[CurrentReplayInfo.Checkpoints.Num() - 1].Time1)
	{
		// If we're after the very last checkpoint, that's the one we want
		CheckpointIndex = CurrentReplayInfo.Checkpoints.Num() - 1;
	}
	else
	{
		// Checkpoints should be sorted by time, return the checkpoint that exists right before the current time
		// For fine scrubbing, we'll fast forward the rest of the way
		// NOTE - If we're right before the very first checkpoint, we'll return -1, which is what we want when we want to start from the very beginning
		for (int32 i = 0; i < CurrentReplayInfo.Checkpoints.Num(); i++)
		{
			if (TimeInMS < CurrentReplayInfo.Checkpoints[i].Time1)
			{
				CheckpointIndex = i - 1;
				break;
			}
		}
	}

	GotoCheckpointIndex( CheckpointIndex, Delegate );
}

bool FLocalFileNetworkReplayStreamer::HasPendingFileRequests() const
{
	// If there is currently one in progress, or we have more to process, return true
	return IsFileRequestInProgress() || QueuedRequests.Num() > 0;
}

bool FLocalFileNetworkReplayStreamer::IsFileRequestInProgress() const
{
	return ActiveRequest.IsValid();
}

bool FLocalFileNetworkReplayStreamer::IsFileRequestPendingOrInProgress(const EQueuedLocalFileRequestType::Type RequestType) const
{
	for (const TSharedPtr<FQueuedLocalFileRequest, ESPMode::ThreadSafe>& Request : QueuedRequests)
	{
		if (Request->GetRequestType() == RequestType)
		{
			return true;
		}
	}

	if (ActiveRequest.IsValid())
	{
		if (ActiveRequest->GetRequestType() == RequestType)
		{
			return true;
		}
	}

	return false;
}


bool FLocalFileNetworkReplayStreamer::ProcessNextFileRequest()
{
	if (IsFileRequestInProgress())
	{
		return false;
	}

	if (QueuedRequests.Num() > 0)
	{
		TSharedPtr<FQueuedLocalFileRequest, ESPMode::ThreadSafe> QueuedRequest = QueuedRequests[0];
		QueuedRequests.RemoveAt( 0 );

		UE_LOG(LogLocalFileReplay, Verbose, TEXT("FLocalFileNetworkReplayStreamer::ProcessNextFileRequest. Dequeue Type: %s"), EQueuedLocalFileRequestType::ToString(QueuedRequest->GetRequestType()));

		check( !ActiveRequest.IsValid() );

		ActiveRequest = QueuedRequest;

		if (ActiveRequest->GetCachedRequest())
		{
			ActiveRequest->FinishRequest();

			return ProcessNextFileRequest();
		}
		else
		{
			ActiveRequest->IssueRequest();
		}

		return true;
	}

	return false;
}

void FLocalFileNetworkReplayStreamer::Tick(float DeltaSeconds)
{
	// Attempt to process the next file request
	if (ProcessNextFileRequest())
	{
		check(IsFileRequestInProgress());
	}

	if (bStopStreamingCalled)
	{
		return;
	}

	if (StreamerState == EStreamerState::Recording)
	{
		ConditionallyFlushStream();
	}
	else if (StreamerState == EStreamerState::Playback)
	{
		if (IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::StartPlayback))
		{
			// If we're still waiting on finalizing the start request then return
			return;
		}

		// Check to see if we're done loading the high priority portion of the stream
		// If so, we can cancel the request
		if (HighPriorityEndTime > 0 && StreamTimeRange.Contains(HighPriorityEndTime))
		{
			HighPriorityEndTime = 0;
		}

		// Check to see if we're at the end of non live streams
		if (StreamChunkIndex >= CurrentReplayInfo.DataChunks.Num() && !CurrentReplayInfo.bIsLive)
		{
			// Make note of when we reach the end of non live stream
			StreamAr.bAtEndOfReplay = true;
		}

		ConditionallyLoadNextChunk();
		ConditionallyRefreshReplayInfo();
	}
}

const TArray<uint8>& FLocalFileNetworkReplayStreamer::GetCachedFileContents(const FString& Filename) const
{
	TArray<uint8>& Data = FileContentsCache.FindOrAdd(Filename);
	if (Data.Num() == 0)
	{
		// Read the whole file into memory
		FArchive* Ar = IFileManager::Get().CreateFileReader(*Filename, FILEREAD_AllowWrite);
		if (Ar)
		{
			Data.AddUninitialized(Ar->TotalSize());
			Ar->Serialize(Data.GetData(), Data.Num());
			delete Ar;
		}
	}

	return Data;
}

TSharedPtr<FArchive> FLocalFileNetworkReplayStreamer::CreateLocalFileReader(const FString& InFilename) const
{
	if (bCacheFileReadsInMemory)
	{
		const TArray<uint8>& Data = GetCachedFileContents(InFilename);
		return (Data.Num() > 0) ? MakeShareable(new FLargeMemoryReader((uint8*)Data.GetData(), Data.Num())) : nullptr;
	}
	else
	{
		return MakeShareable(IFileManager::Get().CreateFileReader(*InFilename, FILEREAD_AllowWrite));
	}
}

TSharedPtr<FArchive> FLocalFileNetworkReplayStreamer::CreateLocalFileWriter(const FString& InFilename) const
{
	return MakeShareable(IFileManager::Get().CreateFileWriter(*InFilename, FILEWRITE_Append | FILEWRITE_AllowRead));
}

TSharedPtr<FArchive> FLocalFileNetworkReplayStreamer::CreateLocalFileWriterForOverwrite(const FString& InFilename) const
{
	return MakeShareable(IFileManager::Get().CreateFileWriter(*InFilename, FILEWRITE_AllowRead));
}

FString FLocalFileNetworkReplayStreamer::GetDemoPath() const
{
	return DemoSavePath;
}

FString FLocalFileNetworkReplayStreamer::GetDemoFullFilename(const FString& StreamName) const
{
	if (FPaths::IsRelative(StreamName))
	{
		// Treat relative paths as demo stream names.
		return FPaths::Combine(*GetDemoPath(), *StreamName) + TEXT(".replay");
	}
	else
	{
		// Return absolute paths without modification.
		return StreamName;
	}
}

FString FLocalFileNetworkReplayStreamer::GetAutomaticDemoName() const
{
	SCOPE_CYCLE_COUNTER(STAT_LocalReplay_AutomaticName);

	const int32 MaxDemos = FNetworkReplayStreaming::GetMaxNumberOfAutomaticReplays();
	const bool bUnlimitedDemos = (MaxDemos <= 0);
	const bool bUseDatePostfix = FNetworkReplayStreaming::UseDateTimeAsAutomaticReplayPostfix();
	const FString AutoPrefix = FNetworkReplayStreaming::GetAutomaticReplayPrefix();

	IFileManager& FileManager = IFileManager::Get();

	if (bUseDatePostfix)
	{
		if (!bUnlimitedDemos)
		{
			const FString WildCardPath = GetDemoFullFilename(AutoPrefix + FString(TEXT("*")));

			TArray<FString> FoundAutoReplays;
			FileManager.FindFiles(FoundAutoReplays, *WildCardPath, /* bFiles= */ true, /* bDirectories= */ false);

			if (FoundAutoReplays.Num() >= MaxDemos)
			{
				const FString* OldestReplay = nullptr;
				FDateTime OldestReplayTimestamp = FDateTime::MaxValue();

				for (FString& AutoReplay : FoundAutoReplays)
				{
					// Convert the replay name to a full path, making sure to remove the extra .replay postfix
					// that GetDemoFullFilename will add.
					AutoReplay = GetDemoFullFilename(AutoReplay);
					AutoReplay.RemoveFromEnd(TEXT(".replay"));

					FDateTime Timestamp = FileManager.GetTimeStamp(*AutoReplay);
					if (Timestamp < OldestReplayTimestamp)
					{
						OldestReplay = &AutoReplay;
						OldestReplayTimestamp = Timestamp;
					}
				}

				check(OldestReplay != nullptr)

				// Return an empty string to indicate failure.
				if (!ensureMsgf(FileManager.Delete(**OldestReplay, /*bRequireExists=*/ true, /*bEvenIfReadOnly=*/ true), TEXT("FLocalFileNetworkReplayStreamer::GetAutomaticDemoName: Failed to delete old replay %s"), **OldestReplay))
				{
					// TODO: Maybe consider sorting the list of replays, and iterating them.
					//			This would take more time, but may be more robust.
					//			For example, we could delete multiple files to get below the budget.
					//			The current behavior should be sufficient, though, as failure to overwrite
					//			a file would result in similar issues in the indexed case (e.g., no replay would be saved).
					return FString();
				}
			}
		}
		

		return AutoPrefix + FDateTime::Now().ToString();
	}
	else
	{
		FString FinalDemoName;
		FDateTime BestDateTime = FDateTime::MaxValue();

		int32 i = 1;
		while (bUnlimitedDemos || i <= MaxDemos)
		{
			const FString DemoName = FString::Printf(TEXT("%s%i"), *AutoPrefix, i);
			const FString FullDemoName = GetDemoFullFilename(DemoName);

			FDateTime DateTime = FileManager.GetTimeStamp(*FullDemoName);
			if (DateTime == FDateTime::MinValue())
			{
				// If we don't find this file, we can early out now
				FinalDemoName = DemoName;
				break;
			}
			else if (!bUnlimitedDemos && DateTime < BestDateTime)
			{
				// Use the oldest file
				FinalDemoName = DemoName;
				BestDateTime = DateTime;
			}

			++i;
		}

		return FinalDemoName;
	}
}

const FString& FLocalFileNetworkReplayStreamer::GetDefaultDemoSavePath()
{
	static const FString DefaultDemoSavePath = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("Demos/"));
	return DefaultDemoSavePath;
}

uint32 FLocalFileNetworkReplayStreamer::GetMaxFriendlyNameSize() const
{
	return LocalFileReplay::MaxFriendlyNameLen;
}

void FLocalFileNetworkReplayStreamer::DownloadHeader(const FDownloadHeaderCallback& Delegate)
{
	if (CurrentReplayInfo.bIsValid && CurrentReplayInfo.Chunks.IsValidIndex(CurrentReplayInfo.HeaderChunkIndex))
	{
		const FLocalFileChunkInfo& ChunkInfo = CurrentReplayInfo.Chunks[CurrentReplayInfo.HeaderChunkIndex];

		int64 HeaderOffset = ChunkInfo.DataOffset;
		int32 HeaderSize = ChunkInfo.SizeInBytes;

		AddDelegateFileRequestToQueue<FDownloadHeaderResult>(EQueuedLocalFileRequestType::ReadingHeader,
			[this, HeaderSize, HeaderOffset](TLocalFileRequestCommonData<FDownloadHeaderResult>& RequestData)
			{
				SCOPE_CYCLE_COUNTER(STAT_LocalReplay_ReadHeader);

				TArray<uint8>& HeaderData = RequestData.DataBuffer;

				const FString FullDemoFilename = GetDemoFullFilename(CurrentStreamName);

				TSharedPtr<FArchive> LocalFileAr = CreateLocalFileReader(FullDemoFilename);
				if (LocalFileAr.IsValid())
				{
					HeaderData.AddUninitialized(HeaderSize);

					LocalFileAr->Seek(HeaderOffset);
					LocalFileAr->Serialize((void*)HeaderData.GetData(), HeaderData.Num());

					RequestData.DelegateResult.Result = EStreamingOperationResult::Success;

					LocalFileAr = nullptr;
				}
			},
			[this, Delegate](TLocalFileRequestCommonData<FDownloadHeaderResult>& RequestData)
			{
				HeaderAr.Buffer = MoveTemp(RequestData.DataBuffer);
				HeaderAr.Pos = 0;

				Delegate.ExecuteIfBound(RequestData.DelegateResult);
			});
	}
	else
	{
		Delegate.ExecuteIfBound(FDownloadHeaderResult());
	}
}

void FLocalFileNetworkReplayStreamer::WriteHeader()
{
	check(StreamAr.IsSaving());

	if (CurrentStreamName.IsEmpty())
	{
		// IF there is no active session, or we are not recording, we don't need to flush
		UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::WriteHeader. No session name!"));
		return;
	}

	if (HeaderAr.Buffer.Num() == 0)
	{
		// Header wasn't serialized
		UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::WriteHeader. No header to upload"));
		return;
	}

	if (!IsStreaming())
	{
		UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamer::WriteHeader. Not currently streaming"));
		return;
	}

	int32 HeaderChunkIndex = CurrentReplayInfo.HeaderChunkIndex;
	int64 HeaderTypeOffset = (HeaderChunkIndex != INDEX_NONE) ? CurrentReplayInfo.Chunks[HeaderChunkIndex].TypeOffset : 0;
	int32 HeaderSize = (HeaderChunkIndex != INDEX_NONE) ? CurrentReplayInfo.Chunks[HeaderChunkIndex].SizeInBytes : 0;

	AddGenericRequestToQueue<FLocalFileReplayInfo>(EQueuedLocalFileRequestType::WritingHeader, 
		[this, HeaderChunkIndex, HeaderTypeOffset, HeaderSize, HeaderData=MoveTemp(HeaderAr.Buffer)](FLocalFileReplayInfo& ReplayInfo)
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_FlushHeader);

			TSharedPtr<FArchive> LocalFileAr = CreateLocalFileWriter(GetDemoFullFilename(CurrentStreamName));
			if (LocalFileAr.IsValid())
			{
				if (HeaderChunkIndex == INDEX_NONE)
				{
					// not expecting an existing header on disk, so check for it having been written by another process/client
					FLocalFileReplayInfo TestInfo;
					if (ReadReplayInfo(CurrentStreamName, TestInfo))
					{
						UE_LOG(LogLocalFileReplay, Error, TEXT("FLocalFileNetworkReplayStreamer::WriteHeader - Current file already has unexpected header"));
						SetLastError( ENetworkReplayError::ServiceUnavailable );
						return;
					}

					// append new chunk
					LocalFileAr->Seek(LocalFileAr->TotalSize());
				}
				else 
				{
					if (HeaderData.Num() > HeaderSize)
					{
						LocalFileAr->Seek(HeaderTypeOffset);

						// clear chunk type so it will be skipped later
						ELocalFileChunkType ChunkType = ELocalFileChunkType::Unknown;
						*LocalFileAr << ChunkType;

						LocalFileAr->Seek(LocalFileAr->TotalSize());
					}
					else
					{
						LocalFileAr->Seek(HeaderTypeOffset);
					}
				}

				ELocalFileChunkType ChunkType = ELocalFileChunkType::Header;
				*LocalFileAr << ChunkType;

				int32 ChunkSize = HeaderData.Num();
				*LocalFileAr << ChunkSize;

				LocalFileAr->Serialize((void*)HeaderData.GetData(), HeaderData.Num());

				LocalFileAr = nullptr;
			}

			ReadReplayInfo(CurrentStreamName, ReplayInfo);
		},
		[this](FLocalFileReplayInfo& ReplayInfo)
		{
			if (ReplayInfo.bIsValid)
			{
				int32 TotalLengthInMS = CurrentReplayInfo.LengthInMS;
				CurrentReplayInfo = ReplayInfo;
				CurrentReplayInfo.LengthInMS = TotalLengthInMS;
			}
		});

	// We're done with the header archive
	HeaderAr.Buffer.Empty();
	HeaderAr.Pos = 0;

	LastChunkTime = FPlatformTime::Seconds();
}

void FLocalFileNetworkReplayStreamer::RefreshHeader()
{
	AddSimpleRequestToQueue(EQueuedLocalFileRequestType::WriteHeader, 
		[]()
		{
			UE_LOG(LogLocalFileReplay, Verbose, TEXT("FLocalFileNetworkReplayStreamer::RefreshHeader"));
		},
		[this]()
		{
			WriteHeader();
		});
}

void FLocalFileNetworkReplayStreamer::SetHighPriorityTimeRange(const uint32 StartTimeInMS, const uint32 EndTimeInMS)
{
	HighPriorityEndTime = EndTimeInMS;
}

bool FLocalFileNetworkReplayStreamer::IsDataAvailableForTimeRange(const uint32 StartTimeInMS, const uint32 EndTimeInMS)
{
	if (GetLastError() != ENetworkReplayError::None)
	{
		return false;
	}

	// If the time is within the stream range we have loaded, we will return true
	return (StreamTimeRange.Contains(StartTimeInMS) && StreamTimeRange.Contains(EndTimeInMS));
}

bool FLocalFileNetworkReplayStreamer::IsLoadingCheckpoint() const
{
	return IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::ReadingCheckpoint);
}

void FLocalFileNetworkReplayStreamer::OnFileRequestComplete(const TSharedPtr<FQueuedLocalFileRequest, ESPMode::ThreadSafe>& Request)
{
	if (Request.IsValid() && ActiveRequest.IsValid())
	{
		UE_LOG(LogLocalFileReplay, Verbose, TEXT("FLocalFileNetworkReplayStreamer::OnFileRequestComplete. Type: %s"), EQueuedLocalFileRequestType::ToString(Request->GetRequestType()));

		ActiveRequest = nullptr;
	}
}

bool FLocalFileNetworkReplayStreamer::IsStreaming() const
{
	return (StreamerState != EStreamerState::Idle);
}

void FLocalFileNetworkReplayStreamer::ConditionallyFlushStream()
{
	if ( IsFileRequestInProgress() || HasPendingFileRequests() )
	{
		return;
	}

	const float FLUSH_TIME_IN_SECONDS = LocalFileReplay::CVarChunkUploadDelayInSeconds.GetValueOnGameThread();

	if ( FPlatformTime::Seconds() - LastChunkTime > FLUSH_TIME_IN_SECONDS )
	{
		FlushStream(CurrentReplayInfo.LengthInMS);
	}
};

void FLocalFileNetworkReplayStreamer::ConditionallyLoadNextChunk()
{
	if (IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::ReadingCheckpoint))
	{
		// Don't load a stream chunk while we're waiting for a checkpoint to load
		return;
	}

	if (IsFileRequestPendingOrInProgress(EQueuedLocalFileRequestType::ReadingStream))
	{
		// Only load one chunk at a time
		return;
	}

	const bool bMoreChunksDefinitelyAvailable = CurrentReplayInfo.DataChunks.IsValidIndex(StreamChunkIndex);	// We know for a fact there are more chunks available
	if (!bMoreChunksDefinitelyAvailable)
	{
		// don't read if no more chunks available, ConditionallyRefreshReplayInfo will refresh that data for us if bIsLive
		return;
	}

	// Determine if it's time to load the next chunk
	const bool bHighPriorityMode		= (HighPriorityEndTime > 0 && StreamTimeRange.Max < HighPriorityEndTime);			// We're within the high priority time range
	const bool bReallyNeedToLoadChunk	= bHighPriorityMode && bMoreChunksDefinitelyAvailable;

	// If it's not critical to load the next chunk (i.e. we're not scrubbing or at the end already), then check to see if we should grab the next chunk
	if (!bReallyNeedToLoadChunk)
	{
		const double MIN_WAIT_FOR_NEXT_CHUNK_IN_SECONDS = 3;

		const double LoadElapsedTime = FPlatformTime::Seconds() - LastChunkTime;

		if (LoadElapsedTime < MIN_WAIT_FOR_NEXT_CHUNK_IN_SECONDS)
		{
			return;		// Unless it's critical (i.e. bReallyNeedToLoadChunk is true), never try faster than MIN_WAIT_FOR_NEXT_CHUNK_IN_SECONDS
		}

		if ((StreamTimeRange.Max > StreamTimeRange.Min) && (StreamAr.Buffer.Num() > 0))
		{
			// Make a guess on how far we're in
			const float PercentIn		= StreamAr.Buffer.Num() > 0 ? ( float )StreamAr.Pos / ( float )StreamAr.Buffer.Num() : 0.0f;
			const float TotalStreamTime	= ( float )( StreamTimeRange.Size() ) / 1000.0f;
			const float CurrentTime		= TotalStreamTime * PercentIn;
			const float TimeLeft		= TotalStreamTime - CurrentTime;

			// Determine if we have enough buffer to stop streaming for now
			const float MAX_BUFFERED_TIME = LocalFileReplay::CVarChunkUploadDelayInSeconds.GetValueOnAnyThread() * 0.5f;

			if (TimeLeft > MAX_BUFFERED_TIME)
			{
				// Don't stream ahead by more than MAX_BUFFERED_TIME seconds
				UE_LOG(LogLocalFileReplay, VeryVerbose, TEXT("ConditionallyLoadNextChunk. Cancelling due buffer being large enough. TotalStreamTime: %2.2f, PercentIn: %2.2f, TimeLeft: %2.2f"), TotalStreamTime, PercentIn, TimeLeft);
				return;
			}
		}
	}

	UE_LOG(LogLocalFileReplay, Log, TEXT("FLocalFileNetworkReplayStreamer::ConditionallyLoadNextChunk. Index: %d"), StreamChunkIndex);

	int32 RequestedStreamChunkIndex = StreamChunkIndex;

	AddCachedFileRequestToQueue<FStreamingResultBase>(EQueuedLocalFileRequestType::ReadingStream, CurrentReplayInfo.DataChunks[StreamChunkIndex].ChunkIndex,
		[this, RequestedStreamChunkIndex](TLocalFileRequestCommonData<FStreamingResultBase>& RequestData)
		{
			SCOPE_CYCLE_COUNTER(STAT_LocalReplay_ReadStream);

			if (ReadReplayInfo(CurrentStreamName, RequestData.ReplayInfo))
			{
				check(RequestData.ReplayInfo.DataChunks.IsValidIndex(RequestedStreamChunkIndex));

				RequestData.DataBuffer.Empty();
			
				const FString FullDemoFilename = GetDemoFullFilename(CurrentStreamName);
		
				TSharedPtr<FArchive> LocalFileAr = CreateLocalFileReader(FullDemoFilename);
				if (LocalFileAr.IsValid())
				{
					LocalFileAr->Seek(RequestData.ReplayInfo.DataChunks[RequestedStreamChunkIndex].ReplayDataOffset);

					RequestData.DataBuffer.AddUninitialized(RequestData.ReplayInfo.DataChunks[RequestedStreamChunkIndex].SizeInBytes);

					LocalFileAr->Serialize(RequestData.DataBuffer.GetData(), RequestData.DataBuffer.Num());

					if (RequestData.ReplayInfo.bCompressed)
					{
						if (SupportsCompression())
						{
							SCOPE_CYCLE_COUNTER(STAT_LocalReplay_DecompressTime);

							TArray<uint8> UncompressedData;
							if (DecompressBuffer(RequestData.DataBuffer, UncompressedData))
							{
								RequestData.DataBuffer = MoveTemp(UncompressedData);
							}
							else
							{
								RequestData.DataBuffer.Empty();
								return;
							}
						}
						else
						{
							RequestData.DataBuffer.Empty();
							return;
						}
					}

					LocalFileAr = nullptr;
				}
			}
		},
		[this, RequestedStreamChunkIndex](TLocalFileRequestCommonData<FStreamingResultBase>& RequestData)
		{
			// Make sure our stream chunk index didn't change under our feet
			if (RequestedStreamChunkIndex != StreamChunkIndex)
			{
				StreamAr.Buffer.Empty();
				StreamAr.Pos = 0;
				SetLastError( ENetworkReplayError::ServiceUnavailable );
				return;
			}

			if (RequestData.DataBuffer.Num() > 0)
			{
				if (StreamAr.Buffer.Num() == 0)
				{
					StreamTimeRange.Min = CurrentReplayInfo.DataChunks[RequestedStreamChunkIndex].Time1;
				}

				// This is the new end of the stream
				StreamTimeRange.Max = CurrentReplayInfo.DataChunks[RequestedStreamChunkIndex].Time2;

				check(StreamTimeRange.IsValid());

				AddRequestToCache(CurrentReplayInfo.DataChunks[RequestedStreamChunkIndex].ChunkIndex, RequestData.DataBuffer);

				StreamAr.Buffer.Append(RequestData.DataBuffer);

				int32 MaxBufferedChunks = LocalFileReplay::CVarMaxBufferedStreamChunks.GetValueOnAnyThread();
				if (MaxBufferedChunks > 0)
				{
					int32 MinChunkIndex = FMath::Max(0, (RequestedStreamChunkIndex + 1) - MaxBufferedChunks);
					if (MinChunkIndex > 0)
					{
						int32 TrimBytes = CurrentReplayInfo.DataChunks[MinChunkIndex].StreamOffset - StreamDataOffset;
						if (TrimBytes > 0)
						{
							// can't remove chunks if we're actively seeking within that data
							if (StreamAr.Pos >= TrimBytes)
							{
								StreamAr.Buffer.RemoveAt(0, TrimBytes);
								StreamAr.Pos -= TrimBytes;

								StreamTimeRange.Min = CurrentReplayInfo.DataChunks[MinChunkIndex].Time1;
								StreamDataOffset += TrimBytes;

								check(StreamTimeRange.IsValid());
							}
						}
					}
				}				

				StreamChunkIndex++;
			}
			else if (HighPriorityEndTime != 0)
			{
				// We failed to load live content during fast forward
				HighPriorityEndTime = 0;
			}
		});

	LastChunkTime = FPlatformTime::Seconds();
}

void FLocalFileNetworkReplayStreamer::ConditionallyRefreshReplayInfo()
{
	if (IsFileRequestInProgress() || HasPendingFileRequests())
	{
		return;
	}

	if (CurrentReplayInfo.bIsLive)
	{
		const double REFRESH_REPLAYINFO_IN_SECONDS = 10.0;

		if (FPlatformTime::Seconds() - LastRefreshTime > REFRESH_REPLAYINFO_IN_SECONDS)
		{
			int64 LastDataSize = CurrentReplayInfo.TotalDataSizeInBytes;
			const FString FullDemoFilename = GetDemoFullFilename(CurrentStreamName);

			AddGenericRequestToQueue<FLocalFileReplayInfo>(EQueuedLocalFileRequestType::RefreshingLiveStream, 
				[this](FLocalFileReplayInfo& ReplayInfo)
				{
					ReadReplayInfo(CurrentStreamName, ReplayInfo);
				},
				[this, LastDataSize](FLocalFileReplayInfo& ReplayInfo)
				{
					if (ReplayInfo.bIsValid && (ReplayInfo.TotalDataSizeInBytes != LastDataSize))
					{
						CurrentReplayInfo = ReplayInfo;
					}
				});

			LastRefreshTime = FPlatformTime::Seconds();
		}
	}
}

void FLocalFileNetworkReplayStreamer::AddRequestToCache(int32 ChunkIndex, const TArray<uint8>& RequestData)
{
	if (!CurrentReplayInfo.bIsValid)
	{
		return;
	}

	if (!CurrentReplayInfo.Chunks.IsValidIndex(ChunkIndex))
	{
		return;
	}

	if (RequestData.Num() == 0)
	{
		return;
	}

	// Add to cache (or freshen existing entry)
	RequestCache.Add(ChunkIndex, FCachedFileRequest(RequestData, FPlatformTime::Seconds()));

	// Anytime we add something to cache, make sure it's within budget
	CleanupRequestCache();
}

void FLocalFileNetworkReplayStreamer::CleanupRequestCache()
{
	// Remove older entries until we're under the CVarMaxCacheSize threshold
	while (RequestCache.Num())
	{
		double OldestTime = 0.0;
		int32 OldestKey = INDEX_NONE;
		uint32 TotalSize = 0;

		for (auto It = RequestCache.CreateIterator(); It; ++It)
		{
			if ((OldestKey == INDEX_NONE) || It.Value().LastAccessTime < OldestTime)
			{
				OldestTime = It.Value().LastAccessTime;
				OldestKey = It.Key();
			}

			// Accumulate total cache size
			TotalSize += It.Value().RequestData.Num();
		}

		check(OldestKey != INDEX_NONE);

		const uint32 MaxCacheSize = LocalFileReplay::CVarMaxCacheSize.GetValueOnAnyThread();

		if (TotalSize <= MaxCacheSize)
		{
			break;	// We're good
		}

		RequestCache.Remove(OldestKey);
	}
}

void FQueuedLocalFileRequest::CancelRequest()
{
	bCancelled = true;
}

void FGenericQueuedLocalFileRequest::IssueRequest()
{
	auto SharedRef = AsShared();

	TGraphTask<TLocalFileAsyncGraphTask<void>>::CreateTask().ConstructAndDispatchWhenReady(
		[SharedRef]()
		{
			SharedRef->RequestFunction();
		},
		TPromise<void>([SharedRef]() 
		{
			if (!SharedRef->bCancelled)
			{
				AsyncTask(ENamedThreads::GameThread, [SharedRef]()
				{
					SharedRef->FinishRequest();
				});
			}
		})
	);
}

void FGenericQueuedLocalFileRequest::FinishRequest()
{
	if (CompletionCallback)
	{
		CompletionCallback();
	}

	if (!bCancelled && Streamer.IsValid())
	{
		Streamer->OnFileRequestComplete(AsShared());
	}
}

const FString FLocalFileNetworkReplayStreamer::GetUserStringFromUserIndex(const int32 UserIndex)
{
	if (UserIndex != INDEX_NONE && GEngine != nullptr)
	{
		if (UWorld* World = GWorld.GetReference())
		{
			if (ULocalPlayer const * const LocalPlayer = GEngine->GetLocalPlayerFromControllerId(World, UserIndex))
			{
				return LocalPlayer->GetPreferredUniqueNetId().ToString();
			}
		}
	}

	return FString();
}

const void FLocalFileNetworkReplayStreamer::GetUserStringsFromUserIndices(const TArray<int32>& UserIndices, TArray<FString>& OutUserStrings)
{
	if (GEngine != nullptr)
	{
		if (UserIndices.Num() == 1)
		{
			OutUserStrings.Emplace(GetUserStringFromUserIndex(UserIndices[0]));
		}
		else if (UserIndices.Num() > 1)
		{
			if (UWorld* World = GWorld.GetReference())
			{
				TMap<int32, FString> IdToString;
				for (auto ConstIt = GEngine->GetLocalPlayerIterator(World); ConstIt; ++ConstIt)
				{
					if (ULocalPlayer const * const LocalPlayer = *ConstIt)
					{
						IdToString.Emplace(LocalPlayer->GetControllerId(), LocalPlayer->GetPreferredUniqueNetId().ToString());
					}
				}

				for (const int32 UserIndex : UserIndices)
				{
					if (FString const * const UserString = IdToString.Find(UserIndex))
					{
						OutUserStrings.Emplace(*UserString);
					}
				}
			}
		}
	}
}

const int32 FLocalFileNetworkReplayStreamer::GetUserIndexFromUserString(const FString& UserString)
{
	if (!UserString.IsEmpty() && GEngine != nullptr)
	{
		if (UWorld* World = GWorld.GetReference())
		{
			for (auto ConstIt = GEngine->GetLocalPlayerIterator(World); ConstIt; ++ConstIt)
			{
				if (ULocalPlayer const * const LocalPlayer = *ConstIt)
				{
					if (UserString.Equals(LocalPlayer->GetPreferredUniqueNetId().ToString()))
					{
						return LocalPlayer->GetControllerId();
					}
				}
			}
		}
	}

	return INDEX_NONE;
}

const void FLocalFileNetworkReplayStreamer::GetUserIndicesFromUserStrings(const TArray<FString>& UserStrings, TArray<int32>& OutUserIndices)
{
	if (GEngine != nullptr)
	{
		if (UserStrings.Num() == 1)
		{
			OutUserIndices.Emplace(GetUserIndexFromUserString(UserStrings[0]));
		}
		else if (UserStrings.Num() > 1)
		{
			if (UWorld* World = GWorld.GetReference())
			{
				TMap<FString, int32> StringToId;
				for (auto ConstIt = GEngine->GetLocalPlayerIterator(World); ConstIt; ++ConstIt)
				{
					if (ULocalPlayer const * const LocalPlayer = *ConstIt)
					{
						StringToId.Emplace(LocalPlayer->GetPreferredUniqueNetId().ToString(), LocalPlayer->GetControllerId());
					}
				}

				for (const FString& UserString : UserStrings)
				{
					if (int32 const * const UserIndex = StringToId.Find(UserString))
					{
						OutUserIndices.Emplace(*UserIndex);
					}
				}
			}
		}
	}
}

IMPLEMENT_MODULE(FLocalFileNetworkReplayStreamingFactory, LocalFileNetworkReplayStreaming)

TSharedPtr<INetworkReplayStreamer> FLocalFileNetworkReplayStreamingFactory::CreateReplayStreamer() 
{
	TSharedPtr<FLocalFileNetworkReplayStreamer> Streamer = MakeShared<FLocalFileNetworkReplayStreamer>();
	LocalFileStreamers.Add(Streamer);
	return Streamer;
}

void FLocalFileNetworkReplayStreamingFactory::Tick( float DeltaTime )
{
	for (int i = LocalFileStreamers.Num() - 1; i >= 0; i--)
	{
		check(LocalFileStreamers[i].IsValid());

		LocalFileStreamers[i]->Tick(DeltaTime);

		// We can release our hold when streaming is completely done
		if (LocalFileStreamers[i].IsUnique() && !LocalFileStreamers[i]->HasPendingFileRequests())
		{
			if (LocalFileStreamers[i]->IsStreaming())
			{
				UE_LOG(LogLocalFileReplay, Warning, TEXT("FLocalFileNetworkReplayStreamingFactory::Tick. Stream was stopped early."));
			}

			LocalFileStreamers.RemoveAt(i);
		}
	}
}

TStatId FLocalFileNetworkReplayStreamingFactory::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FLocalFileNetworkReplayStreamingFactory, STATGROUP_Tickables);
}