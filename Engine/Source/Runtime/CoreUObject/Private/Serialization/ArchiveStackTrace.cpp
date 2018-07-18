// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveStackTrace.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"
#include "HAL/PlatformStackWalk.h"
#include "Serialization/AsyncLoading.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceHelper.h"
#include "Serialization/StaticMemoryReader.h"
#include "UObject/PropertyTempVal.h"
#include "UObject/LinkerLoad.h"
#include "Serialization/LargeMemoryReader.h"
#include "UObject/LinkerManager.h"
#include "Misc/PackageName.h"
#include "Templates/UniquePtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogArchiveDiff, Log, All);

#if !NO_LOGGING
/** Helper class that holds runtime generated constants for log output formatting */
struct FDiffFormatHelper
{
public:
	const TCHAR* const Indent;
	const TCHAR* const LineTerminator;

	FDiffFormatHelper()
		: Indent(FCString::Spc(FOutputDeviceHelper::FormatLogLine(ELogVerbosity::Warning, LogArchiveDiff.GetCategoryName(), TEXT(""), GPrintLogTimes).Len()))
		, LineTerminator(TEXT("\n")) // because LINE_TERMINATOR doesn't work well wit EC
	{}
	static FDiffFormatHelper& Get()
	{
		static FDiffFormatHelper Instance;
		return Instance;
	}
};
#endif // !NO_LOGGING

class FIgnoreDiffManager
{
	int32 IgnoreCount;

public:
	FIgnoreDiffManager()
		: IgnoreCount(0)
	{}
	void Push()
	{
		IgnoreCount++;
	}
	void Pop()
	{
		IgnoreCount--;
		check(IgnoreCount >= 0);
	}
	bool ShouldIgnoreDiff() const
	{
		return !!IgnoreCount;
	}
};

static FIgnoreDiffManager GIgnoreDiffManager;

static const ANSICHAR* DebugDataStackMarker = "\r\nDebugDataStack:\r\n";

FArchiveStackTraceIgnoreScope::FArchiveStackTraceIgnoreScope(bool bInIgnore /* = true */)
	: bIgnore(bInIgnore)
{
	if (bIgnore)
	{
		GIgnoreDiffManager.Push();
	}
}
FArchiveStackTraceIgnoreScope::~FArchiveStackTraceIgnoreScope()
{
	if (bIgnore)
	{
		GIgnoreDiffManager.Pop();
	}
}

FArchiveStackTrace::FCallstackData::FCallstackData()
	: Callstack(nullptr)
	, SerializedProp(nullptr)
{
}

FArchiveStackTrace::FCallstackData::FCallstackData(ANSICHAR* InCallstack, UObject* InSerializedObject, UProperty* InSerializedProperty)
	: Callstack(InCallstack)
	, SerializedProp(InSerializedProperty)
{
	if (InSerializedObject)
	{
		SerializedObjectName = InSerializedObject->GetFullName();
	}
	if (InSerializedProperty)
	{
		SerializedPropertyName = InSerializedProperty->GetFullName();
	}
}

FString FArchiveStackTrace::FCallstackData::ToString(const TCHAR* CallstackCutoffText) const
{
	FString HumanReadableString;

#if !NO_LOGGING
	const TCHAR* const LineTerminator = FDiffFormatHelper::Get().LineTerminator;
	const TCHAR* const Indent = FDiffFormatHelper::Get().Indent;
	
	FString StackTraceText = Callstack;
	if (CallstackCutoffText != nullptr)
	{
		// If the cutoff string is provided, remove all functions starting with the one specifiec in the cutoff string
		int32 CutoffIndex = StackTraceText.Find(CallstackCutoffText, ESearchCase::CaseSensitive);
		if (CutoffIndex > 0)
		{
			CutoffIndex = StackTraceText.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, CutoffIndex - 1);
			if (CutoffIndex > 0)
			{
				StackTraceText = StackTraceText.Left(CutoffIndex + 1);
			}
		}
	}

	TArray<FString> StackLines;
	StackTraceText.ParseIntoArrayLines(StackLines);
	for (FString& StackLine : StackLines)
	{
		if (StackLine.StartsWith(TEXT("0x")))
		{
			int32 CutoffIndex = StackLine.Find(TEXT(" "), ESearchCase::CaseSensitive);
			if (CutoffIndex >= -1 && CutoffIndex < StackLine.Len() - 2)
			{
				StackLine = StackLine.Mid(CutoffIndex + 1);
			}
		}
		HumanReadableString += Indent;
		HumanReadableString += StackLine;
		HumanReadableString += LineTerminator;
	}

	if (!SerializedObjectName.IsEmpty())
	{
		HumanReadableString += LineTerminator;
		HumanReadableString += Indent;
		HumanReadableString += TEXT("Serialized Object: ");
		HumanReadableString += SerializedObjectName;
		HumanReadableString += LineTerminator;
	}
	if (!SerializedPropertyName.IsEmpty())
	{
		if (SerializedObjectName.IsEmpty())
		{
			HumanReadableString += LineTerminator;
		}
		HumanReadableString += Indent;
		HumanReadableString += TEXT("Serialized Property: ");
		HumanReadableString += SerializedPropertyName;
		HumanReadableString += LineTerminator;
	}
#endif // !NO_LOGGING
	return HumanReadableString;
}

FArchiveStackTrace::FArchiveStackTrace(UObject* InAsset, const TCHAR* InFilename, bool bInCollectCallstacks, const FArchiveDiffMap* InDiffMap)
	: FLargeMemoryWriter(0, false, InFilename)
	, Asset(InAsset)
	, AssetClass(InAsset ? InAsset->GetClass()->GetFName() : NAME_None)
	, DiffMap(InDiffMap)
	, bCollectCallstacks(bInCollectCallstacks)
	, bCallstacksDirty(true)
	, StackTraceSize(65535)
	, LastSerializeCallstack(nullptr)
	, ThreadContext(FUObjectThreadContext::Get())
{
	this->SetIsSaving(true);

	StackTrace = (ANSICHAR*)FMemory::Malloc(StackTraceSize);
	StackTrace[0] = 0;
}

FArchiveStackTrace::~FArchiveStackTrace()
{
	FMemory::Free(StackTrace);

	for (TPair<uint32, FCallstackData>& UniqueCallstackPair : UniqueCallstacks)
	{
		FMemory::Free(UniqueCallstackPair.Value.Callstack);
	}
}

ANSICHAR* FArchiveStackTrace::AddUniqueCallstack(UObject* InSerializedObject, UProperty* InSerializedProperty, uint32& OutCallstackCRC)
{
	ANSICHAR* Callstack = nullptr;
	if (bCollectCallstacks)
	{
		OutCallstackCRC = FCrc::StrCrc32(StackTrace);

		if (FCallstackData* ExistingCallstack = UniqueCallstacks.Find(OutCallstackCRC))
		{
			Callstack = ExistingCallstack->Callstack;
		}
		else
		{
			int32 CallstackSize = FCStringAnsi::Strlen(StackTrace) + 1;
			Callstack = (ANSICHAR*)FMemory::Malloc(CallstackSize);
			FCStringAnsi::Strcpy(Callstack, CallstackSize, StackTrace);
			UniqueCallstacks.Add(OutCallstackCRC, FCallstackData(Callstack,
				InSerializedObject,
				InSerializedProperty));
		}
	}
	else
	{
		OutCallstackCRC = 0;
	}
	return Callstack;
}

void FArchiveStackTrace::Serialize(void* InData, int64 Num)
{
	if (Num)
	{
#if UE_BUILD_DEBUG
		const int32 StackIgnoreCount = 5;
#else
		const int32 StackIgnoreCount = 4;
#endif

		static struct FBreakAtOffsetSettings
		{
			FString PackageToBreakOn;
			int64 OffsetToBreakOn;

			FBreakAtOffsetSettings()
				: OffsetToBreakOn(-1)
			{
				if (!FParse::Param(FCommandLine::Get(), TEXT("cooksinglepackage")))
				{
					return;
				}

				FString Package;
				if (!FParse::Value(FCommandLine::Get(), TEXT("map="), Package))
				{
					return;
				}

				int64 Offset;
				if (!FParse::Value(FCommandLine::Get(), TEXT("diffonlybreakoffset="), Offset) || Offset <= 0)
				{
					return;
				}

				OffsetToBreakOn = Offset;
				PackageToBreakOn = TEXT("/") + FPackageName::GetShortName(Package);
			}
		} BreakAtOffsetSettings;

		int64 CurrentOffset = Tell();

		if (BreakAtOffsetSettings.OffsetToBreakOn >= 0 && BreakAtOffsetSettings.OffsetToBreakOn >= CurrentOffset && BreakAtOffsetSettings.OffsetToBreakOn < CurrentOffset + Num)
		{
			FString ArcName = GetArchiveName();
			int32 SubnameIndex = ArcName.Find(BreakAtOffsetSettings.PackageToBreakOn, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (SubnameIndex >= 0)
			{
				int32 SubnameEndIndex = SubnameIndex + BreakAtOffsetSettings.PackageToBreakOn.Len();
				if (SubnameEndIndex == ArcName.Len() || ArcName[SubnameEndIndex] == TEXT('.'))
				{
					UE_DEBUG_BREAK();
				}
			}
		}

		// Walk the stack and dump it to the allocated memory.
		bool bShouldCollectCallstack = bCollectCallstacks && (DiffMap == nullptr || IsInDiffMap(CurrentOffset)) && !GIgnoreDiffManager.ShouldIgnoreDiff();
		if (bShouldCollectCallstack)
		{
			StackTrace[0] = '\0';
			FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, StackIgnoreCount);
#if WITH_EDITOR
			//if we have a debug name stack, plaster it onto the end of the current stack buffer so that it's a part of the unique stack entry.
			if (DebugDataStack.Num() > 0)
			{
				FCStringAnsi::Strcat(StackTrace, StackTraceSize, DebugDataStackMarker);

				const FString SubIndent = FString(FDiffFormatHelper::Get().Indent) + FString(TEXT("    "));

				bool bIsIndenting = true;
				for (const auto& DebugData : DebugDataStack)
				{
					if (bIsIndenting)
					{
						FCStringAnsi::Strcat(StackTrace, StackTraceSize, TCHAR_TO_ANSI(*SubIndent));
					}
					FCStringAnsi::Strcat(StackTrace, StackTraceSize, DebugData.GetPlainANSIString());

					//these are special-cased, as we assume they'll be followed by object/property names and want the names on the same line for readability's sake.
					const bool bIsPropertyLabel = (DebugData == TEXT("SerializeScriptProperties") || DebugData == TEXT("PropertySerialize") || DebugData == TEXT("SerializeTaggedProperty"));
					const ANSICHAR* const LineEnd = bIsPropertyLabel ? ": " : "\r\n";
					FCStringAnsi::Strcat(StackTrace, StackTraceSize, LineEnd);
					bIsIndenting = !bIsPropertyLabel;
				}
			}
#endif
			// Make sure we compare the new stack trace with the last one in the next if statement
			bCallstacksDirty = true;
		}

		if (LastSerializeCallstack == nullptr || (bCallstacksDirty && FCStringAnsi::Strcmp(LastSerializeCallstack, StackTrace) != 0))
		{
			uint32 CallstackCRC = 0;
			if (CallstackAtOffsetMap.Num() == 0 || CurrentOffset > CallstackAtOffsetMap.Last().Offset)
			{
				// New data serialized at the end of archive buffer
				LastSerializeCallstack = AddUniqueCallstack(ThreadContext.SerializedObject, GetSerializedProperty(), CallstackCRC);
				CallstackAtOffsetMap.Add(FCallstactAtOffset(CurrentOffset, CallstackCRC, GIgnoreDiffManager.ShouldIgnoreDiff()));
			}
			else
			{
				// This happens usually after Seek() so we need to find the exiting offset or insert a new one
				int32 CallstackToUpdateIndex = GetCallstackAtOffset(CurrentOffset, 0);
				check(CallstackToUpdateIndex != -1);
				FCallstactAtOffset& CallstackToUpdate = CallstackAtOffsetMap[CallstackToUpdateIndex];
				LastSerializeCallstack = AddUniqueCallstack(ThreadContext.SerializedObject, GetSerializedProperty(), CallstackCRC);
				if (CallstackToUpdate.Offset == CurrentOffset)
				{
					CallstackToUpdate.Callstack = CallstackCRC;
				}
				else
				{
					// Insert a new callstack
					check(CallstackToUpdate.Offset < CurrentOffset);
					CallstackAtOffsetMap.Insert(FCallstactAtOffset(CurrentOffset, CallstackCRC, GIgnoreDiffManager.ShouldIgnoreDiff()), CallstackToUpdateIndex + 1);
				}
			}
			check(CallstackCRC != 0 || !bShouldCollectCallstack);
		}
		else if (LastSerializeCallstack)
		{
			// Skip callstack comparison on next serialize call unless we grab a stack trace
			bCallstacksDirty = false;
		}
	}
	FLargeMemoryWriter::Serialize(InData, Num);
}

int32 FArchiveStackTrace::GetCallstackAtOffset(int64 InOffset, int32 MinOffsetIndex)
{
	if (InOffset < 0 || InOffset > TotalSize() || MinOffsetIndex < 0 || MinOffsetIndex >= CallstackAtOffsetMap.Num())
	{
		return -1;
	}

	// Find the index of the offset the InOffset maps to
	int32 OffsetForCallstackIndex = -1;
	int32 MaxOffsetIndex = CallstackAtOffsetMap.Num() - 1;

	// Binary search
	for (; MinOffsetIndex <= MaxOffsetIndex; )
	{
		int32 SearchIndex = (MinOffsetIndex + MaxOffsetIndex) / 2;
		if (CallstackAtOffsetMap[SearchIndex].Offset < InOffset)
		{
			MinOffsetIndex = SearchIndex + 1;
		}
		else if (CallstackAtOffsetMap[SearchIndex].Offset > InOffset)
		{
			MaxOffsetIndex = SearchIndex - 1;
		}
		else
		{
			OffsetForCallstackIndex = SearchIndex;
			break;
		}
	}
	
	if (OffsetForCallstackIndex == -1)
	{
		// We didn't find the exact offset value so let's try to find the first one that is lower than the requested one
		MinOffsetIndex = FMath::Min(MinOffsetIndex, CallstackAtOffsetMap.Num() - 1);
		for (int32 FirstLowerOffsetIndex = MinOffsetIndex; FirstLowerOffsetIndex >= 0; --FirstLowerOffsetIndex)
		{
			if (CallstackAtOffsetMap[FirstLowerOffsetIndex].Offset < InOffset)
			{
				OffsetForCallstackIndex = FirstLowerOffsetIndex;
				break;
			}
		}
		check(OffsetForCallstackIndex != -1);
		check(CallstackAtOffsetMap[OffsetForCallstackIndex].Offset < InOffset);
		check(OffsetForCallstackIndex == (CallstackAtOffsetMap.Num() - 1) || CallstackAtOffsetMap[OffsetForCallstackIndex + 1].Offset > InOffset);
	}

	return OffsetForCallstackIndex;
}

bool FArchiveStackTrace::LoadPackageIntoMemory(const TCHAR* InFilename, FPackageData& OutPackageData)
{
	FArchive* UAssetFileArchive = IFileManager::Get().CreateFileReader(InFilename);
	if (!UAssetFileArchive || UAssetFileArchive->TotalSize() == 0)
	{
		// The package doesn't exist on disk
		OutPackageData.Data = nullptr;
		OutPackageData.Size = 0;
		OutPackageData.HeaderSize = 0;
		OutPackageData.StartOffset = 0;
		return false;
	}
	else
	{
		// Handle EDL packages (uexp files)
		FArchive* ExpFileArchive = nullptr;
		OutPackageData.Size = UAssetFileArchive->TotalSize();
		if (IsEventDrivenLoaderEnabledInCookedBuilds())
		{
			FString UExpFilename = FPaths::ChangeExtension(InFilename, TEXT("uexp"));
			ExpFileArchive = IFileManager::Get().CreateFileReader(*UExpFilename);
			if (ExpFileArchive)
			{				
				// The header size is the current package size
				OutPackageData.HeaderSize = OutPackageData.Size;
				// Grow the buffer size to append the uexp file contents
				OutPackageData.Size += ExpFileArchive->TotalSize();
			}
		}
		OutPackageData.Data = (uint8*)FMemory::Malloc(OutPackageData.Size);
		UAssetFileArchive->Serialize(OutPackageData.Data, UAssetFileArchive->TotalSize());

		if (ExpFileArchive)
		{
			// If uexp file is present, append its contents at the end of the buffer
			ExpFileArchive->Serialize(OutPackageData.Data + OutPackageData.HeaderSize, ExpFileArchive->TotalSize());
			delete ExpFileArchive;
			ExpFileArchive = nullptr;
		}
	}
	delete UAssetFileArchive;

	return true;
}

namespace
{
	bool ShouldDumpPropertyValueState(UProperty* Prop)
	{
		if (Prop->IsA<UNumericProperty>()
			|| Prop->IsA<UStrProperty>()
			|| Prop->IsA<UBoolProperty>()
			|| Prop->IsA<UNameProperty>())
		{
			return true;
		}

		if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop))
		{
			return ShouldDumpPropertyValueState(ArrayProp->Inner);
		}

		if (UMapProperty* MapProp = Cast<UMapProperty>(Prop))
		{
			return ShouldDumpPropertyValueState(MapProp->KeyProp) && ShouldDumpPropertyValueState(MapProp->ValueProp);
		}

		if (USetProperty* SetProp = Cast<USetProperty>(Prop))
		{
			return ShouldDumpPropertyValueState(SetProp->ElementProp);
		}

		if (UStructProperty* StructProp = Cast<UStructProperty>(Prop))
		{
			if (StructProp->Struct == TBaseStructure<FVector>::Get()
				|| StructProp->Struct == TBaseStructure<FGuid>::Get())
			{
				return true;
			}
		}

		return false;
	}
}

void FArchiveStackTrace::CompareWithInternal(const FPackageData& SourcePackage, const FPackageData& DestPackage, const TCHAR* AssetFilename, const TCHAR* CallstackCutoffText, const int64 MaxDiffsToLog, int32& InOutDiffsLogged, TMap<FName, FArchiveDiffStats>& OutStats)
{
#if !NO_LOGGING
	const TCHAR* const Indent = FDiffFormatHelper::Get().Indent;
	const TCHAR* const LineTerminator = FDiffFormatHelper::Get().LineTerminator;
	const int64 SourceSize = SourcePackage.Size - SourcePackage.StartOffset;
	const int64 DestSize = DestPackage.Size - DestPackage.StartOffset;
	const int64 SizeToCompare = FMath::Min(SourceSize, DestSize);
	
	if (SourceSize != DestSize)
	{
		UE_LOG(LogArchiveDiff, Warning, TEXT("%s: Size mismatch: on disk: %lld vs memory: %lld"), AssetFilename, SourceSize, DestSize);
		int64 SizeDiff = DestPackage.Size - SourcePackage.Size;
		OutStats.FindOrAdd(AssetClass).DiffSize += SizeDiff;
	}

	FString LastDifferenceCallstackDataText;
	int32 LastDifferenceCallstackOffsetIndex = -1;
	int64 NumDiffsLocal = 0;
	int64 NumDiffsLoggedLocal = 0;
	int64 FirstUnreportedDiffIndex = -1;

	for (int64 LocalOffset = 0; LocalOffset < SizeToCompare; ++LocalOffset)
	{
		const int64 SourceAbsoluteOffset = LocalOffset + SourcePackage.StartOffset;
		const int64 DestAbsoluteOffset = LocalOffset + DestPackage.StartOffset;

		if (SourcePackage.Data[SourceAbsoluteOffset] != DestPackage.Data[DestAbsoluteOffset])
		{
			if (DiffMap == nullptr || IsInDiffMap(DestAbsoluteOffset))
			{
				int32 DifferenceCallstackoffsetIndex = GetCallstackAtOffset(DestAbsoluteOffset, FMath::Max(LastDifferenceCallstackOffsetIndex, 0));
				if (DifferenceCallstackoffsetIndex >= 0 && DifferenceCallstackoffsetIndex != LastDifferenceCallstackOffsetIndex)
				{
					const FCallstactAtOffset& CallstackAtOffset = CallstackAtOffsetMap[DifferenceCallstackoffsetIndex];
					const FCallstackData& DifferenceCallstackData = UniqueCallstacks[CallstackAtOffset.Callstack];
					FString DifferenceCallstackDataText = DifferenceCallstackData.ToString(CallstackCutoffText);
					if (LastDifferenceCallstackDataText.Compare(DifferenceCallstackDataText, ESearchCase::CaseSensitive) != 0)
					{
						if (!CallstackAtOffset.bIgnore && (MaxDiffsToLog < 0 || InOutDiffsLogged < MaxDiffsToLog))
						{
							FString BeforePropertyVal;
							FString AfterPropertyVal;
							if (UProperty* SerProp = DifferenceCallstackData.SerializedProp)
							{
								if (SourceSize == DestSize && ShouldDumpPropertyValueState(SerProp))
								{
									// Walk backwards until we find a callstack which wasn't from the given property
									int32 OffsetX = DestAbsoluteOffset;
									for (;;)
									{
										if (OffsetX == 0)
										{
											break;
										}

										int32 CallstackIndex = GetCallstackAtOffset(OffsetX - 1, 0);

										const FCallstactAtOffset& PreviousCallstack = CallstackAtOffsetMap[CallstackIndex];
										if (UniqueCallstacks[PreviousCallstack.Callstack].SerializedProp != SerProp)
										{
											break;
										}

										--OffsetX;
									}

									FPropertyTempVal SourceVal(SerProp);
									FPropertyTempVal DestVal  (SerProp);

									FStaticMemoryReader SourceReader(&SourcePackage.Data[SourceAbsoluteOffset - (DestAbsoluteOffset - OffsetX)], SourcePackage.Size - SourceAbsoluteOffset);
									FStaticMemoryReader DestReader(&DestPackage.Data[OffsetX], DestPackage.Size - DestAbsoluteOffset);

									SourceVal.Serialize(SourceReader);
									DestVal  .Serialize(DestReader);

									if (!SourceReader.ArIsError && !DestReader.ArIsError)
									{
										SourceVal.ExportText(BeforePropertyVal);
										DestVal  .ExportText(AfterPropertyVal);
									}
								}
							}

							FString DiffValues;
							if (BeforePropertyVal != AfterPropertyVal)
							{
								DiffValues = FString::Printf(TEXT("\r\n%sBefore: %s\r\n%sAfter:  %s"), Indent, *BeforePropertyVal, Indent, *AfterPropertyVal);
							}

							FString DebugDataStackText;
#if WITH_EDITOR
							//check for a debug data stack as part of the unique stack entry, and log it out if we find it.
							FString FullStackText = DifferenceCallstackData.Callstack;
							int32 DebugDataIndex = FullStackText.Find(ANSI_TO_TCHAR(DebugDataStackMarker), ESearchCase::CaseSensitive);
							if (DebugDataIndex > 0)
							{
								DebugDataStackText = FString::Printf(TEXT("\r\n%s"), FDiffFormatHelper::Get().Indent) + FullStackText.RightChop(DebugDataIndex + 2);
							}
#endif
							UE_LOG(
								LogArchiveDiff,
								Warning,
								TEXT("%s: Difference at offset %lld%s (absolute offset: %lld), callstack:%s%s%s%s%s"),
								AssetFilename,
								CallstackAtOffset.Offset - DestPackage.StartOffset,
								DestAbsoluteOffset > CallstackAtOffset.Offset ? *FString::Printf(TEXT("(+%lld)"), DestAbsoluteOffset - CallstackAtOffset.Offset) : TEXT(""),
								DestAbsoluteOffset,
								LineTerminator,
								LineTerminator,
								*DifferenceCallstackDataText,
								*DiffValues,
								*DebugDataStackText
							);
							InOutDiffsLogged++;
							NumDiffsLoggedLocal++;
						}
						else if (FirstUnreportedDiffIndex == -1)
						{
							FirstUnreportedDiffIndex = DestAbsoluteOffset;
						}
						LastDifferenceCallstackDataText = MoveTemp(DifferenceCallstackDataText);
						OutStats.FindOrAdd(AssetClass).NumDiffs++;
						NumDiffsLocal++;
					}
				}
				else if (DifferenceCallstackoffsetIndex < 0)
				{
					UE_LOG(LogArchiveDiff, Warning, TEXT("%s: Difference at offset %lld (absolute offset: %lld), unknown callstack"), AssetFilename, LocalOffset, DestAbsoluteOffset);
				}
				LastDifferenceCallstackOffsetIndex = DifferenceCallstackoffsetIndex;
			}
			else
			{
				// Each byte will count as a difference but without callstack data there's no way around it
				OutStats.FindOrAdd(AssetClass).NumDiffs++;
				NumDiffsLocal++;
				if (FirstUnreportedDiffIndex == -1)
				{
					FirstUnreportedDiffIndex = DestAbsoluteOffset;
				}
			}
			OutStats.FindOrAdd(AssetClass).DiffSize++;
		}
	}

	if (MaxDiffsToLog >= 0 && NumDiffsLocal > NumDiffsLoggedLocal)
	{
		if (FirstUnreportedDiffIndex != -1)
		{
			UE_LOG(LogArchiveDiff, Warning, TEXT("%s: %lld difference(s) not logged (first at absolute offset: %lld)."), AssetFilename, NumDiffsLocal - NumDiffsLoggedLocal, FirstUnreportedDiffIndex);
		}
		else
		{
			UE_LOG(LogArchiveDiff, Warning, TEXT("%s: %lld difference(s) not logged."), AssetFilename, NumDiffsLocal - NumDiffsLoggedLocal);
		}
	}
#endif
}

void FArchiveStackTrace::CompareWith(const TCHAR* InFilename, const int64 TotalHeaderSize, const TCHAR* CallstackCutoffText, const int32 MaxDiffsToLog, TMap<FName, FArchiveDiffStats>& OutStats)
{
	FPackageData SourcePackage;

	OutStats.FindOrAdd(AssetClass).NewFileTotalSize = TotalSize();

	if (LoadPackageIntoMemory(InFilename, SourcePackage))
	{	
		FPackageData DestPackage;
		DestPackage.Data = GetData();
		DestPackage.Size = TotalSize();
		DestPackage.HeaderSize = TotalHeaderSize;
		DestPackage.StartOffset = 0;

		UE_LOG(LogArchiveDiff, Display, TEXT("Comparing: %s"), *GetArchiveName());

		int32 NumLoggedDiffs = 0;

		FPackageData SourcePackageHeader = SourcePackage;
		SourcePackageHeader.Size = SourcePackageHeader.HeaderSize;
		SourcePackageHeader.HeaderSize = 0;
		SourcePackageHeader.StartOffset = 0;

		FPackageData DestPackageHeader = DestPackage;
		DestPackageHeader.Size = TotalHeaderSize;
		DestPackageHeader.HeaderSize = 0;
		DestPackageHeader.StartOffset = 0;

		CompareWithInternal(SourcePackageHeader, DestPackageHeader, InFilename, CallstackCutoffText, MaxDiffsToLog, NumLoggedDiffs, OutStats);

		if (TotalHeaderSize > 0 && OutStats.FindOrAdd(AssetClass).NumDiffs > 0)
		{
			DumpPackageHeaderDiffs(SourcePackage, DestPackage, InFilename, MaxDiffsToLog);
		}

		FPackageData SourcePackageExports = SourcePackage;
		SourcePackageExports.HeaderSize = 0;
		SourcePackageExports.StartOffset = SourcePackage.HeaderSize;

		FPackageData DestPackageExports = DestPackage;
		DestPackageExports.HeaderSize = 0;
		DestPackageExports.StartOffset = TotalHeaderSize;

		FString AssetName;
		if (DestPackage.HeaderSize > 0)
		{
			AssetName = FPaths::ChangeExtension(InFilename, TEXT("uexp"));
		}
		else
		{
			AssetName = InFilename;
		}

		CompareWithInternal(SourcePackageExports, DestPackageExports, *AssetName, CallstackCutoffText, MaxDiffsToLog, NumLoggedDiffs, OutStats);

		// Optionally save out any differences we detected.
		const FArchiveDiffStats& Stats = OutStats.FindOrAdd(AssetClass);
		if (Stats.NumDiffs > 0)
		{
			static struct FDiffOutputSettings
			{
				FString DiffOutputDir;

				FDiffOutputSettings()
				{
					FString Dir;
					if (!FParse::Value(FCommandLine::Get(), TEXT("diffoutputdir="), Dir))
					{
						return;
					}

					FPaths::NormalizeDirectoryName(Dir);
					DiffOutputDir = MoveTemp(Dir) + TEXT("/");
				}
			} DiffOutputSettings;

			// Only save out the differences if we have a -diffoutputdir set.
			if (!DiffOutputSettings.DiffOutputDir.IsEmpty())
			{
				FString OutputFilename = FPaths::ConvertRelativePathToFull(InFilename);
				FString SavedDir       = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
				if (OutputFilename.StartsWith(SavedDir))
				{
					OutputFilename.ReplaceInline(*SavedDir, *DiffOutputSettings.DiffOutputDir);

					IFileManager& FileManager = IFileManager::Get();

					// Copy the original asset as '.before.uasset'.
					{
						TUniquePtr<FArchive> DiffUAssetArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".before.") + FPaths::GetExtension(InFilename))));
						DiffUAssetArchive->Serialize(SourcePackageHeader.Data + SourcePackageHeader.StartOffset, SourcePackageHeader.Size - SourcePackageHeader.StartOffset);
					}
					{
						TUniquePtr<FArchive> DiffUExpArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".before.uexp"))));
						DiffUExpArchive->Serialize(SourcePackageExports.Data + SourcePackageExports.StartOffset, SourcePackageExports.Size - SourcePackageExports.StartOffset);
					}

					// Save out the in-memory data as '.after.uasset'.
					{
						TUniquePtr<FArchive> DiffUAssetArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".after.") + FPaths::GetExtension(InFilename))));
						DiffUAssetArchive->Serialize(DestPackageHeader.Data + DestPackageHeader.StartOffset, DestPackageHeader.Size - DestPackageHeader.StartOffset);
					}
					{
						TUniquePtr<FArchive> DiffUExpArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".after.uexp"))));
						DiffUExpArchive->Serialize(DestPackageExports.Data + DestPackageExports.StartOffset, DestPackageExports.Size - DestPackageExports.StartOffset);
					}
				}
				else
				{
					UE_LOG(LogArchiveDiff, Warning, TEXT("Package '%s' doesn't seem to be writing to the Saved directory - skipping writing diff"), *OutputFilename);
				}
			}
		}

		FMemory::Free(SourcePackage.Data);
	}
	else
	{		
		UE_LOG(LogArchiveDiff, Warning, TEXT("New package: %s"), *GetArchiveName());
		OutStats.FindOrAdd(AssetClass).DiffSize = OutStats.FindOrAdd(AssetClass).NewFileTotalSize;
	}
}

bool FArchiveStackTrace::GenerateDiffMapInternal(const FPackageData& SourcePackage, const FPackageData& DestPackage, int32 MaxDiffsToFind, FArchiveDiffMap& OutDiffMap)
{
	bool bIdentical = true;
	int32 LastDifferenceCallstackOffsetIndex = -1;
	FCallstackData* DifferenceCallstackData = nullptr;

	const int64 SourceSize = SourcePackage.Size - SourcePackage.StartOffset;
	const int64 DestSize = DestPackage.Size - DestPackage.StartOffset;
	const int64 SizeToCompare = FMath::Min(SourceSize, DestSize);
	
	for (int64 LocalOffset = 0; LocalOffset < SizeToCompare; ++LocalOffset)
	{
		const int64 SourceAbsoluteOffset = LocalOffset + SourcePackage.StartOffset;
		const int64 DestAbsoluteOffset = LocalOffset + DestPackage.StartOffset;
		if (SourcePackage.Data[SourceAbsoluteOffset] != DestPackage.Data[DestAbsoluteOffset])
		{
			bIdentical = false;
			if (OutDiffMap.Num() < MaxDiffsToFind)
			{
				int64 DifferenceCallstackoffsetIndex = GetCallstackAtOffset(DestAbsoluteOffset, FMath::Max(LastDifferenceCallstackOffsetIndex, 0));
				if (DifferenceCallstackoffsetIndex >= 0 && DifferenceCallstackoffsetIndex != LastDifferenceCallstackOffsetIndex)
				{
					const FCallstactAtOffset& CallstackAtOffset = CallstackAtOffsetMap[DifferenceCallstackoffsetIndex];
					if (!CallstackAtOffset.bIgnore)
					{
						FArchiveDiffInfo OffsetAndSize;
						OffsetAndSize.Offset = CallstackAtOffset.Offset;
						OffsetAndSize.Size = GetSerializedDataSizeForOffsetIndex(DifferenceCallstackoffsetIndex);
						OutDiffMap.Add(OffsetAndSize);
					}
				}
				LastDifferenceCallstackOffsetIndex = DifferenceCallstackoffsetIndex;
			}
		}
	}

	if (SourceSize < DestSize)
	{
		bIdentical = false;

		// Add all the remaining callstacks to the diff map
		for (int32 OffsetIndex = LastDifferenceCallstackOffsetIndex + 1; OffsetIndex < CallstackAtOffsetMap.Num() && OutDiffMap.Num() < MaxDiffsToFind; ++OffsetIndex)
		{
			const FCallstactAtOffset& CallstackAtOffset = CallstackAtOffsetMap[OffsetIndex];
			// Compare against the size without start offset as all callstack offsets are absolute (from the merged header + exports file)
			if (CallstackAtOffset.Offset < DestPackage.Size)
			{
				if (!CallstackAtOffset.bIgnore)
				{
					FArchiveDiffInfo OffsetAndSize;
					OffsetAndSize.Offset = CallstackAtOffset.Offset;
					OffsetAndSize.Size = GetSerializedDataSizeForOffsetIndex(OffsetIndex);
					OutDiffMap.Add(OffsetAndSize);
				}
			}
			else
			{
				break;
			}
		}
	}
	else if (SourceSize > DestSize)
	{
		bIdentical = false;
	}
	return bIdentical;
}

bool FArchiveStackTrace::GenerateDiffMap(const TCHAR* InFilename, int64 TotalHeaderSize, int32 MaxDiffsToFind, FArchiveDiffMap& OutDiffMap)
{
	check(MaxDiffsToFind > 0);

	FPackageData SourcePackage;
	bool bIdentical = LoadPackageIntoMemory(InFilename, SourcePackage);
	if (bIdentical)
	{
		bool bHeaderIdentical = true;
		bool bExportsIdentical = true;

		FPackageData DestPackage;
		DestPackage.Data = GetData();
		DestPackage.Size = TotalSize();
		DestPackage.HeaderSize = TotalHeaderSize;
		DestPackage.StartOffset = 0;

		{
			FPackageData SourcePackageHeader = SourcePackage;
			SourcePackageHeader.Size = SourcePackageHeader.HeaderSize;
			SourcePackageHeader.HeaderSize = 0;
			SourcePackageHeader.StartOffset = 0;

			FPackageData DestPackageHeader = DestPackage;
			DestPackageHeader.Size = TotalHeaderSize;
			DestPackageHeader.HeaderSize = 0;
			DestPackageHeader.StartOffset = 0;

			bHeaderIdentical = GenerateDiffMapInternal(SourcePackageHeader, DestPackageHeader, MaxDiffsToFind, OutDiffMap);
		}

		{
			FPackageData SourcePackageExports = SourcePackage;
			SourcePackageExports.HeaderSize = 0;
			SourcePackageExports.StartOffset = SourcePackage.HeaderSize;

			FPackageData DestPackageExports = DestPackage;
			DestPackageExports.HeaderSize = 0;
			DestPackageExports.StartOffset = TotalHeaderSize;

			bExportsIdentical = GenerateDiffMapInternal(SourcePackageExports, DestPackageExports, MaxDiffsToFind, OutDiffMap);
		}

		bIdentical = bHeaderIdentical && bExportsIdentical;

		FMemory::Free(SourcePackage.Data);
	}

	return bIdentical;
}


bool FArchiveStackTrace::IsIdentical(const TCHAR* InFilename, int64 BufferSize, const uint8* BufferData)
{
	FPackageData SourcePackage;
	bool bIdentical = LoadPackageIntoMemory(InFilename, SourcePackage);

	if (bIdentical)
	{
		if (BufferSize == SourcePackage.Size)
		{
			bIdentical = (FMemory::Memcmp(SourcePackage.Data, BufferData, BufferSize) == 0);
		}
		else
		{
			bIdentical = false;
		}
		FMemory::Free(SourcePackage.Data);
	}
	
	return bIdentical;
}

FLinkerLoad* FArchiveStackTrace::CreateLinkerForPackage(const FString& InPackageName, const FString& InFilename, const FPackageData& PackageData)
{
	// First create a temp package to associate the linker with
	UPackage* Package = FindObjectFast<UPackage>(nullptr, *InPackageName);
	if (!Package)
	{
		Package = CreatePackage(nullptr, *InPackageName);
	}
	// Create an archive for the linker. The linker will take ownership of it.
	FLargeMemoryReader* PackageReader = new FLargeMemoryReader(PackageData.Data, PackageData.Size, ELargeMemoryReaderFlags::None, *InPackageName);	
	FLinkerLoad* Linker = FLinkerLoad::CreateLinker(Package, *InFilename, LOAD_NoVerify, PackageReader);

	if (Linker && Package)
	{
		Package->SetPackageFlags(PKG_ForDiffing);
	}

	return Linker;
}

bool ComparePackageIndices(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FPackageIndex& SourceIndex, const FPackageIndex& DestIndex, bool bMoveOnly);

template <typename T>
bool CompareTableItem(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const T& SourceItem, const T& DestItem, bool bMoveOnly)
{
	check(false);
	return false;
}

template <typename T>
FString ConvertItemToText(const T& Item, FLinkerLoad* Linker)
{
	check(false);
	return TEXT("");
}

template<>
bool CompareTableItem(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FName& SourceName, const FName& DestName, bool bMoveOnly)
{
	return SourceName == DestName;
}

template<>
FString ConvertItemToText(const FName& Name, FLinkerLoad* Linker)
{
	return Name.ToString();
}

template<>
bool CompareTableItem(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FObjectImport& SourceImport, const FObjectImport& DestImport, bool bMoveOnly)
{
	if (SourceImport.ObjectName != DestImport.ObjectName ||
		SourceImport.ClassName != DestImport.ClassName ||
		SourceImport.ClassPackage != DestImport.ClassPackage ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceImport.OuterIndex, DestImport.OuterIndex, bMoveOnly))
	{
		return false;
	}
	else
	{
		return true;
	}
}

static uint32 GetTypeHash(const FObjectImport& Import)
{
	return HashCombine(GetTypeHash(Import.ObjectName), HashCombine(GetTypeHash(Import.OuterIndex), GetTypeHash(Import.ClassName)));
}

static bool IsImportMapIdentical(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker)
{
	bool bIdentical = (SourceLinker->ImportMap.Num() == DestLinker->ImportMap.Num());
	if (bIdentical)
	{
		for (int32 ImportIndex = 0; ImportIndex < SourceLinker->ImportMap.Num(); ++ImportIndex)
		{
			if (!CompareTableItem<FObjectImport>(SourceLinker, DestLinker, SourceLinker->ImportMap[ImportIndex], DestLinker->ImportMap[ImportIndex], false))
			{
				bIdentical = false;
				break;
			}
		}
	}
	return bIdentical;
}

template <>
FString ConvertItemToText(const FObjectImport& Import, FLinkerLoad* Linker)
{
	return FString::Printf(TEXT("%s %s.%s"),
		*Import.ClassName.ToString(),
		!Import.OuterIndex.IsNull() ? *Linker->ImpExp(Import.OuterIndex).ObjectName.ToString() : TEXT("NULL"),
		*Import.ObjectName.ToString());
}

template <>
bool CompareTableItem(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FObjectExport& SourceExport, const FObjectExport& DestExport, bool bMoveOnly)
{
	if (SourceExport.ObjectName != DestExport.ObjectName ||
		SourceExport.TemplateIndex != DestExport.TemplateIndex ||
		SourceExport.PackageGuid != DestExport.PackageGuid ||
		SourceExport.PackageFlags != DestExport.PackageFlags ||
		SourceExport.ObjectFlags != DestExport.ObjectFlags ||
		SourceExport.SerialSize != DestExport.SerialSize ||
		(!bMoveOnly && SourceExport.SerialOffset != DestExport.SerialOffset) || // Offset will be different when two otherwise identical exports are re-arranged
		SourceExport.bForcedExport != DestExport.bForcedExport ||
		SourceExport.bNotForClient != DestExport.bNotForClient ||
		SourceExport.bNotForServer != DestExport.bNotForServer ||
		SourceExport.bNotAlwaysLoadedForEditorGame != DestExport.bNotAlwaysLoadedForEditorGame ||
		SourceExport.bIsAsset != DestExport.bIsAsset ||
		SourceExport.FirstExportDependency != DestExport.FirstExportDependency ||
		SourceExport.SerializationBeforeSerializationDependencies != DestExport.SerializationBeforeSerializationDependencies ||
		SourceExport.CreateBeforeSerializationDependencies != DestExport.CreateBeforeSerializationDependencies ||
		SourceExport.SerializationBeforeCreateDependencies != DestExport.SerializationBeforeCreateDependencies ||
		SourceExport.CreateBeforeCreateDependencies != DestExport.CreateBeforeCreateDependencies ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceExport.OuterIndex, DestExport.OuterIndex, bMoveOnly) ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceExport.ClassIndex, DestExport.ClassIndex, bMoveOnly) ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceExport.SuperIndex, DestExport.SuperIndex, bMoveOnly))
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool ComparePackageIndices(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FPackageIndex& SourceIndex, const FPackageIndex& DestIndex, bool bMoveOnly)
{
	if (SourceIndex.IsNull() && DestIndex.IsNull())
	{
		return true;
	}

	if (SourceIndex.IsExport() && DestIndex.IsExport())
	{
		int32 SourceArrayIndex = SourceIndex.ToExport();
		int32 DestArrayIndex   = DestIndex  .ToExport();

		if (!SourceLinker->ExportMap.IsValidIndex(SourceArrayIndex) || !DestLinker->ExportMap.IsValidIndex(DestArrayIndex))
		{
			UE_LOG(LogArchiveDiff, Warning, TEXT("Invalid export indices found, source: %d (of %d), dest: %d (of %d)"), SourceArrayIndex, SourceLinker->ExportMap.Num(), DestArrayIndex, DestLinker->ExportMap.Num());
			return false;
		}

		const FObjectExport& SourceOuterExport = SourceLinker->Exp(SourceIndex);
		const FObjectExport& DestOuterExport   = DestLinker  ->Exp(DestIndex);

		return CompareTableItem(SourceLinker, DestLinker, SourceOuterExport, DestOuterExport, bMoveOnly);
	}

	if (SourceIndex.IsImport() && DestIndex.IsImport())
	{
		int32 SourceArrayIndex = SourceIndex.ToImport();
		int32 DestArrayIndex   = DestIndex  .ToImport();

		if (!SourceLinker->ImportMap.IsValidIndex(SourceArrayIndex) || !DestLinker->ImportMap.IsValidIndex(DestArrayIndex))
		{
			UE_LOG(LogArchiveDiff, Warning, TEXT("Invalid import indices found, source: %d (of %d), dest: %d (of %d)"), SourceArrayIndex, SourceLinker->ExportMap.Num(), DestArrayIndex, DestLinker->ExportMap.Num());
			return false;
		}

		const FObjectImport& SourceOuterImport = SourceLinker->Imp(SourceIndex);
		const FObjectImport& DestOuterImport   = DestLinker  ->Imp(DestIndex);

		return CompareTableItem(SourceLinker, DestLinker, SourceOuterImport, DestOuterImport, bMoveOnly);
	}

	return false;
}

static uint32 GetTypeHash(const FObjectExport& Export)
{
	return HashCombine(GetTypeHash(Export.ObjectName), 
		HashCombine(GetTypeHash(Export.OuterIndex), 
			HashCombine(GetTypeHash(Export.ClassIndex), GetTypeHash(Export.SuperIndex))));
}

template <>
FString ConvertItemToText(const FObjectExport& Export, FLinkerLoad* Linker)
{
	FName ClassName = Export.ClassIndex.IsNull() ? FName(NAME_Class) : Linker->ImpExp(Export.ClassIndex).ObjectName;
	return FString::Printf(TEXT("%s %s.%s Super: %d, Template: %d, Flags: %d, Size: %lld, Offset: %lld"),
		*ClassName.ToString(),
		!Export.OuterIndex.IsNull() ? *Linker->ImpExp(Export.OuterIndex).ObjectName.ToString() : *FPackageName::GetShortName(Linker->LinkerRoot),
		*Export.ObjectName.ToString(),
		Export.SuperIndex.ForDebugging(),
		Export.TemplateIndex.ForDebugging(),
		(int32)Export.ObjectFlags,
		Export.SerialSize,
		Export.SerialOffset);
}

static bool IsExportMapIdentical(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker)
{
	bool bIdentical = (SourceLinker->ExportMap.Num() == DestLinker->ExportMap.Num());
	if (bIdentical)
	{
		for (int32 ExportIndex = 0; ExportIndex < SourceLinker->ExportMap.Num(); ++ExportIndex)
		{
			if (CompareTableItem<FObjectExport>(SourceLinker, DestLinker, SourceLinker->ExportMap[ExportIndex], DestLinker->ExportMap[ExportIndex], false))
			{
				bIdentical = false;
				break;
			}
		}
	}
	return bIdentical;
}

static void ForceKillPackageAndLinker(FLinkerLoad* Linker)
{
	UPackage* Package = Linker->LinkerRoot;
	Linker->Detach();
	FLinkerManager::Get().RemoveLinker(Linker);
	if (Package)
	{
		Package->ClearPackageFlags(PKG_ContainsMapData | PKG_ContainsMap);
		Package->SetInternalFlags(EInternalObjectFlags::PendingKill);
	}
}

// This is sad, but we can't pass data to the keyfuncs
namespace
{
	FLinkerLoad* GSourceLinker = nullptr;
	FLinkerLoad* GDestLinker   = nullptr;
}

/** Structure that holds an item from the NameMap/ImportMap/ExportMap in a TSet for diffing */
template <typename T>
struct TTableItem
{
	/** Pointer to the original item */
	const T* Item;
	/** Index in the original *Map (table). Only for information purposes. */
	int32 Index;

	TTableItem(const T* InItem, int32 InIndex)
		: Item(InItem)
		, Index(InIndex)
	{}
	TTableItem()
		: Item(nullptr)
		, Index(-1)
	{}

	FORCENOINLINE friend uint32 GetTypeHash(const TTableItem& TableItem)
	{
		// Only get the item hash, ignore Index completely
		return GetTypeHash(*TableItem.Item);
	}
	FORCENOINLINE bool operator == (const TTableItem& Other) const
	{
		// Only compare the item, ignore Index completely
		return CompareTableItem(GSourceLinker, GDestLinker, *Item, *Other.Item, false);
	}
};

/** Dumps differences between Linker tables */
template <typename T>
static void DumpTableDifferences(
	FLinkerLoad* SourceLinker, 
	FLinkerLoad* DestLinker, 
	TArray<T>& SourceTable, 
	TArray<T>& DestTable,
	const TCHAR* AssetFilename,
	const TCHAR* ItemName,
	const int32 MaxDiffsToLog
)
{
#if !NO_LOGGING
	const TCHAR* const LineTerminator = FDiffFormatHelper::Get().LineTerminator;
	const TCHAR* const Indent = FDiffFormatHelper::Get().Indent;

	FString HumanReadableString;
	int32 LoggedDiffs = 0;
	int32 NumDiffs = 0;

	TGuardValue<FLinkerLoad*> SourceLinkerGuard(GSourceLinker, SourceLinker);
	TGuardValue<FLinkerLoad*> DestLinkerGuard  (GDestLinker,   DestLinker);

	TSet<TTableItem<T>> SourceSet;
	TSet<TTableItem<T>> DestSet;

	SourceSet.Reserve(SourceTable.Num());
	DestSet.Reserve(DestTable.Num());

	for (int32 Index = 0; Index < SourceTable.Num(); ++Index)
	{
		SourceSet.Add(TTableItem<T>(&SourceTable[Index], Index));
	}
	for (int32 Index = 0; Index < DestTable.Num(); ++Index)
	{
		DestSet.Add(TTableItem<T>(&DestTable[Index], Index));
	}

	// Determine the list of items removed from the source package and added to the dest package
	TArray<TTableItem<T>> RemovedItems = SourceSet.Difference(DestSet).Array();
	TArray<TTableItem<T>> AddedItems = DestSet.Difference(SourceSet).Array();

	// Now find all items from the above lists that were simply moved to a different index
	TArray<TKeyValuePair<int32, TTableItem<T>>> MovedItems;
	MovedItems.Reserve(FMath::Max(RemovedItems.Num(), AddedItems.Num()));
	for (int32 RemovedItemIndex = RemovedItems.Num() - 1; RemovedItemIndex >= 0; --RemovedItemIndex)
	{
		const TTableItem<T>& RemovedItem = RemovedItems[RemovedItemIndex];
		for (int32 AddedItemIndex = AddedItems.Num() - 1; AddedItemIndex >= 0; --AddedItemIndex)
		{
			const TTableItem<T>& AddedItem = AddedItems[AddedItemIndex];
			// Special compare case here since we don't want to compare item properties that we know change when the item is moved
			if (CompareTableItem(SourceLinker, DestLinker, *RemovedItem.Item, *AddedItem.Item, true))
			{
				MovedItems.Add(TKeyValuePair<int32, TTableItem<T>>(RemovedItem.Index, AddedItem));
				RemovedItems.RemoveAt(RemovedItemIndex);
				AddedItems.RemoveAt(AddedItemIndex);
				break;
			}
		}
	}

	// Dump all changes
	for (const TTableItem<T>& RemovedItem : RemovedItems)
	{
		HumanReadableString += Indent;
		HumanReadableString += FString::Printf(TEXT("-[%d] %s"), RemovedItem.Index, *ConvertItemToText<T>(*RemovedItem.Item, SourceLinker));
		HumanReadableString += LineTerminator;
	}
	for (const TTableItem<T>& AddedItem : AddedItems)
	{
		HumanReadableString += Indent;
		HumanReadableString += FString::Printf(TEXT("+[%d] %s"), AddedItem.Index, *ConvertItemToText<T>(*AddedItem.Item, DestLinker));
		HumanReadableString += LineTerminator;
	}
// This generates a lot of noise about different offsets/indices, which doesn't help us find what we're looking for  - removed for now
#if 0
	for (int32 MovedItemIndex = MovedItems.Num() - 1; MovedItemIndex >= 0; --MovedItemIndex)
	{
		TKeyValuePair<int32, TTableItem<T>>& MovedItem = MovedItems[MovedItemIndex];
		HumanReadableString += Indent;
		HumanReadableString += FString::Printf(TEXT(" [%d] -> [%d] %s"), MovedItem.Key, MovedItem.Value.Index, *ConvertItemToText<T>(*MovedItem.Value.Item, SourceLinker));
		HumanReadableString += LineTerminator;
	}
#endif

	// For now just log everything out. When this becomes too spammy, respect the MaxDiffsToLog parameter
	NumDiffs = RemovedItems.Num() + AddedItems.Num();
	LoggedDiffs = NumDiffs;

	if (NumDiffs > LoggedDiffs)
	{
		HumanReadableString += Indent;
		HumanReadableString += FString::Printf(TEXT("+ %d differences not logged."), (NumDiffs - LoggedDiffs));
		HumanReadableString += LineTerminator;
	}

	UE_LOG(
		LogArchiveDiff,
		Warning,
		TEXT("%s: %sMap is different (%d %ss in source package vs %d %ss in dest package):%s%s"),		
		AssetFilename,
		ItemName,
		SourceTable.Num(),
		ItemName,
		DestTable.Num(),
		ItemName,
		LineTerminator,
		*HumanReadableString);
#endif // !NO_LOGGING
}


void FArchiveStackTrace::DumpPackageHeaderDiffs(const FPackageData& SourcePackage, const FPackageData& DestPackage, const FString& AssetFilename, const int32 MaxDiffsToLog)
{
#if !NO_LOGGING
	FString AssetPathName = FPaths::Combine(*FPaths::GetPath(AssetFilename.Mid(AssetFilename.Find(TEXT(":"), ESearchCase::CaseSensitive) + 1)), *FPaths::GetBaseFilename(AssetFilename));
	FString SourceAssetPackageName = FPaths::Combine(TEXT("/Memory"), TEXT("/SourceForDiff"), *AssetPathName);
	FString DestAssetPackageName = FPaths::Combine(TEXT("/Memory"), TEXT("/DestForDiff"), *AssetPathName);

	TGuardValue<bool> GuardIsSavingPackage(GIsSavingPackage, false);
	TGuardValue<bool> GuardAllowUnversionedContentInEditor(GAllowUnversionedContentInEditor, true);

	// Create linkers. Note there's no need to clean them up here since they will be removed by the package associated with them
	BeginLoad();
	FLinkerLoad* SourceLinker = CreateLinkerForPackage(SourceAssetPackageName, AssetFilename, SourcePackage);
	EndLoad();

	BeginLoad();
	FLinkerLoad* DestLinker = CreateLinkerForPackage(DestAssetPackageName, AssetFilename, DestPackage);
	EndLoad();

	if (SourceLinker && DestLinker)
	{
		if (SourceLinker->NameMap != DestLinker->NameMap)
		{
			DumpTableDifferences<FName>(SourceLinker, DestLinker, SourceLinker->NameMap, DestLinker->NameMap, *AssetFilename, TEXT("Name"), MaxDiffsToLog);
		}

		if (!IsImportMapIdentical(SourceLinker, DestLinker))
		{
			DumpTableDifferences<FObjectImport>(SourceLinker, DestLinker, SourceLinker->ImportMap, DestLinker->ImportMap, *AssetFilename, TEXT("Import"), MaxDiffsToLog);
		}

		if (!IsExportMapIdentical(SourceLinker, DestLinker))
		{
			DumpTableDifferences<FObjectExport>(SourceLinker, DestLinker, SourceLinker->ExportMap, DestLinker->ExportMap, *AssetFilename, TEXT("Export"), MaxDiffsToLog);
		}
	}

	if (SourceLinker)
	{
		ForceKillPackageAndLinker(SourceLinker);
	}
	if (DestLinker)
	{
		ForceKillPackageAndLinker(DestLinker);
	}
#endif // !NO_LOGGING
}

FArchiveStackTraceReader::FArchiveStackTraceReader(const TCHAR* InFilename, const uint8* InData, const int64 Num)
	: FLargeMemoryReader(InData, Num, ELargeMemoryReaderFlags::TakeOwnership, InFilename)
	, ThreadContext(FUObjectThreadContext::Get())
{

}

void FArchiveStackTraceReader::Serialize(void* OutData, int64 Num)
{
	bool bAddData = true;
	FSerializeData NewData(Tell(), Num, ThreadContext.SerializedObject, GetSerializedProperty());
	if (SerializeTrace.Num())
	{
		FSerializeData& Last = SerializeTrace.Last();
		if (Last != NewData)
		{
			SerializeTrace.Add(NewData);
		}
		else
		{
			Last.Size += Num;
			Last.Count++;
		}
	}
	else
	{		
		SerializeTrace.Add(NewData);
	} 
	FLargeMemoryReader::Serialize(OutData, Num);
}

FArchiveStackTraceReader* FArchiveStackTraceReader::CreateFromFile(const TCHAR* InFilename)
{
	FArchiveStackTraceReader* Reader = nullptr;
	FArchiveStackTrace::FPackageData PackageData;
	if (FArchiveStackTrace::LoadPackageIntoMemory(InFilename, PackageData))
	{
		Reader = new FArchiveStackTraceReader(InFilename, PackageData.Data, PackageData.Size);
	}
	return Reader;
}