// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "../SocialTypes.h"

/**
 * Represents a single discrete interaction between the local player and another user.
 * Only necessary when you'd like to create some tangible list of interactions to iterate through.
 * If, on the other hand, you're only interested in a few interactions, feel free to access their static APIs directly.
 */
class ISocialInteractionHandle : public TSharedFromThis<ISocialInteractionHandle>
{
public:
	virtual ~ISocialInteractionHandle() {}

	virtual FString GetInteractionName() const = 0;
	virtual FText GetDisplayName() const = 0;
	virtual FString GetSlashCommandToken() const = 0;

	virtual void GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems) const = 0;
	virtual void ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User) const = 0;
};

/** Link between the class-polymorphism-based interaction handle and the static template-polymorphism-based interactions */
template <typename InteractionT>
class TSocialInteractionHandle : public ISocialInteractionHandle
{
public:
	virtual FString GetInteractionName() const override final { return InteractionT::GetInteractionName(); }
	virtual FText GetDisplayName() const override final { return InteractionT::GetDisplayName(); }
	virtual FString GetSlashCommandToken() const override final { return InteractionT::GetSlashCommandToken(); }

	virtual void GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems) const override final { return InteractionT::GetAvailability(User, OutAvailableSubsystems); }
	virtual void ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User) const override final { return InteractionT::ExecuteAction(SocialSubsystem, User); }

private:
	friend InteractionT;
	TSocialInteractionHandle() {}
};