// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "Engine/EngineTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "MediaProfileSettings.generated.h"

class UMediaProfile;
class UProxyMediaOutput;
class UProxyMediaSource;


/**
 * Settings for the media profile.
 */
UCLASS(config=Game, defaultconfig)
class MEDIAFRAMEWORKUTILITIES_API UMediaProfileSettings
	: public UObject
{
	GENERATED_BODY()	

public:

	/**
	 * Apply the startup media profile even when we are running a commandlet.
	 * @note We always try to apply the user media profile before the startup media profile in the editor or standalone.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MediaProfile")
	bool bApplyInCommandlet;

private:

	UPROPERTY(AdvancedDisplay, Config, EditAnywhere, Category="MediaProfile")
	TArray<TSoftObjectPtr<UProxyMediaSource>> MediaSourceProxy;

	UPROPERTY(AdvancedDisplay, Config, EditAnywhere, Category="MediaProfile")
	TArray<TSoftObjectPtr<UProxyMediaOutput>> MediaOutputProxy;

	/**
	 * The media profile to use at startup.
	 * @note The media profile can be overriden in the editor by user.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MediaProfile")
	TSoftObjectPtr<UMediaProfile> StartupMediaProfile;

public:

	/**
	 * Get all the media source proxy.
	 *
	 * @return The an array of the media source proxy.
	 */
	TArray<UProxyMediaSource*> GetAllMediaSourceProxy() const;

	/**
	 * Get all the media output proxy.
	 *
	 * @return The an array of the media output proxy.
	 */
	TArray<UProxyMediaOutput*> GetAllMediaOutputProxy() const;

	/**
	 * Get the media profile used by the engine.
	 *
	 * @return The media profile, or nullptr if not set.
	 */
	UMediaProfile* GetStartupMediaProfile() const;
};

/**
 * Settings for the media profile in the editor or standalone.
 * @note For cook games always use the startup media profile
 */
UCLASS(config=EditorPerProjectUserSettings)
class MEDIAFRAMEWORKUTILITIES_API UMediaProfileEditorSettings
	: public UObject
{
	GENERATED_BODY()

	UMediaProfileEditorSettings();

public:

	/**
	 * Display the media profile icon in the editor toolbar.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "MediaProfile", meta=(ConfigRestartRequired=true))
	bool bDisplayInToolbar;

private:

	/**
	 * The media profile to use in standalone & editor.
	 * @note The startup media profile in the project setting will be used when in cooked game.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MediaProfile")
	TSoftObjectPtr<UMediaProfile> UserMediaProfile;

public:

	/**
	 * Get the media profile used by the engine when in the editor & standalone.
	 *
	 * @return The media profile, or nullptr if not set.
	 */
	UMediaProfile* GetUserMediaProfile() const;

	/** Set the media profile used by the engine when in the editor & standalone. */
	void SetUserMediaProfile(UMediaProfile* InMediaProfile) ;
};
