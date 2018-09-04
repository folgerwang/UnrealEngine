// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TimecodeProvider.h"
#include "Misc/FrameRate.h"
#include "UObject/UnrealType.h"
#include "SystemTimeTimecodeProvider.generated.h"

/**
 * Converts the current system time to timecode, relative to a provided frame rate.
 */
UCLASS()
class ENGINE_API USystemTimeTimecodeProvider : public UTimecodeProvider
{
	GENERATED_BODY()

private:

	UPROPERTY(EditAnywhere, Category = Timecode)
	FFrameRate FrameRate;

	ETimecodeProviderSynchronizationState State;

public:

	USystemTimeTimecodeProvider():
		FrameRate(60, 1),
		State(ETimecodeProviderSynchronizationState::Closed)
	{
	}

	//~ Begin UTimecodeProvider Interface
	virtual FTimecode GetTimecode() const override;

	virtual FFrameRate GetFrameRate() const override
	{
		return FrameRate;
	}
	
	virtual ETimecodeProviderSynchronizationState GetSynchronizationState() const override
	{
		return State;
	}

	virtual bool Initialize(class UEngine* InEngine) override
	{
		State = ETimecodeProviderSynchronizationState::Synchronized;
		return true;
	}

	virtual void Shutdown(class UEngine* InEngine) override
	{
		State = ETimecodeProviderSynchronizationState::Closed;
	}
	//~ End UTimecodeProvider Interface

	UFUNCTION()
	void SetFrameRate(const FFrameRate& InFrameRate)
	{
		FrameRate = InFrameRate;
	}
};
