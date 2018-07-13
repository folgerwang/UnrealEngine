// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "GameFramework/GameMode.h"
#include "DisplayClusterGameMode.generated.h"


struct IPDisplayCluster;

/**
 * Extended game mode
 */
UCLASS()
class DISPLAYCLUSTER_API ADisplayClusterGameMode
	: public AGameMode
{
	GENERATED_BODY()
	
public:
	ADisplayClusterGameMode();
	virtual ~ADisplayClusterGameMode();

public:
	UFUNCTION(BlueprintCallable, Category = "DisplayCluster")
	bool IsDisplayClusterActive() const
	{ return bIsDisplayClusterActive; }

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DisplayCluster")
	bool bIsDisplayClusterActive = true;

protected:
	bool bGameStarted = false;

#if WITH_EDITOR
protected:
	static bool bNeedSessionStart;
	static bool bSessionStarted;

	FDelegateHandle EndPIEDelegate;
	void OnEndPIE(const bool bSimulate);
#endif
};
