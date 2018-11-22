// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialInteractionHandle.h"

// Helper macros for declaring a social interaction class
// Not required to create one, but all static functions provided by the macro must be defined in order for a class to qualify as an interaction

#define DECLARE_SOCIAL_INTERACTION_EXPORT(APIMacro, InteractionName)	\
class APIMacro FSocialInteraction_##InteractionName	\
{	\
public:	\
	static const ISocialInteractionHandleRef GetHandle()	\
	{	\
		ISocialInteractionHandleRef MyHandle = MakeShareable(new TSocialInteractionHandle<FSocialInteraction_##InteractionName>);	\
		return MyHandle;	\
	}	\
	static FString GetInteractionName()  \
	{ \
		return #InteractionName; \
	} \
	\
	static FText GetDisplayName();	\
	static FString GetSlashCommandToken();	\
	\
	static void GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems);	\
	static void ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User);	\
}

#define DECLARE_SOCIAL_INTERACTION(InteractionName) DECLARE_SOCIAL_INTERACTION_EXPORT(, InteractionName)