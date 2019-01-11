// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Framework/Docking/LayoutExtender.h"

void FLayoutExtender::ExtendLayout(FTabId PredicateTabId, ELayoutExtensionPosition Position, FTabManager::FTab TabToAdd)
{
	TabExtensions.Add(PredicateTabId, FExtendedTab(Position, TabToAdd));
}

void FLayoutExtender::ExtendArea(FName ExtensionId, const FAreaExtension& AreaExtension)
{
	AreaExtensions.Add(ExtensionId, FExtendedArea(AreaExtension));
}

void FLayoutExtender::ExtendAreaRecursive(const TSharedRef<FTabManager::FArea>& Area) const
{
	FName ExtensionId = Area->GetExtensionId();
	if (ExtensionId == NAME_None)
	{
		return;
	}

	for (auto It = AreaExtensions.CreateConstKeyIterator(ExtensionId); It; ++It)
	{
		It.Value().ExtensionCallback(Area);
	}

	for (TSharedRef<FTabManager::FLayoutNode>& ChildNode : Area->ChildNodes)
	{
		TSharedPtr<FTabManager::FArea> ChildArea = ChildNode->AsArea();
		if (ChildArea)
		{
			ExtendAreaRecursive(ChildArea.ToSharedRef());
		}
	}
}