// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "NavigationDataInterface.generated.h"


struct FNavigableGeometryExport;
struct FNavigationRelevantData;


UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavigationDataInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavigationDataInterface
{
	GENERATED_IINTERFACE_BODY()
public:
	/**	Tries to project given Point to this navigation type, within given Extent.
	*	@param OutLocation if successful this variable will be filed with result
	*	@return true if successful, false otherwise
	*/
	virtual bool ProjectPoint(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent, FSharedConstNavQueryFilter Filter = nullptr, const UObject* Querier = nullptr) const PURE_VIRTUAL(INavigationDataInterface::ProjectPoint, return false;);
};
