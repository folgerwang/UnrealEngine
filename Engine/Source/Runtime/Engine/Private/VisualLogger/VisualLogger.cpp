// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VisualLogger/VisualLogger.h"
#include "Misc/CoreMisc.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CommandLine.h"
#include "Serialization/CustomVersion.h"
#include "Modules/ModuleManager.h"
#include "EngineGlobals.h"
#include "AI/NavDataGenerator.h"
#include "AI/NavigationSystemBase.h"
#include "Framework/Docking/TabManager.h"
#include "VisualLogger/VisualLoggerBinaryFileDevice.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif


DEFINE_LOG_CATEGORY(LogVisual);
#if ENABLE_VISUAL_LOG 
DEFINE_STAT(STAT_VisualLog);

namespace
{
	UWorld* GetWorldForVisualLogger(const UObject* Object)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Object, EGetWorldErrorMode::ReturnNull);
#if WITH_EDITOR
		UEditorEngine *EEngine = Cast<UEditorEngine>(GEngine);
		if (GIsEditor && EEngine != nullptr && World == nullptr)
		{
			// lets use PlayWorld during PIE/Simulate and regular world from editor otherwise, to draw debug information
			World = EEngine->PlayWorld != nullptr ? EEngine->PlayWorld : EEngine->GetEditorWorldContext().World();
		}

#endif
		if (!GIsEditor && World == nullptr)
		{
			World = GEngine->GetWorld();
		}

		return World;
	}
}

TMap<const UWorld*, FVisualLogger::FOwnerToChildrenRedirectionMap> FVisualLogger::WorldToRedirectionMap;
int32 FVisualLogger::bIsRecording = false;
FVisualLogger::FNavigationDataDump FVisualLogger::NavigationDataDumpDelegate;

bool FVisualLogger::CheckVisualLogInputInternal(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, UWorld **World, FVisualLogEntry **CurrentEntry)
{
	if (FVisualLogger::IsRecording() == false || !Object || !GEngine || GEngine->bDisableAILogging || Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}

	FVisualLogger& VisualLogger = FVisualLogger::Get();
	if (VisualLogger.IsBlockedForAllCategories() && VisualLogger.IsWhiteListed(CategoryName) == false)
	{
		return false;
	}

	*World = GEngine->GetWorldFromContextObject(Object, EGetWorldErrorMode::ReturnNull);
	if (ensure(*World != nullptr) == false)
	{
		return false;
	}

	*CurrentEntry = VisualLogger.GetEntryToWrite(Object, (*World)->TimeSeconds);
	if (*CurrentEntry == nullptr)
	{
		return false;
	}

	return true;
}

void FVisualLogger::AddWhitelistedClass(UClass& InClass)
{
	ClassWhitelist.AddUnique(&InClass);
}

bool FVisualLogger::IsClassWhitelisted(const UClass& InClass) const
{
	for (const UClass* WhitelistedClass : ClassWhitelist)
	{
		if (InClass.IsChildOf(WhitelistedClass))
		{
			return true;
		}
	}

	return false;
}

void FVisualLogger::AddWhitelistedObject(const UObject& InObject)
{
	const int32 PrevNum = ObjectWhitelist.Num();
	ObjectWhitelist.Add(&InObject);

	const bool bChanged = (PrevNum != ObjectWhitelist.Num());
	if (bChanged)
	{
		FVisualLogEntry* CurrentEntry = CurrentEntryPerObject.Find(&InObject);
		if (CurrentEntry)
		{
			CurrentEntry->bIsObjectWhitelisted = true;
			CurrentEntry->UpdateAllowedToLog();
		}
	}
}

void FVisualLogger::ClearObjectWhitelist()
{
	for (const UObject* It : ObjectWhitelist)
	{
		FVisualLogEntry* CurrentEntry = CurrentEntryPerObject.Find(It);
		if (CurrentEntry)
		{
			CurrentEntry->bIsObjectWhitelisted = false;
			CurrentEntry->UpdateAllowedToLog();
		}
	}

	ObjectWhitelist.Empty();
}

bool FVisualLogger::IsObjectWhitelisted(const UObject* InObject) const
{
	return ObjectWhitelist.Contains(InObject);
}

FVisualLogEntry* FVisualLogger::GetLastEntryForObject(const UObject* Object)
{
	UObject* LogOwner = FVisualLogger::FindRedirection(Object);
	return CurrentEntryPerObject.Contains(LogOwner) ? &CurrentEntryPerObject[LogOwner] : nullptr;
}

FVisualLogEntry* FVisualLogger::GetEntryToWrite(const UObject* Object, float TimeStamp, ECreateIfNeeded ShouldCreate)
{
	FVisualLogEntry* CurrentEntry = nullptr;
	UObject * LogOwner = FVisualLogger::FindRedirection(Object);
	if (LogOwner == nullptr)
	{
		return nullptr;
	}

	bool bInitializeNewEntry = false;

	const UWorld* World = GetWorldForVisualLogger(LogOwner);
	if (CurrentEntryPerObject.Contains(LogOwner))
	{
		CurrentEntry = &CurrentEntryPerObject[LogOwner];
		if (CurrentEntry->bIsAllowedToLog)
		{
			bInitializeNewEntry = (TimeStamp > CurrentEntry->TimeStamp) && (ShouldCreate == ECreateIfNeeded::Create);
			if (World && IsInGameThread())
			{
				World->GetTimerManager().ClearTimer(VisualLoggerCleanupTimerHandle);
				for (auto& CurrentPair : CurrentEntryPerObject)
				{
					FVisualLogEntry* Entry = &CurrentPair.Value;
					if (Entry->TimeStamp >= 0 && Entry->TimeStamp < TimeStamp)
					{
						for (FVisualLogDevice* Device : OutputDevices)
						{
							Device->Serialize(CurrentPair.Key, ObjectToNameMap[CurrentPair.Key], ObjectToClassNameMap[CurrentPair.Key], *Entry);
						}
						Entry->Reset();
					}
				}
			}
		}
	}

	if (CurrentEntry == nullptr)
	{
		// It's first and only one usage of LogOwner as regular object to get names. We assume once that LogOwner is correct here and only here.
		CurrentEntry = &CurrentEntryPerObject.Add(LogOwner);
		ObjectToNameMap.Add(LogOwner, LogOwner->GetFName());
		ObjectToClassNameMap.Add(LogOwner, *(LogOwner->GetClass()->GetName()));
		ObjectToPointerMap.Add(LogOwner, LogOwner);
		ObjectToWorldMap.Add(LogOwner, World);
		
		// IsClassWhitelisted isn't super fast, but this gets calculated only once for every 
		// object trying to log something
		CurrentEntry->bIsClassWhitelisted = (ClassWhitelist.Num() == 0) || IsClassWhitelisted(*LogOwner->GetClass()) || IsClassWhitelisted(*Object->GetClass());
		CurrentEntry->bIsObjectWhitelisted = IsObjectWhitelisted(LogOwner);
		CurrentEntry->UpdateAllowedToLog();

		bInitializeNewEntry = CurrentEntry->bIsAllowedToLog;
	}

	if (bInitializeNewEntry)
	{
		CurrentEntry->Reset();
		CurrentEntry->TimeStamp = TimeStamp;

		auto& RedirectionMap = GetRedirectionMap(LogOwner);
		if (RedirectionMap.Contains(LogOwner))
		{
			if (ObjectToPointerMap.Contains(LogOwner) && ObjectToPointerMap[LogOwner].IsValid())
			{
				const IVisualLoggerDebugSnapshotInterface* DebugSnapshotInterface = Cast<const IVisualLoggerDebugSnapshotInterface>(LogOwner);
				if (DebugSnapshotInterface)
				{
					DebugSnapshotInterface->GrabDebugSnapshot(CurrentEntry);
				}
			}
			for (auto Child : RedirectionMap[LogOwner])
			{
				if (Child.IsValid())
				{
					const IVisualLoggerDebugSnapshotInterface* DebugSnapshotInterface = Cast<const IVisualLoggerDebugSnapshotInterface>(Child.Get());
					if (DebugSnapshotInterface)
					{
						DebugSnapshotInterface->GrabDebugSnapshot(CurrentEntry);
					}
				}
			}
		}
		else
		{
			const AActor* ObjectAsActor = Cast<AActor>(LogOwner);
			if (ObjectAsActor)
			{
				CurrentEntry->Location = ObjectAsActor->GetActorLocation();
			}
			const IVisualLoggerDebugSnapshotInterface* DebugSnapshotInterface = Cast<const IVisualLoggerDebugSnapshotInterface>(LogOwner);
			if (DebugSnapshotInterface)
			{
				DebugSnapshotInterface->GrabDebugSnapshot(CurrentEntry);
			}
		}
	}

	return CurrentEntry->bIsAllowedToLog ? CurrentEntry : nullptr;
}


void FVisualLogger::Flush()
{
	for (auto &CurrentEntry : CurrentEntryPerObject)
	{
		if (CurrentEntry.Value.TimeStamp >= 0)
		{
			for (FVisualLogDevice* Device : OutputDevices)
			{
				Device->Serialize(CurrentEntry.Key, ObjectToNameMap[CurrentEntry.Key], ObjectToClassNameMap[CurrentEntry.Key], CurrentEntry.Value);
			}
			CurrentEntry.Value.Reset();
		}
	}
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5, const FVisualLogEventBase& Event6)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3, Event4, Event5);
	EventLog(Object, EventTag1, Event6);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3, Event4);
	EventLog(Object, EventTag1, Event5);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3);
	EventLog(Object, EventTag1, Event4);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3)
{
	EventLog(Object, EventTag1, Event1, Event2);
	EventLog(Object, EventTag1, Event3);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2)
{
	EventLog(Object, EventTag1, Event1);
	EventLog(Object, EventTag1, Event2);
}


void FVisualLogger::EventLog(const UObject* LogOwner, const FVisualLogEventBase& Event1, const FName EventTag1, const FName EventTag2, const FName EventTag3, const FName EventTag4, const FName EventTag5, const FName EventTag6)
{
	EventLog(LogOwner, EventTag1, Event1, EventTag2, EventTag3, EventTag4, EventTag5, EventTag6);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event, const FName EventTag2, const FName EventTag3, const FName EventTag4, const FName EventTag5, const FName EventTag6)
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	const FName CategoryName(*Event.Name);
	if (CheckVisualLogInputInternal(Object, CategoryName, ELogVerbosity::Log, &World, &CurrentEntry) == false)
	{
		return;
	}

	int32 Index = CurrentEntry->Events.Find(FVisualLogEvent(Event));
	if (Index != INDEX_NONE)
	{
		CurrentEntry->Events[Index].Counter++;
	}
	else
	{
		Index = CurrentEntry->AddEvent(Event);
	}

	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag1)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag2)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag3)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag4)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag5)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag6)++;
	CurrentEntry->Events[Index].EventTags.Remove(NAME_None);
}

void FVisualLogger::NavigationDataDump(const UObject* Object, const FLogCategoryBase& Category, const ELogVerbosity::Type Verbosity, const FBox& Box)
{
	NavigationDataDump(Object, Category.GetCategoryName(), Verbosity, Box);
}

void FVisualLogger::NavigationDataDump(const UObject* Object, const FName& CategoryName, const ELogVerbosity::Type Verbosity, const FBox& Box)
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);

	UWorld* World = nullptr;
	FVisualLogEntry* CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, CategoryName, Verbosity, &World, &CurrentEntry) == false 
		|| CurrentEntry == nullptr)
	{
		return;
	}
	
	NavigationDataDumpDelegate.Broadcast(Object, CategoryName, Verbosity, Box, *World, *CurrentEntry);
}


FVisualLogger::FVisualLogger()
{
	BlockAllCategories(false);
	AddDevice(&FVisualLoggerBinaryFileDevice::Get());
	SetIsRecording(GEngine ? !!GEngine->bEnableVisualLogRecordingOnStart : false);
	SetIsRecordingOnServer(false);

	if (FParse::Param(FCommandLine::Get(), TEXT("EnableAILogging")))
	{
		SetIsRecording(true);
		SetIsRecordingToFile(true);
	}
}

void FVisualLogger::Shutdown()
{
	SetIsRecording(false);
	SetIsRecordingToFile(false);

	if (UseBinaryFileDevice)
	{
		RemoveDevice(&FVisualLoggerBinaryFileDevice::Get());
	}
}

void FVisualLogger::Cleanup(UWorld* OldWorld, bool bReleaseMemory)
{
	const bool WasRecordingToFile = IsRecordingToFile();
	if (WasRecordingToFile)
	{
		SetIsRecordingToFile(false);
	}

	Flush();
	for (FVisualLogDevice* Device : FVisualLogger::Get().OutputDevices)
	{
		Device->Cleanup(bReleaseMemory);
	}

	if (OldWorld != nullptr && WorldToRedirectionMap.Find(OldWorld))
	{
		WorldToRedirectionMap.Remove(OldWorld);

		if (WorldToRedirectionMap.Num() == 0)
		{
			WorldToRedirectionMap.Reset();
			ObjectToWorldMap.Reset();
			ChildToOwnerMap.Reset();
			CurrentEntryPerObject.Reset();
			ObjectToNameMap.Reset();
			ObjectToClassNameMap.Reset();
			ObjectToPointerMap.Reset();
		}
		else
		{
			for (auto It = ObjectToWorldMap.CreateIterator(); It; ++It)
			{
				if (It.Value() == OldWorld)
				{
					const UObject* Obj = It.Key();
					ObjectToWorldMap.Remove(Obj);
					CurrentEntryPerObject.Remove(Obj);
					ObjectToNameMap.Remove(Obj);
					ObjectToClassNameMap.Remove(Obj);
					ObjectToPointerMap.Remove(Obj);
				}
			}

			for (FChildToOwnerRedirectionMap::TIterator It = ChildToOwnerMap.CreateIterator(); It; ++It)
			{
				if (It->Key.IsValid() == false
					|| It->Key->GetWorld() == OldWorld)
				{
					It.RemoveCurrent();
				}
			}
		}
	}
	else
	{
		WorldToRedirectionMap.Reset();
		ObjectToWorldMap.Reset();
		ChildToOwnerMap.Reset();
		CurrentEntryPerObject.Reset();
		ObjectToNameMap.Reset();
		ObjectToClassNameMap.Reset();
		ObjectToPointerMap.Reset();
	}

	LastUniqueIds.Reset();

	if (WasRecordingToFile)
	{
		SetIsRecordingToFile(true);
	}
}

int32 FVisualLogger::GetUniqueId(float Timestamp)
{
	return LastUniqueIds.FindOrAdd(Timestamp)++;
}

FVisualLogger::FOwnerToChildrenRedirectionMap& FVisualLogger::GetRedirectionMap(const UObject* InObject)
{
	const UWorld* World = nullptr;
	if (FVisualLogger::Get().ObjectToWorldMap.Contains(InObject))
	{
		World = FVisualLogger::Get().ObjectToWorldMap[InObject].Get();
	}

	if (World == nullptr)
	{
		World = GetWorldForVisualLogger(nullptr);
	}

	return WorldToRedirectionMap.FindOrAdd(World);
}

void FVisualLogger::Redirect(UObject* FromObject, UObject* ToObject)
{
	if (FromObject == ToObject || FromObject == nullptr || ToObject == nullptr)
	{
		return;
	}

	TWeakObjectPtr<const UObject> FromWeakPtr(FromObject);
	UObject* OldRedirection = FindRedirection(FromObject);
	UObject* NewRedirection = FindRedirection(ToObject);

	if (OldRedirection != NewRedirection)
	{
		FOwnerToChildrenRedirectionMap& OwnerToChildrenMap = GetRedirectionMap(FromObject);

		auto OldArray = OwnerToChildrenMap.Find(OldRedirection);
		if (OldArray)
		{
			OldArray->RemoveSingleSwap(FromWeakPtr);
		}

		OwnerToChildrenMap.FindOrAdd(NewRedirection).AddUnique(FromWeakPtr);
	}

	FChildToOwnerRedirectionMap& ChildToOwnerMap = FVisualLogger::Get().GetChildToOwnerRedirectionMap();
	ChildToOwnerMap.FindOrAdd(FromWeakPtr) = ToObject;

	UE_CVLOG(FromObject != nullptr, FromObject, LogVisual, Log, TEXT("Redirected '%s' to '%s'"), *FromObject->GetName(), *NewRedirection->GetName());
}

UObject* FVisualLogger::FindRedirection(const UObject* Object)
{
	FChildToOwnerRedirectionMap& Map = FVisualLogger::Get().GetChildToOwnerRedirectionMap();

	TWeakObjectPtr<const UObject> TargetWeakPtr(Object);
	TWeakObjectPtr<const UObject>* Parent = &TargetWeakPtr;

	while (Parent)
	{
		Parent = Map.Find(TargetWeakPtr);
		if (Parent)
		{
			if (Parent->IsValid())
			{
				TargetWeakPtr = *Parent;
			}
			else
			{
				Parent = nullptr;
				Map.Remove(TargetWeakPtr);
			}
		}
	}

	return const_cast<UObject*>(TargetWeakPtr.Get());
}

void FVisualLogger::SetIsRecording(bool InIsRecording) 
{ 
	if (InIsRecording == false && InIsRecording != !!bIsRecording && FParse::Param(FCommandLine::Get(), TEXT("LogNavOctree")))
	{
		FVisualLogger::NavigationDataDump(GetWorldForVisualLogger(nullptr), LogNavigation, ELogVerbosity::Log, FBox());
	}
	if (IsRecordingToFile())
	{
		SetIsRecordingToFile(false);
	}
	bIsRecording = InIsRecording; 
};

void FVisualLogger::SetIsRecordingToFile(bool InIsRecording)
{
	if (!bIsRecording && InIsRecording)
	{
		SetIsRecording(true);
	}

	UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;

	const FString BaseFileName = LogFileNameGetter.IsBound() ? LogFileNameGetter.Execute() : TEXT("VisualLog");
	const FString MapName = World ? World->GetMapName() : TEXT("");

	FString OutputFileName = FString::Printf(TEXT("%s_%s"), *BaseFileName, *MapName);

	if (bIsRecordingToFile && !InIsRecording)
	{
		for (FVisualLogDevice* Device : OutputDevices)
		{
			if (Device->HasFlags(EVisualLoggerDeviceFlags::CanSaveToFile))
			{
				Device->SetFileName(OutputFileName);
				Device->StopRecordingToFile(World ? World->TimeSeconds : StartRecordingToFileTime);
			}
		}
	}
	else if (!bIsRecordingToFile && InIsRecording)
	{
		StartRecordingToFileTime = World ? World->TimeSeconds : 0;
		for (FVisualLogDevice* Device : OutputDevices)
		{
			if (Device->HasFlags(EVisualLoggerDeviceFlags::CanSaveToFile))
			{
				Device->StartRecordingToFile(StartRecordingToFileTime);
			}
		}
	}

	bIsRecordingToFile = InIsRecording;
}

void FVisualLogger::DiscardRecordingToFile()
{
	if (bIsRecordingToFile)
	{
		for (FVisualLogDevice* Device : OutputDevices)
		{
			if (Device->HasFlags(EVisualLoggerDeviceFlags::CanSaveToFile))
			{
				Device->DiscardRecordingToFile();
			}
		}

		bIsRecordingToFile = false;
	}
}

bool FVisualLogger::IsCategoryLogged(const FLogCategoryBase& Category) const
{
	if ((GEngine && GEngine->bDisableAILogging) || IsRecording() == false)
	{
		return false;
	}

	const FName CategoryName = Category.GetCategoryName();
	if (IsBlockedForAllCategories() && IsWhiteListed(CategoryName) == false)
	{
		return false;
	}

	return true;
}

#endif //ENABLE_VISUAL_LOG

const FGuid EVisualLoggerVersion::GUID = FGuid(0xA4237A36, 0xCAEA41C9, 0x8FA218F8, 0x58681BF3);
FCustomVersionRegistration GVisualLoggerVersion(EVisualLoggerVersion::GUID, EVisualLoggerVersion::LatestVersion, TEXT("VisualLogger"));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static class FLogVisualizerExec : private FSelfRegisteringExec
{
public:
	/** Console commands, see embeded usage statement **/
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("VISLOG")))
		{
			if (FModuleManager::Get().LoadModulePtr<IModuleInterface>("LogVisualizer") != nullptr)
			{
#if ENABLE_VISUAL_LOG
				FString Command = FParse::Token(Cmd, 0);
				if (Command == TEXT("record"))
				{
					FVisualLogger::Get().SetIsRecording(true);
					return true;
				}
				else if (Command == TEXT("stop"))
				{
					FVisualLogger::Get().SetIsRecording(false);
					return true;
				}
				else if (Command == TEXT("disableallbut"))
				{
					FString Category = FParse::Token(Cmd, 1);
					FVisualLogger::Get().BlockAllCategories(true);
					FVisualLogger::Get().AddCategoryToWhitelist(*Category);
					return true;
				}
#if WITH_EDITOR
				else
				{
					FGlobalTabmanager::Get()->InvokeTab(FName(TEXT("VisualLogger")));
					return true;
				}
#endif
#else
			UE_LOG(LogVisual, Warning, TEXT("Unable to open LogVisualizer - logs are disabled"));
#endif
			}
		}
#if ENABLE_VISUAL_LOG
		else if (FParse::Command(&Cmd, TEXT("LogNavOctree")))
		{
			FVisualLogger::NavigationDataDump(GetWorldForVisualLogger(nullptr), LogNavigation, ELogVerbosity::Log, FBox());
		}
#endif
		return false;
	}
} LogVisualizerExec;


#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
