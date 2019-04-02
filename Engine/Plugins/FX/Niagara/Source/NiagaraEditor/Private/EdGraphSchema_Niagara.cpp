// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorModule.h"
#include "INiagaraEditorTypeUtilities.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorCommon.h"
#include "INiagaraCompiler.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraComponent.h"
#include "ScopedTransaction.h"
#include "NiagaraGraph.h"
#include "GraphEditorSettings.h"
#include "GraphEditorActions.h"
#include "NiagaraConstants.h"

#include "NiagaraScript.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeReadDataSet.h"
#include "NiagaraNodeWriteDataSet.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeOp.h"
#include "NiagaraNodeConvert.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraDataInterface.h"
#include "NiagaraNodeIf.h"
#include "Misc/MessageDialog.h"
#include "NiagaraScriptSource.h"
#include "NiagaraEmitter.h"
#include "NiagaraNodeEmitter.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraNodeReroute.h"
#include "NiagaraNodeUsageSelector.h"
#include "EdGraphNode_Comment.h"

#include "Modules/ModuleManager.h"
#include "AssetRegistryModule.h"
#include "NiagaraNodeSimTargetSelector.h"

#define LOCTEXT_NAMESPACE "NiagaraSchema"

#define SNAP_GRID (16) // @todo ensure this is the same as SNodePanel::GetSnapGridSize()

const FLinearColor UEdGraphSchema_Niagara::NodeTitleColor_Attribute = FLinearColor::Green;
const FLinearColor UEdGraphSchema_Niagara::NodeTitleColor_Constant = FLinearColor::Red;
const FLinearColor UEdGraphSchema_Niagara::NodeTitleColor_SystemConstant = FLinearColor::White;
const FLinearColor UEdGraphSchema_Niagara::NodeTitleColor_FunctionCall = FLinearColor::Blue;
const FLinearColor UEdGraphSchema_Niagara::NodeTitleColor_CustomHlsl = FLinearColor::Yellow;
const FLinearColor UEdGraphSchema_Niagara::NodeTitleColor_Event = FLinearColor::Red;
const FLinearColor UEdGraphSchema_Niagara::NodeTitleColor_TranslatorConstant = FLinearColor::Gray;
const FLinearColor UEdGraphSchema_Niagara::NodeTitleColor_RapidIteration = FLinearColor::Black;

const FName UEdGraphSchema_Niagara::PinCategoryType("Type");
const FName UEdGraphSchema_Niagara::PinCategoryMisc("Misc");
const FName UEdGraphSchema_Niagara::PinCategoryClass("Class");
const FName UEdGraphSchema_Niagara::PinCategoryEnum("Enum");

namespace 
{
	// Maximum distance a drag can be off a node edge to require 'push off' from node
	const int32 NodeDistance = 60;
}

UEdGraphNode* FNiagaraSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
		FString OutErrorMsg;
		UNiagaraNode* NiagaraNodeTemplate = Cast<UNiagaraNode>(NodeTemplate);
		if (NiagaraNodeTemplate && !NiagaraNodeTemplate->CanAddToGraph(CastChecked<UNiagaraGraph>(ParentGraph), OutErrorMsg))
		{
			if (OutErrorMsg.Len() > 0)
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(OutErrorMsg));
			}
			return ResultNode;
		}

		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorNewNode", "Niagara Editor: New Node"));
		ParentGraph->Modify();

		NodeTemplate->SetFlags(RF_Transactional);

		// set outer to be the graph so it doesn't go away
		NodeTemplate->Rename(NULL, ParentGraph, REN_NonTransactional);
		ParentGraph->AddNode(NodeTemplate, true, bSelectNewNode);

		NodeTemplate->CreateNewGuid();
		NodeTemplate->PostPlacedNewNode();
		NodeTemplate->AllocateDefaultPins();
		NodeTemplate->AutowireNewNode(FromPin);

		// For input pins, new node will generally overlap node being dragged off
		// Work out if we want to visually push away from connected node
		int32 XLocation = Location.X;
		if (FromPin && FromPin->Direction == EGPD_Input)
		{
			UEdGraphNode* PinNode = FromPin->GetOwningNode();
			const float XDelta = FMath::Abs(PinNode->NodePosX - Location.X);

			if (XDelta < NodeDistance)
			{
				// Set location to edge of current node minus the max move distance
				// to force node to push off from connect node enough to give selection handle
				XLocation = PinNode->NodePosX - NodeDistance;
			}
		}

		NodeTemplate->NodePosX = XLocation;
		NodeTemplate->NodePosY = Location.Y;
		NodeTemplate->SnapToGrid(SNAP_GRID);

		ResultNode = NodeTemplate;

		//ParentGraph->NotifyGraphChanged();
	}

	return ResultNode;
}

UEdGraphNode* FNiagaraSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode/* = true*/) 
{
	UEdGraphNode* ResultNode = NULL;

	if (FromPins.Num() > 0)
	{
		ResultNode = PerformAction(ParentGraph, FromPins[0], Location, bSelectNewNode);

		if (ResultNode)
		{
			// Try autowiring the rest of the pins
			for (int32 Index = 1; Index < FromPins.Num(); ++Index)
			{
				ResultNode->AutowireNewNode(FromPins[Index]);
			}
		}
	}
	else
	{
		ResultNode = PerformAction(ParentGraph, NULL, Location, bSelectNewNode);
	}

	return ResultNode;
}

void FNiagaraSchemaAction_NewNode::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction::AddReferencedObjects( Collector );

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject( NodeTemplate );
}

//////////////////////////////////////////////////////////////////////////

static int32 GbAllowAllNiagaraNodesInEmitterGraphs = 1;
static FAutoConsoleVariableRef CVarAllowAllNiagaraNodesInEmitterGraphs(
	TEXT("niagara.AllowAllNiagaraNodesInEmitterGraphs"),
	GbAllowAllNiagaraNodesInEmitterGraphs,
	TEXT("If true, all nodes will be allowed in the Niagara emitter graphs. \n"),
	ECVF_Default
);

UEdGraphSchema_Niagara::UEdGraphSchema_Niagara(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<FNiagaraSchemaAction_NewNode> AddNewNodeAction(TArray<TSharedPtr<FNiagaraSchemaAction_NewNode> >& NewActions, const FText& Category, const FText& MenuDesc, const FName& InternalName, const FText& Tooltip, FText Keywords = FText())
{
	TSharedPtr<FNiagaraSchemaAction_NewNode> NewAction = TSharedPtr<FNiagaraSchemaAction_NewNode>(new FNiagaraSchemaAction_NewNode(Category, MenuDesc, InternalName, Tooltip, 0, Keywords));
	NewActions.Add(NewAction);
	return NewAction;
}



bool IsSystemGraph(const UNiagaraGraph* NiagaraGraph)
{
	TArray<UNiagaraNodeEmitter*> Emitters;
	NiagaraGraph->GetNodesOfClass<UNiagaraNodeEmitter>(Emitters);
	bool bSystemGraph = Emitters.Num() != 0 || NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::SystemSpawnScript) != nullptr || NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::SystemUpdateScript) != nullptr;
	return bSystemGraph;
}

bool IsParticleGraph(const UNiagaraGraph* NiagaraGraph)
{
	bool bParticleGraph = NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::ParticleSpawnScriptInterpolated) != nullptr || NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::ParticleSpawnScript) != nullptr || NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::ParticleUpdateScript) != nullptr;
	return bParticleGraph;
}

bool IsModuleGraph(const UNiagaraGraph* NiagaraGraph)
{
	bool bModuleGraph = NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::Module) != nullptr;
	return bModuleGraph;
}

bool IsDynamicInputGraph(const UNiagaraGraph* NiagaraGraph)
{
	bool bDynamicInputGraph = NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::DynamicInput) != nullptr;
	return bDynamicInputGraph;
}


bool IsFunctionGraph(const UNiagaraGraph* NiagaraGraph)
{
	bool bFunctionGraph = NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::Function) != nullptr;
	return bFunctionGraph;
}


bool IsParticleUpdateGraph(const UNiagaraGraph* NiagaraGraph)
{
	bool bUpdateGraph = NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::ParticleUpdateScript) != nullptr;
	return bUpdateGraph;
}


const UNiagaraGraph* GetAlternateGraph(const UNiagaraGraph* NiagaraGraph)
{
	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(NiagaraGraph->GetOuter());
	if (ScriptSource != nullptr)
	{
		UNiagaraScript* Script = Cast<UNiagaraScript>(ScriptSource->GetOuter());
		if (Script != nullptr)
		{
			UNiagaraEmitter* EmitterProperties = Cast<UNiagaraEmitter>(Script->GetOuter());
			if (EmitterProperties != nullptr)
			{
				if (EmitterProperties->SpawnScriptProps.Script == Script)
				{
					return CastChecked<UNiagaraScriptSource>(EmitterProperties->UpdateScriptProps.Script->GetSource())->NodeGraph;
				}
				else if (EmitterProperties->UpdateScriptProps.Script == Script)
				{
					return CastChecked<UNiagaraScriptSource>(EmitterProperties->SpawnScriptProps.Script->GetSource())->NodeGraph;
				}
			}
		}
	}
	return nullptr;
}

FText GetGraphTypeTitle(const UNiagaraGraph* NiagaraGraph)
{
	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(NiagaraGraph->GetOuter());
	if (ScriptSource != nullptr)
	{
		UNiagaraScript* Script = Cast<UNiagaraScript>(ScriptSource->GetOuter());
		if (Script != nullptr)
		{
			if (Script->IsParticleSpawnScript())
			{
				return LOCTEXT("Parameter Menu Title Spawn", "Spawn Parameters");
			}
			else if (Script->IsParticleUpdateScript())
			{
				return LOCTEXT("Parameter Menu Title Update", "Update Parameters");
			}
		}
	}
	return LOCTEXT("Parameter Menu Title Generic", "Script Parameters");
}

void AddParametersForGraph(TArray<TSharedPtr<FNiagaraSchemaAction_NewNode> >& NewActions, const UNiagaraGraph* CurrentGraph,  UEdGraph* OwnerOfTemporaries, const UNiagaraGraph* NiagaraGraph)
{
	FText GraphParameterCategory = GetGraphTypeTitle(NiagaraGraph);
	TArray<UNiagaraNodeInput*> InputNodes;
	NiagaraGraph->GetNodesOfClass(InputNodes);

	TArray<FNiagaraVariable> SeenParams;
	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter && !SeenParams.Contains(InputNode->Input))
		{
			SeenParams.Add(InputNode->Input);
			FName Name = InputNode->Input.GetName();
			FText MenuDesc = FText::FromName(Name);
			if (NiagaraGraph != CurrentGraph)
			{
				Name = UNiagaraNodeInput::GenerateUniqueName(CastChecked<UNiagaraGraph>(CurrentGraph), Name, InputNode->Usage);
				MenuDesc = FText::Format(LOCTEXT("Parameter Menu Copy Param","Copy \"{0}\" to this Graph"), FText::FromName(Name));
			}

			TSharedPtr<FNiagaraSchemaAction_NewNode> ExistingInputAction = AddNewNodeAction(NewActions, GraphParameterCategory, MenuDesc, Name, FText::GetEmpty());

			UNiagaraNodeInput* InputNodeTemplate = NewObject<UNiagaraNodeInput>(OwnerOfTemporaries);
			InputNodeTemplate->Input = InputNode->Input;
			InputNodeTemplate->Usage = InputNode->Usage;
			InputNodeTemplate->ExposureOptions = InputNode->ExposureOptions;
			InputNodeTemplate->SetDataInterface(nullptr);

			// We also support parameters from an alternate graph. If that was used, then we need to take special care
			// to make the parameter unique to that graph.
			if (NiagaraGraph != CurrentGraph)
			{
				InputNodeTemplate->Input.SetName(Name);

				if (InputNode->GetDataInterface())
				{
					InputNodeTemplate->SetDataInterface(Cast<UNiagaraDataInterface>(StaticDuplicateObject(InputNode->GetDataInterface(), InputNodeTemplate, NAME_None, ~RF_Transient)));
				}
			}

			ExistingInputAction->NodeTemplate = InputNodeTemplate;
		}
	}
}

void AddParameterMenuOptions(TArray<TSharedPtr<FNiagaraSchemaAction_NewNode> >& NewActions, const UNiagaraGraph* CurrentGraph, UEdGraph* OwnerOfTemporaries, const UNiagaraGraph* NiagaraGraph)
{
	AddParametersForGraph(NewActions, CurrentGraph, OwnerOfTemporaries, NiagaraGraph);

	const UNiagaraGraph* AltGraph = GetAlternateGraph(NiagaraGraph);
	if (AltGraph != nullptr)
	{
		AddParametersForGraph(NewActions, CurrentGraph, OwnerOfTemporaries, AltGraph);
	}
}

void UEdGraphSchema_Niagara::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const UNiagaraGraph* NiagaraGraph = CastChecked<UNiagaraGraph>(ContextMenuBuilder.CurrentGraph);
	TArray<TSharedPtr<FNiagaraSchemaAction_NewNode> > NewActions = GetGraphContextActions(NiagaraGraph, ContextMenuBuilder.SelectedObjects, ContextMenuBuilder.FromPin, ContextMenuBuilder.OwnerOfTemporaries);
	for (int32 i = 0; i < NewActions.Num(); i++)
	{
		ContextMenuBuilder.AddAction(NewActions[i]);
	}
}

TArray<TSharedPtr<FNiagaraSchemaAction_NewNode> > UEdGraphSchema_Niagara::GetGraphContextActions(const UEdGraph* CurrentGraph, TArray<UObject*>& SelectedObjects, const UEdGraphPin* FromPin, UEdGraph* OwnerOfTemporaries) const
{
	TArray<TSharedPtr<FNiagaraSchemaAction_NewNode> > NewActions;

	const UNiagaraGraph* NiagaraGraph = CastChecked<UNiagaraGraph>(CurrentGraph);
	
	bool bSystemGraph = IsSystemGraph(NiagaraGraph);
	bool bModuleGraph = IsModuleGraph(NiagaraGraph);
	bool bDynamicInputGraph = IsDynamicInputGraph(NiagaraGraph);
	bool bFunctionGraph = IsFunctionGraph(NiagaraGraph);
	bool bParticleUpdateGraph = IsParticleUpdateGraph(NiagaraGraph);
	
	if (GbAllowAllNiagaraNodesInEmitterGraphs || bModuleGraph || bFunctionGraph || bSystemGraph)
	{
		const TArray<FNiagaraOpInfo>& OpInfos = FNiagaraOpInfo::GetOpInfoArray();
		for (const FNiagaraOpInfo& OpInfo : OpInfos)
		{
			TSharedPtr<FNiagaraSchemaAction_NewNode> AddOpAction = AddNewNodeAction(NewActions, OpInfo.Category, OpInfo.FriendlyName, OpInfo.Name, OpInfo.Description, OpInfo.Keywords);
			UNiagaraNodeOp* OpNode = NewObject<UNiagaraNodeOp>(OwnerOfTemporaries);
			OpNode->OpName = OpInfo.Name;
			AddOpAction->NodeTemplate = OpNode;
		}
	}

	// Add custom code
	{
		const FText MenuDesc = LOCTEXT("CustomHLSLNode","Custom Hlsl");
		const FText TooltipDesc = LOCTEXT("CustomHlslPopupTooltip", "Add a node with custom hlsl content");
		TSharedPtr<FNiagaraSchemaAction_NewNode> FunctionCallAction = AddNewNodeAction(NewActions, LOCTEXT("Function Menu Title", "Functions"), MenuDesc, TEXT("CustomHLSL"), TooltipDesc);
		UNiagaraNodeCustomHlsl* CustomHlslNode = NewObject<UNiagaraNodeCustomHlsl>(OwnerOfTemporaries);
		CustomHlslNode->CustomHlsl = TEXT("// Insert the body of the function here and add any inputs\r\n// and outputs by name using the add pins above.\r\n// Currently, complicated branches, for loops, switches, etc are not advised.");
		FunctionCallAction->NodeTemplate = CustomHlslNode;
	}

	auto AddScriptFunctionAction = [&NewActions, OwnerOfTemporaries](const FText& Category, const FAssetData& ScriptAsset)
	{
		FText AssetDesc;
		ScriptAsset.GetTagValue(GET_MEMBER_NAME_CHECKED(UNiagaraScript, Description), AssetDesc);

		FText Keywords;
		ScriptAsset.GetTagValue(GET_MEMBER_NAME_CHECKED(UNiagaraScript, Keywords), Keywords);

		FString DisplayNameString = FName::NameToDisplayString(ScriptAsset.AssetName.ToString(), false);

		const FText MenuDesc = FText::FromString(DisplayNameString);
		const FText TooltipDesc = FNiagaraEditorUtilities::FormatScriptAssetDescription(AssetDesc, ScriptAsset.ObjectPath);

		TSharedPtr<FNiagaraSchemaAction_NewNode> FunctionCallAction = AddNewNodeAction(NewActions, Category, MenuDesc, *DisplayNameString, TooltipDesc, Keywords);

		UNiagaraNodeFunctionCall* FunctionCallNode = NewObject<UNiagaraNodeFunctionCall>(OwnerOfTemporaries);
		FunctionCallNode->FunctionScriptAssetObjectPath = ScriptAsset.ObjectPath;
		FunctionCallAction->NodeTemplate = FunctionCallNode;
	};

	//Add functions
	if (GbAllowAllNiagaraNodesInEmitterGraphs || bModuleGraph || bFunctionGraph || bDynamicInputGraph)
	{
		TArray<FAssetData> FunctionScriptAssets;
		FNiagaraEditorUtilities::FGetFilteredScriptAssetsOptions FunctionScriptFilterOptions;
		FunctionScriptFilterOptions.ScriptUsageToInclude = ENiagaraScriptUsage::Function;
		FNiagaraEditorUtilities::GetFilteredScriptAssets(FunctionScriptFilterOptions, FunctionScriptAssets);

		for (const FAssetData& FunctionScriptAsset : FunctionScriptAssets)
		{
			AddScriptFunctionAction(LOCTEXT("Function Menu Title", "Functions"), FunctionScriptAsset);
		}
	}

	//Add modules
	if (!bFunctionGraph)
	{
		TArray<FAssetData> ModuleScriptAssets;
		FNiagaraEditorUtilities::FGetFilteredScriptAssetsOptions ModuleScriptFilterOptions;
		ModuleScriptFilterOptions.ScriptUsageToInclude = ENiagaraScriptUsage::Module;
		FNiagaraEditorUtilities::GetFilteredScriptAssets(ModuleScriptFilterOptions, ModuleScriptAssets);

		for (const FAssetData& ModuleScriptAsset : ModuleScriptAssets)
		{
			AddScriptFunctionAction(LOCTEXT("Module Menu Title", "Modules"), ModuleScriptAsset);
		}
	}

	// Add dynamic inputs for default usage in module and dynamic input graphs
	if (bModuleGraph || bDynamicInputGraph)
	{
		TArray<FAssetData> DynamicInputScriptAssets;
		FNiagaraEditorUtilities::FGetFilteredScriptAssetsOptions DynamicInputScriptFilterOptions;
		DynamicInputScriptFilterOptions.ScriptUsageToInclude = ENiagaraScriptUsage::DynamicInput;
		FNiagaraEditorUtilities::GetFilteredScriptAssets(DynamicInputScriptFilterOptions, DynamicInputScriptAssets);

		for (const FAssetData& DynamicInputScriptAsset : DynamicInputScriptAssets)
		{
			AddScriptFunctionAction(LOCTEXT("Dynamic Input Menu Title", "Dynamic Inputs"), DynamicInputScriptAsset);
		}
	}

	//Add event read and writes nodes
	if (bModuleGraph)
	{
		const FText MenuCat = LOCTEXT("NiagaraEventMenuCat", "Events");
		const TArray<FNiagaraTypeDefinition>& RegisteredTypes = FNiagaraTypeRegistry::GetRegisteredPayloadTypes();
		for (FNiagaraTypeDefinition Type : RegisteredTypes)
		{
			if (Type.GetStruct() && !Type.GetStruct()->IsA(UNiagaraDataInterface::StaticClass()))
			{
				{
					const FText MenuDescFmt = LOCTEXT("AddEventReadFmt", "Add {0} Event Read");
					const FText MenuDesc = FText::Format(MenuDescFmt, Type.GetNameText());

					TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, MenuCat, MenuDesc, *MenuDesc.ToString(), FText::GetEmpty());

					UNiagaraNodeReadDataSet* EventReadNode = NewObject<UNiagaraNodeReadDataSet>(OwnerOfTemporaries);
					EventReadNode->InitializeFromStruct(Type.GetStruct());
					Action->NodeTemplate = EventReadNode;
				}
				{
					const FText MenuDescFmt = LOCTEXT("AddEventWriteFmt", "Add {0} Event Write");
					const FText MenuDesc = FText::Format(MenuDescFmt, Type.GetNameText());

					TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, MenuCat, MenuDesc, *MenuDesc.ToString(), FText::GetEmpty());

					UNiagaraNodeWriteDataSet* EventWriteNode = NewObject<UNiagaraNodeWriteDataSet>(OwnerOfTemporaries);
					EventWriteNode->InitializeFromStruct(Type.GetStruct());
					Action->NodeTemplate = EventWriteNode;
				}
			}
		}
	}
	
	TArray<ENiagaraScriptUsage> UsageTypesToAdd;
	if (bParticleUpdateGraph)
	{
		UsageTypesToAdd.Add(ENiagaraScriptUsage::ParticleEventScript);
		UsageTypesToAdd.Add(ENiagaraScriptUsage::EmitterSpawnScript);
		UsageTypesToAdd.Add(ENiagaraScriptUsage::EmitterUpdateScript);
	}

	if (bSystemGraph)
	{
		UsageTypesToAdd.Add(ENiagaraScriptUsage::SystemSpawnScript);
		UsageTypesToAdd.Add(ENiagaraScriptUsage::SystemUpdateScript);
	}

	if (UsageTypesToAdd.Num() != 0)
	{
		for (ENiagaraScriptUsage Usage : UsageTypesToAdd)
		{
			const FText MenuCat = LOCTEXT("NiagaraUsageMenuCat", "Output Nodes");

			UNiagaraNodeOutput* OutputNode = NewObject<UNiagaraNodeOutput>(OwnerOfTemporaries);
			OutputNode->SetUsage(Usage);

			FText MenuDesc = FText::Format(LOCTEXT("AddOutput", "Add {0}"), OutputNode->GetNodeTitle(ENodeTitleType::FullTitle));
			TSharedPtr<FNiagaraSchemaAction_NewNode> OutputNodeAction = AddNewNodeAction(NewActions, MenuCat, MenuDesc, *MenuDesc.ToString(), FText::GetEmpty());

			UNiagaraNodeOutput* UpdateOutputNode = NiagaraGraph->FindOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);
			if (UpdateOutputNode)
			{
				OutputNode->Outputs = UpdateOutputNode->Outputs;
			}
			else
			{
				OutputNode->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Out")));
			}
			OutputNodeAction->NodeTemplate = OutputNode;
		}
	}


	// Add Convert Nodes
	{
		FNiagaraTypeDefinition PinType = FNiagaraTypeDefinition::GetGenericNumericDef();
		bool bAddMakes = true;
		bool bAddBreaks = true;
		if (FromPin)
		{
			PinType = PinToTypeDefinition(FromPin);
			if (FromPin->Direction == EGPD_Input)
			{
				bAddBreaks = false;
			}
			else
			{
				bAddMakes = false;
			}
		}

		if (PinType.GetScriptStruct())
		{
			FText MakeCat = LOCTEXT("NiagaraMake", "Make");
			FText BreakCat = LOCTEXT("NiagaraBreak", "Break");

			FText DescFmt = LOCTEXT("NiagaraMakeBreakFmt", "{0}");
			auto MakeBreakType = [&](FNiagaraTypeDefinition Type, bool bMake)
			{
				FText DisplayName = Type.GetNameText();

				FText Desc = FText::Format(DescFmt, DisplayName);
				TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, bMake ? MakeCat : BreakCat, Desc, *Type.GetStruct()->GetName(), FText::GetEmpty());
				UNiagaraNodeConvert* ConvertNode = NewObject<UNiagaraNodeConvert>(OwnerOfTemporaries);
				if (bMake)
				{
					ConvertNode->InitAsMake(Type);
				}
				else
				{
					ConvertNode->InitAsBreak(Type);
				}
				Action->NodeTemplate = ConvertNode;
			};

			if (PinType == FNiagaraTypeDefinition::GetGenericNumericDef())
			{
				if (bAddMakes)
				{
					const TArray<FNiagaraTypeDefinition>& RegisteredTypes = FNiagaraTypeRegistry::GetRegisteredTypes();
					for (FNiagaraTypeDefinition Type : RegisteredTypes)
					{
						// Data interfaces can't be made.
						if (!UNiagaraDataInterface::IsDataInterfaceType(Type))
						{
							MakeBreakType(Type, true);
						}
					}
				}

				if (bAddBreaks)
				{
					const TArray<FNiagaraTypeDefinition>& RegisteredTypes = FNiagaraTypeRegistry::GetRegisteredTypes();
					for (FNiagaraTypeDefinition Type : RegisteredTypes)
					{
						//Don't break scalars. Allow makes for now as a convenient method of getting internal script constants when dealing with numeric pins.
						// Data interfaces can't be broken.
						if (!FNiagaraTypeDefinition::IsScalarDefinition(Type) && !UNiagaraDataInterface::IsDataInterfaceType(Type))
						{
							MakeBreakType(Type, false);
						}
					}
				}
			}
			else
			{
				//If we have a valid type then add it as a convenience at the top level.
				FText TypedMakeBreakFmt = LOCTEXT("NiagaraTypedMakeBreakFmt", "{0} {1}");
				FText DisplayName = PinType.GetStruct()->GetDisplayNameText();
				if (PinType.GetEnum())
				{
					DisplayName = FText::FromString(PinType.GetEnum()->GetName());
				}
				FText Desc = FText::Format(TypedMakeBreakFmt, bAddMakes ? MakeCat : BreakCat, DisplayName);
				TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, FText::GetEmpty(), Desc, *Desc.ToString(), FText::GetEmpty());
				UNiagaraNodeConvert* ConvertNode = NewObject<UNiagaraNodeConvert>(OwnerOfTemporaries);
				if (bAddMakes)
				{
					ConvertNode->InitAsMake(PinType);
				}
				else
				{
					ConvertNode->InitAsBreak(PinType);
				}
				Action->NodeTemplate = ConvertNode;
			}

			//Always add generic convert as an option.
			FText Desc = LOCTEXT("NiagaraConvert", "Convert");
			TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, FText::GetEmpty(), Desc, TEXT("Convert"), FText::GetEmpty());
			UNiagaraNodeConvert* ConvertNode = NewObject<UNiagaraNodeConvert>(OwnerOfTemporaries);
			Action->NodeTemplate = ConvertNode;
		}
	}

	if (FromPin)
	{
		//Add pin specific menu options.
		FNiagaraTypeDefinition PinType = PinToTypeDefinition(FromPin);
		UNiagaraDataInterface* DataInterface = nullptr;
		const UClass* Class = PinType.GetClass();
		if (Class != nullptr)
		{
			if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(FromPin->GetOwningNode()))
			{
				DataInterface = InputNode->GetDataInterface();
			}
			else 
			{
				DataInterface = Cast<UNiagaraDataInterface>(const_cast<UClass*>(Class)->GetDefaultObject());
			}

			if (DataInterface)
			{
				FText MenuCat = Class->GetDisplayNameText();
				TArray<FNiagaraFunctionSignature> Functions;
				DataInterface->GetFunctions(Functions);
				for (FNiagaraFunctionSignature& Sig : Functions)
				{
					TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, MenuCat, FText::FromString(Sig.GetName()), *Sig.GetName(), FText::GetEmpty());
					UNiagaraNodeFunctionCall* FuncNode = NewObject<UNiagaraNodeFunctionCall>(OwnerOfTemporaries);
					Action->NodeTemplate = FuncNode;
					FuncNode->Signature = Sig;
				}
			}
		}

		if (FromPin->Direction == EGPD_Output)
		{
			//Add all swizzles for this type if it's a vector.
			if (FHlslNiagaraTranslator::IsHlslBuiltinVector(PinType))
			{
				TArray<FString> Components;
				for (TFieldIterator<UProperty> PropertyIt(PinType.GetStruct(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
				{
					UProperty* Property = *PropertyIt;
					Components.Add(Property->GetName().ToLower());
				}

				TArray<FString> Swizzles;
				TFunction<void(FString)> GenSwizzles = [&](FString CurrStr)
				{
					if (CurrStr.Len() == 4) return;//Only generate down to float4
					for (FString& CompStr : Components)
					{
						Swizzles.Add(CurrStr + CompStr);
						GenSwizzles(CurrStr + CompStr);
					}
				};

				GenSwizzles(FString());

				for (FString Swiz : Swizzles)
				{
					const FText Category = LOCTEXT("NiagaraSwizzles", "Swizzles");

					TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, Category, FText::FromString(Swiz), *Swiz, FText::GetEmpty());

					UNiagaraNodeConvert* ConvertNode = NewObject<UNiagaraNodeConvert>(OwnerOfTemporaries);
					Action->NodeTemplate = ConvertNode;
					ConvertNode->InitAsSwizzle(Swiz);
				}
			}
		}
	}

	// Handle parameter map get/set
	{
		FText MenuCat = FText::FromString("Parameter Map");
		{
			FString Name = TEXT("Parameter Map Get");
			TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, MenuCat, FText::FromString(Name), *Name, FText::GetEmpty());
			UNiagaraNodeParameterMapGet* BaseNode = NewObject<UNiagaraNodeParameterMapGet>(OwnerOfTemporaries);
			Action->NodeTemplate = BaseNode;
		}
		{
			FString Name = TEXT("Parameter Map Set");
			TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, MenuCat, FText::FromString(Name), *Name, FText::GetEmpty());
			UNiagaraNodeParameterMapSet* BaseNode = NewObject<UNiagaraNodeParameterMapSet>(OwnerOfTemporaries);
			Action->NodeTemplate = BaseNode;
		}
	}

	// Handle comment nodes
	{
		FText MenuCat = FText::FromString("Comments");

		{
			FString Name = TEXT("Add Comment");
			TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, MenuCat, FText::FromString(Name), *Name, FText::GetEmpty());
			UEdGraphNode_Comment* BaseNode = NewObject<UEdGraphNode_Comment>(OwnerOfTemporaries);
			Action->NodeTemplate = BaseNode;
		}		
	}



	//Add all input node options for input pins or no pin.
	if (FromPin == nullptr || FromPin->Direction == EGPD_Input)
	{
		TArray<UNiagaraNodeInput*> InputNodes;
		NiagaraGraph->GetNodesOfClass(InputNodes);

		if (bFunctionGraph)
		{
			//Emitter constants managed by the system.
			const TArray<FNiagaraVariable>& SystemConstants = FNiagaraConstants::GetEngineConstants();
			for (const FNiagaraVariable& SysConst : SystemConstants)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("Constant"), FText::FromName(SysConst.GetName()));
				const FText MenuDesc = FText::Format(LOCTEXT("GetSystemConstant", "Get {Constant}"), Args);

				TSharedPtr<FNiagaraSchemaAction_NewNode> GetConstAction = AddNewNodeAction(NewActions, LOCTEXT("System Parameters Menu Title", "System Parameters"), MenuDesc, SysConst.GetName(), FText::GetEmpty());

				UNiagaraNodeInput* InputNode = NewObject<UNiagaraNodeInput>(OwnerOfTemporaries);
				InputNode->Usage = ENiagaraInputNodeUsage::SystemConstant;
				InputNode->Input = SysConst;
				GetConstAction->NodeTemplate = InputNode;
			}
		}

		//Emitter constants managed by the Translator.
		const TArray<FNiagaraVariable>& TranslatorConstants = FNiagaraConstants::GetTranslatorConstants();
		for (const FNiagaraVariable& TransConst : TranslatorConstants)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Constant"), FText::FromName(TransConst.GetName()));
			const FText MenuDesc = FText::Format(LOCTEXT("GetTranslatorConstant", "{Constant}"), Args);

			TSharedPtr<FNiagaraSchemaAction_NewNode> GetConstAction = AddNewNodeAction(NewActions, LOCTEXT("Translator Parameters Menu Title", "Special Purpose Parameters"), MenuDesc, TransConst.GetName(), FText::GetEmpty());

			UNiagaraNodeInput* InputNode = NewObject<UNiagaraNodeInput>(OwnerOfTemporaries);
			InputNode->Usage = ENiagaraInputNodeUsage::TranslatorConstant;
			InputNode->ExposureOptions.bCanAutoBind = true;
			InputNode->ExposureOptions.bHidden = true;
			InputNode->ExposureOptions.bRequired = false;
			InputNode->ExposureOptions.bExposed = false;
			InputNode->Input = TransConst;
			GetConstAction->NodeTemplate = InputNode;
		}

		AddParameterMenuOptions(NewActions, NiagaraGraph, OwnerOfTemporaries, NiagaraGraph);

		//Add a generic Parameter node to allow easy creation of parameters.
		{
			FNiagaraTypeDefinition PinType = FNiagaraTypeDefinition::GetGenericNumericDef();
			if (FromPin)
			{
				PinType = PinToTypeDefinition(FromPin);
			}

			if (PinType.GetStruct())
			{
				const FText MenuDescFmt = LOCTEXT("Add ParameterFmt", "Add {0} Parameter");
				const TArray<FNiagaraTypeDefinition>& RegisteredTypes = FNiagaraTypeRegistry::GetRegisteredParameterTypes();
				for (FNiagaraTypeDefinition Type : RegisteredTypes)
				{
					FText MenuCat;
					if (const UClass* Class = Type.GetClass())
					{						
						MenuCat = Class->GetMetaDataText(TEXT("Category"), TEXT("UObjectCategory"), Class->GetFullGroupName(false));
					}
					else
					{
						MenuCat = LOCTEXT("AddParameterCat", "Add Parameter");

						// If you are in dynamic inputs or modules, we only allow free-range variables for 
						// data interfaces and parameter maps.
						if (bDynamicInputGraph || bModuleGraph)
						{
							if (Type != FNiagaraTypeDefinition::GetParameterMapDef())
							{
								continue;
							}
						}
					}
						
					const FText MenuDesc = FText::Format(MenuDescFmt, Type.GetNameText());
					TSharedPtr<FNiagaraSchemaAction_NewNode> InputAction = AddNewNodeAction(NewActions, MenuCat, MenuDesc, *MenuDesc.ToString(), FText::GetEmpty());
					UNiagaraNodeInput* InputNode = NewObject<UNiagaraNodeInput>(OwnerOfTemporaries);
					FNiagaraEditorUtilities::InitializeParameterInputNode(*InputNode, Type, NiagaraGraph);
					InputAction->NodeTemplate = InputNode;
				}

				// TODO sckime please remove this..
				if (bSystemGraph || IsParticleGraph(NiagaraGraph))
				{
					for (FNiagaraTypeDefinition Type : RegisteredTypes)
					{
						FText MenuCat;
						if (const UClass* Class = Type.GetClass())
						{
							continue;
						}
						else
						{
							MenuCat = LOCTEXT("AddRIParameterCat", "Add Rapid Iteration Param");
						}

						const FText MenuDesc = FText::Format(MenuDescFmt, Type.GetNameText());
						TSharedPtr<FNiagaraSchemaAction_NewNode> InputAction = AddNewNodeAction(NewActions, MenuCat, MenuDesc, *MenuDesc.ToString(), FText::GetEmpty());
						UNiagaraNodeInput* InputNode = NewObject<UNiagaraNodeInput>(OwnerOfTemporaries);
						FNiagaraEditorUtilities::InitializeParameterInputNode(*InputNode, Type, NiagaraGraph);
						InputNode->Usage = ENiagaraInputNodeUsage::RapidIterationParameter;
						InputAction->NodeTemplate = InputNode;
					}
				}
				
				if (PinType != FNiagaraTypeDefinition::GetGenericNumericDef())
				{
					//For correctly typed pins, offer the correct type at the top level.				
					const FText MenuDesc = FText::Format(MenuDescFmt, PinType.GetNameText());
					TSharedPtr<FNiagaraSchemaAction_NewNode> InputAction = AddNewNodeAction(NewActions, FText::GetEmpty(), MenuDesc, *MenuDesc.ToString(), FText::GetEmpty());
					UNiagaraNodeInput* InputNode = NewObject<UNiagaraNodeInput>(OwnerOfTemporaries);
					FNiagaraEditorUtilities::InitializeParameterInputNode(*InputNode, PinType, NiagaraGraph);
					InputAction->NodeTemplate = InputNode;
				}
			}
		}
	}

	const FText MenuCat = LOCTEXT("NiagaraLogicMenuCat", "Logic");
	{
		const FText MenuDesc = LOCTEXT("If", "If");

		TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, MenuCat, MenuDesc, TEXT("If"), FText::GetEmpty());

		UNiagaraNodeIf* IfNode = NewObject<UNiagaraNodeIf>(OwnerOfTemporaries);
		Action->NodeTemplate = IfNode;
	}
	//TODO: Add quick commands for certain UNiagaraStructs and UNiagaraScripts to be added as functions

	// Add reroute node
	{
		const FText UtilMenuCat = LOCTEXT("NiagaraRerouteMenuCat", "Util");
		const FText RerouteMenuDesc = LOCTEXT("NiagaraRerouteMenuDesc", "Reroute ");
		TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, UtilMenuCat, RerouteMenuDesc, TEXT("Reroute"), FText::GetEmpty());
		UNiagaraNodeReroute* RerouteNode = NewObject<UNiagaraNodeReroute>(OwnerOfTemporaries);
		Action->NodeTemplate = RerouteNode;
	}

	// Add usage selector node
	{
		const FText UtilMenuCat = LOCTEXT("NiagaraUsageSelectorMenuCat", "Util");
		const FText UsageSelectorMenuDesc = LOCTEXT("NiagaraUsageSelectorMenuDesc", "Select By Use");
		TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, UtilMenuCat, UsageSelectorMenuDesc, TEXT("Select By Use"), FText::GetEmpty());
		UNiagaraNodeUsageSelector* Node = NewObject<UNiagaraNodeUsageSelector>(OwnerOfTemporaries);
		Action->NodeTemplate = Node;
	}

	// Add simulation target selector node
	{
		const FText UtilMenuCat = LOCTEXT("NiagaraSimTargetSelectorMenuCat", "Util");
		const FText SimTargetSelectorMenuDesc = LOCTEXT("NiagaraSimTargetSelectorMenuDesc", "Select By Simulation Target");
		TSharedPtr<FNiagaraSchemaAction_NewNode> Action = AddNewNodeAction(NewActions, UtilMenuCat, SimTargetSelectorMenuDesc, TEXT("Select By Simulation Target"), FText::GetEmpty());
		UNiagaraNodeSimTargetSelector* Node = NewObject<UNiagaraNodeSimTargetSelector>(OwnerOfTemporaries);
		Action->NodeTemplate = Node;
	}

	return NewActions;
}

const FPinConnectionResponse UEdGraphSchema_Niagara::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are on the same node"));
	}

	// Check both pins support connections
	if(PinA->bNotConnectable || PinB->bNotConnectable)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Pin doesn't support connections."));
	}

	// Compare the directions
	const UEdGraphPin* InputPin = NULL;
	const UEdGraphPin* OutputPin = NULL;

	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Directions are not compatible"));
	}

	if (PinA->PinType.PinCategory != TEXT("wildcard") && PinB->PinType.PinCategory != TEXT("wildcard"))
	{
		// Check for compatible type pins.
		if (PinA->PinType.PinCategory == PinCategoryType &&
			PinB->PinType.PinCategory == PinCategoryType &&
			PinA->PinType != PinB->PinType)
		{
			FNiagaraTypeDefinition PinTypeA = PinToTypeDefinition(PinA);
			FNiagaraTypeDefinition PinTypeB = PinToTypeDefinition(PinB);

			if (PinTypeA == FNiagaraTypeDefinition::GetParameterMapDef() || PinTypeB == FNiagaraTypeDefinition::GetParameterMapDef())
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Types are not compatible"));
			}

			else if (FNiagaraTypeDefinition::TypesAreAssignable(PinTypeA, PinTypeB) == false)
			{
				//Do some limiting on auto conversions here?
				if (PinTypeA.GetClass())
				{
					return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Types are not compatible"));
				}
				else
				{
					return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, FString::Printf(TEXT("Convert %s to %s"), *(PinToTypeDefinition(PinA).GetNameText().ToString()), *(PinToTypeDefinition(PinB).GetNameText().ToString())));
				}
			}
		}

		// Check for compatible misc pins
		if (PinA->PinType.PinCategory == PinCategoryMisc ||
			PinB->PinType.PinCategory == PinCategoryMisc)
		{
			// TODO: This shouldn't be handled explicitly here.
			bool PinAIsConvertAddAndPinBIsNonGenericType =
				PinA->PinType.PinCategory == PinCategoryMisc && PinA->PinType.PinSubCategory == UNiagaraNodeWithDynamicPins::AddPinSubCategory &&
				PinB->PinType.PinCategory == PinCategoryType && PinToTypeDefinition(PinB) != FNiagaraTypeDefinition::GetGenericNumericDef() &&
				PinToTypeDefinition(PinB) != FNiagaraTypeDefinition::GetParameterMapDef();

			bool PinBIsConvertAddAndPinAIsNonGenericType =
				PinB->PinType.PinCategory == PinCategoryMisc && PinB->PinType.PinSubCategory == UNiagaraNodeWithDynamicPins::AddPinSubCategory &&
				PinA->PinType.PinCategory == PinCategoryType && PinToTypeDefinition(PinA) != FNiagaraTypeDefinition::GetGenericNumericDef() &&
				PinToTypeDefinition(PinA) != FNiagaraTypeDefinition::GetParameterMapDef();

			if (PinAIsConvertAddAndPinBIsNonGenericType == false && PinBIsConvertAddAndPinAIsNonGenericType == false)
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Types are not compatible"));
			}
		}

		if (PinA->PinType.PinCategory == PinCategoryClass || PinB->PinType.PinCategory == PinCategoryClass)
		{
			FNiagaraTypeDefinition AType = PinToTypeDefinition(PinA);
			FNiagaraTypeDefinition BType = PinToTypeDefinition(PinB);
			if (AType != BType)
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Types are not compatible"));
			}
		}

		if (PinA->PinType.PinCategory == PinCategoryEnum || PinB->PinType.PinCategory == PinCategoryEnum)
		{
			FNiagaraTypeDefinition PinTypeA = PinToTypeDefinition(PinA);
			FNiagaraTypeDefinition PinTypeB = PinToTypeDefinition(PinB);
			if (FNiagaraTypeDefinition::TypesAreAssignable(PinTypeA, PinTypeB) == false)
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Types are not compatible"));
			}
		}
	}

	int32 Depth = 0;
	if (UEdGraphSchema_Niagara::CheckCircularConnection(PinB->GetOwningNode(), PinB->Direction, PinA, Depth))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Circular connection found"));
	}

	// See if we want to break existing connections (if its an input with an existing connection)
	const bool bBreakExistingDueToDataInput = (InputPin->LinkedTo.Num() > 0);
	if (bBreakExistingDueToDataInput)
	{
		const ECanCreateConnectionResponse ReplyBreakInputs = (PinA == InputPin) ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B;
		return FPinConnectionResponse(ReplyBreakInputs, TEXT("Replace existing input connections"));
	}
	else
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, FString());
	}
}

void UEdGraphSchema_Niagara::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorBreakConnection", "Niagara Editor: Break Connection"));

	Super::BreakSinglePinLink(SourcePin, TargetPin);
}

void UEdGraphSchema_Niagara::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorBreakPinLinks", "Niagara Editor: Break Pin Links"));

	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
}

FConnectionDrawingPolicy* UEdGraphSchema_Niagara::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FNiagaraConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

void UEdGraphSchema_Niagara::ResetPinToAutogeneratedDefaultValue(UEdGraphPin* Pin, bool bCallModifyCallbacks) const
{
	Pin->DefaultValue = Pin->AutogeneratedDefaultValue;
	if (bCallModifyCallbacks)
	{
		Pin->GetOwningNode()->PinDefaultValueChanged(Pin);
	}
}

void UEdGraphSchema_Niagara::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	const FScopedTransaction Transaction(LOCTEXT("CreateRerouteNodeOnWire", "Create Reroute Node"));

	//@TODO: This constant is duplicated from inside of SGraphNodeKnot
	const FVector2D NodeSpacerSize(42.0f, 24.0f);
	const FVector2D KnotTopLeft = GraphPosition - (NodeSpacerSize * 0.5f);

	// Create a new knot
	UEdGraph* ParentGraph = PinA->GetOwningNode()->GetGraph();
	UNiagaraNodeReroute* NewReroute = FNiagaraSchemaAction_NewNode::SpawnNodeFromTemplate<UNiagaraNodeReroute>(ParentGraph, NewObject<UNiagaraNodeReroute>(), KnotTopLeft);

	// Move the connections across (only notifying the knot, as the other two didn't really change)
	PinA->BreakLinkTo(PinB);
	PinA->MakeLinkTo((PinA->Direction == EGPD_Output) ? NewReroute->GetInputPin(0) : NewReroute->GetOutputPin(0));
	PinB->MakeLinkTo((PinB->Direction == EGPD_Output) ? NewReroute->GetInputPin(0) : NewReroute->GetOutputPin(0));
	NewReroute->PropagatePinType();
}

bool UEdGraphSchema_Niagara::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorCreateConnection", "Niagara Editor: Create Connection"));

	const FPinConnectionResponse Response = CanCreateConnection(PinA, PinB);
	bool bModified = false;

	switch (Response.Response)
	{
	case CONNECT_RESPONSE_MAKE:
		PinA->Modify();
		PinB->Modify();
		PinA->MakeLinkTo(PinB);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_A:
		PinA->Modify();
		PinB->Modify();
		PinA->BreakAllPinLinks();
		PinA->MakeLinkTo(PinB);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_B:
		PinA->Modify();
		PinB->Modify();
		PinB->BreakAllPinLinks();
		PinA->MakeLinkTo(PinB);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_AB:
		PinA->Modify();
		PinB->Modify();
		PinA->BreakAllPinLinks();
		PinB->BreakAllPinLinks();
		PinA->MakeLinkTo(PinB);
		bModified = true;
		break;

	case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:
	{
		if (PinA->Direction == EGPD_Input)
		{
			//Swap so that A is the from pin and B is the to pin.
			UEdGraphPin* Temp = PinA;
			PinA = PinB;
			PinB = Temp;
		}

		FNiagaraTypeDefinition AType = PinToTypeDefinition(PinA);
		FNiagaraTypeDefinition BType = PinToTypeDefinition(PinB);
		if (AType != BType && AType.GetClass() == nullptr && BType.GetClass() == nullptr)
		{
			UEdGraphNode* ANode = PinA->GetOwningNode();
			UEdGraphNode* BNode = PinB->GetOwningNode();
			UEdGraph* Graph = ANode->GetTypedOuter<UEdGraph>();
			
			// Since we'll be adding a node, make sure to modify the graph itself.
			Graph->Modify();
			FGraphNodeCreator<UNiagaraNodeConvert> NodeCreator(*Graph);
			UNiagaraNodeConvert* AutoConvertNode = NodeCreator.CreateNode(false);
			AutoConvertNode->AllocateDefaultPins();
			AutoConvertNode->NodePosX = (ANode->NodePosX + BNode->NodePosX) >> 1;
			AutoConvertNode->NodePosY = (ANode->NodePosY + BNode->NodePosY) >> 1;
			NodeCreator.Finalize();

			if (AutoConvertNode->InitConversion(PinA, PinB))
			{
				PinA->Modify();
				PinB->Modify();
				bModified = true;
			}
			else
			{
				Graph->RemoveNode(AutoConvertNode);
			}
		}
	}
	break;

	case CONNECT_RESPONSE_DISALLOW:
	default:
		break;
	}

#if WITH_EDITOR
	if (bModified)
	{
		PinA->GetOwningNode()->PinConnectionListChanged(PinA);
		PinB->GetOwningNode()->PinConnectionListChanged(PinB);
	}
#endif	//#if WITH_EDITOR

	return bModified;
}

FLinearColor UEdGraphSchema_Niagara::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	if (PinType.PinCategory == PinCategoryType)
	{
		FNiagaraTypeDefinition Type(CastChecked<UScriptStruct>(PinType.PinSubCategoryObject.Get()));
		return GetTypeColor(Type);
	}
		
	const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();
	return Settings->WildcardPinTypeColor;
}

FLinearColor UEdGraphSchema_Niagara::GetTypeColor(const FNiagaraTypeDefinition& Type)
{
	const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();
	if (Type == FNiagaraTypeDefinition::GetFloatDef())
	{
		return Settings->FloatPinTypeColor;
	}
	else if (Type == FNiagaraTypeDefinition::GetIntDef())
	{
		return Settings->IntPinTypeColor;
	}
	else if (Type == FNiagaraTypeDefinition::GetBoolDef())
	{
		return Settings->BooleanPinTypeColor;
	}
	else if (Type == FNiagaraTypeDefinition::GetVec3Def())
	{
		return Settings->VectorPinTypeColor;
	}
	else if (Type == FNiagaraTypeDefinition::GetParameterMapDef())
	{
		return Settings->ExecutionPinTypeColor;
	}
	else
	{
		return Settings->StructPinTypeColor;
	}
}

bool UEdGraphSchema_Niagara::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	check(Pin != NULL);

	if (Pin->bDefaultValueIsIgnored)
	{
		return true;
	}

	return false;
}

FNiagaraVariable UEdGraphSchema_Niagara::PinToNiagaraVariable(const UEdGraphPin* Pin, bool bNeedsValue)const
{
	FNiagaraVariable Var = FNiagaraVariable(PinToTypeDefinition(Pin), Pin->PinName);
	bool bHasValue = false;
	if (Pin->bDefaultValueIsIgnored == false && Pin->DefaultValue.IsEmpty() == false)
	{
		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(Var.GetType());
		if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanHandlePinDefaults())
		{
			bHasValue = TypeEditorUtilities->SetValueFromPinDefaultString(Pin->DefaultValue, Var);
			if (bHasValue == false)
			{
				FString OwningNodePath = Pin->GetOwningNode() != nullptr ? Pin->GetOwningNode()->GetPathName() : TEXT("Unknown");
				UE_LOG(LogNiagaraEditor, Error, TEXT("PinToNiagaraVariable: Failed to convert default value '%s' to type %s. Owning node path: %s"), *Pin->DefaultValue, *Var.GetType().GetName(), *OwningNodePath);
			}
		}
		else
		{
			if (Pin->GetOwningNode() != nullptr && nullptr == Cast<UNiagaraNodeOp>(Pin->GetOwningNode()))
			{
				FString OwningNodePath = Pin->GetOwningNode() != nullptr ? Pin->GetOwningNode()->GetPathName() : TEXT("Unknown");
				UE_LOG(LogNiagaraEditor, Error, TEXT("Pin had default value string, but default values aren't supported for variables of type {%s}. Owning node path: %s"), *Var.GetType().GetName(), *OwningNodePath);
			}
		}
	}

	if (bNeedsValue && bHasValue == false)
	{
		FNiagaraEditorUtilities::ResetVariableToDefaultValue(Var);
		if (Var.GetData() == nullptr)
		{
			UE_LOG(LogNiagaraEditor, Error, TEXT("ResetVariableToDefaultValue called, but failed on var %s type %s. "), *Var.GetName().ToString(), *Var.GetType().GetName());
		}
	}

	return Var;
}

bool UEdGraphSchema_Niagara::TryGetPinDefaultValueFromNiagaraVariable(const FNiagaraVariable& Variable, FString& OutPinDefaultValue) const
{
	// Create a variable we can be sure is allocated since it's required for the call to GetPinDefaultStringFromValue.
	FNiagaraVariable PinDefaultVariable = Variable;
	if (Variable.IsDataAllocated() == false)
	{
		FNiagaraEditorUtilities::ResetVariableToDefaultValue(PinDefaultVariable);
	}

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(PinDefaultVariable.GetType());
	if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanHandlePinDefaults())
	{
		OutPinDefaultValue = TypeEditorUtilities->GetPinDefaultStringFromValue(PinDefaultVariable);
		return true;
	}
	
	OutPinDefaultValue = FString();
	return false;
}

FNiagaraTypeDefinition UEdGraphSchema_Niagara::PinToTypeDefinition(const UEdGraphPin* Pin) const
{
	if (Pin == nullptr)
	{
		return FNiagaraTypeDefinition();
	}
	if (Pin->PinType.PinCategory == PinCategoryType && Pin->PinType.PinSubCategoryObject.IsValid())
	{
		UScriptStruct* Struct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
		if (Struct == nullptr)
		{
			UE_LOG(LogNiagaraEditor, Error, TEXT("Pin states that it is of struct type, but is missing its struct object. This is usually the result of a registered type going away. Pin Name '%s' Owning Node '%s'."),
				*Pin->PinName.ToString(), *Pin->GetOwningNode()->GetName());
			return FNiagaraTypeDefinition();
		}
		return FNiagaraTypeDefinition(Struct);
	}
	else if (Pin->PinType.PinCategory == PinCategoryClass)
	{
		UClass* Class = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());
		if (Class == nullptr)
		{
			UE_LOG(LogNiagaraEditor, Error, TEXT("Pin states that it is of class type, but is missing its class object. This is usually the result of a registered type going away. Pin Name '%s' Owning Node '%s'."),
				*Pin->PinName.ToString(), *Pin->GetOwningNode()->GetName());
			return FNiagaraTypeDefinition();
		}
		return FNiagaraTypeDefinition(Class);
	}
	else if (Pin->PinType.PinCategory == PinCategoryEnum)
	{
		UEnum* Enum = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get());
		if (Enum == nullptr)
		{
			UE_LOG(LogNiagaraEditor, Error, TEXT("Pin states that it is of Enum type, but is missing its Enum! Pin Name '%s' Owning Node '%s'. Turning into standard int definition!"), *Pin->PinName.ToString(),
				*Pin->GetOwningNode()->GetName());
			return FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetIntDef());
		}
		return FNiagaraTypeDefinition(Enum);
	}
	return FNiagaraTypeDefinition();
}

FEdGraphPinType UEdGraphSchema_Niagara::TypeDefinitionToPinType(FNiagaraTypeDefinition TypeDef)const
{
	if (TypeDef.GetClass())
	{
		return FEdGraphPinType(PinCategoryClass, NAME_None, const_cast<UClass*>(TypeDef.GetClass()), EPinContainerType::None, false, FEdGraphTerminalType());
	}
	else if (TypeDef.GetEnum())
	{
		return FEdGraphPinType(PinCategoryEnum, NAME_None, const_cast<UEnum*>(TypeDef.GetEnum()), EPinContainerType::None, false, FEdGraphTerminalType());
	}
	else
	{
		//TODO: Are base types better as structs or done like BPS as a special name?
		return FEdGraphPinType(PinCategoryType, NAME_None, const_cast<UScriptStruct*>(TypeDef.GetScriptStruct()), EPinContainerType::None, false, FEdGraphTerminalType());
	}
}

bool UEdGraphSchema_Niagara::IsSystemConstant(const FNiagaraVariable& Variable)const
{
	return FNiagaraConstants::GetEngineConstants().Find(Variable) != INDEX_NONE;
}

UNiagaraParameterCollection* UEdGraphSchema_Niagara::VariableIsFromParameterCollection(const FNiagaraVariable& Var)const
{
	FString VarName = Var.GetName().ToString();
	if (VarName.StartsWith(TEXT("NPC.")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> CollectionAssets;
		AssetRegistryModule.Get().GetAssetsByClass(UNiagaraParameterCollection::StaticClass()->GetFName(), CollectionAssets);
		TArray<FName> ExistingNames;
		for (FAssetData& CollectionAsset : CollectionAssets)
		{
			if (UNiagaraParameterCollection* Collection = CastChecked<UNiagaraParameterCollection>(CollectionAsset.GetAsset()))
			{
				if (VarName.StartsWith(Collection->GetFullNamespace()))
				{
					return Collection;
				}
			}
		}
	}
	return nullptr;
}

UNiagaraParameterCollection* UEdGraphSchema_Niagara::VariableIsFromParameterCollection(const FString& VarName, bool bAllowPartialMatch, FNiagaraVariable& OutVar)const
{
	OutVar = FNiagaraVariable();

	if (VarName.StartsWith(TEXT("NPC.")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> CollectionAssets;
		AssetRegistryModule.Get().GetAssetsByClass(UNiagaraParameterCollection::StaticClass()->GetFName(), CollectionAssets);
		TArray<FName> ExistingNames;
		for (FAssetData& CollectionAsset : CollectionAssets)
		{
			if (UNiagaraParameterCollection* Collection = CastChecked<UNiagaraParameterCollection>(CollectionAsset.GetAsset()))
			{
				if (VarName.StartsWith(Collection->GetFullNamespace()))
				{
					const TArray<FNiagaraVariable>& CollectionVariables = Collection->GetParameters();
					FString BestMatchSoFar;

					for (const FNiagaraVariable& CollVar : CollectionVariables)
					{
						FString CollVarName = CollVar.GetName().ToString();
						if (CollVarName == VarName)
						{
							OutVar = CollVar;
							break;
						}
						else if (bAllowPartialMatch && VarName.StartsWith(CollVarName + TEXT(".")) && (BestMatchSoFar.Len() == 0 || CollVarName.Len() > BestMatchSoFar.Len()))
						{
							OutVar = CollVar;
							BestMatchSoFar = CollVarName;
						}
					}
					return Collection;
				}
			}
		}
	}
	return nullptr;
}

FNiagaraTypeDefinition UEdGraphSchema_Niagara::GetTypeDefForProperty(const UProperty* Property)const
{
	if (Property->IsA(UFloatProperty::StaticClass()))
	{
		return FNiagaraTypeDefinition::GetFloatDef();
	}
	else if (Property->IsA(UIntProperty::StaticClass()))
	{
		return FNiagaraTypeDefinition::GetIntDef();
	}
	else if (Property->IsA(UBoolProperty::StaticClass()))
	{
		return FNiagaraTypeDefinition::GetBoolDef();
	}	
	else if (Property->IsA(UEnumProperty::StaticClass()))
	{
		const UEnumProperty* EnumProp = Cast<UEnumProperty>(Property);
		return FNiagaraTypeDefinition(EnumProp->GetEnum());
	}
	else if (const UStructProperty* StructProp = CastChecked<UStructProperty>(Property))
	{
		return FNiagaraTypeDefinition(StructProp->Struct);
	}

	check(0);
	return FNiagaraTypeDefinition::GetFloatDef();//Some invalid type?
}

void UEdGraphSchema_Niagara::GetBreakLinkToSubMenuActions(class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin)
{
	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for (TArray<class UEdGraphPin*>::TConstIterator Links(InGraphPin->LinkedTo); Links; ++Links)
	{
		UEdGraphPin* Pin = *Links;
		FString TitleString = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString();
		FText Title = FText::FromString(TitleString);
		if (!Pin->PinName.IsNone())
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *Pin->PinName.ToString());

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add(TEXT("NodeTitle"), Title);
			Args.Add(TEXT("PinName"), Pin->GetDisplayName());
			Title = FText::Format(LOCTEXT("BreakDescPin", "{NodeTitle} ({PinName})"), Args);
		}

		uint32 &Count = LinkTitleCount.FindOrAdd(TitleString);

		FText Description;
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeTitle"), Title);
		Args.Add(TEXT("NumberOfNodes"), Count);

		if (Count == 0)
		{
			Description = FText::Format(LOCTEXT("BreakDesc", "Break link to {NodeTitle}"), Args);
		}
		else
		{
			Description = FText::Format(LOCTEXT("BreakDescMulti", "Break link to {NodeTitle} ({NumberOfNodes})"), Args);
		}
		++Count;
		MenuBuilder.AddMenuEntry(Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateUObject((UEdGraphSchema_Niagara*const)this, &UEdGraphSchema::BreakSinglePinLink, const_cast<UEdGraphPin*>(InGraphPin), *Links)));
	}
}

void UEdGraphSchema_Niagara::ConvertNumericPinToType(UEdGraphPin* InGraphPin, FNiagaraTypeDefinition TypeDef)
{
	if (PinToTypeDefinition(InGraphPin) != TypeDef)
	{
		UNiagaraNode* Node = Cast<UNiagaraNode>(InGraphPin->GetOwningNode());
		if (Node)
		{
			FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorChangeNumericPinType", "Change Pin Type"));
			if (false == Node->ConvertNumericPinToType(InGraphPin, TypeDef))
			{
				Transaction.Cancel();
			}
		}
	}
}

bool UEdGraphSchema_Niagara::CheckCircularConnection(const UEdGraphNode* InRootNode, const EEdGraphPinDirection InRootPinDirection, const UEdGraphPin* InPin, int32& OutDepth)
{
	if (InPin->GetOwningNode() == InRootNode)
	{
		return true;
	}

	static const int32 MaxDepth = 3;
	OutDepth++;
	if (OutDepth > MaxDepth)
	{
		return false;
	}

	for (const UEdGraphPin* Pin : InPin->GetOwningNode()->GetAllPins())
	{
		if (Pin->Direction == InRootPinDirection && Pin != InPin)
		{
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (UEdGraphSchema_Niagara::CheckCircularConnection(InRootNode, InRootPinDirection, LinkedPin, OutDepth))
				{
					return true;
				}

				// If the CheckCircularConnection call above returned without finding the root node and was too deep.
				if (OutDepth > MaxDepth)
				{
					return false;
				}
			}
		}
	}

	OutDepth--;
	return false;
}

void UEdGraphSchema_Niagara::GetNumericConversionToSubMenuActions(class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin)
{
	// Add all the types we could convert to
	for (const FNiagaraTypeDefinition& TypeDef : FNiagaraTypeRegistry::GetNumericTypes())
	{
		FText Title = TypeDef.GetNameText();

		FText Description;
		FFormatNamedArguments Args;
		Args.Add(TEXT("TypeTitle"), Title);
		Description = FText::Format(LOCTEXT("NumericConversionText", "{TypeTitle}"), Args);
		MenuBuilder.AddMenuEntry(Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateUObject((UEdGraphSchema_Niagara*const)this, &UEdGraphSchema_Niagara::ConvertNumericPinToType, const_cast<UEdGraphPin*>(InGraphPin), FNiagaraTypeDefinition(TypeDef))));
	}
}

void UEdGraphSchema_Niagara::ToggleNodeEnabledState(UNiagaraNode* InNode) const
{
	if (InNode != nullptr)
	{
		if (InNode->GetDesiredEnabledState() == ENodeEnabledState::Disabled)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorSetNodeEnabled", "Enabled Node"));
			InNode->Modify();
			InNode->SetEnabledState(ENodeEnabledState::Enabled, true);
			InNode->MarkNodeRequiresSynchronization(__FUNCTION__, true);
		}
		else if (InNode->GetDesiredEnabledState() == ENodeEnabledState::Enabled)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorSetNodeDisabled", "Disabled Node"));
			InNode->Modify();
			InNode->SetEnabledState(ENodeEnabledState::Disabled, true);
			InNode->MarkNodeRequiresSynchronization(__FUNCTION__, true);
		}
	}
}

void UEdGraphSchema_Niagara::RefreshNode(UNiagaraNode* InNode) const
{
	if (InNode != nullptr)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorRefreshNode", "Refresh Node"));
		InNode->Modify();
		if (InNode->RefreshFromExternalChanges())
		{
			InNode->MarkNodeRequiresSynchronization(__FUNCTION__, true);
		}
	}
}

bool UEdGraphSchema_Niagara::CanPromoteSinglePinToParameter(const UEdGraphPin* SourcePin) 
{
	const UNiagaraGraph* NiagaraGraph = Cast<UNiagaraGraph>(SourcePin->GetOwningNode()->GetGraph());
	if (IsFunctionGraph(NiagaraGraph))
	{
		return true;
	}
	return false;
}

void UEdGraphSchema_Niagara::PromoteSinglePinToParameter(UEdGraphPin* SourcePin)
{
	if (SourcePin)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "NiagaraEditorPromote", "Promote To Parameter"));
		{
			TSharedPtr<FNiagaraSchemaAction_NewNode> InputAction = TSharedPtr<FNiagaraSchemaAction_NewNode>(new FNiagaraSchemaAction_NewNode(FText::GetEmpty(), FText::GetEmpty(), NAME_None, FText::GetEmpty(), 0));
			UNiagaraNodeInput* InputNode = NewObject<UNiagaraNodeInput>(GetTransientPackage());
			FNiagaraVariable Var = PinToNiagaraVariable(SourcePin);
			UNiagaraGraph* Graph = Cast<UNiagaraGraph>(SourcePin->GetOwningNode()->GetGraph());
			FNiagaraEditorUtilities::InitializeParameterInputNode(*InputNode, Var.GetType(), Graph);
			InputAction->NodeTemplate = InputNode;

			UEdGraphNode* PinNode = SourcePin->GetOwningNode();

			const float PinVisualOffsetX = 175.0f;
			InputAction->PerformAction(Graph, SourcePin, FVector2D(PinNode->NodePosX - PinVisualOffsetX, PinNode->NodePosY));
		}
	}
}

bool CanResetPinToDefault(const UEdGraphSchema_Niagara* Schema, const UEdGraphPin* Pin)
{
	return Schema->DoesDefaultValueMatchAutogenerated(*Pin) == false;
}

void UEdGraphSchema_Niagara::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if (InGraphPin)
	{
		MenuBuilder->BeginSection("EdGraphSchema_NiagaraPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			if (PinToTypeDefinition(InGraphPin) == FNiagaraTypeDefinition::GetGenericNumericDef() && InGraphPin->LinkedTo.Num() == 0)
			{
				MenuBuilder->AddSubMenu(
					LOCTEXT("ConvertNumericSpecific", "Convert Numeric To..."),
					LOCTEXT("ConvertNumericSpecificToolTip", "Convert Numeric pin to specific typed pin."),
				FNewMenuDelegate::CreateUObject((UEdGraphSchema_Niagara*const)this, &UEdGraphSchema_Niagara::GetNumericConversionToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
			}

			// Only display the 'Break Link' option if there is a link to break!
			if (InGraphPin->LinkedTo.Num() > 0)
			{
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakPinLinks);

				// add sub menu for break link to
				if (InGraphPin->LinkedTo.Num() > 1)
				{
					MenuBuilder->AddSubMenu(
						LOCTEXT("BreakLinkTo", "Break Link To..."),
						LOCTEXT("BreakSpecificLinks", "Break a specific link..."),
						FNewMenuDelegate::CreateUObject((UEdGraphSchema_Niagara*const)this, &UEdGraphSchema_Niagara::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
				}
				else
				{
					((UEdGraphSchema_Niagara*const)this)->GetBreakLinkToSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
				}
			}

			if (InGraphPin->Direction == EEdGraphPinDirection::EGPD_Input)
			{
				MenuBuilder->AddMenuEntry(LOCTEXT("PromoteToParameter", "Promote to Parameter"), LOCTEXT("PromoteToParameterTooltip", "Create a parameter argument and connect this pin to that parameter."), FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject((UEdGraphSchema_Niagara*const)this, &UEdGraphSchema_Niagara::PromoteSinglePinToParameter, const_cast<UEdGraphPin*>(InGraphPin)),
						FCanExecuteAction::CreateStatic(&UEdGraphSchema_Niagara::CanPromoteSinglePinToParameter, InGraphPin)));
				if (InGraphPin->LinkedTo.Num() == 0 && InGraphPin->bDefaultValueIsIgnored == false)
				{
					MenuBuilder->AddMenuEntry(
						LOCTEXT("ResetInputToDefault", "Reset to Default"), 
						LOCTEXT("ResetInputToDefaultToolTip", "Reset this input to its default value."),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateUObject((UEdGraphSchema_Niagara*const)this, &UEdGraphSchema_Niagara::ResetPinToAutogeneratedDefaultValue, const_cast<UEdGraphPin*>(InGraphPin), true),
							FCanExecuteAction::CreateStatic(&CanResetPinToDefault, this, InGraphPin)));
				}
			}
		}
		MenuBuilder->EndSection();
	}
	else if (InGraphNode)
	{
		const UNiagaraNode* Node = Cast<UNiagaraNode>(InGraphNode);
		MenuBuilder->BeginSection("EdGraphSchema_NiagaraNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
		MenuBuilder->AddMenuEntry(LOCTEXT("ToggleEnabledState", "Toggle Enabled State"), LOCTEXT("ToggleEnabledStateTooltip", "Toggle this node between Enbled (default) and Disabled (skipped from compilation)."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateUObject((UEdGraphSchema_Niagara*const)this, &UEdGraphSchema_Niagara::ToggleNodeEnabledState, const_cast<UNiagaraNode*>(Node))));
		MenuBuilder->AddMenuEntry(LOCTEXT("RefreshNode", "Refresh Node"), LOCTEXT("RefreshNodeTooltip", "Refresh this node."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateUObject((UEdGraphSchema_Niagara*const)this, &UEdGraphSchema_Niagara::RefreshNode, const_cast<UNiagaraNode*>(Node))));

		MenuBuilder->EndSection();
	}

	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}

FNiagaraConnectionDrawingPolicy::FNiagaraConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraph)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	, Graph(CastChecked<UNiagaraGraph>(InGraph))
{
	ArrowImage = nullptr;
	ArrowRadius = FVector2D::ZeroVector;
}

void FNiagaraConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);
	if (HoveredPins.Contains(InputPin) && HoveredPins.Contains(OutputPin))
	{
		Params.WireThickness = Params.WireThickness * 5;
	}

	if (Graph)
	{
		const UEdGraphSchema_Niagara* NSchema = Cast<UEdGraphSchema_Niagara>(Graph->GetSchema());
		if (NSchema && OutputPin)
		{
			Params.WireColor = NSchema->GetPinTypeColor(OutputPin->PinType);
			if (NSchema->PinToTypeDefinition(OutputPin) == FNiagaraTypeDefinition::GetGenericNumericDef())
			{
				FNiagaraTypeDefinition NewDef = Graph->GetCachedNumericConversion(OutputPin);
				if (NewDef.IsValid())
				{
					FEdGraphPinType NewPinType = NSchema->TypeDefinitionToPinType(NewDef);
					Params.WireColor = NSchema->GetPinTypeColor(NewPinType);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
