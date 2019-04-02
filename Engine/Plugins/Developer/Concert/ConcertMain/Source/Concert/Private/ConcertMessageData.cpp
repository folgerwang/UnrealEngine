// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertMessageData.h"
#include "IdentifierTable/ConcertTransportArchives.h"

#include "Misc/App.h"
#include "UObject/StructOnScope.h"

void FConcertInstanceInfo::Initialize()
{
	InstanceId = FApp::GetInstanceId();
	InstanceName = FApp::GetInstanceName();

	if (IsRunningDedicatedServer())
	{
		InstanceType = TEXT("Server");
	}
	else if (FApp::IsGame())
	{
		InstanceType = TEXT("Game");
	}
	else if (IsRunningCommandlet())
	{
		InstanceType = TEXT("Commandlet");
	}
	else if (GIsEditor)
	{
		InstanceType = TEXT("Editor");
	}
	else
	{
		InstanceType = TEXT("Other");
	}
}

FText FConcertInstanceInfo::ToDisplayString() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertInstanceInfo", "InstanceName", "Instance Name: {0}"), FText::FromString(InstanceName));
	return TextBuilder.ToText();
}

void FConcertServerInfo::Initialize()
{
	ServerName = FPlatformProcess::ComputerName();
	InstanceInfo.Initialize();
	InstanceInfo.InstanceType = TEXT("Server");
	ServerFlags = EConcertSeverFlags::None;
}

FText FConcertServerInfo::ToDisplayString() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertServerInfo", "ServerName", "Server Name: {0}"), FText::FromString(ServerName));
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertServerInfo", "AdminEndpointId", "Admin Endpoint ID: {0}"), FText::FromString(AdminEndpointId.ToString()));
	TextBuilder.AppendLine(InstanceInfo.ToDisplayString());
	return TextBuilder.ToText();
}

void FConcertClientInfo::Initialize()
{
	InstanceInfo.Initialize();
	DeviceName = FPlatformProcess::ComputerName();
	PlatformName = FPlatformProperties::PlatformName();
	UserName = FApp::GetSessionOwner();
	bHasEditorData = WITH_EDITORONLY_DATA;
	bRequiresCookedData = FPlatformProperties::RequiresCookedData();
}

FText FConcertClientInfo::ToDisplayString() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertClientInfo", "DeviceName", "Device Name: {0}"), FText::FromString(DeviceName));
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertClientInfo", "PlatformName", "Platform Name: {0}"), FText::FromString(PlatformName));
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertClientInfo", "UserName", "User Name: {0}"), FText::FromString(UserName));
	TextBuilder.AppendLine(InstanceInfo.ToDisplayString());
	return TextBuilder.ToText();
}

FText FConcertSessionClientInfo::ToDisplayString() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLine(ClientInfo.ToDisplayString());
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertSessionClientInfo", "ClientEndpointId", "Client Endpoint ID: {0}"), FText::FromString(ClientEndpointId.ToString()));
	return TextBuilder.ToText();
}

FText FConcertSessionInfo::ToDisplayString() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertSessionInfo", "SessionName", "Session Name: {0}"), FText::FromString(SessionName));
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertSessionInfo", "OwnerUserName", "Owner User Name: {0}"), FText::FromString(OwnerUserName));
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertSessionInfo", "ProjectName", "Session Project: {0}"), FText::FromString(Settings.ProjectName));
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertSessionInfo", "CompatibleVersion", "Session Version: {0}"), FText::FromString(Settings.CompatibleVersion));
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertSessionInfo", "BaseRevision", "Session Base Revision: {0}"), FText::AsNumber(Settings.BaseRevision, &FNumberFormattingOptions::DefaultNoGrouping()));
	TextBuilder.AppendLineFormat(NSLOCTEXT("ConcertSessionInfo", "ServerEndpointId", "Server Endpoint ID: {0}"), FText::FromString(ServerEndpointId.ToString()));
	return TextBuilder.ToText();
}

namespace PayloadDetail
{
	bool SerializePayload(const UScriptStruct* InEventType, const void* InEventData, int32& OutUncompressedDataSizeBytes, TArray<uint8>& OutCompressedData)
	{
		bool bSuccess = false;

		OutUncompressedDataSizeBytes = 0;
		OutCompressedData.Reset();

		if (InEventType && InEventData)
		{
			// Serialize the uncompressed data
			TArray<uint8> UncompressedData;
			{
				FConcertIdentifierWriter Archive(nullptr, UncompressedData);
				Archive.SetWantBinaryPropertySerialization(true);
				const_cast<UScriptStruct*>(InEventType)->SerializeItem(Archive, (uint8*)InEventData, nullptr);
				bSuccess = !Archive.GetError();
			}

			if (bSuccess)
			{
				// if we serialized something, compress it
				if (UncompressedData.Num() > 0)
				{
					// Compress the result to send on the wire
					int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, UncompressedData.Num());
					OutCompressedData.SetNumUninitialized(CompressedSize);
					if (FCompression::CompressMemory(NAME_Zlib, OutCompressedData.GetData(), CompressedSize, UncompressedData.GetData(), UncompressedData.Num()))
					{
						OutUncompressedDataSizeBytes = UncompressedData.Num();
						OutCompressedData.SetNum(CompressedSize, false);
					}
					else
					{
						bSuccess = false;
						OutUncompressedDataSizeBytes = 0;
						OutCompressedData.Reset();
					}
				}
				// didn't have anything to compress or serialize
				else
				{
					bSuccess = true;
					OutUncompressedDataSizeBytes = 0;
				}
			}

		}

		return bSuccess;
	}

	bool DeserializePayload(const UScriptStruct* InEventType, void* InOutEventData, const int32 InUncompressedDataSizeBytes, const TArray<uint8>& InCompressedData)
	{
		bool bSuccess = false;

		if (InEventType && InOutEventData)
		{
			// Don't bother if we do not actually have anything to deserialize
			if (InUncompressedDataSizeBytes > 0)
			{
				// Uncompress the data
				TArray<uint8> UncompressedData;
				UncompressedData.SetNumUninitialized(InUncompressedDataSizeBytes);
				if (FCompression::UncompressMemory(NAME_Zlib, UncompressedData.GetData(), UncompressedData.Num(), InCompressedData.GetData(), InCompressedData.Num()))
				{
					// Deserialize the uncompressed data
					{
						FConcertIdentifierReader Archive(nullptr, UncompressedData);
						Archive.SetWantBinaryPropertySerialization(true);
						const_cast<UScriptStruct*>(InEventType)->SerializeItem(Archive, (uint8*)InOutEventData, nullptr);
						bSuccess = !Archive.GetError();
					}
				}
			}
			else
			{
				bSuccess = true;
			}
		}

		return bSuccess;
	}
}

bool FConcertSessionSerializedPayload::SetPayload(const FStructOnScope& InPayload)
{
	const UStruct* PayloadStruct = InPayload.GetStruct();
	check(PayloadStruct->IsA<UScriptStruct>());
	return SetPayload((UScriptStruct*)PayloadStruct, InPayload.GetStructMemory());
}

bool FConcertSessionSerializedPayload::SetPayload(const UScriptStruct* InPayloadType, const void* InPayloadData)
{
	PayloadTypeName = *InPayloadType->GetPathName();
	return PayloadDetail::SerializePayload(InPayloadType, InPayloadData, UncompressedPayloadSize, CompressedPayload);
}

bool FConcertSessionSerializedPayload::GetPayload(FStructOnScope& OutPayload) const
{
	const UStruct* PayloadType = FindObject<UStruct>(nullptr, *PayloadTypeName.ToString());
	if (PayloadType != nullptr)
	{
		OutPayload.Initialize(PayloadType);
		const UStruct* PayloadStruct = OutPayload.GetStruct();
		check(PayloadStruct->IsA<UScriptStruct>());
		return PayloadDetail::DeserializePayload((UScriptStruct*)PayloadStruct, OutPayload.GetStructMemory(), UncompressedPayloadSize, CompressedPayload);
	}
	return false;
}

uint32 FConcertSessionSerializedPayload::GetPayloadDataHash() const
{
	return FCrc::MemCrc32(CompressedPayload.GetData(), CompressedPayload.Num());
}
