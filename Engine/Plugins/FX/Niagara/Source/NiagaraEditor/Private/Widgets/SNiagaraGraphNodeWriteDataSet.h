// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphNode.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Framework/Commands/UICommandList.h"

/** A graph node widget representing a niagara write data set node. */
class SNiagaraGraphNodeWriteDataSet : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNiagaraGraphNodeWriteDataSet) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode);
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
};
