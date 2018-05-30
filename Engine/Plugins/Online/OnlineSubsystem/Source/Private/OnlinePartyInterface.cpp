// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Interfaces/OnlinePartyInterface.h"
#include "OnlineSubsystem.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonTypes.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogOnlineParty);

bool FOnlinePartyData::operator==(const FOnlinePartyData& Other) const
{
	// Only compare KeyValAttrs, other fields are optimization details
	return KeyValAttrs.OrderIndependentCompareEqual(Other.KeyValAttrs);
}

bool FOnlinePartyData::operator!=(const FOnlinePartyData& Other) const
{
	return !operator==(Other);
}

void FOnlinePartyData::ToJsonFull(FString& JsonString) const
{
	JsonString.Empty();

	// iterate over key/val attrs and convert each entry to a json string
	TSharedRef<FJsonObject> JsonObject(new FJsonObject());
	TSharedRef<FJsonObject> JsonProperties = MakeShared<FJsonObject>();
	for (auto Iterator : KeyValAttrs)
	{
		const FString& PropertyName = Iterator.Key;
		const FVariantData& PropertyValue = Iterator.Value;

		PropertyValue.AddToJsonObject(JsonProperties, PropertyName);
	}
	JsonObject->SetNumberField(TEXT("Rev"), RevisionCount);
	JsonObject->SetObjectField(TEXT("Attrs"), JsonProperties);

	auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject, JsonWriter);
	JsonWriter->Close();
}

void FOnlinePartyData::ToJsonDirty(FString& JsonString) const
{
	JsonString.Empty();

	// iterate over key/val attrs and convert each entry to a json string
	TSharedRef<FJsonObject> JsonObject(new FJsonObject());
	TSharedRef<FJsonObject> JsonProperties = MakeShared<FJsonObject>();
	for (auto& PropertyName : DirtyKeys)
	{
		const FVariantData* PropertyValue = KeyValAttrs.Find(PropertyName);
		check(PropertyValue);

		PropertyValue->AddToJsonObject(JsonProperties, PropertyName);
	}
	JsonObject->SetNumberField(TEXT("Rev"), RevisionCount);
	JsonObject->SetObjectField(TEXT("Attrs"), JsonProperties);

	auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject, JsonWriter);
	JsonWriter->Close();
}

void FOnlinePartyData::FromJson(const FString& JsonString)
{
	// json string to key/val attrs
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
		JsonObject.IsValid())
	{
		if (JsonObject->HasTypedField<EJson::Object>(TEXT("Attrs")))
		{
			const TSharedPtr<FJsonObject>& JsonProperties = JsonObject->GetObjectField(TEXT("Attrs"));
			for (auto& JsonProperty : JsonProperties->Values)
			{
				FString PropertyName;
				FVariantData PropertyData;
				if (PropertyData.FromJsonValue(JsonProperty.Key, JsonProperty.Value.ToSharedRef(), PropertyName))
				{
					KeyValAttrs.Add(PropertyName, PropertyData);
				}
			}
		}

		if (JsonObject->HasTypedField<EJson::Number>(TEXT("Rev")))
		{
			int32 NewRevisionCount = JsonObject->GetIntegerField(TEXT("Rev"));
			if ((RevisionCount != 0) && (NewRevisionCount != RevisionCount) && (NewRevisionCount != (RevisionCount + 1)))
			{
				UE_LOG_ONLINE_PARTY(Warning, TEXT("Unexpected revision received.  Current %d, new %d"), RevisionCount, NewRevisionCount);
			}
			RevisionCount = NewRevisionCount;
		}
	}
}

bool FPartyConfiguration::operator==(const FPartyConfiguration& Other) const
{
	return JoinRequestAction == Other.JoinRequestAction &&
		PresencePermissions == Other.PresencePermissions &&
		InvitePermissions == Other.InvitePermissions &&
		bChatEnabled == Other.bChatEnabled &&
		bIsAcceptingMembers == Other.bIsAcceptingMembers &&
		NotAcceptingMembersReason == Other.NotAcceptingMembersReason &&
		MaxMembers == Other.MaxMembers &&
		Nickname == Other.Nickname &&
		Description == Other.Description &&
		Password == Other.Password;
}

bool FPartyConfiguration::operator!=(const FPartyConfiguration& Other) const
{
	return !operator==(Other);
}
