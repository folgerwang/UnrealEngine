// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapIdentity.h"
#include "IMagicLeapIdentityPlugin.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "MagicLeapPluginUtil.h"

#if WITH_MLSDK
#include "ml_identity.h"
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapIdentity, Display, All);

class FMagicLeapIdentityPlugin : public IMagicLeapIdentityPlugin
{
public:
	void StartupModule() override
	{
		IModuleInterface::StartupModule();
		APISetup.Startup();
#if WITH_MLSDK
		APISetup.LoadDLL(TEXT("ml_identity"));
#endif
	}

	void ShutdownModule() override
	{
		APISetup.Shutdown();
		IModuleInterface::ShutdownModule();
	}

private:
	FMagicLeapAPISetup APISetup;
};

IMPLEMENT_MODULE(FMagicLeapIdentityPlugin, MagicLeapIdentity);

//////////////////////////////////////////////////////////////////////////

class FIdentityImpl
{
#if WITH_MLSDK
public:
	struct FRequestAttribData
	{
	public:
		FRequestAttribData()
		{}

		FRequestAttribData(MLIdentityProfile* profile, const UMagicLeapIdentity::FRequestIdentityAttributeValueDelegate& requestDelegate)
			: Profile(profile)
			, RequestDelegate(requestDelegate)
		{}

		MLIdentityProfile* Profile;
		UMagicLeapIdentity::FRequestIdentityAttributeValueDelegate RequestDelegate;
	};

	struct FModifyAttribData
	{
	public:
		FModifyAttribData()
		{}

		FModifyAttribData(MLIdentityProfile* profile, const UMagicLeapIdentity::FModifyIdentityAttributeValueDelegate& requestDelegate)
			: Profile(profile)
			, RequestDelegate(requestDelegate)
		{}

		MLIdentityProfile* Profile;
		UMagicLeapIdentity::FModifyIdentityAttributeValueDelegate RequestDelegate;
	};

	EMagicLeapIdentityError MLToUnrealIdentityError(MLIdentityError error)
	{
		switch (error)
		{
		case MLIdentityError_Ok:
			return EMagicLeapIdentityError::Ok;
		case MLIdentityError_FailedToConnectToLocalService:
			return EMagicLeapIdentityError::FailedToConnectToLocalService;
		case MLIdentityError_FailedToConnectToCloudService:
			return EMagicLeapIdentityError::FailedToConnectToCloudService;
		case MLIdentityError_CloudAuthentication:
			return EMagicLeapIdentityError::CloudAuthentication;
		case MLIdentityError_InvalidInformationFromCloud:
			return EMagicLeapIdentityError::InvalidInformationFromCloud;
		case MLIdentityError_InvalidArgument:
			return EMagicLeapIdentityError::InvalidArgument;
		case MLIdentityError_AsyncOperationNotComplete:
			return EMagicLeapIdentityError::AsyncOperationNotComplete;
		case MLIdentityError_OtherError:
			return EMagicLeapIdentityError::OtherError;
		}

		return EMagicLeapIdentityError::OtherError;
	}

	EMagicLeapIdentityAttribute MLToUnrealIdentityAttribute(MLIdentityAttributeEnum attribute)
	{
		switch (attribute)
		{
		case MLIdentityAttributeEnum_UserId:
			return EMagicLeapIdentityAttribute::UserID;
		case MLIdentityAttributeEnum_GivenName:
			return EMagicLeapIdentityAttribute::GivenName;
		case MLIdentityAttributeEnum_FamilyName:
			return EMagicLeapIdentityAttribute::FamilyName;
		case MLIdentityAttributeEnum_Email:
			return EMagicLeapIdentityAttribute::Email;
		case MLIdentityAttributeEnum_Status:
			return EMagicLeapIdentityAttribute::Status;
		case MLIdentityAttributeEnum_TermsAccepted:
			return EMagicLeapIdentityAttribute::TermsAccepted;
		case MLIdentityAttributeEnum_Birthday:
			return EMagicLeapIdentityAttribute::Birthday;
		case MLIdentityAttributeEnum_Company:
			return EMagicLeapIdentityAttribute::Company;
		case MLIdentityAttributeEnum_Industry:
			return EMagicLeapIdentityAttribute::Industry;
		case MLIdentityAttributeEnum_Location:
			return EMagicLeapIdentityAttribute::Location;
		case MLIdentityAttributeEnum_Tagline:
			return EMagicLeapIdentityAttribute::Tagline;
		case MLIdentityAttributeEnum_PhoneNumber:
			return EMagicLeapIdentityAttribute::PhoneNumber;
		case MLIdentityAttributeEnum_Avatar2D:
			return EMagicLeapIdentityAttribute::Avatar2D;
		case MLIdentityAttributeEnum_Avatar3D:
			return EMagicLeapIdentityAttribute::Avatar3D;
		case MLIdentityAttributeEnum_IsDeveloper:
			return EMagicLeapIdentityAttribute::IsDeveloper;
		case MLIdentityAttributeEnum_Unknown:
			return EMagicLeapIdentityAttribute::Unknown;
		}
		return EMagicLeapIdentityAttribute::Unknown;
	}

	MLIdentityAttributeEnum UnrealToMLIdentityAttribute(EMagicLeapIdentityAttribute attribute)
	{
		switch (attribute)
		{
		case EMagicLeapIdentityAttribute::UserID:
			return MLIdentityAttributeEnum_UserId;
		case EMagicLeapIdentityAttribute::GivenName:
			return MLIdentityAttributeEnum_GivenName;
		case EMagicLeapIdentityAttribute::FamilyName:
			return MLIdentityAttributeEnum_FamilyName;
		case EMagicLeapIdentityAttribute::Email:
			return MLIdentityAttributeEnum_Email;
		case EMagicLeapIdentityAttribute::Status:
			return MLIdentityAttributeEnum_Status;
		case EMagicLeapIdentityAttribute::TermsAccepted:
			return MLIdentityAttributeEnum_TermsAccepted;
		case EMagicLeapIdentityAttribute::Birthday:
			return MLIdentityAttributeEnum_Birthday;
		case EMagicLeapIdentityAttribute::Company:
			return MLIdentityAttributeEnum_Company;
		case EMagicLeapIdentityAttribute::Industry:
			return MLIdentityAttributeEnum_Industry;
		case EMagicLeapIdentityAttribute::Location:
			return MLIdentityAttributeEnum_Location;
		case EMagicLeapIdentityAttribute::Tagline:
			return MLIdentityAttributeEnum_Tagline;
		case EMagicLeapIdentityAttribute::PhoneNumber:
			return MLIdentityAttributeEnum_PhoneNumber;
		case EMagicLeapIdentityAttribute::Avatar2D:
			return MLIdentityAttributeEnum_Avatar2D;
		case EMagicLeapIdentityAttribute::Avatar3D:
			return MLIdentityAttributeEnum_Avatar3D;
		case EMagicLeapIdentityAttribute::IsDeveloper:
			return MLIdentityAttributeEnum_IsDeveloper;
		case EMagicLeapIdentityAttribute::Unknown:
			return MLIdentityAttributeEnum_Unknown;
		}
		return MLIdentityAttributeEnum_Unknown;
	}


	TMap<MLInvokeFuture*, UMagicLeapIdentity::FAvailableIdentityAttributesDelegate> AllAvailableAttribsFutures;
	TMap<MLInvokeFuture*, FRequestAttribData> AllRequestAttribsFutures;
#endif //WITH_MLSDK
};

UMagicLeapIdentity::UMagicLeapIdentity()
	: Impl(new FIdentityImpl())
{}

UMagicLeapIdentity::~UMagicLeapIdentity()
{
	delete Impl;
}

EMagicLeapIdentityError UMagicLeapIdentity::GetAllAvailableAttributes(TArray<EMagicLeapIdentityAttribute>& AvailableAttributes)
{
#if WITH_MLSDK
	MLIdentityProfile* profile = nullptr;
	MLIdentityError result = MLIdentityGetAttributeNames(&profile);

	AvailableAttributes.Empty();
	if (result == MLIdentityError_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			const MLIdentityAttribute* attribute = profile->attribute_ptrs[i];
			AvailableAttributes.Add(Impl->MLToUnrealIdentityAttribute(attribute->enum_value));
		}

		MLIdentityReleaseUserProfile(profile);
	}

	return Impl->MLToUnrealIdentityError(result);
#else
	return EMagicLeapIdentityError::OtherError;
#endif //WITH_MLSDK

}

void UMagicLeapIdentity::GetAllAvailableAttributesAsync(const FAvailableIdentityAttributesDelegate& ResultDelegate)
{
#if WITH_MLSDK
	Impl->AllAvailableAttribsFutures.Add(MLIdentityGetAttributeNamesAsync(), ResultDelegate);
#endif //WITH_MLSDK
}

EMagicLeapIdentityError UMagicLeapIdentity::RequestAttributeValue(const TArray<EMagicLeapIdentityAttribute>& Attribute, TArray<FMagicLeapIdentityAttribute>& AttributeValue)
{
#if WITH_MLSDK
	MLIdentityProfile* profile = nullptr;
	TArray<MLIdentityAttributeEnum> mlAttributes;
	for (EMagicLeapIdentityAttribute attrib : Attribute)
	{
		mlAttributes.Add(Impl->UnrealToMLIdentityAttribute(attrib));
	}
	MLIdentityError result = MLIdentityGetKnownAttributeNames(mlAttributes.GetData(), mlAttributes.Num(), &profile);

	AttributeValue.Empty();
	if (result == MLIdentityError_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			profile->attribute_ptrs[i]->is_requested = true;
		}

		result = MLIdentityRequestAttributeValues(profile);

		if (result == MLIdentityError_Ok)
		{
			for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
			{
				if (profile->attribute_ptrs[i]->is_granted)
				{
					AttributeValue.Add(FMagicLeapIdentityAttribute(Impl->MLToUnrealIdentityAttribute(profile->attribute_ptrs[i]->enum_value), FString(ANSI_TO_TCHAR(profile->attribute_ptrs[i]->value))));
				}
			}
		}

		MLIdentityReleaseUserProfile(profile);
	}

	return Impl->MLToUnrealIdentityError(result);
#else
	return EMagicLeapIdentityError::OtherError;
#endif //WITH_MLSDK
}

EMagicLeapIdentityError UMagicLeapIdentity::RequestAttributeValueAsync(const TArray<EMagicLeapIdentityAttribute>& Attribute, const FRequestIdentityAttributeValueDelegate& ResultDelegate)
{
#if WITH_MLSDK
	MLIdentityProfile* profile = nullptr;
	TArray<MLIdentityAttributeEnum> mlAttributes;
	for (EMagicLeapIdentityAttribute attrib : Attribute)
	{
		mlAttributes.Add(Impl->UnrealToMLIdentityAttribute(attrib));
	}
	MLIdentityError result = MLIdentityGetKnownAttributeNames(mlAttributes.GetData(), mlAttributes.Num(), &profile);

	if (result == MLIdentityError_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			profile->attribute_ptrs[i]->is_requested = true;
		}

		Impl->AllRequestAttribsFutures.Add(MLIdentityRequestAttributeValuesAsync(profile), FIdentityImpl::FRequestAttribData(profile, ResultDelegate));
	}

	return Impl->MLToUnrealIdentityError(result);
#else
	return EMagicLeapIdentityError::OtherError;
#endif //WITH_MLSDK
}

EMagicLeapIdentityError UMagicLeapIdentity::ModifyAttributeValue(const TArray<FMagicLeapIdentityAttribute>& UpdatedAttributeValue, TArray<EMagicLeapIdentityAttribute>& AttributesUpdatedSuccessfully)
{
	return EMagicLeapIdentityError::OtherError;
}

EMagicLeapIdentityError UMagicLeapIdentity::ModifyAttributeValueAsync(const TArray<FMagicLeapIdentityAttribute>& UpdatedAttributeValue, const FModifyIdentityAttributeValueDelegate& ResultDelegate)
{
	return EMagicLeapIdentityError::OtherError;
}

UWorld* UMagicLeapIdentity::GetWorld() const
{
	return Cast<UWorld>(GetOuter());
}

void UMagicLeapIdentity::Tick(float DeltaTime)
{
#if WITH_MLSDK
	TArray<MLInvokeFuture*> FuturesToDelete;

	// MLIdentityGetAttributeNamesAsync()
	for (const auto& AvailableAttribsFuture : Impl->AllAvailableAttribsFutures)
	{
		MLInvokeFuture* future = AvailableAttribsFuture.Key;
		MLIdentityError error = MLIdentityError_OtherError;
		MLIdentityProfile* profile = nullptr;

		if (MLIdentityGetAttributeNamesWait(future, 0, &error, &profile))
		{
			FuturesToDelete.Add(future);
			TArray<EMagicLeapIdentityAttribute> AvailableAttributes;
			if (error == MLIdentityError_Ok && profile != nullptr)
			{
				for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
				{
					const MLIdentityAttribute* attribute = profile->attribute_ptrs[i];
					AvailableAttributes.Add(Impl->MLToUnrealIdentityAttribute(attribute->enum_value));
				}
				MLIdentityReleaseUserProfile(profile);
			}
			AvailableAttribsFuture.Value.ExecuteIfBound(Impl->MLToUnrealIdentityError(error), AvailableAttributes);
		}
	}

	for (const MLInvokeFuture* future : FuturesToDelete)
	{
		Impl->AllAvailableAttribsFutures.Remove(future);
	}

	FuturesToDelete.Empty();

	// MLIdentityRequestAttributeValuesAsync()
	for (const auto& RequestAttribsFuture : Impl->AllRequestAttribsFutures)
	{
		MLInvokeFuture* future = RequestAttribsFuture.Key;
		MLIdentityError error = MLIdentityError_OtherError;

		if (MLIdentityRequestAttributeValuesWait(future, 0, &error))
		{
			FuturesToDelete.Add(future);
			TArray<FMagicLeapIdentityAttribute> AttributeValue;
			MLIdentityProfile* profile = RequestAttribsFuture.Value.Profile;

			if (error == MLIdentityError_Ok)
			{
				for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
				{
					if (profile->attribute_ptrs[i]->is_granted)
					{
						AttributeValue.Add(FMagicLeapIdentityAttribute(Impl->MLToUnrealIdentityAttribute(profile->attribute_ptrs[i]->enum_value), FString(ANSI_TO_TCHAR(profile->attribute_ptrs[i]->value))));
					}
				}
			}
			RequestAttribsFuture.Value.RequestDelegate.ExecuteIfBound(Impl->MLToUnrealIdentityError(error), AttributeValue);
			MLIdentityReleaseUserProfile(profile);
		}
	}

	for (const MLInvokeFuture* future : FuturesToDelete)
	{
		Impl->AllRequestAttribsFutures.Remove(future);
	}

	FuturesToDelete.Empty();
#endif //WITH_MLSDK
}

bool UMagicLeapIdentity::IsTickable() const
{
	return HasAnyFlags(RF_ClassDefaultObject) == false;
}

TStatId UMagicLeapIdentity::GetStatId() const
{
	return GetStatID(false);
}

UWorld* UMagicLeapIdentity::GetTickableGameObjectWorld() const
{
	return GetWorld();
}
