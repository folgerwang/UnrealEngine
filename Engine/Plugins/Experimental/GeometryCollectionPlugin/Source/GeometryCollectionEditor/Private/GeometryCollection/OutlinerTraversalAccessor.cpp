// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/OutlinerTraversalAccessor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionTreeItem.h"
#include "SSceneOutliner.h"

#define LOCTEXT_NAMESPACE "OutlinerTraversalAccessor"

using namespace SceneOutliner;

bool FOutlinerTraversalAccessor::ConstructTreeItem(SSceneOutliner& SSceneOutliner, UActorComponent* Component)
{
	bool Handled = false;
	if (Cast<UGeometryCollectionComponent>(Component))
	{
		SSceneOutliner.ConstructItemFor<FGeometryCollectionTreeItem>(Cast<UGeometryCollectionComponent>(Component));
		Handled = true;
	}

	return Handled;
}

#undef LOCTEXT_NAMESPACE
