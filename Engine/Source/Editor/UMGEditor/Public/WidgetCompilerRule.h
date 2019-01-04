// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet2/CompilerResultsLog.h"

#include "WidgetCompilerRule.generated.h"

class UWidgetBlueprint;

/**
 * 
 */
UCLASS(Abstract)
class UMGEDITOR_API UWidgetCompilerRule : public UObject
{
	GENERATED_BODY()
public:
	UWidgetCompilerRule();

	virtual void ExecuteRule(UWidgetBlueprint* WidgetBlueprint, FCompilerResultsLog& MessageLog);
};
