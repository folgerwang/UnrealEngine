// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"

namespace FNiagaraStackEditorWidgetsUtilities
{
	FName GetColorNameForExecutionCategory(FName ExecutionCategoryName);

	FName GetIconNameForExecutionSubcategory(FName ExecutionSubcategoryName, bool bIsHighlighted);

	FName GetIconColorNameForExecutionCategory(FName ExecutionCategoryName);
}