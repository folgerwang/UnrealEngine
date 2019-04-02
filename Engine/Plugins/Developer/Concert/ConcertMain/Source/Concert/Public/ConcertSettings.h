// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "ConcertTransportSettings.h"
#include "ConcertSettings.generated.h"

USTRUCT()
struct FConcertSessionSettings
{
	GENERATED_BODY()

	FConcertSessionSettings()
		: BaseRevision(0)
	{}

	void Initialize()
	{
		ProjectName = FApp::GetProjectName();
		CompatibleVersion = FEngineVersion::CompatibleWith().ToString(EVersionComponent::Changelist);
		BaseRevision = FEngineVersion::Current().GetChangelist(); // TODO: This isn't good enough for people using binary builds
	}

	bool ValidateRequirements(const FConcertSessionSettings& Other, FText* OutFailureReason = nullptr) const
	{
		if (ProjectName != Other.ProjectName)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FText::Format(NSLOCTEXT("ConcertMain", "Error_InvalidProjectNameFmt", "Invalid project name (expected '{0}', got '{1}')"), FText::AsCultureInvariant(ProjectName), FText::AsCultureInvariant(Other.ProjectName));
			}
			return false;
		}

		if (CompatibleVersion != Other.CompatibleVersion)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FText::Format(NSLOCTEXT("ConcertMain", "Error_InvalidEngineVersionFmt", "Invalid engine version (expected '{0}', got '{1}')"), FText::AsCultureInvariant(CompatibleVersion), FText::AsCultureInvariant(Other.CompatibleVersion));
			}
			return false;
		}

		if (BaseRevision != Other.BaseRevision)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FText::Format(NSLOCTEXT("ConcertMain", "Error_InvalidBaseRevisionFmt", "Invalid base revision (expected '{0}', got '{1}')"), BaseRevision, Other.BaseRevision);
			}
			return false;
		}

		return true;
	}

	/**
	 * Name of the project of the session.
	 * Can be specified on the server cmd with `-CONCERTPROJECT=`
	 */
	UPROPERTY(config, VisibleAnywhere, Category="Session Settings")
	FString ProjectName;

	/**
	 * Compatible editor version for the session.
	 * Can be specified on the server cmd with `-CONCERTVERSION=`
	 */
	UPROPERTY(config, VisibleAnywhere, Category="Session Settings")
	FString CompatibleVersion;

	/**
	 * Base Revision the session is created at.
	 * Can be specified on the server cmd with `-CONCERTREVISION=`
	 */
	UPROPERTY(config, VisibleAnywhere, Category="Session Settings")
	uint32 BaseRevision;

	/**
	 * This allow the session to be created with the data from a saved session.
	 * Set the name of the desired save to restore its content in your session.
	 * Leave this blank if you want to create an empty session.
	 * Can be specified on the server cmd with `-CONCERTSESSIONTORESTORE=`
	 */
	UPROPERTY(config, VisibleAnywhere, Category="Session Settings")
	FString SessionToRestore;

	/**
	 * This allow the session data to be saved when the session is deleted.
	 * Set the name desired for the save and the session data will be moved in that save when the session is deleted
	 * Leave this blank if you don't want to save the session data.
	 * Can be specified on the server cmd with `-CONCERTSAVESESSIONAS=`
	 */
	UPROPERTY(config, VisibleAnywhere, Category="Session Settings")
	FString SaveSessionAs;

	// TODO: private session, password, etc etc,
};

USTRUCT()
struct FConcertServerSettings
{
	GENERATED_BODY()

	FConcertServerSettings()
		: bIgnoreSessionSettingsRestriction(false)
		, SessionTickFrequencySeconds(1)
	{}

	/** The server will allow client to join potentially incompatible sessions */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category="Server Settings")
	bool bIgnoreSessionSettingsRestriction;

	/** The timespan at which session updates are processed. */
	UPROPERTY(config, EditAnywhere, DisplayName="Session Tick Frequency", AdvancedDisplay, Category="Server Settings", meta=(ForceUnits=s))
	int32 SessionTickFrequencySeconds;
};

UCLASS(config=Engine)
class CONCERT_API UConcertServerConfig : public UObject
{
	GENERATED_BODY()
public:
	UConcertServerConfig();

	/**
	 * Clean server sessions working directory when booting
	 * Can be specified on the server cmd with `-CONCERTCLEAN`
	 */
	UPROPERTY(config, EditAnywhere, Category="Server Settings")
	bool bCleanWorkingDir;

	/** 
	 * Name of the default session created on the server.
	 * Can be specified on the server cmd with `-CONCERTSESSION=`
	 */
	UPROPERTY(config, EditAnywhere, Category="Session Settings")
	FString DefaultSessionName;

	/** Default server session settings */
	UPROPERTY(config, EditAnywhere, Category="Session Settings")
	FConcertSessionSettings DefaultSessionSettings;

	/** Server & server session settings */
	UPROPERTY(config, EditAnywhere, Category="Server Settings", meta=(ShowOnlyInnerProperties))
	FConcertServerSettings ServerSettings;

	/** Endpoint settings passed down to endpoints on creation */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category="Endpoint Settings", meta=(ShowOnlyInnerProperties))
	FConcertEndpointSettings EndpointSettings;
};

USTRUCT()
struct FConcertClientSettings
{
	GENERATED_BODY()

	FConcertClientSettings()
		: DisplayName()
		, AvatarColor(1.0f, 1.0f, 1.0f, 1.0f)
		, DesktopAvatarActorClass(TEXT("/ConcertSyncClient/DesktopPresence.DesktopPresence_C"))
		, VRAvatarActorClass(TEXT("/ConcertSyncClient/VRPresence.VRPresence_C"))
		, DiscoveryTimeoutSeconds(5)
		, SessionTickFrequencySeconds(1)
		, LatencyCompensationMs(0)
	{}

	/** 
	 * The display name to use when in a session. 
	 * Can be specified on the editor cmd with `-CONCERTDISPLAYNAME=`.
	 */
	UPROPERTY(config, EditAnywhere, Category="Client Settings")
	FString DisplayName;

	/** The color used for the presence avatar in a session. */
	UPROPERTY(config, EditAnywhere, Category="Client Settings")
	FLinearColor AvatarColor;

	/** The desktop representation of this editor's user to other connected users */
	UPROPERTY(config, EditAnywhere, NoClear, Category = "Client Settings", meta = (MetaClass = "ConcertClientDesktopPresenceActor"))
	FSoftClassPath DesktopAvatarActorClass;

	/** The VR representation of this editor's user to other connected users */
	UPROPERTY(config, EditAnywhere, NoClear, Category = "Client Settings", meta = (MetaClass = "ConcertClientVRPresenceActor", DisplayName = "VR Avatar Actor Class"))
	FSoftClassPath VRAvatarActorClass;

	/** The timespan at which discovered Concert server are considered stale if they haven't answered back */
	UPROPERTY(config, EditAnywhere, DisplayName="Discovery Timeout", AdvancedDisplay, Category="Client Settings", meta=(ForceUnits=s))
	int32 DiscoveryTimeoutSeconds;

	/** The timespan at which session updates are processed. */
	UPROPERTY(config, EditAnywhere, DisplayName="Session Tick Frequency", AdvancedDisplay, Category="Client Settings", meta=(ForceUnits=s))
	int32 SessionTickFrequencySeconds;

	/** Amount of latency compensation to apply to time-synchronization sensitive interactions */
	UPROPERTY(config, EditAnywhere, DisplayName="Latency Compensation", AdvancedDisplay, Category="Client Settings", meta=(ForceUnits=ms))
	float LatencyCompensationMs;
};

UCLASS(config=Engine)
class CONCERT_API UConcertClientConfig : public UObject
{
	GENERATED_BODY()
public:
	UConcertClientConfig();

	/** 
	 * Automatically connect or create default session on default server. 
	 * Can be specified on the editor cmd with `-CONCERTAUTOCONNECT` or `-CONCERTAUTOCONNECT=<true/false>`.
	 */
	UPROPERTY(config, EditAnywhere, Category="Client Settings")
	bool bAutoConnect;

	/** 
	 * Default server url (just a name for now) to look for on auto or default connect. 
 	 * Can be specified on the editor cmd with `-CONCERTSERVER=`.
	 */
	UPROPERTY(config, EditAnywhere, Category="Client Settings")
	FString DefaultServerURL;

	/** 
	 * Default session name to look for on auto connect or default connect.
	 * Can be specified on the editor cmd with `-CONCERTSESSION=`.
	 */
	UPROPERTY(config, EditAnywhere, Category="Client Settings")
	FString DefaultSessionName;

	/**
	 * If this client create the default session, should the session restore a saved session.
	 * Set the name of the desired save to restore its content in your session.
	 * Leave this blank if you want to create an empty session.
	 * Can be specified on the editor cmd with `-CONCERTSESSIONTORESTORE=`.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Client Settings")
	FString DefaultSessionToRestore;

	/**
	 * If this client create the default session, should the session data be saved when it's deleted.
	 * Set the name desired for the save and the session data will be moved in that save when the session is deleted
	 * Leave this blank if you don't want to save the session data.
	 * Can be specified on the editor cmd with `-CONCERTSAVESESSIONAS=`.
	*/
	UPROPERTY(config, EditAnywhere, Category = "Client Settings")
	FString DefaultSaveSessionAs;

	/** Client & client session settings */
	UPROPERTY(config, EditAnywhere, Category="Client Settings", meta=(ShowOnlyInnerProperties))
	FConcertClientSettings ClientSettings;

	/** Endpoint settings passed down to endpoints on creation */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category="Endpoint Settings", meta=(ShowOnlyInnerProperties))
	FConcertEndpointSettings EndpointSettings;
};
