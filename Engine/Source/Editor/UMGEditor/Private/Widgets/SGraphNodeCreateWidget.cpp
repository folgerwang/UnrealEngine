// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SGraphNodeCreateWidget.h"

#include "Nodes/K2Node_CreateWidget.h"
#include "KismetPins/SGraphPinClass.h"

TSharedPtr<SGraphPin> SGraphNodeCreateWidget::CreatePinWidget(UEdGraphPin* Pin) const
{
	UK2Node_CreateWidget* CreateWidgetNode = CastChecked<UK2Node_CreateWidget>(GraphNode);
	UEdGraphPin* ClassPin = CreateWidgetNode->GetClassPin();
	if ((ClassPin == Pin) && (!ClassPin->bHidden || (ClassPin->LinkedTo.Num() > 0)))
	{
		TSharedPtr<SGraphPinClass> NewPin = SNew(SGraphPinClass, ClassPin);
		check(NewPin.IsValid());
		NewPin->SetAllowAbstractClasses(false);
		return NewPin;
	}
	return SGraphNodeK2Default::CreatePinWidget(Pin);
}
