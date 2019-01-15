// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraphPanelPinFactory.h"
#include "Graph/ControlRigGraphSchema.h"
#include "NodeFactory.h"

TSharedPtr<SGraphPin> FControlRigGraphPanelPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	TSharedPtr<SGraphPin> K2PinWidget = FNodeFactory::CreateK2PinWidget(InPin);
	if(K2PinWidget.IsValid())
	{
		return K2PinWidget;
	}

	return nullptr;
}