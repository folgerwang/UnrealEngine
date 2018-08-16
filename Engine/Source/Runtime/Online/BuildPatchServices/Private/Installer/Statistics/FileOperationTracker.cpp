// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Installer/Statistics/FileOperationTracker.h"
#include "Templates/Tuple.h"
#include "Containers/Union.h"
#include "Containers/Queue.h"
#include "Core/AsyncHelpers.h"
#include "Common/StatsCollector.h"
#include "BuildPatchManifest.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFileOperationTracker, Warning, All);
DEFINE_LOG_CATEGORY(LogFileOperationTracker);

namespace BuildPatchServices
{
	typedef TTuple<FGuid, EFileOperationState> FDataState;
	typedef TTuple<FString, EFileOperationState> FFileState;
	typedef TTuple<FString, FByteRange, EFileOperationState> FFileByteRangeState;
	typedef TUnion<FDataState, FFileState, FFileByteRangeState> FUpdateMessage;

	class FFileOperationTracker
		: public IFileOperationTracker
	{
	public:
		FFileOperationTracker(FTicker& Ticker, FBuildPatchAppManifest* Manifest);
		~FFileOperationTracker();

	public:
		// IFileOperationTracker interface begin.
		virtual const TArray<FFileOperation>& GetStates() const override;
		virtual void OnDataStateUpdate(const FGuid& DataId, EFileOperationState State) override;
		virtual void OnDataStateUpdate(const TSet<FGuid>& DataIds, EFileOperationState State) override;
		virtual void OnDataStateUpdate(const TArray<FGuid>& DataIds, EFileOperationState State) override;
		virtual void OnFileStateUpdate(const FString& Filename, EFileOperationState State) override;
		virtual void OnFileStateUpdate(const TSet<FString>& Filenames, EFileOperationState State) override;
		virtual void OnFileStateUpdate(const TArray<FString>& Filenames, EFileOperationState State) override;
		virtual void OnFileByteRangeStateUpdate(const FString& Filename, FByteRange ByteRange, EFileOperationState State) override;
		// IFileOperationTracker interface end.

		void ProcessMessage(const FDataState& Message);
		void ProcessMessage(const FFileState& Message);
		void ProcessMessage(const FFileByteRangeState& Message);

	private:
		bool Tick(float Delta);

	private:
		FTicker& Ticker;
		FBuildPatchAppManifest* Manifest;
		FDelegateHandle TickerHandle;
		TArray<FFileOperation> FileOperationStates;
		TArray<FFileOperation> DummyOperationStates;
		TMap<FGuid, TArray<FFileOperation*>> FileOperationStatesDataIdLookup;
		TMap<FString, TArray<FFileOperation*>> FileOperationStatesFilenameLookup;
		TQueue<FUpdateMessage, EQueueMode::Mpsc> UpdateMessages;
	};

	FFileOperationTracker::FFileOperationTracker(FTicker& InTicker, FBuildPatchAppManifest* InManifest)
		: Ticker(InTicker)
		, Manifest(InManifest)
	{
		check(IsInGameThread());

		// Get the list of files in the build.
		TArray<FString> Filenames;
		Manifest->GetFileList(Filenames);

		// Initialise all file operations to Unknown.], use dummy operations for empty files.
		for (const FString& Filename : Filenames)
		{
			const FFileManifest* FileManifest = Manifest->GetFileManifest(Filename);
			uint64 FileOffset = 0;
			for (const FChunkPart& FileChunkPart : FileManifest->FileChunkParts)
			{
				FileOperationStates.Emplace(Filename, FileChunkPart.Guid, FileOffset, FileChunkPart.Size, EFileOperationState::Unknown);
				FileOffset += FileChunkPart.Size;
			}
			if (FileManifest->FileChunkParts.Num() == 0)
			{
				DummyOperationStates.Emplace(Filename, FGuid(), 0, 0, EFileOperationState::Unknown);
			}
		}

		// Create lookups to data for faster finding.
		for (FFileOperation& FileOperationState : FileOperationStates)
		{
			FileOperationStatesDataIdLookup.FindOrAdd(FileOperationState.DataId).Add(&FileOperationState);
			FileOperationStatesFilenameLookup.FindOrAdd(FileOperationState.Filename).Add(&FileOperationState);
		}
		for (FFileOperation& DummyOperationState : DummyOperationStates)
		{
			FileOperationStatesFilenameLookup.FindOrAdd(DummyOperationState.Filename).Add(&DummyOperationState);
		}

		// Need ticker to process incoming updates.
		TickerHandle = Ticker.AddTicker(FTickerDelegate::CreateRaw(this, &FFileOperationTracker::Tick));
	}

	FFileOperationTracker::~FFileOperationTracker()
	{
		check(IsInGameThread());
		// Remove ticker.
		Ticker.RemoveTicker(TickerHandle);
	}

	const TArray<FFileOperation>& FFileOperationTracker::GetStates() const
	{
		check(IsInGameThread());
		return FileOperationStates;
	}

	void FFileOperationTracker::OnDataStateUpdate(const FGuid& DataId, EFileOperationState State)
	{
		UpdateMessages.Enqueue(FUpdateMessage(FDataState(DataId, State)));
	}

	void FFileOperationTracker::OnDataStateUpdate(const TSet<FGuid>& DataIds, EFileOperationState State)
	{
		for (const FGuid& DataId : DataIds)
		{
			UpdateMessages.Enqueue(FUpdateMessage(FDataState(DataId, State)));
		}
	}

	void FFileOperationTracker::OnDataStateUpdate(const TArray<FGuid>& DataIds, EFileOperationState State)
	{
		for (const FGuid& DataId : DataIds)
		{
			UpdateMessages.Enqueue(FUpdateMessage(FDataState(DataId, State)));
		}
	}

	void FFileOperationTracker::OnFileStateUpdate(const FString& Filename, EFileOperationState State)
	{
		UpdateMessages.Enqueue(FUpdateMessage(FFileState(Filename, State)));
	}

	void FFileOperationTracker::OnFileStateUpdate(const TSet<FString>& Filenames, EFileOperationState State)
	{
		for (const FString& Filename : Filenames)
		{
			UpdateMessages.Enqueue(FUpdateMessage(FFileState(Filename, State)));
		}
	}

	void FFileOperationTracker::OnFileStateUpdate(const TArray<FString>& Filenames, EFileOperationState State)
	{
		for (const FString& Filename : Filenames)
		{
			UpdateMessages.Enqueue(FUpdateMessage(FFileState(Filename, State)));
		}
	}

	void FFileOperationTracker::OnFileByteRangeStateUpdate(const FString& Filename, FByteRange ByteRange, EFileOperationState State)
	{
		UpdateMessages.Enqueue(FUpdateMessage(FFileByteRangeState(Filename, ByteRange, State)));
	}

	bool FFileOperationTracker::Tick(float Delta)
	{
		const double TimeLimitSeconds = 1.0 / 120.0;
		// Use a time limit as setting huge file state can take a while. We will catch up easily over a handful of ticks.
		uint64 TimeLimitCycles = FStatsCollector::GetCycles() + FStatsCollector::SecondsToCycles(TimeLimitSeconds);
		FUpdateMessage UpdateMessage;
		while (FStatsCollector::GetCycles() < TimeLimitCycles && UpdateMessages.Dequeue(UpdateMessage))
		{
			if (UpdateMessage.HasSubtype<FDataState>())
			{
				ProcessMessage(UpdateMessage.GetSubtype<FDataState>());
			}
			else if (UpdateMessage.HasSubtype<FFileState>())
			{
				ProcessMessage(UpdateMessage.GetSubtype<FFileState>());
			}
			else if (UpdateMessage.HasSubtype<FFileByteRangeState>())
			{
				ProcessMessage(UpdateMessage.GetSubtype<FFileByteRangeState>());
			}
		}
		return true;
	}

	void FFileOperationTracker::ProcessMessage(const FDataState& Message)
	{
		for (FFileOperation* FileOp : FileOperationStatesDataIdLookup[Message.Get<0>()])
		{
			if (FileOp->CurrentState <= EFileOperationState::DataInMemoryStore)
			{
				FileOp->CurrentState = Message.Get<1>();
			}
		}
	}

	void FFileOperationTracker::ProcessMessage(const FFileState& Message)
	{
		for (FFileOperation* FileOp : FileOperationStatesFilenameLookup[Message.Get<0>()])
		{
			FileOp->CurrentState = Message.Get<1>();
		}
	}

	void FFileOperationTracker::ProcessMessage(const FFileByteRangeState& Message)
	{
		const uint64 FileByteStart = Message.Get<1>().Get<0>();
		const uint64 FileByteEnd = Message.Get<1>().Get<1>();
		for (FFileOperation* FileOp : FileOperationStatesFilenameLookup[Message.Get<0>()])
		{
			const uint64 FileByteFirst = FileOp->Offest;
			const uint64 FileByteLast = FileByteFirst + FileOp->Size;
			if (FileByteFirst < FileByteEnd && FileByteLast > FileByteStart)
			{
				FileOp->CurrentState = Message.Get<2>();
			}
			if (FileByteFirst >= FileByteEnd)
			{
				break;
			}
		}
	}

	IFileOperationTracker* FFileOperationTrackerFactory::Create(FTicker& Ticker, FBuildPatchAppManifest* Manifest)
	{
		return new FFileOperationTracker(Ticker, Manifest);
	}
}