// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "OpenColorIOColorSpace.h"
#include "RHIDefinitions.h"

#if WITH_EDITOR && WITH_OCIO
#include "OpenColorIO/OpenColorIO.h"
#endif

#include "OpenColorIOConfiguration.generated.h"


class FOpenColorIOTransformResource;
class FTextureResource;
class UOpenColorIOColorTransform;


/**
 * Asset to manage whitelisted OpenColorIO color spaces. This will create required transform objects.
 */
UCLASS(BlueprintType)
class OPENCOLORIO_API UOpenColorIOConfiguration : public UObject
{
	GENERATED_BODY()

public:

	UOpenColorIOConfiguration(const FObjectInitializer& ObjectInitializer);

public:
	bool GetShaderAndLUTResources(ERHIFeatureLevel::Type InFeatureLevel, const FString& InSourceColorSpace, const FString& InDestinationColorSpace, FOpenColorIOTransformResource*& OutShaderResource, FTextureResource*& OutLUT3dResource);
	bool HasTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace);
	bool Validate() const;

#if WITH_EDITORONLY_DATA && WITH_OCIO
	OCIO_NAMESPACE::ConstConfigRcPtr GetLoadedConfigurationFile() const { return LoadedConfig; }
#endif

protected:
	void CreateColorTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace);
	void CleanupTransforms();

public:

	//~ Begin UObject interface
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

private:
	void LoadConfigurationFile();

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config", meta = (FilePathFilter = "ocio", RelativeToGameDir))
	FFilePath ConfigurationFile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ColorSpace", meta=(OCIOConfigFile="ConfigurationFile"))
	TArray<FOpenColorIOColorSpace> DesiredColorSpaces;

private:

	UPROPERTY()
	TArray<UOpenColorIOColorTransform*> ColorTransforms;

#if WITH_EDITORONLY_DATA && WITH_OCIO
	OCIO_NAMESPACE::ConstConfigRcPtr LoadedConfig;
#endif //WITH_EDITORONLY_DATA
};
