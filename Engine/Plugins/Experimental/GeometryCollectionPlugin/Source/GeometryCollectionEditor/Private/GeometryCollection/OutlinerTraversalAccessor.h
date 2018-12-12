// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISceneOutlinerTraversal.h"


class GEOMETRYCOLLECTIONEDITOR_API FOutlinerTraversalAccessor : public ISceneOutlinerTraversal
{
public:

	///** ISceneOutlinerTraversal implementation */
	virtual bool ConstructTreeItem(SceneOutliner::SSceneOutliner& SSceneOutliner, class UActorComponent* Component) override;
	
};
