// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "VPSettings.generated.h"

/**
 * 
 */
UCLASS(config=Game)
class VPUTILITIES_API UVPSettings : public UObject
{
	GENERATED_BODY()

public:
	UVPSettings();

protected:
	/**
	 * The machine role(s) in a virtual production context.
	 * @note The role may be override via the command line, "-VPRole=[Role.SubRole1|Role.SubRole2]"
	 */
	UPROPERTY(config, EditAnywhere, Category="Virtual Production")
	FGameplayTagContainer Roles;

	/**
	 * The machine role(s) in a virtual production context read from the command line.
	 * ie. "-VPRole=[Role.SubRole1|Role.SubRole2]"
	 */
	bool bIsCommandLineRolesValid;
	UPROPERTY(transient)
	FGameplayTagContainer CommandLineRoles;

public:
	const FGameplayTagContainer& GetRoles() const { return bIsCommandLineRolesValid ? CommandLineRoles : Roles; }

public:
#if WITH_EDITOR
	//~ UObject interface
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
	//~ End UObject interface

	FSimpleMulticastDelegate OnRolesChanged;
#endif //WITH_EDITOR
};
