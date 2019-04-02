// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOConfiguration.h"

#include "Engine/VolumeTexture.h"
#include "IOpenColorIOModule.h"
#include "Math/PackedVector.h"
#include "Modules/ModuleManager.h"
#include "OpenColorIOColorTransform.h"
#include "TextureResource.h"


#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#include "Interfaces/ITargetPlatform.h"
#endif //WITH_EDITOR


UOpenColorIOConfiguration::UOpenColorIOConfiguration(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UOpenColorIOConfiguration::GetShaderAndLUTResources(ERHIFeatureLevel::Type InFeatureLevel, const FString& InSourceColorSpace, const FString& InDestinationColorSpace, FOpenColorIOTransformResource*& OutShaderResource, FTextureResource*& OutLUT3dResource)
{
	UOpenColorIOColorTransform** TransformPtr = ColorTransforms.FindByPredicate([&](const UOpenColorIOColorTransform* InTransform)
	{
		return InTransform->SourceColorSpace == InSourceColorSpace && InTransform->DestinationColorSpace == InDestinationColorSpace;
	});

	if (TransformPtr == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Color transform data from %s to %s was not found."), *InSourceColorSpace, *InDestinationColorSpace);
		return false;
	}

	UOpenColorIOColorTransform* Transform = *TransformPtr;
	return Transform->GetShaderAndLUTResouces(InFeatureLevel, OutShaderResource, OutLUT3dResource);
}

bool UOpenColorIOConfiguration::HasTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace)
{
	UOpenColorIOColorTransform** TransformData = ColorTransforms.FindByPredicate([&](const UOpenColorIOColorTransform* InTransformData)
	{
		return InTransformData->IsTransform(InSourceColorSpace, InDestinationColorSpace);
	});

	return (TransformData != nullptr);
}

bool UOpenColorIOConfiguration::Validate() const
{
#if WITH_EDITOR 

#if WITH_OCIO
	if (!ConfigurationFile.FilePath.IsEmpty())
	{
		//When loading the configuration file, if any errors are detected, it will throw an exception. Thus, our pointer won't be valid.
		return LoadedConfig != nullptr;
	}

	return false;
#else
	return false;
#endif // WITH_OCIO

#else
	return true;
#endif // WITH_EDITOR
}

void UOpenColorIOConfiguration::CreateColorTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace)
{
	if (InSourceColorSpace.IsEmpty() || InDestinationColorSpace.IsEmpty())
	{
		return;
	}

	if (HasTransform(InSourceColorSpace, InDestinationColorSpace))
	{
		UE_LOG(LogOpenColorIO, Log, TEXT("OCIOConfig already contains %s to %s transform."), *InSourceColorSpace, *InDestinationColorSpace);
		return;
	}

	UOpenColorIOColorTransform* NewTransform = NewObject<UOpenColorIOColorTransform>(this, NAME_None, RF_NoFlags);
	const bool bSuccess = NewTransform->Initialize(this, InSourceColorSpace, InDestinationColorSpace);

	if (bSuccess)
	{
		ColorTransforms.Add(NewTransform);
	}
	else
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Could not create color space transform from %s to %s. Verify your OCIO config file, it may have errors in it."), *InSourceColorSpace, *InDestinationColorSpace);
	}
}

void UOpenColorIOConfiguration::CleanupTransforms()
{
	for (int32 i = 0; i < ColorTransforms.Num(); ++i)
	{
		UOpenColorIOColorTransform* Transform = ColorTransforms[i];

		//Check if the source color space of this transform is desired. If not, remove that transform from the list and move on.
		const FOpenColorIOColorSpace* FoundSourceColorPtr = DesiredColorSpaces.FindByPredicate([&](const FOpenColorIOColorSpace& InOtherColorSpace)
		{
			return InOtherColorSpace.ColorSpaceName == Transform->SourceColorSpace;
		});

		if (FoundSourceColorPtr == nullptr)
		{
			ColorTransforms.RemoveSingleSwap(Transform, true);
			--i;
			continue;
		}

		//The source was there so check if the destination color space of this transform is desired. If not, remove that transform from the list and move on.
		const FOpenColorIOColorSpace* FoundDestinationColorPtr = DesiredColorSpaces.FindByPredicate([&](const FOpenColorIOColorSpace& InOtherColorSpace)
		{
			return InOtherColorSpace.ColorSpaceName == Transform->DestinationColorSpace;
		});

		if (FoundDestinationColorPtr == nullptr)
		{
			ColorTransforms.RemoveSingleSwap(Transform, true);
			--i;
			continue;
		}
	}
}

void UOpenColorIOConfiguration::PostLoad()
{
	Super::PostLoad();

	LoadConfigurationFile();

	for (UOpenColorIOColorTransform* Transform : ColorTransforms)
	{
		Transform->ConditionalPostLoad();
	}
}

#if WITH_EDITOR

void UOpenColorIOConfiguration::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, ConfigurationFile))
	{
		LoadConfigurationFile();
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, DesiredColorSpaces))
	{
		if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayAdd | EPropertyChangeType::Duplicate | EPropertyChangeType::ValueSet))
		{
			for (int32 i = 0; i < DesiredColorSpaces.Num(); ++i)
			{
				const FOpenColorIOColorSpace& TopColorSpace = DesiredColorSpaces[i];

				for (int32 j = i + 1; j < DesiredColorSpaces.Num(); ++j)
				{
					const FOpenColorIOColorSpace& OtherColorSpace = DesiredColorSpaces[j];

					CreateColorTransform(TopColorSpace.ColorSpaceName, OtherColorSpace.ColorSpaceName);
					CreateColorTransform(OtherColorSpace.ColorSpaceName, TopColorSpace.ColorSpaceName);
				}
			}
		}

		if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayRemove | EPropertyChangeType::ArrayClear | EPropertyChangeType::ValueSet))
		{
			CleanupTransforms();
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR

void UOpenColorIOConfiguration::LoadConfigurationFile()
{
#if WITH_EDITOR && WITH_OCIO
	if (!ConfigurationFile.FilePath.IsEmpty())
	{
#if !PLATFORM_EXCEPTIONS_DISABLED
		try
#endif
		{
			LoadedConfig.reset();

			FString FullPath;
			if (!FPaths::IsRelative(ConfigurationFile.FilePath))
			{
				FullPath = ConfigurationFile.FilePath;
			}
			else
			{
				const FString AbsoluteGameDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
				FullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(AbsoluteGameDir, ConfigurationFile.FilePath));
			}

			OCIO_NAMESPACE::ConstConfigRcPtr NewConfig = OCIO_NAMESPACE::Config::CreateFromFile(StringCast<ANSICHAR>(*FullPath).Get());
			if (NewConfig)
			{
				UE_LOG(LogOpenColorIO, Verbose, TEXT("Loaded OCIO configuration file %s"), *FullPath);
				LoadedConfig = NewConfig;
			}
			else
			{
				UE_LOG(LogOpenColorIO, Error, TEXT("Could not load OCIO configuration file %s. Verify that the path is good or that the file is valid."), *ConfigurationFile.FilePath);
			}
		}
#if !PLATFORM_EXCEPTIONS_DISABLED
		catch (OCIO_NAMESPACE::Exception& exception)
		{
			UE_LOG(LogOpenColorIO, Error, TEXT("Could not load OCIO configuration file %s. Error message: %s."), *ConfigurationFile.FilePath, StringCast<TCHAR>(exception.what()).Get());
		}
#endif
	}
#endif
}
