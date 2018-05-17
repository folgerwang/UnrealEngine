// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraTypes.h"
#include "NiagaraGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "GraphEditorDragDropAction.h"
#include "NiagaraActions.generated.h"

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraMenuAction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	DECLARE_DELEGATE(FOnExecuteStackAction);
	DECLARE_DELEGATE_RetVal(bool, FCanExecuteStackAction);

	FNiagaraMenuAction()
	{
	}

	FNiagaraMenuAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, FOnExecuteStackAction InAction, int32 InSectionID = 0);
	FNiagaraMenuAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, FOnExecuteStackAction InAction, FCanExecuteStackAction InCanPerformAction, int32 InSectionID = 0);

	void ExecuteAction()
	{
		if (CanExecute())
		{
			Action.ExecuteIfBound();
		}
	}

	bool CanExecute() const
	{
		// Fire the 'can execute' delegate if we have one, otherwise always return true
		return CanPerformAction.IsBound() ? CanPerformAction.Execute() : true;
	}

private:
	FOnExecuteStackAction Action;
	FCanExecuteStackAction CanPerformAction;
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraParameterAction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	FNiagaraParameterAction() {}
	FNiagaraParameterAction(const FNiagaraVariable& InParameter,
		const TArray<FNiagaraGraphParameterReferenceCollection>& InReferenceCollection,
		FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, int32 InSectionID = 0);

	FNiagaraVariable& GetParameter() { return Parameter; }

	UPROPERTY()
	FNiagaraVariable Parameter;

	UPROPERTY()
	TArray<FNiagaraGraphParameterReferenceCollection> ReferenceCollection;
};

class NIAGARAEDITOR_API FNiagaraParameterGraphDragOperation : public FGraphSchemaActionDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraParameterGraphDragOperation, FGraphSchemaActionDragDropAction)

	static TSharedRef<FNiagaraParameterGraphDragOperation> New(const TSharedPtr<FEdGraphSchemaAction>& InActionNode);

	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnPanel(const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	// End of FGraphEditorDragDropAction

	/** Set if operation is modified by alt */
	void SetAltDrag(bool InIsAltDrag) { bAltDrag = InIsAltDrag; }

	/** Set if operation is modified by the ctrl key */
	void SetCtrlDrag(bool InIsCtrlDrag) { bControlDrag = InIsCtrlDrag; }

protected:
	/** Constructor */
	FNiagaraParameterGraphDragOperation();

	/** Structure for required node construction parameters */
	struct FNiagaraParameterNodeConstructionParams
	{
		FVector2D GraphPosition;
		UEdGraph* Graph;
		FNiagaraVariable Parameter;
	};

	static void MakeGetMap(FNiagaraParameterNodeConstructionParams InParams);
	static void MakeSetMap(FNiagaraParameterNodeConstructionParams InParams);

	virtual EVisibility GetIconVisible() const override;
	virtual EVisibility GetErrorIconVisible() const override;

	/** Was ctrl held down at start of drag */
	bool bControlDrag;
	/** Was alt held down at the start of drag */
	bool bAltDrag;
};

class NIAGARAEDITOR_API FNiagaraStackDragOperation : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraStackDragOperation, FDragDropOperation)

	static TSharedRef<FNiagaraStackDragOperation> New(TSharedPtr<FEdGraphSchemaAction> InActionNode);

	void Construct();

	/** Set if operation is modified by alt */
	void SetAltDrag(bool InIsAltDrag) { bAltDrag = InIsAltDrag; }

	/** Set if operation is modified by the ctrl key */
	void SetCtrlDrag(bool InIsCtrlDrag) { bControlDrag = InIsCtrlDrag; }

	TSharedPtr<FEdGraphSchemaAction> GetAction() { return SourceAction; }

	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override;
	virtual void OnDragged(const class FDragDropEvent& DragDropEvent) override;
protected:
	FNiagaraStackDragOperation();

	bool HasFeedbackMessage();
	void SetFeedbackMessage(const TSharedPtr<SWidget>& Message);
	void SetSimpleFeedbackMessage(const FText& Message);
	
	TSharedPtr<FEdGraphSchemaAction> SourceAction;

	/** Was ctrl held down at start of drag */
	bool bControlDrag;
	/** Was alt held down at the start of drag */
	bool bAltDrag;
};