// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Features/IModularFeature.h"
#include "SSceneOutliner.h"

class ISceneOutlinerTraversal : public IModularFeature
{
public:
	virtual bool ConstructTreeItem(class SceneOutliner::SSceneOutliner& SSceneOutliner, class UActorComponent* Component) = 0;
};
