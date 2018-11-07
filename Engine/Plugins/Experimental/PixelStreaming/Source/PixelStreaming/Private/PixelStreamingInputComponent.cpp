// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingInputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IPixelStreamingPlugin.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/GameUserSettings.h"

extern TAutoConsoleVariable<float> CVarStreamerBitrateReduction;

UPixelStreamingInputComponent::UPixelStreamingInputComponent()
	: PixelStreamingPlugin(FModuleManager::Get().GetModule("PixelStreaming") ? &FModuleManager::Get().GetModuleChecked<IPixelStreamingPlugin>("PixelStreaming") : nullptr)
{
}

bool UPixelStreamingInputComponent::OnCommand(const FString& Descriptor)
{
	FString ConsoleCommand;
	if (GetJsonStringField(Descriptor, TEXT("ConsoleCommand"), ConsoleCommand))
	{
		return GEngine->Exec(GetWorld(), *ConsoleCommand);
	}
	
	FString WidthString;
	FString HeightString;
	if (GetJsonStringField(Descriptor, TEXT("Resolution.Width"), WidthString) &&
		GetJsonStringField(Descriptor, TEXT("Resolution.Height"), HeightString))
	{
		FIntPoint Resolution = { FCString::Atoi(*WidthString), FCString::Atoi(*HeightString) };
		GEngine->GameUserSettings->SetScreenResolution(Resolution);
		GEngine->GameUserSettings->ApplySettings(false);
		return true;
	}

	FString BitrateReductionString;
	if (GetJsonStringField(Descriptor, TEXT("Encoder.BitrateReduction"), BitrateReductionString))
	{
		float BitrateReduction = FCString::Atof(*BitrateReductionString);
		CVarStreamerBitrateReduction->Set(BitrateReduction);
		return true;
	}
	
	return false;
}

void UPixelStreamingInputComponent::SendPixelStreamingResponse(const FString& Descriptor)
{
	PixelStreamingPlugin->SendResponse(Descriptor);
}

bool UPixelStreamingInputComponent::GetJsonStringField(FString Descriptor, FString FieldName, FString& StringField)
{
	bool Success;
	GetJsonStringValue(Descriptor, FieldName, StringField, Success);
	return Success;
}

void UPixelStreamingInputComponent::GetJsonStringValue(FString Descriptor, FString FieldName, FString& StringValue, bool& Success)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Descriptor);
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* JsonObjectPtr = &JsonObject;

		if (FieldName.Contains(TEXT(".")))
		{
			TArray<FString> FieldComponents;
			FieldName.ParseIntoArray(FieldComponents, TEXT("."));
			FieldName = FieldComponents.Pop();

			for (const FString& FieldComponent : FieldComponents)
			{
				if (!(*JsonObjectPtr)->TryGetObjectField(FieldComponent, JsonObjectPtr))
				{
					Success = false;
					return;
				}
			}
		}

		Success = (*JsonObjectPtr)->TryGetStringField(FieldName, StringValue);
	}
	else
	{
		Success = false;
	}
}

void UPixelStreamingInputComponent::AddJsonStringValue(const FString& Descriptor, FString FieldName, FString StringValue, FString& NewDescriptor, bool& Success)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	if (!Descriptor.IsEmpty())
	{
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Descriptor);
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			Success = false;
			return;
		}
	}
	
	TSharedRef<FJsonValueString> JsonValueObject = MakeShareable(new FJsonValueString(StringValue));
	JsonObject->SetField(FieldName, JsonValueObject);

	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&NewDescriptor);
	Success = FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter);
}
