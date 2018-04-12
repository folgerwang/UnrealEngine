// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraphPanelPinFactory.h"
#include "ControlRigGraphSchema.h"
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