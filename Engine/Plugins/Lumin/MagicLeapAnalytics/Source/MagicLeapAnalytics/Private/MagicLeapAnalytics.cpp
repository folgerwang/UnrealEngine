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

#include "MagicLeapAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "MagicLeapAnalyticsProvider.h"
#include "MagicLeapPluginUtil.h"

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_app_analytics.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapAnalytics, Display, All);

class FMagicLeapAnalytics : public IMagicLeapAnalyticsPlugin
{
public:
	void StartupModule() override
	{
		IMagicLeapAnalyticsPlugin::StartupModule();
		APISetup.Startup();
		APISetup.LoadDLL(TEXT("ml_app_analytics"));

		MagicLeapAnalyticsProvider = MakeShareable(new FMagicLeapAnalyticsProvider());
	}

	void ShutdownModule() override
	{
		if (MagicLeapAnalyticsProvider.IsValid())
		{
			MagicLeapAnalyticsProvider->EndSession();
		}

		APISetup.Shutdown();
		IMagicLeapAnalyticsPlugin::ShutdownModule();
	}

	TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const override
	{
		return MagicLeapAnalyticsProvider;
	}

private:
	FMagicLeapAPISetup APISetup;
	TSharedPtr<IAnalyticsProvider> MagicLeapAnalyticsProvider;
};

IMPLEMENT_MODULE(FMagicLeapAnalytics, MagicLeapAnalytics);

//////////////////////////////////////////////////////////////////////////

FMagicLeapAnalyticsProvider::FMagicLeapAnalyticsProvider()
: bHasSessionStarted(false)
, LogJson(nullptr)
, FileArchive(nullptr)
{
	UserId = FPlatformMisc::GetLoginId();
}

FMagicLeapAnalyticsProvider::~FMagicLeapAnalyticsProvider()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FMagicLeapAnalyticsProvider::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		EndSession();
	}

	SessionId = UserId + TEXT("-") + FDateTime::Now().ToString();
	LogJson = MakeShareable(new FJsonObject());

	LogJson->SetStringField(TEXT("sessionId"), SessionId);
	LogJson->SetStringField(TEXT("userId"), *UserId);

	TArray<TSharedPtr<FJsonValue>> EventsJsons;
	LogJson->SetArrayField("events", EventsJsons);

	// TODO: add session begin Attributes

	bHasSessionStarted = true;
	UE_LOG(LogMagicLeapAnalytics, Display, TEXT("Session created for user (%s)"), *UserId);

	if (Attributes.Num() > 0)
	{
		RecordEvent(TEXT("sessionStart"), Attributes);
	}

#if !PLATFORM_LUMIN
	const FString FileName = FPaths::ProjectSavedDir() + TEXT("Analytics/") + TEXT("ML-") + SessionId + TEXT(".analytics");
	// Close the old file and open a new one
	FileArchive = IFileManager::Get().CreateFileWriter(*FileName);
	UE_LOG(LogMagicLeapAnalytics, Display, TEXT("Analytics will be saved in : %s"), *FileName);
#endif

	return true;
}

void FMagicLeapAnalyticsProvider::EndSession()
{
#if WITH_MLSDK
	if (LogJson.IsValid())
	{
		FString JsonStr;
		TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
		FJsonSerializer::Serialize<TCHAR>(LogJson.ToSharedRef(), Writer, true);

		MLAppAnalyticsError error;
		bool bResult = MLAppAnalyticsUpload(TCHAR_TO_UTF8(*SessionId), TCHAR_TO_UTF8(*JsonStr), &error);
		if (bResult)
		{
			UE_LOG(LogMagicLeapAnalytics, Display, TEXT("Session ended for user (%s) and session id (%s)"), *UserId, *SessionId);
		}
		else
		{
			UE_LOG(LogMagicLeapAnalytics, Error, TEXT("Error saving analytics. Error code = %d"), static_cast<int32>(error));
		}

#if !PLATFORM_LUMIN
		if (FileArchive != nullptr)
		{
			FileArchive->Logf(TEXT("%s"), *JsonStr);
			FileArchive->Flush();
			FileArchive->Close();
			delete FileArchive;
			FileArchive = nullptr;
		}
#endif

		LogJson.Reset();
	}

	bHasSessionStarted = false;
#endif //WITH_MLSDK
}

void FMagicLeapAnalyticsProvider::FlushEvents()
{}

void FMagicLeapAnalyticsProvider::SetUserID(const FString& InUserID)
{
	if (!bHasSessionStarted)
	{
		UserId = InUserID;
		UE_LOG(LogMagicLeapAnalytics, Display, TEXT("User is now (%s)"), *UserId);
	}
	else
	{
		// Log that we shouldn't switch users during a session
		UE_LOG(LogMagicLeapAnalytics, Warning, TEXT("FMagicLeapAnalyticsProvider::SetUserID called while a session is in progress. Ignoring."));
	}
}

FString FMagicLeapAnalyticsProvider::GetUserID() const
{
	return UserId;
}

FString FMagicLeapAnalyticsProvider::GetSessionID() const
{
	return SessionId;
}

bool FMagicLeapAnalyticsProvider::SetSessionID(const FString& InSessionID)
{
	if (!bHasSessionStarted)
	{
		SessionId = InSessionID;
		UE_LOG(LogMagicLeapAnalytics, Display, TEXT("Session is now (%s)"), *SessionId);
	}
	else
	{
		// Log that we shouldn't switch session ids during a session
		UE_LOG(LogMagicLeapAnalytics, Warning, TEXT("FMagicLeapAnalyticsProvider::SetSessionID called while a session is in progress. Ignoring."));
	}
	return !bHasSessionStarted;
}

void FMagicLeapAnalyticsProvider::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		check(LogJson.IsValid());

		TSharedPtr<FJsonObject> Event = MakeShareable(new FJsonObject());
		Event->SetStringField(TEXT("eventName"), *EventName);

		if (Attributes.Num() > 0)
		{
			TSharedPtr<FJsonObject> AttributeJson = MakeShareable(new FJsonObject());

			for (auto Attr : Attributes)
			{
				AttributeJson->SetStringField(*Attr.AttrName, *Attr.AttrValueString);
			}
			Event->SetObjectField(TEXT("attributes"), AttributeJson);
		}

		TArray<TSharedPtr<FJsonValue>> AllEventsJsons;
		AllEventsJsons = LogJson->GetArrayField(TEXT("events"));
		TSharedRef<FJsonValueObject> EventJsonValue = MakeShareable(new FJsonValueObject(Event));
		AllEventsJsons.Add(EventJsonValue);
		LogJson->SetArrayField(TEXT("events"), AllEventsJsons);

		UE_LOG(LogMagicLeapAnalytics, Display, TEXT("Analytics event (%s) written with (%d) attributes"), *EventName, Attributes.Num());
	}
	else
	{
		UE_LOG(LogMagicLeapAnalytics, Warning, TEXT("FMagicLeapAnalyticsProvider::RecordEvent called before StartSession. Ignoring."));
	}
}
