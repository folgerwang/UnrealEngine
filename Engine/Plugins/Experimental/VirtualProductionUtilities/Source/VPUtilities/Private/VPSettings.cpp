// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VPSettings.h"

#include "Misc/CommandLine.h"
#include "VPUtilitiesModule.h"

UVPSettings::UVPSettings()
{
	FString ComandlineRoles;
	bIsCommandLineRolesValid = FParse::Value(FCommandLine::Get(), TEXT("-VPRole="), ComandlineRoles);

	if (bIsCommandLineRolesValid)
	{
		TArray<FString> RoleList;
		ComandlineRoles.ParseIntoArray(RoleList, TEXT("|"), true);

		for (const FString& Role : RoleList)
		{
			FGameplayTag Tag = FGameplayTag::RequestGameplayTag(*Role, false);
			if (Tag.IsValid())
			{
				CommandLineRoles.AddTag(Tag);
			}
			else
			{
				UE_LOG(LogVPUtilities, Fatal, TEXT("Role %s doesn't exist."), *Role);
			}
		}
	}
}


#if WITH_EDITOR
void UVPSettings::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ThisClass, Roles))
	{
		OnRolesChanged.Broadcast();
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif
