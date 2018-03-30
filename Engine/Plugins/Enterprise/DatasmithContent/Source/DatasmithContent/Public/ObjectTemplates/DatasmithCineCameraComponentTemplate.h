// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithCineCameraComponentTemplate.generated.h"

struct FCameraFilmbackSettings;
struct FCameraFocusSettings;
struct FCameraLensSettings;

USTRUCT()
struct FDatasmithCameraFilmbackSettingsTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	float SensorWidth;

	UPROPERTY()
	float SensorHeight;

	void Apply( FCameraFilmbackSettings* Destination, const FDatasmithCameraFilmbackSettingsTemplate* PreviousTemplate );
	void Load( const FCameraFilmbackSettings& Source );
};

USTRUCT()
struct FDatasmithCameraLensSettingsTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	float MaxFStop;

	void Apply( FCameraLensSettings* Destination, const FDatasmithCameraLensSettingsTemplate* PreviousTemplate );
	void Load( const FCameraLensSettings& Source );
};

USTRUCT()
struct FDatasmithCameraFocusSettingsTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	float ManualFocusDistance;

	void Apply( FCameraFocusSettings* Destination, const FDatasmithCameraFocusSettingsTemplate* PreviousTemplate );
	void Load( const FCameraFocusSettings& Source );
};

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithPostProcessSettingsTemplate
{
	GENERATED_BODY()

public:
	FDatasmithPostProcessSettingsTemplate();

	UPROPERTY()
	uint32 bOverride_WhiteTemp:1;

	UPROPERTY()
	uint32 bOverride_ColorSaturation:1;

	UPROPERTY()
	uint32 bOverride_VignetteIntensity:1;

	UPROPERTY()
	uint32 bOverride_FilmWhitePoint:1;

	UPROPERTY()
	float WhiteTemp;

	UPROPERTY()
	float VignetteIntensity;

	UPROPERTY()
	FLinearColor FilmWhitePoint;

	UPROPERTY()
	FVector4 ColorSaturation;

	void Apply( struct FPostProcessSettings* Destination, const FDatasmithPostProcessSettingsTemplate* PreviousTemplate );
	void Load( const FPostProcessSettings& Source );
};

UCLASS()
class DATASMITHCONTENT_API UDatasmithCineCameraComponentTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FDatasmithCameraFilmbackSettingsTemplate FilmbackSettings;

	UPROPERTY()
	FDatasmithCameraLensSettingsTemplate LensSettings;

	UPROPERTY()
	FDatasmithCameraFocusSettingsTemplate FocusSettings;

	UPROPERTY()
	float CurrentFocalLength;

	UPROPERTY()
	float CurrentAperture;

	UPROPERTY()
	FDatasmithPostProcessSettingsTemplate PostProcessSettings;

	virtual void Apply( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
};
