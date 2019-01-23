// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintCompiler.h"
#include "ControlRig.h"
#include "KismetCompiler.h"
#include "ControlRigBlueprint.h"
#include "Units/RigUnit.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "Kismet2/KismetReinstanceUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogControlRigCompiler, Log, All);

bool FControlRigBlueprintCompiler::CanCompile(const UBlueprint* Blueprint)
{
	if (Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UControlRig::StaticClass()))
	{
		return true;
	}

	return false;
}

void FControlRigBlueprintCompiler::Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results)
{
	FControlRigBlueprintCompilerContext Compiler(Blueprint, Results, CompileOptions);
	Compiler.Compile();
}

void FControlRigBlueprintCompilerContext::BuildPropertyLinks()
{
	UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Blueprint);
	if (ControlRigBlueprint)
	{
		// remove all property links
		ControlRigBlueprint->PropertyLinks.Reset();

		// Build property links from pin links
		for(UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			for(UEdGraphNode* Node : Graph->Nodes)
			{
				for(UEdGraphPin* Pin : Node->Pins)
				{
					if(Pin->Direction == EGPD_Output)
					{
						for(UEdGraphPin* LinkedPin : Pin->LinkedTo)
						{
							ControlRigBlueprint->MakePropertyLink(Pin->PinName.ToString(), LinkedPin->PinName.ToString());
						}
					}
				}
			}
		}
	}
}

void FControlRigBlueprintCompilerContext::MergeUbergraphPagesIn(UEdGraph* Ubergraph)
{
	BuildPropertyLinks();
}

FString RetrieveRootName(FString Input)
{
	int32 ParseIndex = 0;
	if (Input.FindChar(TCHAR('.'), ParseIndex))
	{
		return Input.Left(ParseIndex);
	}

	return Input;
}

void FControlRigBlueprintCompilerContext::AddRootPropertyLinks(TArray<FControlRigBlueprintPropertyLink>& InLinks, TArray<FName>& OutSourceArray, TArray<FName>& OutDestArray) const
{
	for (int32 Index = 0; Index < InLinks.Num(); ++Index)
	{
		const FControlRigBlueprintPropertyLink& PropertyLink = InLinks[Index];
		FString Source = RetrieveRootName(PropertyLink.GetSourcePropertyPath());
		FString Dest = RetrieveRootName(PropertyLink.GetDestPropertyPath());
		OutSourceArray.Add(FName(*Source));
		OutDestArray.Add(FName(*Dest));
	}
}

//// dependency graph builder for property links
struct FDependencyGraph
{
	int32 NumVertices;
	TMultiMap<int32, int32> Edges;

public:
	FDependencyGraph(int32 InNumVertices)
		:NumVertices(InNumVertices)
	{
	}

	// function to add an edge to graph
	void AddEdge(const int32 From, const int32 To)
	{
		check(From < NumVertices);
		check(To < NumVertices);
		// we're saving incoming edges
		Edges.AddUnique(From, To);
	}

	// prints a Topological Sort of the complete graph
	bool TopologicalSort(TArray<int32>& OutSortedList)
	{
		TArray<int32> InDegrees;
		InDegrees.AddZeroed(NumVertices);

		TArray<int32> NoInVertices;

		for (TMultiMap<int32, int32>::TConstIterator Iter = Edges.CreateConstIterator(); Iter; ++Iter)
		{
			++InDegrees[Iter.Value()];
		}

		for (int32 Index = 0; Index < InDegrees.Num(); ++Index)
		{
			if (InDegrees[Index] == 0)
			{
				NoInVertices.Push(Index);
			}
		}

		TArray<int32> Values;
		while (NoInVertices.Num() > 0)
		{
			int32 CurrentIndex = NoInVertices.Pop(false);
			OutSortedList.Push(CurrentIndex);

			Values.Reset();
			Edges.MultiFind(CurrentIndex, Values);

			for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
			{
				int32 NeighborValue = Values[ValueIndex];
				if (--InDegrees[NeighborValue] == 0)
				{
					NoInVertices.Push(NeighborValue);
				}
			}
		}

		if (OutSortedList.Num() != NumVertices)
		{
			// problem with sort
			return false;
		}

		return true;
	}
};
///////////////////////////////////////////////////////////

void FControlRigBlueprintCompilerContext::PostCompile()
{
	UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Blueprint);
	if (ControlRigBlueprint)
	{
		// create sorted graph
		ControlRigBlueprint->Operators.Reset();

		// create list of property link
		// we only care for root property for now
		// @todo: this may have to be done by sub properties - i.e. A.Translation not at A -
		// but that can come later 
		TArray<FName> SourcePropertyArray;
		TArray<FName> DestPropertyArray;

		AddRootPropertyLinks(ControlRigBlueprint->PropertyLinks, SourcePropertyArray, DestPropertyArray);

		check(SourcePropertyArray.Num() == DestPropertyArray.Num());

		if (SourcePropertyArray.Num() > 0)
		{
			TArray<FName> MergedArray;
			int32 ArrayIndex = 0;

			// now merge two array 
			// slow, yep!
			while (ArrayIndex < SourcePropertyArray.Num())
			{
				MergedArray.AddUnique(SourcePropertyArray[ArrayIndex++]);
			}

			ArrayIndex = 0;

			while (ArrayIndex < DestPropertyArray.Num())
			{
				MergedArray.AddUnique(DestPropertyArray[ArrayIndex++]);
			}

			// verify there is no overlaps between
			// dependency graph
			FDependencyGraph Graph(MergedArray.Num());
			for (int32 Index = 0; Index < SourcePropertyArray.Num(); ++Index)
			{
				int32 FromIndex = MergedArray.Find(SourcePropertyArray[Index]);
				int32 ToIndex = MergedArray.Find(DestPropertyArray[Index]);

				Graph.AddEdge(FromIndex, ToIndex);
			}

			TArray<int32> SortedArray;
			TArray<FName> OrderOfProperties;
			if (Graph.TopologicalSort(SortedArray))
			{
				for (int32 SortedIndex = 0; SortedIndex < SortedArray.Num(); ++SortedIndex)
				{
					OrderOfProperties.Add(MergedArray[SortedArray[SortedIndex]]);
				}

				for (int32 Index = 0; Index < OrderOfProperties.Num(); ++Index)
				{
					UE_LOG(LogControlRigCompiler, Log, TEXT("%d. %s"), Index + 1, *OrderOfProperties[Index].ToString());
				}
			}
			else
			{

				MessageLog.Error(*FString::Printf(TEXT("Failed to create DAG. Make sure cycle doesn't exist between nodes. ")));
				return;
			}

			// now we have list of order of properties, 

			// sort property links by dest properties, so that we can figure out order of copies
			TArray<FControlRigBlueprintPropertyLink> OrderedPropertyLinks = ControlRigBlueprint->PropertyLinks;

			struct FComparePropertyLinksByDestination
			{
				const TArray<FName>& OrderOfProperties;
				FComparePropertyLinksByDestination(const TArray<FName>& InOrderOfProperties)
					: OrderOfProperties(InOrderOfProperties)
				{}

				bool operator()(const FControlRigBlueprintPropertyLink& A, const FControlRigBlueprintPropertyLink& B) const
				{
					int32 A_DestIndex = OrderOfProperties.Find(FName(*RetrieveRootName(A.GetDestPropertyPath())));
					int32 B_DestIndex = OrderOfProperties.Find(FName(*RetrieveRootName(B.GetDestPropertyPath())));
					check(A_DestIndex != INDEX_NONE && B_DestIndex != INDEX_NONE);
					return (A_DestIndex < B_DestIndex);
				}
			};

			OrderedPropertyLinks.Sort(FComparePropertyLinksByDestination(OrderOfProperties));

			// debug only
			for (int32 Index = 0; Index < OrderedPropertyLinks.Num(); ++Index)
			{
				UE_LOG(LogControlRigCompiler, Log, TEXT("%d. %s->%s"), Index + 1, *OrderedPropertyLinks[Index].GetSourcePropertyPath(), *OrderedPropertyLinks[Index].GetDestPropertyPath());
			}

			// create copy/run operator
			// ordered property links are set now. Now we have to figure out which properties are rig units
			TArray<FName> RigUnits;
			for (int32 Index = 0; Index < OrderOfProperties.Num(); ++Index)
			{
				const FName& PropertyName = OrderOfProperties[Index];
				const UStructProperty* StructProp = Cast<UStructProperty>(ControlRigBlueprint->GeneratedClass->FindPropertyByName(PropertyName));
				if (StructProp)
				{
					FString CPPType = StructProp->GetCPPType(nullptr, CPPF_None);
					if (StructProp->Struct->IsChildOf(FRigUnit::StaticStruct()))
					{
						RigUnits.Add(PropertyName);
					}
				}
			}

			TArray<FName> ExecutedRigUnits;

			TArray<FName> DestinationRigUnits;
			for (int32 Index = 0; Index < OrderedPropertyLinks.Num(); ++Index)
			{
				FName SourceProperty = FName(*RetrieveRootName(OrderedPropertyLinks[Index].GetSourcePropertyPath()));
				// we start with the idea of source property. When source property is about to be used, we execute them
				// source property is about to be used, but hasn't been executed yet. Should add it now
				if (!ExecutedRigUnits.Contains(SourceProperty))
				{
					// see if the last property was rig unit
					// then insert execution path
					int32 FoundIndex = RigUnits.Find(SourceProperty);
					if (FoundIndex != INDEX_NONE)
					{
						ControlRigBlueprint->Operators.Add(FControlRigOperator(EControlRigOpCode::Exec, SourceProperty.ToString(), TEXT("")));

						checkSlow(!ExecutedRigUnits.Contains(SourceProperty));
						ExecutedRigUnits.Add(SourceProperty);
					}
				}

				// we save all destination units because we want to make sure they're executed even if they're not used
				FName DestProperty = FName(*RetrieveRootName(OrderedPropertyLinks[Index].GetDestPropertyPath()));
				// see if the last property was rig unit
				// then insert execution path
				int32 FoundIndex = RigUnits.Find(DestProperty);
				if (FoundIndex != INDEX_NONE)
				{
					DestinationRigUnits.AddUnique(DestProperty);
				}

				// add copy instruction
				ControlRigBlueprint->Operators.Add(FControlRigOperator(EControlRigOpCode::Copy, OrderedPropertyLinks[Index].GetSourcePropertyPath(), OrderedPropertyLinks[Index].GetDestPropertyPath()));
			}

			// now add all leftover destination rig units, these are units that
			// doens't have target pin, but at the last units.
			for (int32 Index = 0; Index < DestinationRigUnits.Num(); ++Index)
			{
				if (!ExecutedRigUnits.Contains(DestinationRigUnits[Index]))
				{
					ControlRigBlueprint->Operators.Add(FControlRigOperator(EControlRigOpCode::Exec, DestinationRigUnits[Index].ToString(), TEXT("")));
					ExecutedRigUnits.Add(DestinationRigUnits[Index]);
				}
			}

			// make sure all rig units are inserted.
			if (RigUnits.Num() != ExecutedRigUnits.Num())
			{
				// this means there is a broken link
				// warn users 
				for (int32 Index = 0; Index < RigUnits.Num(); ++Index)
				{
					if (!ExecutedRigUnits.Contains(RigUnits[Index]))
					{
						MessageLog.Warning(*FString::Printf(TEXT("%s is not linked. Won't be executed."), *RigUnits[Index].ToString()));
					}
				}
			}
		}

		ControlRigBlueprint->Operators.Add(FControlRigOperator(EControlRigOpCode::Done));

		// update allow source access properties
		{
			TArray<FName> SourcePropertyLinkArray;
			TArray<FName> DestPropertyLinkArray;

			auto GetPartialPath = [](FString Input) -> FString
			{
				const TCHAR SearchChar = TCHAR('.');
				int32 ParseIndex = 0;
				if (Input.FindChar(SearchChar, ParseIndex))
				{
					FString Root = Input.Left(ParseIndex+1);
					FString Child = Input.RightChop(ParseIndex+1);
					int32 SubParseIndex = 0;
					
					if (Child.FindChar(SearchChar, SubParseIndex))
					{
						return (Root + Child.Left(SubParseIndex));
					}
				}

				return Input;
			};

			//@todo: think about using ordered properties at the end
			for (int32 Index = 0; Index < ControlRigBlueprint->PropertyLinks.Num(); ++Index)
			{
				const FControlRigBlueprintPropertyLink& PropertyLink = ControlRigBlueprint->PropertyLinks[Index];
				SourcePropertyLinkArray.Add(FName(*GetPartialPath(PropertyLink.GetSourcePropertyPath())));
				DestPropertyLinkArray.Add(FName(*GetPartialPath(PropertyLink.GetDestPropertyPath())));
			}

			ControlRigBlueprint->AllowSourceAccessProperties.Reset();

			TArray<FName> PropertyList;
			UClass* MyClass = ControlRigBlueprint->GeneratedClass;
			for (TFieldIterator<UProperty> It(MyClass); It; ++It)
			{
				if (UStructProperty* StructProperty = Cast<UStructProperty>(*It))
				{
					for (TFieldIterator<UProperty> It2(StructProperty->Struct); It2; ++It2)
					{
						if ((*It2)->HasMetaData(TEXT("AllowSourceAccess")))
						{
							FString PartialPropertyPath = StructProperty->GetName() + TEXT(".") + (*It2)->GetName();
							PropertyList.Add(FName(*PartialPropertyPath));
						}
					}
				}
			}

			// this is prototype code and really slow
			for (int32 Index = 0; Index < PropertyList.Num(); ++Index)
			{
				// find source brute force
 				const FName& PropertyToSearch = PropertyList[Index];
				int32 DestIndex = DestPropertyLinkArray.Find(PropertyToSearch);
 				FName LastSource;
				if (DestIndex != INDEX_NONE)
				{
					FString& Value = ControlRigBlueprint->AllowSourceAccessProperties.Add(PropertyToSearch);
					Value = SourcePropertyLinkArray[DestIndex].ToString();
				}

			}
		}
	}

	FKismetCompilerContext::PostCompile();

	// We need to copy any pin defaults over to underlying properties once the class is built
	// as the defaults may not have been propagated from new nodes yet
	for(UEdGraph* UbergraphPage : Blueprint->UbergraphPages)
	{
		if(UControlRigGraph* ControlRigGraph = Cast<UControlRigGraph>(UbergraphPage))
		{
			for(UEdGraphNode* Node : ControlRigGraph->Nodes)
			{
				if(UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(Node))
				{
					for(UEdGraphPin* Pin : ControlRigGraphNode->Pins)
					{
						ControlRigGraphNode->CopyPinDefaultsToProperties(Pin, false, false);
					}
				}
			}
		}
	}
}

void FControlRigBlueprintCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	FKismetCompilerContext::CopyTermDefaultsToDefaultObject(DefaultObject);

	UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Blueprint);
	if (ControlRigBlueprint)
	{
		UControlRig* ControlRig = CastChecked<UControlRig>(DefaultObject);
		ControlRig->Operators = ControlRigBlueprint->Operators;
		ControlRig->Hierarchy.BaseHierarchy = ControlRigBlueprint->Hierarchy;
		// copy available rig units info, so that control rig can do things with it
		ControlRig->AllowSourceAccessProperties = ControlRigBlueprint->AllowSourceAccessProperties;

		ControlRig->Initialize();
	}
}

void FControlRigBlueprintCompilerContext::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if( TargetUClass && !((UObject*)TargetUClass)->IsA(UControlRigBlueprintGeneratedClass::StaticClass()) )
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = nullptr;
	}
}

void FControlRigBlueprintCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	NewControlRigBlueprintGeneratedClass = FindObject<UControlRigBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if (NewControlRigBlueprintGeneratedClass == nullptr)
	{
		NewControlRigBlueprintGeneratedClass = NewObject<UControlRigBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer::Create(NewControlRigBlueprintGeneratedClass);
	}
	NewClass = NewControlRigBlueprintGeneratedClass;
}

void FControlRigBlueprintCompilerContext::OnNewClassSet(UBlueprintGeneratedClass* ClassToUse)
{
	NewControlRigBlueprintGeneratedClass = CastChecked<UControlRigBlueprintGeneratedClass>(ClassToUse);
}

void FControlRigBlueprintCompilerContext::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOldCDO)
{
	FKismetCompilerContext::CleanAndSanitizeClass(ClassToClean, InOldCDO);

	// Make sure our typed pointer is set
	check(ClassToClean == NewClass && NewControlRigBlueprintGeneratedClass == NewClass);

	// Reser cached unit properties
	NewControlRigBlueprintGeneratedClass->ControlUnitProperties.Empty();
}