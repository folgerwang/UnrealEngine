// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Kismet2/KismetDebugUtilities.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/TextProperty.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SScrollBox.h"
#include "EditorStyleSet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/EditorExperimentalSettings.h"
#include "CallStackViewer.h"
#include "WatchPointViewer.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "UnrealEdGlobals.h"
#include "Engine/Breakpoint.h"
#include "ActorEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_Tunnel.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Message.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "AnimGraphNode_Base.h"

#define LOCTEXT_NAMESPACE "BlueprintDebugging"

/** Per-thread data for use by FKismetDebugUtilities functions */
class FKismetDebugUtilitiesData : public TThreadSingleton<FKismetDebugUtilitiesData>
{
public:
	FKismetDebugUtilitiesData()
		: TargetGraphNodes()
		, CurrentInstructionPointer(nullptr)
		, MostRecentBreakpointInstructionPointer(nullptr)
		, MostRecentStoppedNode(nullptr)
		, TargetGraphStackDepth(INDEX_NONE)
		, MostRecentBreakpointGraphStackDepth(INDEX_NONE)
		, MostRecentBreakpointInstructionOffset(INDEX_NONE)
		, StackFrameAtIntraframeDebugging(nullptr)
		, TraceStackSamples(FKismetDebugUtilities::MAX_TRACE_STACK_SAMPLES)
		, bIsSingleStepping(false)
		, bIsSteppingOut(false)
	{
	}

	void Reset()
	{
		TargetGraphNodes.Empty();
		CurrentInstructionPointer = nullptr;
		MostRecentStoppedNode = nullptr;

		TargetGraphStackDepth = INDEX_NONE;
		MostRecentBreakpointGraphStackDepth = INDEX_NONE;
		MostRecentBreakpointInstructionOffset = INDEX_NONE;
		StackFrameAtIntraframeDebugging = nullptr;

		bIsSingleStepping = false;
		bIsSteppingOut = false;
	}

	// List of graph nodes that the user wants to stop at, at the current TargetGraphStackDepth. Used for Step Over:
	TArray< TWeakObjectPtr< class UEdGraphNode> > TargetGraphNodes;

	// Current node:
	TWeakObjectPtr< class UEdGraphNode > CurrentInstructionPointer;

	// The current instruction encountered if we are stopped at a breakpoint; NULL otherwise
	TWeakObjectPtr< class UEdGraphNode > MostRecentBreakpointInstructionPointer;
	
	// The last node that we decided to break on for any reason (e.g. breakpoint, exception, or step operation):
	TWeakObjectPtr< class UEdGraphNode > MostRecentStoppedNode;

	// The target graph call stack depth. INDEX_NONE if not active
	int32 TargetGraphStackDepth;

	// The graph stack depth that a breakpoint was hit at, used to ensure that breakpoints
	// can be hit multiple times in the case of recursion
	int32 MostRecentBreakpointGraphStackDepth;

	// The instruction that we hit a breakpoint at, this is used to ensure that a given node
	// can be stepped over reliably (but still break multiple times in the case of recursion):
	int32 MostRecentBreakpointInstructionOffset;

	// The last message that an exception delivered
	FText LastExceptionMessage;

	// Only valid inside intraframe debugging
	const FFrame* StackFrameAtIntraframeDebugging;

	// This data is used for the 'marching ants' display in the blueprint editor
	TSimpleRingBuffer<FKismetTraceSample> TraceStackSamples;

	// This flag controls whether we're trying to 'step in' to a function
	bool bIsSingleStepping;

	// This flag controls whether we're trying to 'step out' of a graph
	bool bIsSteppingOut;
};

//////////////////////////////////////////////////////////////////////////
// FKismetDebugUtilities

void FKismetDebugUtilities::EndOfScriptExecution()
{
#if DO_BLUEPRINT_GUARD
	FBlueprintExceptionTracker& BlueprintExceptionTracker = FBlueprintExceptionTracker::Get();
	if(BlueprintExceptionTracker.ScriptEntryTag == 1)
	{
		// if this is our last VM frame, then clear stepping data:
		FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

		Data.Reset();
	}
#endif // DO_BLUEPRINT_GUARD
}

void FKismetDebugUtilities::RequestSingleStepIn()
{
#if DO_BLUEPRINT_GUARD
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();
	FBlueprintExceptionTracker& BlueprintExceptionTracker = FBlueprintExceptionTracker::Get();

	Data.bIsSingleStepping = true;
#endif // DO_BLUEPRINT_GUARD
}

void FKismetDebugUtilities::RequestStepOver()
{
#if DO_BLUEPRINT_GUARD
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();
	FBlueprintExceptionTracker& BlueprintExceptionTracker = FBlueprintExceptionTracker::Get();

	if(BlueprintExceptionTracker.ScriptStack.Num() > 0)
	{
		Data.TargetGraphStackDepth = BlueprintExceptionTracker.ScriptStack.Num();
		
		// get the current graph that we're stopped at:
		const FFrame* CurrentFrame = BlueprintExceptionTracker.ScriptStack.Last();
		if(CurrentFrame->Object)
		{
			if(UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(CurrentFrame->Object->GetClass()))
			{
				const int32 BreakpointOffset = CurrentFrame->Code - CurrentFrame->Node->Script.GetData() - 1;
				UEdGraphNode* BlueprintNode = BPGC->DebugData.FindSourceNodeFromCodeLocation(CurrentFrame->Node, BreakpointOffset, true);
				if(BlueprintNode)
				{
					// add any nodes connected via execs as TargetGraphNodes:
					for(UEdGraphPin* Pin : BlueprintNode->Pins)
					{
						if(Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->LinkedTo.Num() > 0)
						{
							for(UEdGraphPin* LinkedTo : Pin->LinkedTo)
							{
								Data.TargetGraphNodes.AddUnique(LinkedTo->GetOwningNode());
							}
						}
					}
				}
			}
		}
	}
#endif // DO_BLUEPRINT_GUARD
}

void FKismetDebugUtilities::RequestStepOut()
{
#if DO_BLUEPRINT_GUARD
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();
	FBlueprintExceptionTracker& BlueprintExceptionTracker = FBlueprintExceptionTracker::Get();

	Data.bIsSingleStepping = false;
	if (BlueprintExceptionTracker.ScriptStack.Num() > 1)
	{
		Data.bIsSteppingOut = true;
		Data.TargetGraphStackDepth = BlueprintExceptionTracker.ScriptStack.Num() - 1;
	}
#endif // DO_BLUEPRINT_GUARD
}

void FKismetDebugUtilities::OnScriptException(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info)
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	struct Local
	{
		static void OnMessageLogLinkActivated(const class TSharedRef<IMessageToken>& Token)
		{
			if( Token->GetType() == EMessageToken::Object )
			{
				const TSharedRef<FUObjectToken> UObjectToken = StaticCastSharedRef<FUObjectToken>(Token);
				if(UObjectToken->GetObject().IsValid())
				{
					FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(UObjectToken->GetObject().Get());
				}	
			}
		}
	};

	checkSlow(ActiveObject != NULL);

	// Ignore script exceptions for preview actors
	if(FActorEditorUtils::IsAPreviewOrInactiveActor(Cast<const AActor>(ActiveObject)))
	{
		return;
	}
	
	UClass* ClassContainingCode = FindClassForNode(ActiveObject, StackFrame.Node);
	UBlueprint* BlueprintObj = (ClassContainingCode ? Cast<UBlueprint>(ClassContainingCode->ClassGeneratedBy) : NULL);
	if (BlueprintObj)
	{
		const FBlueprintExceptionInfo* ExceptionInfo = &Info;
		bool bResetObjectBeingDebuggedWhenFinished = false;
		UObject* ObjectBeingDebugged = BlueprintObj->GetObjectBeingDebugged();
		UObject* SavedObjectBeingDebugged = ObjectBeingDebugged;
		UWorld* WorldBeingDebugged = BlueprintObj->GetWorldBeingDebugged();

		const int32 BreakpointOffset = StackFrame.Code - StackFrame.Node->Script.GetData() - 1;

		bool bShouldBreakExecution = false;
		bool bForceToCurrentObject = false;

		switch (Info.GetType())
		{
		case EBlueprintExceptionType::Breakpoint:
			bShouldBreakExecution = true;
			break;
		case EBlueprintExceptionType::Tracepoint:
			bShouldBreakExecution = Data.bIsSingleStepping || Data.TargetGraphStackDepth != INDEX_NONE;
			break;
		case EBlueprintExceptionType::WireTracepoint:
			break;
		case EBlueprintExceptionType::AccessViolation:
			if ( GIsEditor && GIsPlayInEditorWorld )
			{
				// declared as its own variable since it's flushed (logs pushed to std output) on destruction
				// we want the full message constructed before it's logged
				TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
				Message->AddToken(FTextToken::Create(FText::Format(LOCTEXT("RuntimeErrorMessageFmt", "Blueprint Runtime Error: \"{0}\"."), Info.GetDescription())));

				Message->AddToken(FTextToken::Create(LOCTEXT("RuntimeErrorBlueprintObjectLabel", "Blueprint: ")));
				Message->AddToken(FUObjectToken::Create(BlueprintObj, FText::FromString(BlueprintObj->GetName()))
					->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(&Local::OnMessageLogLinkActivated))
				);

				// NOTE: StackFrame.Node is not a blueprint node like you may think ("Node" has some legacy meaning)
				Message->AddToken(FTextToken::Create(LOCTEXT("RuntimeErrorBlueprintFunctionLabel", "Function: ")));
				Message->AddToken(FUObjectToken::Create(StackFrame.Node, StackFrame.Node->GetDisplayNameText())
					->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(&Local::OnMessageLogLinkActivated))
				);

#if WITH_EDITORONLY_DATA // to protect access to GeneratedClass->DebugData
				UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(ClassContainingCode);
				if ((GeneratedClass != NULL) && GeneratedClass->DebugData.IsValid())
				{
					UEdGraphNode* BlueprintNode = GeneratedClass->DebugData.FindSourceNodeFromCodeLocation(StackFrame.Node, BreakpointOffset, true);
					// if instead, there is a node we can point to...
					if (BlueprintNode != NULL)
					{
						Message->AddToken(FTextToken::Create(LOCTEXT("RuntimeErrorBlueprintGraphLabel", "Graph: ")));
						Message->AddToken(FUObjectToken::Create(BlueprintNode->GetGraph(), FText::FromString(GetNameSafe(BlueprintNode->GetGraph())))
							->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(&Local::OnMessageLogLinkActivated))
						);

						Message->AddToken(FTextToken::Create(LOCTEXT("RuntimeErrorBlueprintNodeLabel", "Node: ")));
						Message->AddToken(FUObjectToken::Create(BlueprintNode, BlueprintNode->GetNodeTitle(ENodeTitleType::ListView))
							->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(&Local::OnMessageLogLinkActivated))
						);
					}
				}
#endif // WITH_EDITORONLY_DATA
				FMessageLog("PIE").AddMessage(Message);
			}
			bForceToCurrentObject = true;
			bShouldBreakExecution = GetDefault<UEditorExperimentalSettings>()->bBreakOnExceptions;
			break;
		case EBlueprintExceptionType::InfiniteLoop:
			bForceToCurrentObject = true;
			bShouldBreakExecution = GetDefault<UEditorExperimentalSettings>()->bBreakOnExceptions;
			break;
		default:
			bForceToCurrentObject = true;
			bShouldBreakExecution = GetDefault<UEditorExperimentalSettings>()->bBreakOnExceptions;
			break;
		}

		// If we are debugging a specific world, the object needs to be in it
		if (WorldBeingDebugged != NULL && !ActiveObject->IsIn(WorldBeingDebugged))
		{
			// Might be a streaming level case, so find the real world to see
			const UObject *ObjOuter = ActiveObject;
			const UWorld *ObjWorld = NULL;
			bool FailedWorldCheck = true;
			while(ObjWorld == NULL && ObjOuter != NULL)
			{
				ObjOuter = ObjOuter->GetOuter();
				ObjWorld = Cast<const UWorld>(ObjOuter);
			}
			if (ObjWorld && ObjWorld->PersistentLevel)
			{
				if (ObjWorld->PersistentLevel->OwningWorld == WorldBeingDebugged)
				{
					// Its ok, the owning world is the world being debugged
					FailedWorldCheck = false;
				}				
			}

			if (FailedWorldCheck)
			{
				bForceToCurrentObject = false;
				bShouldBreakExecution = false;
			}
		}

		if (bShouldBreakExecution)
		{
			if ((ObjectBeingDebugged == NULL) || (bForceToCurrentObject))
			{
				// If there was nothing being debugged, treat this as a one-shot, temporarily set this object as being debugged,
				// and continue allowing any breakpoint to hit later on
				bResetObjectBeingDebuggedWhenFinished = true;
				BlueprintObj->SetObjectBeingDebugged(const_cast<UObject*>(ActiveObject));
			}
		}

		if (BlueprintObj->GetObjectBeingDebugged() == ActiveObject)
		{
			// Record into the trace log
			FKismetTraceSample& Tracer = Data.TraceStackSamples.WriteNewElementUninitialized();
			Tracer.Context = MakeWeakObjectPtr(const_cast<UObject*>(ActiveObject));
			Tracer.Function = StackFrame.Node;
			Tracer.Offset = BreakpointOffset; //@TODO: Might want to make this a parameter of Info
			Tracer.ObservationTime = FPlatformTime::Seconds();

			// Find the node that generated the code which we hit
			UEdGraphNode* NodeStoppedAt = FindSourceNodeForCodeLocation(ActiveObject, StackFrame.Node, BreakpointOffset, /*bAllowImpreciseHit=*/ true);
			if (NodeStoppedAt && (Info.GetType() == EBlueprintExceptionType::Tracepoint || Info.GetType() == EBlueprintExceptionType::Breakpoint))
			{
				// Handle Node stepping and update the stack
				CheckBreakConditions(NodeStoppedAt, Info.GetType() == EBlueprintExceptionType::Breakpoint, BreakpointOffset, bShouldBreakExecution);
			}

			// Can't do intraframe debugging when the editor is actively stopping
			if (GEditor->ShouldEndPlayMap())
			{
				bShouldBreakExecution = false;
			}

			// Handle a breakpoint or single-step
			if (bShouldBreakExecution)
			{
				AttemptToBreakExecution(BlueprintObj, ActiveObject, StackFrame, *ExceptionInfo, NodeStoppedAt, BreakpointOffset);
			}
		}

		// Reset the object being debugged if we forced it to be something different
		if (bResetObjectBeingDebuggedWhenFinished)
		{
			BlueprintObj->SetObjectBeingDebugged(SavedObjectBeingDebugged);
		}

		const auto ShowScriptExceptionError = [&](const FText& InExceptionErrorMsg)
		{
			if (GUnrealEd->PlayWorld != NULL)
			{
				GEditor->RequestEndPlayMap();
				FSlateApplication::Get().LeaveDebuggingMode();
			}

			// Launch a message box notifying the user why they have been booted
			{
				// Callback to display a pop-up showing the Callstack, the user can highlight and copy this if needed
				auto DisplayCallStackLambda = [](const FText CallStack)
				{
					TSharedPtr<SMultiLineEditableText> TextBlock;
					TSharedRef<SWidget> DisplayWidget =
						SNew(SBox)
						.MaxDesiredHeight(512)
						.MaxDesiredWidth(512)
						.Content()
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(TextBlock, SMultiLineEditableText)
									.AutoWrapText(true)
									.IsReadOnly(true)
									.Text(CallStack)
								]
							]
						];

					FSlateApplication::Get().PushMenu(
						FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(),
						FWidgetPath(),
						DisplayWidget,
						FSlateApplication::Get().GetCursorPos(),
						FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
						);

					FSlateApplication::Get().SetKeyboardFocus(TextBlock);
				};

				TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);

				// Display the main error message
				Message->AddToken(FTextToken::Create(InExceptionErrorMsg));

				// Display a link to the UObject and the UFunction that is crashing
				{
					// Get the name of the Blueprint
					FString BlueprintName;
					BlueprintObj->GetName(BlueprintName);

					Message->AddToken(FTextToken::Create(LOCTEXT("ShowScriptExceptionError_BlueprintLabel", "Blueprint: ")));
					Message->AddToken(FUObjectToken::Create(BlueprintObj, FText::FromString(BlueprintName)));
				}
				{
					// If a source node is found, that's the token we want to link, otherwise settle with the UFunction
					const int32 BreakpointOpCodeOffset = StackFrame.Code - StackFrame.Node->Script.GetData() - 1; //@TODO: Might want to make this a parameter of Info
					UEdGraphNode* SourceNode = FindSourceNodeForCodeLocation(ActiveObject, StackFrame.Node, BreakpointOpCodeOffset, /*bAllowImpreciseHit=*/ true);

					Message->AddToken(FTextToken::Create(LOCTEXT("ShowScriptExceptionError_FunctionLabel", "Function: ")));
					if (SourceNode)
					{
						Message->AddToken(FUObjectToken::Create(SourceNode, SourceNode->GetNodeTitle(ENodeTitleType::ListView)));
					}
					else
					{
						Message->AddToken(FUObjectToken::Create(StackFrame.Node, StackFrame.Node->GetDisplayNameText()));
					}
				}

				// Display a pop-up that will display the complete script callstack
				Message->AddToken(FTextToken::Create(LOCTEXT("ShowScriptExceptionError_CallStackLabel", "Call Stack: ")));
				Message->AddToken(FActionToken::Create(LOCTEXT("ShowScriptExceptionError_ShowCallStack", "Show"), LOCTEXT("ShowScriptExceptionError_ShowCallStackDesc", "Displays the underlying callstack, tracing what function calls led to the assert occuring."), FOnActionTokenExecuted::CreateStatic(DisplayCallStackLambda, FText::FromString(StackFrame.GetStackTrace()))));
				FMessageLog("PIE").AddMessage(Message);
			}
		};

		// Extra cleanup after potential interactive handling
		switch (Info.GetType())
		{
		case EBlueprintExceptionType::FatalError:
			ShowScriptExceptionError(FText::Format(LOCTEXT("ShowScriptExceptionError_FatalErrorFmt", "Fatal error detected: \"{0}\"."), Info.GetDescription()));
			break;
		case EBlueprintExceptionType::InfiniteLoop:
			ShowScriptExceptionError(LOCTEXT("ShowScriptExceptionError_InfiniteLoop", "Infinite loop detected."));
			break;
		default:
			// Left empty intentionally
			break;
		}
	}
}

UClass* FKismetDebugUtilities::FindClassForNode(const UObject* Object, UFunction* Function)
{
	if (NULL != Function)
	{
		UClass* FunctionOwner = Function->GetOwnerClass();
		return FunctionOwner;
	}
	if(NULL != Object)
	{
		UClass* ObjClass = Object->GetClass();
		return ObjClass;
	}
	return NULL;
}	

const TSimpleRingBuffer<FKismetTraceSample>& FKismetDebugUtilities::GetTraceStack()
{
	return FKismetDebugUtilitiesData::Get().TraceStackSamples; 
}

UEdGraphNode* FKismetDebugUtilities::FindSourceNodeForCodeLocation(const UObject* Object, UFunction* Function, int32 DebugOpcodeOffset, bool bAllowImpreciseHit)
{
	if (Object != NULL)
	{
		// Find the blueprint that corresponds to the object
		if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(FindClassForNode(Object, Function)))
		{
			return Class->GetDebugData().FindSourceNodeFromCodeLocation(Function, DebugOpcodeOffset, bAllowImpreciseHit);
		}
	}

	return NULL;
}

void FKismetDebugUtilities::CheckBreakConditions(UEdGraphNode* NodeStoppedAt, bool bHitBreakpoint, int32 BreakpointOffset, bool& InOutBreakExecution)
{
#if DO_BLUEPRINT_GUARD
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();
	FBlueprintExceptionTracker& BlueprintExceptionTracker = FBlueprintExceptionTracker::Get();

	if (NodeStoppedAt)
	{
		const bool bIsTryingToBreak = bHitBreakpoint ||
			Data.TargetGraphStackDepth != INDEX_NONE ||
			Data.bIsSingleStepping;

		if(bIsTryingToBreak)
		{
			// Update the TargetGraphStackDepth if we're on the same node - this handles things like
			// event nodes in the Event Graph, which will push another frame on to the stack:
			if(NodeStoppedAt == Data.MostRecentStoppedNode &&
				Data.MostRecentBreakpointGraphStackDepth < BlueprintExceptionTracker.ScriptStack.Num() &&
				Data.TargetGraphStackDepth != INDEX_NONE)
			{
				// when we recurse, when a node increases stack depth itself we want to increase our 
				// target depth to compensate:
				Data.TargetGraphStackDepth += 1;
			}
			else if(NodeStoppedAt != Data.MostRecentStoppedNode)
			{
				Data.MostRecentStoppedNode = nullptr;
			}

			// We should only actually break execution when we're on a new node or we've recursed to the same
			// node. We detect recursion by checking for a deeper stack and an earlier instruction:
			InOutBreakExecution = 
				NodeStoppedAt != Data.MostRecentStoppedNode ||
				(
					Data.MostRecentBreakpointGraphStackDepth < BlueprintExceptionTracker.ScriptStack.Num() &&
					Data.MostRecentBreakpointInstructionOffset >= BreakpointOffset
				);

			// If we have a TargetGraphStackDepth, don't break if we haven't reached that stack depth, or if we've stepped
			// in to a collapsed graph/macro instance:
			if(InOutBreakExecution && Data.TargetGraphStackDepth != INDEX_NONE && !bHitBreakpoint)
			{
				InOutBreakExecution = Data.TargetGraphStackDepth >= BlueprintExceptionTracker.ScriptStack.Num();
				if(InOutBreakExecution && Data.TargetGraphStackDepth == BlueprintExceptionTracker.ScriptStack.Num())
				{
					// we're at the same stack depth, don't break if we've entered a different graph, but do break if we left the 
					// graph that we were trying to step over..
					const FFrame* CurrentFrame = BlueprintExceptionTracker.ScriptStack.Last();
					if(CurrentFrame->Object)
					{
						if(UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(CurrentFrame->Object->GetClass()))
						{
							UEdGraphNode* BlueprintNode = BPGC->DebugData.FindSourceNodeFromCodeLocation(CurrentFrame->Node, BreakpointOffset, true);
							if(Data.TargetGraphNodes.Num() == 0 || Data.TargetGraphNodes.Contains(BlueprintNode))
							{
								InOutBreakExecution = true;
							}
							else
							{
								InOutBreakExecution = false; // nowhere to stop
							}
						}
						else
						{
							InOutBreakExecution = false;
						}
					}
				}
			}
		}
		else if (NodeStoppedAt != Data.MostRecentStoppedNode)
		{
			Data.MostRecentStoppedNode = nullptr;
		}
	}
	
	if (InOutBreakExecution)
	{
		Data.MostRecentStoppedNode = NodeStoppedAt;
		Data.MostRecentBreakpointGraphStackDepth = BlueprintExceptionTracker.ScriptStack.Num();
		Data.MostRecentBreakpointInstructionOffset = BreakpointOffset;
		Data.TargetGraphStackDepth = INDEX_NONE;
		Data.TargetGraphNodes.Empty();
		Data.bIsSteppingOut = false;
	}
	else if(Data.TargetGraphStackDepth != INDEX_NONE && Data.bIsSteppingOut)
	{
		UK2Node_Tunnel* AsTunnel = Cast<UK2Node_Tunnel>(NodeStoppedAt);
		if(AsTunnel)
		{
			// if we go through a tunnel entry/exit node update the target stack depth...
			if(AsTunnel->bCanHaveInputs)
			{
				Data.TargetGraphStackDepth += 1;
			}
			else if(AsTunnel->bCanHaveOutputs)
			{
				Data.TargetGraphStackDepth -= 1;
			}
		}
	}
#endif // DO_BLUEPRINT_GUARD
}

void FKismetDebugUtilities::AttemptToBreakExecution(UBlueprint* BlueprintObj, const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info, UEdGraphNode* NodeStoppedAt, int32 DebugOpcodeOffset)
{
#if DO_BLUEPRINT_GUARD
	checkSlow(BlueprintObj->GetObjectBeingDebugged() == ActiveObject);

	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	// Cannot have re-entrancy while processing a breakpoint; return from this call stack before resuming execution!
	check( !GIntraFrameDebuggingGameThread );
	
	TGuardValue<bool> SignalGameThreadBeingDebugged(GIntraFrameDebuggingGameThread, true);
	TGuardValue<const FFrame*> ResetStackFramePointer(Data.StackFrameAtIntraframeDebugging, &StackFrame);

	// Should we pump Slate messages from this callstack, allowing intra-frame debugging?
	bool bShouldInStackDebug = false;

	if (NodeStoppedAt != NULL)
	{
		bShouldInStackDebug = true;

		Data.CurrentInstructionPointer = NodeStoppedAt;

		Data.MostRecentBreakpointInstructionPointer = NULL;

		// Find the breakpoint object for the node, assuming we hit one
		if (Info.GetType() == EBlueprintExceptionType::Breakpoint)
		{
			UBreakpoint* Breakpoint = FKismetDebugUtilities::FindBreakpointForNode(BlueprintObj, NodeStoppedAt);

			if (Breakpoint != NULL)
			{
				Data.MostRecentBreakpointInstructionPointer = NodeStoppedAt;
				FKismetDebugUtilities::UpdateBreakpointStateWhenHit(Breakpoint, BlueprintObj);
					
				//@TODO: K2: DEBUGGING: Debug print text can go eventually
				UE_LOG(LogBlueprintDebug, Warning, TEXT("Hit breakpoint on node '%s', from offset %d"), *(NodeStoppedAt->GetDescriptiveCompiledName()), DebugOpcodeOffset);
				UE_LOG(LogBlueprintDebug, Log, TEXT("\n%s"), *StackFrame.GetStackTrace());
			}
			else
			{
				UE_LOG(LogBlueprintDebug, Warning, TEXT("Unknown breakpoint hit at node %s in object %s:%04X"), *NodeStoppedAt->GetDescriptiveCompiledName(), *StackFrame.Node->GetFullName(), DebugOpcodeOffset);
			}
		}

		// Turn off single stepping; we've hit a node
		if (Data.bIsSingleStepping)
		{
			Data.bIsSingleStepping = false;
		}
	}
	else if(UEdGraphNode* PreviousNode = FKismetDebugUtilities::GetCurrentInstruction())
	{
		if (UK2Node_Message* MessageNode = Cast<UK2Node_Message>(PreviousNode))
		{
			//Looks like object not implement one of their interfaces
			UE_LOG(LogBlueprintDebug, Warning, TEXT("Can't break execution on function '%s'. Possibly interface '%s' in class '%s' was not fully implemented."),
				*(PreviousNode->GetDocumentationExcerptName()),					  //Function name
				*(MessageNode->GetTargetFunction()->GetOuterUClass()->GetName()), //Interface name
				*(ActiveObject->GetClass()->GetName()));						  //Current object class name
		}
		else
		{
			UE_LOG(LogBlueprintDebug, Warning, TEXT("Can't break execution on function '%s'. Possibly it was not implemented in class '%s'."),
				*(PreviousNode->GetDocumentationExcerptName()),					  //Function name
				*(ActiveObject->GetClass()->GetName()));						  //Current object class name
		}
	}
	else
	{
		UE_LOG(LogBlueprintDebug, Warning, TEXT("Tried to break execution in an unknown spot at object %s:%04X"), *StackFrame.Node->GetFullName(), StackFrame.Code - StackFrame.Node->Script.GetData());
	}

	// A check to !GIsAutomationTesting was removed from here as it seemed redundant.
	// Breakpoints have to be explicitly enabled by the user which shouldn't happen 
	// under automation and this was preventing debugging on automation test bp's.
	if ((GUnrealEd->PlayWorld != NULL) && NodeStoppedAt)
	{
		// Pause the simulation
		GUnrealEd->PlayWorld->bDebugPauseExecution = true;
		GUnrealEd->PlayWorld->bDebugFrameStepExecution = false;
		bShouldInStackDebug = true;
	}
	else
	{
		bShouldInStackDebug = false;
		//@TODO: Determine exactly what behavior we want for breakpoints hit when not in PIE/SIE
		//ensureMsgf(false, TEXT("Breakpoints placed in a function instead of the event graph are not supported yet"));
	}

	// Now enter within-the-frame debugging mode
	if (bShouldInStackDebug)
	{
		TGuardValue<int32> GuardDisablePIE(GPlayInEditorID, INDEX_NONE);
		const TArray<const FFrame*>& ScriptStack = FBlueprintExceptionTracker::Get().ScriptStack;
		Data.LastExceptionMessage = Info.GetDescription();
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NodeStoppedAt);
		CallStackViewer::UpdateDisplayedCallstack(ScriptStack);
		WatchViewer::UpdateInstancedWatchDisplay();
		FSlateApplication::Get().EnterDebuggingMode();
	}
#endif // DO_BLUEPRINT_GUARD
}

UEdGraphNode* FKismetDebugUtilities::GetCurrentInstruction()
{
	// If paused at the end of the frame, or while not paused, there is no 'current instruction' to speak of
	// It only has meaning during intraframe debugging.
	if (GIntraFrameDebuggingGameThread)
	{
		return FKismetDebugUtilitiesData::Get().CurrentInstructionPointer.Get();
	}
	else
	{
		return NULL;
	}
}

UEdGraphNode* FKismetDebugUtilities::GetMostRecentBreakpointHit()
{
	// If paused at the end of the frame, or while not paused, there is no 'current instruction' to speak of
	// It only has meaning during intraframe debugging.
	if (GIntraFrameDebuggingGameThread)
	{
		return FKismetDebugUtilitiesData::Get().MostRecentBreakpointInstructionPointer.Get();
	}
	else
	{
		return NULL;
	}
}

// Notify the debugger of the start of the game frame
void FKismetDebugUtilities::NotifyDebuggerOfStartOfGameFrame(UWorld* CurrentWorld)
{
}

// Notify the debugger of the end of the game frame
void FKismetDebugUtilities::NotifyDebuggerOfEndOfGameFrame(UWorld* CurrentWorld)
{
	FKismetDebugUtilitiesData::Get().bIsSingleStepping = false;
}

bool FKismetDebugUtilities::IsSingleStepping()
{
	const FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();
	return Data.bIsSingleStepping
		|| Data.bIsSteppingOut
		|| Data.TargetGraphStackDepth != INDEX_NONE; 
}

//////////////////////////////////////////////////////////////////////////
// Breakpoint

// Is the node a valid breakpoint target? (i.e., the node is impure and ended up generating code)
bool FKismetDebugUtilities::IsBreakpointValid(UBreakpoint* Breakpoint)
{
	check(Breakpoint);

	// Breakpoints on impure nodes in a macro graph are always considered valid
	UBlueprint* Blueprint = Cast<UBlueprint>(Breakpoint->GetOuter());
	if (Blueprint && Blueprint->BlueprintType == BPTYPE_MacroLibrary)
	{
		UK2Node* K2Node = Cast<UK2Node>(Breakpoint->Node);
		if (K2Node)
		{
			return K2Node->IsA<UK2Node_MacroInstance>()
				|| (!K2Node->IsNodePure() && !K2Node->IsA<UK2Node_Tunnel>());
		}
	}

	TArray<uint8*> InstallSites;
	FKismetDebugUtilities::GetBreakpointInstallationSites(Breakpoint, InstallSites);
	return InstallSites.Num() > 0;
}

// Set the node that the breakpoint should focus on
void FKismetDebugUtilities::SetBreakpointLocation(UBreakpoint* Breakpoint, UEdGraphNode* NewNode)
{
	if (NewNode != Breakpoint->Node)
	{
		// Uninstall it from the old site if needed
		FKismetDebugUtilities::SetBreakpointInternal(Breakpoint, false);

		// Make the new site accurate
		Breakpoint->Node = NewNode;
		FKismetDebugUtilities::SetBreakpointInternal(Breakpoint, Breakpoint->bEnabled);
	}
}

// Set or clear the enabled flag for the breakpoint
void FKismetDebugUtilities::SetBreakpointEnabled(UBreakpoint* Breakpoint, bool bIsEnabled)
{
	if (Breakpoint->bStepOnce && !bIsEnabled)
	{
		// Want to be disabled, but the single-stepping is keeping it enabled
		bIsEnabled = true;
		Breakpoint->bStepOnce_WasPreviouslyDisabled = true;
	}

	Breakpoint->bEnabled = bIsEnabled;
	FKismetDebugUtilities::SetBreakpointInternal(Breakpoint, Breakpoint->bEnabled);
}

// Sets this breakpoint up as a single-step breakpoint (will disable or delete itself after one go if the breakpoint wasn't already enabled)
void FKismetDebugUtilities::SetBreakpointEnabledForSingleStep(UBreakpoint* Breakpoint, bool bDeleteAfterStep)
{
	Breakpoint->bStepOnce = true;
	Breakpoint->bStepOnce_RemoveAfterHit = bDeleteAfterStep;
	Breakpoint->bStepOnce_WasPreviouslyDisabled = !Breakpoint->bEnabled;

	FKismetDebugUtilities::SetBreakpointEnabled(Breakpoint, true);
}

void FKismetDebugUtilities::ReapplyBreakpoint(UBreakpoint* Breakpoint)
{
	FKismetDebugUtilities::SetBreakpointInternal(Breakpoint, Breakpoint->IsEnabled());
}

void FKismetDebugUtilities::StartDeletingBreakpoint(UBreakpoint* Breakpoint, UBlueprint* OwnerBlueprint)
{
#if WITH_EDITORONLY_DATA
	checkSlow(OwnerBlueprint->Breakpoints.Contains(Breakpoint));
	OwnerBlueprint->Breakpoints.Remove(Breakpoint);
	OwnerBlueprint->MarkPackageDirty();

	FKismetDebugUtilities::SetBreakpointLocation(Breakpoint, NULL);
#endif	//#if WITH_EDITORONLY_DATA
}

// Update the internal state of the breakpoint when it got hit
void FKismetDebugUtilities::UpdateBreakpointStateWhenHit(UBreakpoint* Breakpoint, UBlueprint* OwnerBlueprint)
{
	// Handle single-step breakpoints
	if (Breakpoint->bStepOnce)
	{
		Breakpoint->bStepOnce = false;

		if (Breakpoint->bStepOnce_RemoveAfterHit)
		{
			FKismetDebugUtilities::StartDeletingBreakpoint(Breakpoint, OwnerBlueprint);
		}
		else if (Breakpoint->bStepOnce_WasPreviouslyDisabled)
		{
			FKismetDebugUtilities::SetBreakpointEnabled(Breakpoint, false);
		}
	}
}

// Install/uninstall the breakpoint into/from the script code for the generated class that contains the node
void FKismetDebugUtilities::SetBreakpointInternal(UBreakpoint* Breakpoint, bool bShouldBeEnabled)
{
	TArray<uint8*> InstallSites;
	FKismetDebugUtilities::GetBreakpointInstallationSites(Breakpoint, InstallSites);

	for (int i = 0; i < InstallSites.Num(); ++i)
	{
		if (uint8* InstallSite = InstallSites[i])
		{
			*InstallSite = bShouldBeEnabled ? EX_Breakpoint : EX_Tracepoint;
		}
	}
}

// Returns the installation site(s); don't cache these pointers!
void FKismetDebugUtilities::GetBreakpointInstallationSites(UBreakpoint* Breakpoint, TArray<uint8*>& InstallSites)
{
	InstallSites.Empty();

#if WITH_EDITORONLY_DATA
	if (Breakpoint->Node != NULL)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Breakpoint->Node);

		if ((Blueprint != NULL) && (Blueprint->GeneratedClass != NULL))
		{
			if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(*Blueprint->GeneratedClass))
			{
				// Find the insertion point from the debugging data
				Class->GetDebugData().FindBreakpointInjectionSites(Breakpoint->Node, InstallSites);
			}
		}
	}
#endif	//#if WITH_EDITORONLY_DATA
}

// Returns the set of valid breakpoint locations for the given macro instance node
void FKismetDebugUtilities::GetValidBreakpointLocations(const UK2Node_MacroInstance* MacroInstanceNode, TArray<const UEdGraphNode*>& BreakpointLocations)
{
	check(MacroInstanceNode);
	BreakpointLocations.Empty();

	// Gather information from the macro graph associated with the macro instance node
	bool bIsMacroPure = false;
	UK2Node_Tunnel* MacroEntryNode = NULL;
	UK2Node_Tunnel* MacroResultNode = NULL;
	UEdGraph* InstanceNodeMacroGraph = MacroInstanceNode->GetMacroGraph();
	if (ensure(InstanceNodeMacroGraph != nullptr))
	{
		FKismetEditorUtilities::GetInformationOnMacro(InstanceNodeMacroGraph, MacroEntryNode, MacroResultNode, bIsMacroPure);
	}
	if (!bIsMacroPure && MacroEntryNode)
	{
		// Get the execute pin outputs on the entry node
		for (auto PinIt = MacroEntryNode->Pins.CreateConstIterator(); PinIt; ++PinIt)
		{
			const UEdGraphPin* ExecPin = *PinIt;
			if (ExecPin && ExecPin->Direction == EGPD_Output
				&& ExecPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				// For each pin linked to each execute pin, collect the node that owns it
				for (auto LinkedToPinIt = ExecPin->LinkedTo.CreateConstIterator(); LinkedToPinIt; ++LinkedToPinIt)
				{
					const UEdGraphPin* LinkedToPin = *LinkedToPinIt;
					check(LinkedToPin);

					const UEdGraphNode* LinkedToNode = LinkedToPin->GetOwningNode();
					check(LinkedToNode);

					if (LinkedToNode->IsA<UK2Node_MacroInstance>())
					{
						// Recursively descend into macro instance nodes encountered in a macro graph
						TArray<const UEdGraphNode*> SubLocations;
						GetValidBreakpointLocations(Cast<const UK2Node_MacroInstance>(LinkedToNode), SubLocations);
						BreakpointLocations.Append(SubLocations);
					}
					else
					{
						BreakpointLocations.AddUnique(LinkedToNode);
					}
				}
			}
		}
	}
}

// Finds a breakpoint for a given node if it exists, or returns NULL
UBreakpoint* FKismetDebugUtilities::FindBreakpointForNode(UBlueprint* Blueprint, const UEdGraphNode* Node, bool bCheckSubLocations)
{
	// iterate backwards so we can remove invalid breakpoints as we go
	for (int32 Index = Blueprint->Breakpoints.Num()-1; Index >= 0; --Index)
	{
		UBreakpoint* Breakpoint = Blueprint->Breakpoints[Index];
		if (Breakpoint == nullptr)
		{
			Blueprint->Breakpoints.RemoveAtSwap(Index);
			Blueprint->MarkPackageDirty();
			UE_LOG(LogBlueprintDebug, Warning, TEXT("Encountered an invalid blueprint breakpoint in %s (this should not happen... if you know how your blueprint got in this state, then please notify the Engine-Blueprints team)"), *Blueprint->GetPathName());
			continue;
		}

		const UEdGraphNode* BreakpointLocation = Breakpoint->GetLocation();
		if (BreakpointLocation == nullptr)
		{
			Blueprint->Breakpoints.RemoveAtSwap(Index);
			Blueprint->MarkPackageDirty();
			UE_LOG(LogBlueprintDebug, Display, TEXT("Encountered a blueprint breakpoint in %s without an associated node. The blueprint breakpoint has been removed"), *Blueprint->GetPathName());
			continue;
		}

		// Return this breakpoint if the location matches the given node
		if (BreakpointLocation == Node)
		{
			return Breakpoint;
		}
		else if (bCheckSubLocations)
		{
			// If this breakpoint is set on a macro instance node, check the set of valid breakpoint locations. If we find a
			// match in the returned set, return the breakpoint that's set on the macro instance node. This allows breakpoints
			// to be set and hit on macro instance nodes contained in a macro graph that will be expanded during compile.
			const UK2Node_MacroInstance* MacroInstanceNode = Cast<UK2Node_MacroInstance>(BreakpointLocation);
			if (MacroInstanceNode)
			{
				TArray<const UEdGraphNode*> ValidBreakpointLocations;
				GetValidBreakpointLocations(MacroInstanceNode, ValidBreakpointLocations);
				if (ValidBreakpointLocations.Contains(Node))
				{
					return Breakpoint;
				}
			}
		}
	}

	return NULL;
}

bool FKismetDebugUtilities::HasDebuggingData(const UBlueprint* Blueprint)
{
	return Cast<UBlueprintGeneratedClass>(*Blueprint->GeneratedClass)->GetDebugData().IsValid();
}

//////////////////////////////////////////////////////////////////////////
// Blueprint utils

// Looks thru the debugging data for any class variables associated with the node
UProperty* FKismetDebugUtilities::FindClassPropertyForPin(UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	UProperty* FoundProperty = nullptr;

	UClass* Class = Blueprint->GeneratedClass;
	while (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class))
	{
		FoundProperty = BlueprintClass->GetDebugData().FindClassPropertyForPin(Pin);
		if (FoundProperty != nullptr)
		{
			break;
		}

		Class = BlueprintClass->GetSuperClass();
	}

	return FoundProperty;
}

// Looks thru the debugging data for any class variables associated with the node (e.g., temporary variables or timelines)
UProperty* FKismetDebugUtilities::FindClassPropertyForNode(UBlueprint* Blueprint, const UEdGraphNode* Node)
{
	if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(*Blueprint->GeneratedClass))
	{
		return Class->GetDebugData().FindClassPropertyForNode(Node);
	}

	return NULL;
}


void FKismetDebugUtilities::ClearBreakpoints(UBlueprint* Blueprint)
{
	for (int32 BreakpointIndex = 0; BreakpointIndex < Blueprint->Breakpoints.Num(); ++BreakpointIndex)
	{
		UBreakpoint* Breakpoint = Blueprint->Breakpoints[BreakpointIndex];
		FKismetDebugUtilities::SetBreakpointLocation(Breakpoint, NULL);
	}

	Blueprint->Breakpoints.Empty();
	Blueprint->MarkPackageDirty();
}

FKismetDebugUtilities::FOnWatchedPinsListChanged FKismetDebugUtilities::WatchedPinsListChangedEvent;

bool FKismetDebugUtilities::CanWatchPin(const UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	//@TODO: This function belongs in the schema
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraph* Graph = Pin->GetOwningNode()->GetGraph();

	// Inputs should always be followed to their corresponding output in the world above
	const bool bNotAnInput = (Pin->Direction != EGPD_Input);

	//@TODO: Make watching a schema-allowable/denyable thing
	const bool bCanWatchThisGraph = true;

	return bCanWatchThisGraph && !K2Schema->IsMetaPin(*Pin) && bNotAnInput && !IsPinBeingWatched(Blueprint, Pin);
}

bool FKismetDebugUtilities::IsPinBeingWatched(const UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	return Blueprint->WatchedPins.Contains(const_cast<UEdGraphPin*>(Pin));
}

void FKismetDebugUtilities::RemovePinWatch(UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	UEdGraphPin* NonConstPin = const_cast<UEdGraphPin*>(Pin);
	Blueprint->WatchedPins.Remove(NonConstPin);
	Blueprint->MarkPackageDirty();
	Blueprint->PostEditChange();
	WatchedPinsListChangedEvent.Broadcast(Blueprint);
}

void FKismetDebugUtilities::TogglePinWatch(UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	int32 ExistingWatchIndex = Blueprint->WatchedPins.Find(const_cast<UEdGraphPin*>(Pin));

	if (ExistingWatchIndex != INDEX_NONE)
	{
		FKismetDebugUtilities::RemovePinWatch(Blueprint, Pin);
	}
	else
	{
		Blueprint->WatchedPins.Add(const_cast<UEdGraphPin*>(Pin));
		Blueprint->MarkPackageDirty();
		Blueprint->PostEditChange();
	}

	WatchedPinsListChangedEvent.Broadcast(Blueprint);
}

void FKismetDebugUtilities::ClearPinWatches(UBlueprint* Blueprint)
{
	Blueprint->WatchedPins.Empty();
	Blueprint->MarkPackageDirty();
	Blueprint->PostEditChange();

	WatchedPinsListChangedEvent.Broadcast(Blueprint);
}

// Gets the watched tooltip for a specified site
FKismetDebugUtilities::EWatchTextResult FKismetDebugUtilities::GetWatchText(FString& OutWatchText, UBlueprint* Blueprint, UObject* ActiveObject, const UEdGraphPin* WatchPin)
{
	UProperty* PropertyToDebug = nullptr;
	void* DataPtr = nullptr;
	void* DeltaPtr = nullptr;
	UObject* ParentObj = nullptr;
	TArray<UObject*> SeenObjects;
	FKismetDebugUtilities::EWatchTextResult Result = FindDebuggingData(Blueprint, ActiveObject, WatchPin, PropertyToDebug, DataPtr, DeltaPtr, ParentObj, SeenObjects);

	if (Result == FKismetDebugUtilities::EWatchTextResult::EWTR_Valid)
	{
		PropertyToDebug->ExportText_InContainer(/*ArrayElement=*/ 0, /*inout*/ OutWatchText, DataPtr, DeltaPtr, /*Parent=*/ ParentObj, PPF_PropertyWindow | PPF_BlueprintDebugView);
	}

	return Result;
}

FKismetDebugUtilities::EWatchTextResult FKismetDebugUtilities::GetDebugInfo(FDebugInfo& OutDebugInfo, UBlueprint* Blueprint, UObject* ActiveObject, const UEdGraphPin* WatchPin)
{
	UProperty* PropertyToDebug = nullptr;
	void* DataPtr = nullptr;
	void* DeltaPtr = nullptr;
	UObject* ParentObj = nullptr;
	TArray<UObject*> SeenObjects;
	FKismetDebugUtilities::EWatchTextResult Result = FindDebuggingData(Blueprint, ActiveObject, WatchPin, PropertyToDebug, DataPtr, DeltaPtr, ParentObj, SeenObjects);

	if (Result == FKismetDebugUtilities::EWatchTextResult::EWTR_Valid)
	{
		GetDebugInfo_InContainer(0, OutDebugInfo, PropertyToDebug, DataPtr);
	}

	return Result;
}

FKismetDebugUtilities::EWatchTextResult FKismetDebugUtilities::FindDebuggingData(UBlueprint* Blueprint, UObject* ActiveObject, const UEdGraphPin* WatchPin, UProperty*& OutProperty, void*& OutData, void*& OutDelta, UObject*& OutParent, TArray<UObject*>& SeenObjects)
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	if (UProperty* Property = FKismetDebugUtilities::FindClassPropertyForPin(Blueprint, WatchPin))
	{
		if (!Property->IsValidLowLevel())
		{
			//@TODO: Temporary checks to attempt to determine intermittent unreproducable crashes in this function
			static bool bErrorOnce = true;
			if (bErrorOnce)
			{
				ensureMsgf(false, TEXT("Error: Invalid (but non-null) property associated with pin; cannot get variable value"));
				bErrorOnce = false;
			}
			return EWTR_NoProperty;
		}

		if (ActiveObject != nullptr)
		{
			if (!ActiveObject->IsValidLowLevel())
			{
				//@TODO: Temporary checks to attempt to determine intermittent unreproducable crashes in this function
				static bool bErrorOnce = true;
				if (bErrorOnce)
				{
					ensureMsgf(false, TEXT("Error: Invalid (but non-null) active object being debugged; cannot get variable value for property %s"), *Property->GetPathName());
					bErrorOnce = false;
				}
				return EWTR_NoDebugObject;
			}

			void* PropertyBase = nullptr;

			// Walk up the stack frame to see if we can find a function scope that contains the property as a local
			for (const FFrame* TestFrame = Data.StackFrameAtIntraframeDebugging; TestFrame != NULL; TestFrame = TestFrame->PreviousFrame)
			{
				if (Property->IsIn(TestFrame->Node))
				{
					PropertyBase = TestFrame->Locals;
					break;
				}
			}

			// Try at member scope if it wasn't part of a current function scope
			UClass* PropertyClass = Cast<UClass>(Property->GetOuter());
			if (!PropertyBase && PropertyClass)
			{
				if (ActiveObject->GetClass()->IsChildOf(PropertyClass))
				{
					PropertyBase = ActiveObject;
				}
				else if (AActor* Actor = Cast<AActor>(ActiveObject))
				{
					// Try and locate the propertybase in the actor components
					for (auto ComponentIter : Actor->GetComponents())
					{
						if (ComponentIter->GetClass()->IsChildOf(PropertyClass))
						{
							PropertyBase = ComponentIter;
							break;
						}
					}
				}
			}
#if USE_UBER_GRAPH_PERSISTENT_FRAME
			// Try find the propertybase in the persistent ubergraph frame
			UFunction* OuterFunction = Cast<UFunction>(Property->GetOuter());
			if (!PropertyBase && OuterFunction)
			{
				UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
				if (BPGC && ActiveObject->IsA(BPGC))
				{
					PropertyBase = BPGC->GetPersistentUberGraphFrame(ActiveObject, OuterFunction);
				}
			}
#endif // USE_UBER_GRAPH_PERSISTENT_FRAME

			// see if our WatchPin is on a animation node & if so try to get its property info
			UAnimBlueprintGeneratedClass* AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(Blueprint->GeneratedClass);
			if (!PropertyBase && AnimBlueprintGeneratedClass)
			{
				// are we linked to an anim graph node?
				UProperty* LinkedProperty = Property;
				const UAnimGraphNode_Base* Node = Cast<UAnimGraphNode_Base>(WatchPin->GetOuter());
				if (Node == nullptr && WatchPin->LinkedTo.Num() > 0)
				{
					const UEdGraphPin* LinkedPin = WatchPin->LinkedTo[0];
					// When we change Node we *must* change Property, so it's still a sub-element of that.
					LinkedProperty = FKismetDebugUtilities::FindClassPropertyForPin(Blueprint, LinkedPin);
					Node = Cast<UAnimGraphNode_Base>(LinkedPin->GetOuter());
				}

				if (Node && LinkedProperty)
				{
					UStructProperty* NodeStructProperty = Cast<UStructProperty>(FKismetDebugUtilities::FindClassPropertyForNode(Blueprint, Node));
					if (NodeStructProperty)
					{
						for (UStructProperty* NodeProperty : AnimBlueprintGeneratedClass->AnimNodeProperties)
						{
							if (NodeProperty == NodeStructProperty)
							{
								void* NodePtr = NodeProperty->ContainerPtrToValuePtr<void>(ActiveObject);
								OutProperty = LinkedProperty;
								OutData = NodePtr;
								OutDelta = NodePtr;
								OutParent = ActiveObject;
								return EWTR_Valid;
							}
						}
					}
				}
			}

			// If we still haven't found a result, try changing the active object to whatever is passed into the self pin.
			if (!PropertyBase)
			{
				UEdGraphNode* WatchNode = WatchPin->GetOwningNode();

				if (WatchNode)
				{
					UEdGraphPin* SelfPin = WatchNode->FindPin(TEXT("self"));
					if (SelfPin && SelfPin != WatchPin)
					{
						UProperty* SelfPinProperty = nullptr;
						void* SelfPinData = nullptr;
						void* SelfPinDelta = nullptr;
						UObject* SelfPinParent = nullptr;
						SeenObjects.AddUnique(ActiveObject);
						FKismetDebugUtilities::EWatchTextResult Result = FindDebuggingData(Blueprint, ActiveObject, SelfPin, SelfPinProperty, SelfPinData, SelfPinDelta, SelfPinParent, SeenObjects);
						UObjectPropertyBase* SelfPinPropertyBase = Cast<UObjectPropertyBase>(SelfPinProperty);
						if (Result == EWTR_Valid && SelfPinPropertyBase != nullptr)
						{
							void* PropertyValue = SelfPinProperty->ContainerPtrToValuePtr<void>(SelfPinData);
							UObject* TempActiveObject = SelfPinPropertyBase->GetObjectPropertyValue(PropertyValue);
							if (TempActiveObject && TempActiveObject != ActiveObject)
							{
								if (!SeenObjects.Contains(TempActiveObject))
								{
									return FindDebuggingData(Blueprint, TempActiveObject, WatchPin, OutProperty, OutData, OutDelta, OutParent, SeenObjects);
								}
							}
						}
					}
				}
			}

			// Now either print out the variable value, or that it was out-of-scope
			if (PropertyBase != nullptr)
			{
				OutProperty = Property;
				OutData = PropertyBase;
				OutDelta = PropertyBase;
				OutParent = ActiveObject;
				return EWTR_Valid;
			}
			else
			{
				return EWTR_NotInScope;
			}
		}
		else
		{
			return EWTR_NoDebugObject;
		}
	}
	else
	{
		return EWTR_NoProperty;
	}
}

void FKismetDebugUtilities::GetDebugInfo_InContainer(int32 Index, FDebugInfo& DebugInfo, UProperty* Property, const void* Data)
{
	GetDebugInfoInternal(DebugInfo, Property, Property->ContainerPtrToValuePtr<void>(Data, Index));
}

void FKismetDebugUtilities::GetDebugInfoInternal(FDebugInfo& DebugInfo, UProperty* Property, const void* PropertyValue)
{
	if (Property == nullptr)
	{
		return;
	}

	DebugInfo.Type = UEdGraphSchema_K2::TypeToText(Property);
	DebugInfo.DisplayName = Property->GetDisplayNameText();

	UByteProperty* ByteProperty = Cast<UByteProperty>(Property);
	if (ByteProperty)
	{
		UEnum* Enum = ByteProperty->GetIntPropertyEnum();
		if (Enum)
		{
			if (Enum->IsValidEnumValue(*(const uint8*)PropertyValue))
			{
				DebugInfo.Value = Enum->GetDisplayNameTextByValue(*(const uint8*)PropertyValue);
			}
			else
			{
				DebugInfo.Value = FText::FromString(TEXT("(INVALID)"));
			}

			return;
		}

		// if there is no Enum we need to fall through and treat this as a UNumericProperty
	}

	UNumericProperty* NumericProperty = Cast<UNumericProperty>(Property);
	if (NumericProperty)
	{
		DebugInfo.Value = FText::FromString(NumericProperty->GetNumericPropertyValueToString(PropertyValue));
		return;
	}

	UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property);
	if (BoolProperty)
	{
		DebugInfo.Value = BoolProperty->GetPropertyValue(PropertyValue) ? GTrue : GFalse;
		return;
	}

	UNameProperty* NameProperty = Cast<UNameProperty>(Property);
	if (NameProperty)
	{
		DebugInfo.Value = FText::FromName(*(FName*)PropertyValue);
		return;
	}

	UTextProperty* TextProperty = Cast<UTextProperty>(Property);
	if (TextProperty)
	{
		DebugInfo.Value = TextProperty->GetPropertyValue(PropertyValue);
		return;
	}

	UStrProperty* StringProperty = Cast<UStrProperty>(Property);
	if (StringProperty)
	{
		DebugInfo.Value = FText::FromString(StringProperty->GetPropertyValue(PropertyValue));
		return;
	}

	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
	if (ArrayProperty)
	{
		checkSlow(ArrayProperty->Inner);

		FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyValue);

		DebugInfo.Value = FText::Format(LOCTEXT("ArraySize", "Num={0}"), FText::AsNumber(ArrayHelper.Num()));

		for (int32 i = 0; i < ArrayHelper.Num(); i++)
		{
			FDebugInfo ArrayDebugInfo;

			uint8* PropData = ArrayHelper.GetRawPtr(i);
			GetDebugInfoInternal(ArrayDebugInfo, ArrayProperty->Inner, PropData);
			// overwrite the display name with the array index for the current element
			ArrayDebugInfo.DisplayName = FText::Format(LOCTEXT("ArrayIndexName", "[{0}]"), FText::AsNumber(i));
			DebugInfo.Children.Add(ArrayDebugInfo);
		}

		return;
	}

	UStructProperty* StructProperty = Cast<UStructProperty>(Property);
	if (StructProperty)
	{
		FString WatchText;
		StructProperty->ExportTextItem(WatchText, PropertyValue, PropertyValue, nullptr, PPF_PropertyWindow | PPF_BlueprintDebugView, nullptr);
		DebugInfo.Value = FText::FromString(WatchText);

		for (TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It)
		{
			FDebugInfo StructDebugInfo;
			GetDebugInfoInternal(StructDebugInfo, *It, It->ContainerPtrToValuePtr<void>(PropertyValue, 0));

			DebugInfo.Children.Add(StructDebugInfo);
		}

		return;
	}

	UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property);
	if (EnumProperty)
	{
		UNumericProperty* LocalUnderlyingProp = EnumProperty->GetUnderlyingProperty();
		UEnum* Enum = EnumProperty->GetEnum();

		int64 Value = LocalUnderlyingProp->GetSignedIntPropertyValue(PropertyValue);

		// if the value is the max value (the autogenerated *_MAX value), export as "INVALID", unless we're exporting text for copy/paste (for copy/paste,
		// the property text value must actually match an entry in the enum's names array)
		if (Enum)
		{
			if (Enum->IsValidEnumValue(Value))
			{
				DebugInfo.Value = Enum->GetDisplayNameTextByValue(Value);
			}
			else
			{
				DebugInfo.Value = LOCTEXT("Invalid", "(INVALID)");
			}
		}
		else
		{
			DebugInfo.Value = FText::AsNumber(Value);
		}

		return;
	}

	UMapProperty* MapProperty = Cast<UMapProperty>(Property);
	if (MapProperty)
	{
		FScriptMapHelper MapHelper(MapProperty, PropertyValue);
		DebugInfo.Value = FText::Format(LOCTEXT("MapSize", "Num={0}"), FText::AsNumber(MapHelper.Num()));
		uint8* PropData = MapHelper.GetPairPtr(0);

		int32 Index = 0;
		for (int32 Count = MapHelper.Num(); Count; PropData += MapProperty->MapLayout.SetLayout.Size, ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				FDebugInfo ChildInfo;

				GetDebugInfoInternal(ChildInfo, MapProperty->ValueProp, PropData + MapProperty->MapLayout.ValueOffset);

				// use the info from the ValueProp and then overwrite the name with the KeyProp data
				FString NameStr = TEXT("[");
				MapProperty->KeyProp->ExportTextItem(NameStr, PropData, nullptr, nullptr, PPF_PropertyWindow | PPF_BlueprintDebugView | PPF_Delimited, nullptr);
				NameStr += TEXT("] ");

				ChildInfo.DisplayName = FText::FromString(NameStr);

				DebugInfo.Children.Add(ChildInfo);

				--Count;
			}
		}

		return;
	}

	USetProperty* SetProperty = Cast<USetProperty>(Property);
	if (SetProperty)
	{
		FScriptSetHelper SetHelper(SetProperty, PropertyValue);
		DebugInfo.Value = FText::Format(LOCTEXT("SetSize", "Num={0}"), FText::AsNumber(SetHelper.Num()));
		uint8* PropData = SetHelper.GetElementPtr(0);

		int32 Index = 0;
		for (int32 Count = SetHelper.Num(); Count; PropData += SetProperty->SetLayout.Size, ++Index)
		{
			if (SetHelper.IsValidIndex(Index))
			{
				FDebugInfo ChildInfo;
				GetDebugInfoInternal(ChildInfo, SetProperty->ElementProp, PropData);

				// members of sets don't have their own names
				ChildInfo.DisplayName = FText::GetEmpty();

				DebugInfo.Children.Add(ChildInfo);

				--Count;
			}
		}

		return;
	}

	UObjectPropertyBase* ObjectPropertyBase = Cast<UObjectPropertyBase>(Property);
	if (ObjectPropertyBase)
	{
		UObject* Obj = ObjectPropertyBase->GetObjectPropertyValue(PropertyValue);
		if (Obj != nullptr)
		{
			DebugInfo.Value = FText::FromString(Obj->GetFullName());
		}
		else
		{
			DebugInfo.Value = FText::FromString(TEXT("None"));
		}

		return;
	}

	UDelegateProperty* DelegateProperty = Cast<UDelegateProperty>(Property);
	if (DelegateProperty)
	{
		if (DelegateProperty->SignatureFunction)
		{
			DebugInfo.Value = DelegateProperty->SignatureFunction->GetDisplayNameText();
		}
		else
		{
			DebugInfo.Value = LOCTEXT("NoFunc", "(No bound function)");
		}

		return;
	}

	UMulticastDelegateProperty* MulticastDelegateProperty = Cast<UMulticastDelegateProperty>(Property);
	if (MulticastDelegateProperty)
	{
		if (MulticastDelegateProperty->SignatureFunction)
		{
			DebugInfo.Value = MulticastDelegateProperty->SignatureFunction->GetDisplayNameText();
		}
		else
		{
			DebugInfo.Value = LOCTEXT("NoFunc", "(No bound function)");
		}

		return;
	}

	ensure(false);
}

FText FKismetDebugUtilities::GetAndClearLastExceptionMessage()
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();
	const FText Result = Data.LastExceptionMessage;
	Data.LastExceptionMessage = FText();
	return Result;
}

#undef LOCTEXT_NAMESPACE
