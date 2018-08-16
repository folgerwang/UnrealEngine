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
#endif //WITH_MLSDK
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

	EMagicLeapIdentityError MLToUnrealIdentityError(MLResult error)
	{
		#define MLRESULTCASE(x) case MLResult_##x: { return EMagicLeapIdentityError::x; }
		#define MLIDENTITYRESULTCASE(x) case MLIdentityResult_##x: { return EMagicLeapIdentityError::x; }
		switch (error)
		{
			MLRESULTCASE(Ok)
			MLRESULTCASE(InvalidParam)
			MLRESULTCASE(AllocFailed)
			MLRESULTCASE(PrivilegeDenied)
			MLRESULTCASE(UnspecifiedFailure)
			MLIDENTITYRESULTCASE(FailedToConnectToLocalService)
			MLIDENTITYRESULTCASE(FailedToConnectToCloudService)
			MLIDENTITYRESULTCASE(CloudAuthentication)
			MLIDENTITYRESULTCASE(InvalidInformationFromCloud)
			MLIDENTITYRESULTCASE(NotLoggedIn)
			MLIDENTITYRESULTCASE(ExpiredCredentials)
			MLIDENTITYRESULTCASE(FailedToGetUserProfile)
			MLIDENTITYRESULTCASE(Unauthorized)
			MLIDENTITYRESULTCASE(CertificateError)
			MLIDENTITYRESULTCASE(RejectedByCloud)
			MLIDENTITYRESULTCASE(AlreadyLoggedIn)
			MLIDENTITYRESULTCASE(ModifyIsNotSupported)
			MLIDENTITYRESULTCASE(NetworkError)
			default:
				break;
		}

		return EMagicLeapIdentityError::UnspecifiedFailure;
	}

	EMagicLeapIdentityKey MLToUnrealIdentityAttribute(MLIdentityAttributeKey attribute)
	{
#define MLIDENTITYKEYCASE(x) case MLIdentityAttributeKey_##x: { return EMagicLeapIdentityKey::x; }
		switch (attribute)
		{
			MLIDENTITYKEYCASE(GivenName)
			MLIDENTITYKEYCASE(FamilyName)
			MLIDENTITYKEYCASE(Email)
			MLIDENTITYKEYCASE(Bio)
			MLIDENTITYKEYCASE(PhoneNumber)
			MLIDENTITYKEYCASE(Avatar2D)
			MLIDENTITYKEYCASE(Avatar3D)
			default:
				break;
		}
		return EMagicLeapIdentityKey::Unknown;
#undef MLIDENTITYKEYCASE
	}

	MLIdentityAttributeKey UnrealToMLIdentityAttribute(EMagicLeapIdentityKey attribute)
	{
#define MLIDENTITYKEYCASE(x) case EMagicLeapIdentityKey::x: { return MLIdentityAttributeKey_##x; }
		switch (attribute)
		{
			MLIDENTITYKEYCASE(GivenName)
			MLIDENTITYKEYCASE(FamilyName)
			MLIDENTITYKEYCASE(Email)
			MLIDENTITYKEYCASE(Bio)
			MLIDENTITYKEYCASE(PhoneNumber)
			MLIDENTITYKEYCASE(Avatar2D)
			MLIDENTITYKEYCASE(Avatar3D)
			default:
				break;
		}
		return MLIdentityAttributeKey_Unknown;
#undef MLIDENTITYKEYCASE
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

EMagicLeapIdentityError UMagicLeapIdentity::GetAllAvailableAttributes(TArray<EMagicLeapIdentityKey>& AvailableAttributes)
{
#if WITH_MLSDK
	MLIdentityProfile* profile = nullptr;
	MLResult Result = MLIdentityGetAttributeNames(&profile);

	AvailableAttributes.Empty();
	if (Result == MLResult_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			const MLIdentityAttribute* attribute = profile->attribute_ptrs[i];
			AvailableAttributes.Add(Impl->MLToUnrealIdentityAttribute(attribute->key));
		}

		MLIdentityReleaseUserProfile(profile);
	}

	return Impl->MLToUnrealIdentityError(Result);
#else
	return EMagicLeapIdentityError::UnspecifiedFailure;
#endif //WITH_MLSDK
}

void UMagicLeapIdentity::GetAllAvailableAttributesAsync(const FAvailableIdentityAttributesDelegate& ResultDelegate)
{
#if WITH_MLSDK
	MLInvokeFuture* InvokeFuture = nullptr;
	MLResult Result = MLIdentityGetAttributeNamesAsync(&InvokeFuture);
	if (Result == MLResult_Ok)
	{
		Impl->AllAvailableAttribsFutures.Add(InvokeFuture, ResultDelegate);
	}
	else
	{
		UE_LOG(LogMagicLeapIdentity, Error, TEXT("MLIdentityGetAttributeNamesAsync failed with error %d!"), Result);
	}
#endif //WITH_MLSDK
}

EMagicLeapIdentityError UMagicLeapIdentity::RequestAttributeValue(const TArray<EMagicLeapIdentityKey>& Attribute, TArray<FMagicLeapIdentityAttribute>& AttributeValue)
{
#if WITH_MLSDK
	MLIdentityProfile* profile = nullptr;
	TArray<MLIdentityAttributeKey> mlAttributes;
	for (EMagicLeapIdentityKey attrib : Attribute)
	{
		mlAttributes.Add(Impl->UnrealToMLIdentityAttribute(attrib));
	}
	MLResult Result = MLIdentityGetKnownAttributeNames(mlAttributes.GetData(), mlAttributes.Num(), &profile);

	AttributeValue.Empty();
	if (Result == MLResult_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			profile->attribute_ptrs[i]->is_requested = true;
		}

		Result = MLIdentityRequestAttributeValues(profile);

		if (Result == MLResult_Ok)
		{
			for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
			{
				if (profile->attribute_ptrs[i]->is_granted)
				{
					AttributeValue.Add(FMagicLeapIdentityAttribute(Impl->MLToUnrealIdentityAttribute(profile->attribute_ptrs[i]->key), FString(ANSI_TO_TCHAR(profile->attribute_ptrs[i]->value))));
				}
			}
		}

		MLIdentityReleaseUserProfile(profile);
	}

	return Impl->MLToUnrealIdentityError(Result);
#else
	return EMagicLeapIdentityError::UnspecifiedFailure;
#endif //WITH_MLSDK
}

EMagicLeapIdentityError UMagicLeapIdentity::RequestAttributeValueAsync(const TArray<EMagicLeapIdentityKey>& Attribute, const FRequestIdentityAttributeValueDelegate& ResultDelegate)
{
#if WITH_MLSDK
	MLIdentityProfile* profile = nullptr;
	TArray<MLIdentityAttributeKey> mlAttributes;
	for (EMagicLeapIdentityKey attrib : Attribute)
	{
		mlAttributes.Add(Impl->UnrealToMLIdentityAttribute(attrib));
	}
	MLResult Result = MLIdentityGetKnownAttributeNames(mlAttributes.GetData(), mlAttributes.Num(), &profile);

	if (Result == MLResult_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			profile->attribute_ptrs[i]->is_requested = true;
		}

		MLInvokeFuture* InvokeFuture = nullptr;
		MLResult RequestAtrrValRes = MLIdentityRequestAttributeValuesAsync(profile, &InvokeFuture);
		if (RequestAtrrValRes == MLResult_Ok)
		{
			Impl->AllRequestAttribsFutures.Add(InvokeFuture, FIdentityImpl::FRequestAttribData(profile, ResultDelegate));
		}
		else
		{
			UE_LOG(LogMagicLeapIdentity, Error, TEXT("MLIdentityRequestAttributeValuesAsync failed with error %d!"), RequestAtrrValRes);
		}
	}

	return Impl->MLToUnrealIdentityError(Result);
#else
	return EMagicLeapIdentityError::UnspecifiedFailure;
#endif //WITH_MLSDK
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
		MLIdentityProfile* profile = nullptr;
		MLResult Result = MLIdentityGetAttributeNamesWait(future, 0, &profile);

		TArray<EMagicLeapIdentityKey> AvailableAttributes;
		if (Result == MLResult_Ok)
		{
			FuturesToDelete.Add(future);
			if (profile != nullptr)
			{
				for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
				{
					const MLIdentityAttribute* attribute = profile->attribute_ptrs[i];
					AvailableAttributes.Add(Impl->MLToUnrealIdentityAttribute(attribute->key));
				}
				MLIdentityReleaseUserProfile(profile);
			}
			AvailableAttribsFuture.Value.ExecuteIfBound(Impl->MLToUnrealIdentityError(Result), AvailableAttributes);
		}
		else if (Result != MLResult_Pending)
		{
			FuturesToDelete.Add(future);			
			AvailableAttribsFuture.Value.ExecuteIfBound(Impl->MLToUnrealIdentityError(Result), AvailableAttributes);
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
		MLResult Result = MLIdentityRequestAttributeValuesWait(future, 0);

		TArray<FMagicLeapIdentityAttribute> AttributeValue;
		if (Result == MLResult_Ok)
		{
			FuturesToDelete.Add(future);
			MLIdentityProfile* profile = RequestAttribsFuture.Value.Profile;

			for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
			{
				if (profile->attribute_ptrs[i]->is_granted)
				{
					AttributeValue.Add(FMagicLeapIdentityAttribute(Impl->MLToUnrealIdentityAttribute(profile->attribute_ptrs[i]->key), FString(ANSI_TO_TCHAR(profile->attribute_ptrs[i]->value))));
				}
			}

			RequestAttribsFuture.Value.RequestDelegate.ExecuteIfBound(Impl->MLToUnrealIdentityError(Result), AttributeValue);
			MLIdentityReleaseUserProfile(profile);
		}
		else if (Result != MLResult_Pending)
		{
			FuturesToDelete.Add(future);			
			RequestAttribsFuture.Value.RequestDelegate.ExecuteIfBound(Impl->MLToUnrealIdentityError(Result), AttributeValue);
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
