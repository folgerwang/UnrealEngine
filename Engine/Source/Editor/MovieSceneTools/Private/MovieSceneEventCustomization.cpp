// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneEventCustomization.h"
#include "Modules/ModuleManager.h"
#include "Algo/Find.h"

#include "UObject/UnrealType.h"
#include "Channels/MovieSceneEvent.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventSectionBase.h"
#include "MovieSceneSequence.h"
#include "ISequencerModule.h"
#include "ScopedTransaction.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SEditableTextBox.h"

#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"

#include "K2Node_Variable.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"

#include "EditorStyleSet.h"
#include "EditorFontGlyphs.h"

#define LOCTEXT_NAMESPACE "MovieSceneEventCustomization"

TSharedRef<IPropertyTypeCustomization> FMovieSceneEventCustomization::MakeInstance()
{
	return MakeShared<FMovieSceneEventCustomization>();
}

TSharedRef<IPropertyTypeCustomization> FMovieSceneEventCustomization::MakeInstance(UMovieSceneSection* InSection)
{
	TSharedRef<FMovieSceneEventCustomization> Custo = MakeShared<FMovieSceneEventCustomization>();
	Custo->WeakExternalSection = InSection;
	return Custo;
}

void FMovieSceneEventCustomization::GetEditObjects(TArray<UObject*>& OutObjects) const
{
	UMovieSceneEventSectionBase* ExternalSection = Cast<UMovieSceneEventSectionBase>(WeakExternalSection.Get());
	if (ExternalSection)
	{
		OutObjects.Add(ExternalSection);
	}
	else
	{
		PropertyHandle->GetOuterObjects(OutObjects);
	}
}

void FMovieSceneEventCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{}

void FMovieSceneEventCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	ChildBuilder.AddCustomRow(FText())
	.NameContent()
	[
		SNew(STextBlock)
		.Font(CustomizationUtils.GetRegularFont())
		.Text(LOCTEXT("EventValueText", "Event"))
	]
	.ValueContent()
	.MinDesiredWidth(200.f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		[
			SNew(SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.OnGetMenuContent(this, &FMovieSceneEventCustomization::GetMenuContent)
			.CollapseMenuOnParentFocus(true)
			.ContentPadding(FMargin(4.f, 0.f))
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &FMovieSceneEventCustomization::GetEventIcon)
				]

				+ SHorizontalBox::Slot()
				.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(CustomizationUtils.GetRegularFont())
					.Text(this, &FMovieSceneEventCustomization::GetEventName)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FMovieSceneEventCustomization::NavigateToDefinition), LOCTEXT("NavigateToDefinition_Tip", "Navigate to this event's definition"))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Visibility(this, &FMovieSceneEventCustomization::GetErrorVisibility)
			.ToolTipText(this, &FMovieSceneEventCustomization::GetErrorTooltip)
			.TextStyle(FEditorStyle::Get(), "Log.Warning")
			.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
			.Text(FEditorFontGlyphs::Exclamation_Triangle)
		]
	];
}

UClass* FMovieSceneEventCustomization::FindObjectBindingClass()
{
	UMovieSceneSequence* CommonSequence = GetCommonSequence();
	UMovieScene*         MovieScene     = CommonSequence ? CommonSequence->GetMovieScene() : nullptr;

	if (!MovieScene)
	{
		return nullptr;
	}

	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	UClass* BindingClass = nullptr;

	for (UObject* EditObject : EditObjects)
	{
		UMovieSceneTrack* Track = EditObject->GetTypedOuter<UMovieSceneTrack>();
		if (Track)
		{
			for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
			{
				if (Algo::Find(Binding.GetTracks(), Track))
				{
					UClass* ThisTrackClass = nullptr;
					if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid()))
					{
						ThisTrackClass = const_cast<UClass*>(Possessable->GetPossessedObjectClass());
					}
					else if(FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Binding.GetObjectGuid()))
					{
						ThisTrackClass = Spawnable->GetObjectTemplate()->GetClass();
					}

					if (!BindingClass)
					{
						BindingClass = ThisTrackClass;
					}
					else if (BindingClass != ThisTrackClass)
					{
						return nullptr;
					}
					break;
				}
			}
		}
	}

	return BindingClass;
}

UMovieSceneSequence* FMovieSceneEventCustomization::GetCommonSequence() const
{
	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	UMovieSceneSequence* CommonEventSequence = nullptr;

	for (UObject* Obj : EditObjects)
	{
		UMovieSceneSequence* ThisSequence = Obj ? Obj->GetTypedOuter<UMovieSceneSequence>() : nullptr;
		if (CommonEventSequence && CommonEventSequence != ThisSequence)
		{
			return nullptr;
		}

		CommonEventSequence = ThisSequence;
	}
	return CommonEventSequence;
}

UMovieSceneEventTrack* FMovieSceneEventCustomization::GetCommonTrack() const
{
	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	UMovieSceneEventTrack* CommonEventTrack = nullptr;

	for (UObject* Obj : EditObjects)
	{
		UMovieSceneEventTrack* ThisTrack = Obj ? Obj->GetTypedOuter<UMovieSceneEventTrack>() : nullptr;
		if (CommonEventTrack && CommonEventTrack != ThisTrack)
		{
			return nullptr;
		}

		CommonEventTrack = ThisTrack;
	}
	return CommonEventTrack;
}

UK2Node_FunctionEntry* FMovieSceneEventCustomization::GetCommonEndpoint() const
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	UK2Node_FunctionEntry* CommonEndpoint = nullptr;

	for (void* Ptr : RawData)
	{
		if (Ptr)
		{
			UK2Node_FunctionEntry* ThisEndpoint = static_cast<FMovieSceneEvent*>(Ptr)->GetFunctionEntry();
			if (CommonEndpoint && CommonEndpoint != ThisEndpoint)
			{
				return nullptr;
			}

			CommonEndpoint = ThisEndpoint;
		}
	}
	return CommonEndpoint;
}

TSharedRef<SWidget> FMovieSceneEventCustomization::GetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr, nullptr, true);

	UMovieSceneSequence*       Sequence       = GetCommonSequence();
	UK2Node_FunctionEntry*     CommonEndpoint = GetCommonEndpoint();
	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);

	UBlueprint* DirectorBP = SequenceEditor ? SequenceEditor->GetDirectorBlueprint(Sequence) : nullptr;

	CachedCommonEndpoint = CommonEndpoint;

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CreateEventEndpoint_Text",    "Create New Endpoint"),
		LOCTEXT("CreateEventEndpoint_Tooltip", "Creates a new event endpoint in this sequence's blueprint."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.CreateEventBinding"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMovieSceneEventCustomization::CreateEventEndpoint)
		)
	);

	UClass* TemplateClass = FindObjectBindingClass();
	if (TemplateClass)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("CreateQuickBinding_Text",    "Create Quick Binding"),
			LOCTEXT("CreateQuickBinding_Tooltip", "Shows a list of functions on this object binding that can be bound directly to this event."),
			FNewMenuDelegate::CreateSP(this, &FMovieSceneEventCustomization::PopulateQuickBindSubMenu, TemplateClass),
			false /* bInOpenSubMenuOnClick */,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.CreateQuickBinding"),
			false /* bInShouldCloseWindowAfterMenuSelection */
		);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ClearEventEndpoint_Text",    "Clear"),
		LOCTEXT("ClearEventEndpoint_Tooltip", "Unbinds this event from its current binding."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.ClearEventBinding"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMovieSceneEventCustomization::ClearEventEndpoint)
		)
	);

	if (DirectorBP)
	{
		FSlateIcon Icon(FEditorStyle::GetStyleSetName(), "GraphEditor.Function_16x");

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("ExistingEndpoints", "Existing"));

		TArray<UK2Node_FunctionEntry*> EntryNodes;

		for (UEdGraph* FunctionGraph : DirectorBP->FunctionGraphs)
		{
			EntryNodes.Reset();
			FunctionGraph->GetNodesOfClass<UK2Node_FunctionEntry>(EntryNodes);

			if (EntryNodes.Num() == 1 && FMovieSceneEvent::IsValidFunction(EntryNodes[0]))
			{
				MenuBuilder.AddMenuEntry(
					EntryNodes[0]->GetNodeTitle(ENodeTitleType::MenuTitle),
					EntryNodes[0]->GetTooltipText(),
					Icon,
					FUIAction(
						FExecuteAction::CreateSP(this, &FMovieSceneEventCustomization::SetEventEndpoint, EntryNodes[0]),
						FCanExecuteAction::CreateLambda([]{ return true; }),
						FIsActionChecked::CreateSP(this, &FMovieSceneEventCustomization::CompareCurrentEventEndpoint, EntryNodes[0])
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);
			}
		}

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void FMovieSceneEventCustomization::PopulateQuickBindSubMenu(FMenuBuilder& MenuBuilder, UClass* TemplateClass)
{
	FSlateIcon Icon(FEditorStyle::GetStyleSetName(), "GraphEditor.Function_16x");
	static const FName DeprecatedFunctionName(TEXT("DeprecatedFunction"));

	TArray<UFunction*> Functions;

	UClass* SuperClass = TemplateClass;
	while (SuperClass)
	{
		MenuBuilder.BeginSection(NAME_None, SuperClass->GetDisplayNameText());

		Functions.Reset();
		for (UFunction* Function : TFieldRange<UFunction>(SuperClass, EFieldIteratorFlags::ExcludeSuper, EFieldIteratorFlags::ExcludeDeprecated))
		{
			if (Function->HasAllFunctionFlags(FUNC_BlueprintCallable|FUNC_Public) && !Function->HasMetaData(DeprecatedFunctionName))
			{
				Functions.Add(Function);
			}
		}
	
		Algo::SortBy(Functions, &UFunction::GetFName);

		for (UFunction* Function : Functions)
		{
			MenuBuilder.AddMenuEntry(
				FText::FromName(Function->GetFName()),
				FText(),
				Icon,
				FUIAction(
					FExecuteAction::CreateSP(this, &FMovieSceneEventCustomization::CreateEventEndpointFromFunction, Function, TemplateClass)
				)
			);
		}

		MenuBuilder.EndSection();

		SuperClass = SuperClass->GetSuperClass();
	}
}

const FSlateBrush* FMovieSceneEventCustomization::GetEventIcon() const
{
	UK2Node_FunctionEntry* CommonEndpoint = GetCommonEndpoint();

	if (FMovieSceneEvent::IsValidFunction(CommonEndpoint))
	{
		return FEditorStyle::GetBrush("GraphEditor.Function_16x");
	}
	else if (!CommonEndpoint)
	{
		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);
		if (RawData.Num() > 1)
		{
			return FEditorStyle::GetBrush("Sequencer.MultipleEvents");
		}
	}

	return FEditorStyle::GetBrush("Sequencer.UnboundEvent");
}

FText FMovieSceneEventCustomization::GetEventName() const
{
	UK2Node_FunctionEntry* CommonEndpoint = GetCommonEndpoint();

	if (FMovieSceneEvent::IsValidFunction(CommonEndpoint))
	{
		return CommonEndpoint->GetNodeTitle(ENodeTitleType::MenuTitle);
	}
	else if (!CommonEndpoint)
	{
		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);
		if (RawData.Num() != 1)
		{
			return LOCTEXT("MultipleValuesText", "Multiple Values");
		}
	}

	return LOCTEXT("UnboundText", "Unbound");
}

EVisibility FMovieSceneEventCustomization::GetErrorVisibility() const
{
	UK2Node_FunctionEntry* CommonEndpoint = GetCommonEndpoint();
	if (CommonEndpoint)
	{
		return FMovieSceneEvent::IsValidFunction(CommonEndpoint) ? EVisibility::Collapsed : EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FText FMovieSceneEventCustomization::GetErrorTooltip() const
{
	UK2Node_FunctionEntry* CommonEndpoint = GetCommonEndpoint();
	if (CommonEndpoint)
	{
		return FText::Format(LOCTEXT("ErrorToolTipFormat", "The currently assigned function '{0}' is not a valid event endpoint. Event endpoints must have a single object or interface parameter that is not passed by-reference."), CommonEndpoint->GetNodeTitle(ENodeTitleType::MenuTitle));
	}
	return FText::GetEmpty();
}

void FMovieSceneEventCustomization::SetEventEndpoint(UK2Node_FunctionEntry* NewEndpoint)
{
	FScopedTransaction Transaction(LOCTEXT("SetEventEndpoint", "Set Event Endpoint"));

	UBlueprint* Blueprint = NewEndpoint->GetBlueprint();

	CachedCommonEndpoint = NewEndpoint;

	// Modify and assign the blueprint for outer sections
	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	for (UObject* Outer : EditObjects)
	{
		UMovieSceneEventSectionBase* BaseEventSection = Cast<UMovieSceneEventSectionBase>(Outer);
		if (BaseEventSection)
		{
			BaseEventSection->Modify();
			BaseEventSection->SetDirectorBlueprint(Blueprint);
		}
	}

	// Assign the endpoints to all events
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	for (void* Ptr : RawData)
	{
		FMovieSceneEvent* Event = static_cast<FMovieSceneEvent*>(Ptr);
		if (Event)
		{
			Event->SetFunctionEntry(NewEndpoint);
		}
	}

	// Ensure that anything listening for property changed notifications are notified of the new binding
	PropertyHandle->NotifyFinishedChangingProperties();
}

bool FMovieSceneEventCustomization::CompareCurrentEventEndpoint(UK2Node_FunctionEntry* NewEndpoint)
{
	return CachedCommonEndpoint == NewEndpoint;
}

void FMovieSceneEventCustomization::CreateEventEndpoint()
{
	UMovieSceneSequence*       Sequence       = GetCommonSequence();
	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);
	if (!SequenceEditor)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("CreateEventEndpoint", "Create Event Endpoint"));

	// Create a single event binding and point all events in this property handle to it
	UK2Node_FunctionEntry* NewFunctionEntry = SequenceEditor->CreateEventEndpoint(Sequence);
	if (!ensure(NewFunctionEntry))
	{
		return;
	}

	if (UMovieSceneEventTrack* CommonTrack = GetCommonTrack())
	{
		SequenceEditor->InitializeEndpointForTrack(CommonTrack, NewFunctionEntry);
	}

	SetEventEndpoint(NewFunctionEntry);
	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NewFunctionEntry, false);
}

void FMovieSceneEventCustomization::CreateEventEndpointFromFunction(UFunction* QuickBindFunction, UClass* PinClassType)
{
	UMovieSceneSequence*       Sequence       = GetCommonSequence();
	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);

	if (!SequenceEditor)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("CreateEventEndpoint", "Create Event Endpoint"));

	FString DesiredNewEventName = FString(TEXT("Call ")) + QuickBindFunction->GetName();

	// Create a single event binding and point all events in this property handle to it
	UK2Node_FunctionEntry* NewFunctionEntry = SequenceEditor->CreateEventEndpoint(Sequence, DesiredNewEventName);
	if (!ensure(NewFunctionEntry))
	{
		return;
	}

	if (ensure(NewFunctionEntry->UserDefinedPins.Num() == 0))
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = PinClassType->IsChildOf(UInterface::StaticClass()) ? UEdGraphSchema_K2::PC_Interface : UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = PinClassType;

		NewFunctionEntry->CreateUserDefinedPin(FMovieSceneSequenceEditor::TargetPinName, PinType, EGPD_Output, true);
		NewFunctionEntry->ReconstructNode();
	}

	NewFunctionEntry->bCommentBubblePinned = false;
	NewFunctionEntry->bCommentBubbleVisible = false;

	UEdGraph* Graph = NewFunctionEntry->GetGraph();

	// Make a call function template
	UK2Node_CallFunction* CallFuncNode = NewObject<UK2Node_CallFunction>(Graph);
	CallFuncNode->NodePosX = NewFunctionEntry->NodePosX + NewFunctionEntry->NodeWidth + 200;
	CallFuncNode->NodePosY = NewFunctionEntry->NodePosY;
	CallFuncNode->CreateNewGuid();
	CallFuncNode->SetFromFunction(QuickBindFunction);
	CallFuncNode->PostPlacedNewNode();
	CallFuncNode->ReconstructNode();

	Graph->AddNode(CallFuncNode, false, false);

	{
		// Connect the exec pins together
		UEdGraphPin* ThenPin = NewFunctionEntry->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* ExecPin = CallFuncNode->GetExecPin();

		if (ThenPin && ExecPin)
		{
			const UEdGraphSchema* Schema = ThenPin->GetSchema();
			Schema->TryCreateConnection(ThenPin, ExecPin);
		}
	}

	// Connect the object target pin to the self (input) pin on the call function node
	if (UEdGraphPin* SelfPin = CallFuncNode->FindPin(UEdGraphSchema_K2::PSC_Self))
	{
		for (UEdGraphPin* Pin : NewFunctionEntry->Pins)
		{
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface  || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
			{
				const UEdGraphSchema* Schema = Pin->GetSchema();
				Schema->TryCreateConnection(Pin, SelfPin);
			}
		}
	}

	SetEventEndpoint(NewFunctionEntry);
	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NewFunctionEntry, false);
}

void FMovieSceneEventCustomization::ClearEventEndpoint()
{
	FScopedTransaction Transaction(LOCTEXT("ClearEventEndpoint", "Clear Event Endpoint"));

	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	for (UObject* Outer : EditObjects)
	{
		if (Outer)
		{
			Outer->Modify();
		}
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	for (void* Ptr : RawData)
	{
		FMovieSceneEvent* Event = static_cast<FMovieSceneEvent*>(Ptr);
		if (Event)
		{
			Event->SetFunctionEntry(nullptr);
		}
	}

	// Ensure that anything listening for property changed notifications are notified of the new binding
	PropertyHandle->NotifyFinishedChangingProperties();
}

void FMovieSceneEventCustomization::NavigateToDefinition()
{
	UK2Node_FunctionEntry* CommonEndpoint = GetCommonEndpoint();
	if (CommonEndpoint)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(CommonEndpoint, false);
	}
}

#undef LOCTEXT_NAMESPACE
