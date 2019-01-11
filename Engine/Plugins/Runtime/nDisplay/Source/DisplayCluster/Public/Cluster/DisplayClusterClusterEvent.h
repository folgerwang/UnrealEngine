// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterClusterEvent.generated.h"


//////////////////////////////////////////////////////////////////////////////////////////////
// Cluster event
//////////////////////////////////////////////////////////////////////////////////////////////
USTRUCT(BlueprintType)
struct FDisplayClusterClusterEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "nDisplay")
	FString Name;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "nDisplay")
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "nDisplay")
	FString Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "nDisplay")
	TMap<FString, FString> Parameters;

public:
	//@todo: this must be regulated by IDisplayClusterStringSerializable (need to be converted to UINTERFACE)
	FString SerializeToString() const;
	bool    DeserializeFromString(const FString& Arch);

private:
	FString SerializeParametersToString() const;
	bool    DeserializeParametersFromString(const FString& Arch);
};
