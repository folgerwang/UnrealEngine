// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraGraphNode.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Framework/Commands/UICommandList.h"

/** A graph node widget representing a niagara input node. */
class SNiagaraGraphNodeCustomHlsl : public SNiagaraGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNiagaraGraphNodeCustomHlsl) {}
	SLATE_END_ARGS();

	SNiagaraGraphNodeCustomHlsl();

	void Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode);

	//~ SGraphNode api
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle) override;
	virtual bool IsNameReadOnly() const override;
	virtual void RequestRenameOnSpawn() override;

	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox);
protected:


};
