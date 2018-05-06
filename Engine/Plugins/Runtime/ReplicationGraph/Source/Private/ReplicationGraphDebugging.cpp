// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ReplicationGraph.h"
#include "ReplicationGraphTypes.h"

#include "Misc/CoreDelegates.h"
#include "Engine/ActorChannel.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Info.h"
#include "GameFramework/HUD.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectBaseUtility.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "EngineUtils.h"
#include "Engine/NetConnection.h"
#include "Engine/ActorChannel.h"

/**
 *	
 *	===================== Debugging Tools (WIP) =====================
 *	
 *	Net.RepGraph.PrintGraph												Prints replication graph to log (hierarchical representation of graph and its lists)
 *	Net.RepGraph.DrawGraph												Draws replication graph on HUD
 *	
 *	Net.RepGraph.PrintAllActorInfo <MatchString>						Prints global and connection specific info about actors whose pathname contains MatchString. Can be called from client.
 *	
 *	Net.RepGraph.PrioritizedLists.Print	<ConnectionIdx>					Prints prioritized replication list to log 
 *	Net.RepGraph.PrioritizedLists.Draw <ConnectionIdx>					Draws prioritized replication list on HUD
 *	
 *	Net.RepGraph.PrintAll <Frames> <ConnectionIdx> <"Class"/"Num">		Prints the replication graph and prioritized list for given ConnectionIdx for given Frames.
 *	
 *	Net.PacketBudget.HUD												Draws Packet Budget details on HUD
 *	Net.PacketBudget.HUD.Toggle											Toggles capturing/updating the Packet Budget details HUD
 *	
 *	Net.RepGraph.Lists.DisplayDebug										Displays RepActoList stats on HUD
 *	Net.RepGraph.Lists.Stats											Prints RepActorList stats to Log
 *	Net.RepGraph.Lists.Details											Prints extended RepActorList details to log
 *	
 *	Net.RepGraph.StarvedList <ConnectionIdx>							Prints actor starvation stats to HUD
 *	
 */

// ----------------------------------------------------------
//	Console Commands
// ----------------------------------------------------------

UNetConnection* AReplicationGraphDebugActor::GetNetConnection() const
{
	if (ConnectionManager)
	{
		return ConnectionManager->NetConnection;
	}

	if (UNetDriver* Driver = GetNetDriver())
	{
		return Driver->ServerConnection;
	}
	
	return nullptr;
}

// -------------------------------------------------------------

bool AReplicationGraphDebugActor::ServerStartDebugging_Validate()
{
	return true;
}

void AReplicationGraphDebugActor::ServerStartDebugging_Implementation()
{
	UE_LOG(LogReplicationGraph, Display, TEXT("ServerStartDebugging"));
	ConnectionManager->bEnableDebugging = true;

	UReplicationGraphNode_GridSpatialization2D* GridNode =  nullptr;
	for (UReplicationGraphNode* Node : ReplicationGraph->GlobalGraphNodes)
	{
		GridNode = Cast<UReplicationGraphNode_GridSpatialization2D>(Node);
		if (GridNode)
		{
			break;
		}
	}

	if (GridNode == nullptr)
	{
		return;
	}

	int32 TotalNumCells = 0; // How many cells have been allocated
	int32 TotalLeafNodes = 0; // How many cells have leaf nodes allocated

	TSet<AActor*> UniqueActors;
	int32 TotalElementsInLists = 0;

	TMap<int32, int32> NumStreamLevelsMap;

	int32 MaxY = 0;
	for (TArray<UReplicationGraphNode_GridCell*>& GridY : GridNode->Grid)
	{
		for (UReplicationGraphNode_GridCell* LeafNode : GridY)
		{
			TotalNumCells++;
			if (LeafNode)
			{
				TotalLeafNodes++;

				TArray<FActorRepListType> NodeActors;
				LeafNode->GetAllActorsInNode_Debugging(NodeActors);

				TotalElementsInLists += NodeActors.Num();
				UniqueActors.Append(NodeActors);
				
				NumStreamLevelsMap.FindOrAdd(LeafNode->StreamingLevelCollection.NumLevels())++;
			}
		}
		
		MaxY = FMath::Max<int32>(MaxY, GridY.Num());
	}

	UE_LOG(LogReplicationGraph, Display, TEXT("Grid Dimensions: %d x %d (%d)"), GridNode->Grid.Num(), MaxY, GridNode->Grid.Num() * MaxY);
	UE_LOG(LogReplicationGraph, Display, TEXT("Total Num Cells: %d"), TotalNumCells);
	UE_LOG(LogReplicationGraph, Display, TEXT("Total Num Leaf Nodes: %d"), TotalLeafNodes);
	UE_LOG(LogReplicationGraph, Display, TEXT("Total List Elements: %d"), TotalElementsInLists);
	UE_LOG(LogReplicationGraph, Display, TEXT("Total Unique Spatial Actors: %d"), UniqueActors.Num());

	UE_LOG(LogReplicationGraph, Display, TEXT("Stream Levels per grid Frequency Report:"));
	NumStreamLevelsMap.ValueSort(TGreater<int32>());
	for (auto It : NumStreamLevelsMap)
	{
		UE_LOG(LogReplicationGraph, Display, TEXT("%d Levels --> %d"), It.Key, It.Value);
	}
}

FAutoConsoleCommandWithWorldAndArgs NetRepGraphDebugActorStart(TEXT("Net.RepGraph.Debug.Start"),TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TActorIterator<AReplicationGraphDebugActor> It(World); It; ++It)
		{
			It->ServerStartDebugging();
		}
	})
);

// -------------------------------------------------------------

bool AReplicationGraphDebugActor::ServerStopDebugging_Validate()
{
	return true;
}

void AReplicationGraphDebugActor::ServerStopDebugging_Implementation()
{
	
}

// -------------------------------------------------------------

void AReplicationGraphDebugActor::PrintCullDistances()
{
	struct FData
	{
		UClass* Class = nullptr;
		float DistSq;
		int32 Count;
	};

	TArray<FData> DataList;

	for (auto It = ReplicationGraph->GlobalActorReplicationInfoMap.CreateActorMapIterator(); It; ++It)
	{
		AActor* Actor = It.Key();
		FGlobalActorReplicationInfo& Info = It.Value();

		bool bFound = false;
		for (FData& ExistingData : DataList)
		{
			if (ExistingData.Class == Actor->GetClass() && FMath::IsNearlyZero(ExistingData.DistSq - Info.Settings.CullDistanceSquared))
			{
				ExistingData.Count++;
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			continue;
		}

		FData NewData;
		NewData.Class = Actor->GetClass();
		NewData.DistSq = Info.Settings.CullDistanceSquared;
		NewData.Count = 1;
		DataList.Add(NewData);
	}

	DataList.Sort([](const FData& LHS, const FData& RHS) { return LHS.DistSq < RHS.DistSq; });

	for (FData& Data : DataList)
	{
		const UClass* NativeParent = Data.Class;
		while(NativeParent && !NativeParent->IsNative())
		{
			NativeParent = NativeParent->GetSuperClass();
		}


		UE_LOG(LogReplicationGraph, Display, TEXT("%s (%s) [%d] = %.2f"), *GetNameSafe(Data.Class), *GetNameSafe(NativeParent), Data.Count, FMath::Sqrt(Data.DistSq));
	}
}

FAutoConsoleCommandWithWorldAndArgs NetRepGraphPrintCullDistancesCommand(TEXT("Net.RepGraph.PrintCullDistances"),TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TActorIterator<AReplicationGraphDebugActor> It(World); It; ++It)
		{
			It->PrintCullDistances();
		}
	})
);


// -------------------------------------------------------------

void AReplicationGraphDebugActor::ServerPrintAllActorInfo_Implementation(const FString& Str)
{
	PrintAllActorInfo(Str);
}

bool AReplicationGraphDebugActor::ServerPrintAllActorInfo_Validate(const FString& Str)
{
	return true;
}

void AReplicationGraphDebugActor::PrintAllActorInfo(FString MatchString)
{
	auto Matches = [&MatchString](UObject* Obj) { return MatchString.IsEmpty() || Obj->GetPathName().Contains(MatchString); };

	GLog->Logf(TEXT("================================================================"));
	GLog->Logf(TEXT("Printing All Actor Info. Replication Frame: %d. MatchString: %s"), ReplicationGraph->GetReplicationGraphFrame(), *MatchString);
	GLog->Logf(TEXT("================================================================"));


	for (auto ClassRepInfoIt = ReplicationGraph->GlobalActorReplicationInfoMap.CreateClassMapIterator(); ClassRepInfoIt; ++ClassRepInfoIt)
	{
		UClass* Class = CastChecked<UClass>(ClassRepInfoIt.Key().ResolveObjectPtr());
		const FClassReplicationInfo& ClassInfo = ClassRepInfoIt.Value();

		if (!Matches(Class))
		{
			continue;
		}

		UClass* ParentClass = Class;
		while(ParentClass && !ParentClass->IsNative() && ParentClass->GetSuperClass() && ParentClass->GetSuperClass() != AActor::StaticClass())
		{
			ParentClass = ParentClass->GetSuperClass();
		}

		GLog->Logf(TEXT(""));
		GLog->Logf(TEXT("ClassInfo for %s (Native: %s)"), *GetNameSafe(Class), *GetNameSafe(ParentClass));
		GLog->Logf(TEXT("  %s"), *ClassInfo.BuildDebugStringDelta());
	}

	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if ( IsActorValidForReplication(Actor) == false)
		{
			continue;
		}

		if (!Matches(Actor))
		{
			continue;
		}

		if (FGlobalActorReplicationInfo* Info = ReplicationGraph->GlobalActorReplicationInfoMap.Find(Actor))
		{
			GLog->Logf(TEXT(""));
			GLog->Logf(TEXT("GlobalInfo for %s"), *Actor->GetPathName());
			Info->LogDebugString(*GLog);
		}

		if (FConnectionReplicationActorInfo* Info = ConnectionManager->ActorInfoMap.Find(Actor))
		{
			GLog->Logf(TEXT(""));
			GLog->Logf(TEXT("ConnectionInfo for %s"), *Actor->GetPathName());
			Info->LogDebugString(*GLog);
		}
	}
}

FAutoConsoleCommandWithWorldAndArgs NetRepGraphPrintAllActorInfoCmd(TEXT("Net.RepGraph.PrintAllActorInfo"),TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		FString MatchString;
		if (Args.Num() > 0)
		{
			MatchString = Args[0];
		}

		for (TActorIterator<AReplicationGraphDebugActor> It(World); It; ++It)
		{
			It->ServerPrintAllActorInfo(MatchString);
		}
	})
);

// -------------------------------------------------------------

bool AReplicationGraphDebugActor::ServerCellInfo_Validate()
{
	return true;
}

void AReplicationGraphDebugActor::ServerCellInfo_Implementation()
{
	FNetViewer Viewer(GetNetConnection(), 0.f);

	UReplicationGraphNode_GridSpatialization2D* GridNode =  nullptr;
	for (UReplicationGraphNode* Node : ReplicationGraph->GlobalGraphNodes)
	{
		GridNode = Cast<UReplicationGraphNode_GridSpatialization2D>(Node);
		if (GridNode)
		{
			break;
		}
	}

	if (GridNode == nullptr)
	{
		return;
	}

	int32 CellX = FMath::Max<int32>(0, (Viewer.ViewLocation.X - GridNode->SpatialBias.X) / GridNode->CellSize);
	int32 CellY = FMath::Max<int32>(0, (Viewer.ViewLocation.Y - GridNode->SpatialBias.Y) / GridNode->CellSize);

	TArray<AActor*> ActorsInCell;

	FVector CellLocation(GridNode->SpatialBias.X + (((float)(CellX)+0.5f) * GridNode->CellSize), GridNode->SpatialBias.Y + (((float)(CellY)+0.5f) * GridNode->CellSize), Viewer.ViewLocation.Z);
	FVector CellExtent(GridNode->CellSize, GridNode->CellSize, 10.f);

	if (GridNode->Grid.IsValidIndex(CellX))
	{
		TArray<UReplicationGraphNode_GridCell*>& GridY = GridNode->Grid[CellX];
		if (GridY.IsValidIndex(CellY))
		{
			if (UReplicationGraphNode_GridCell* LeafNode = GridY[CellY])
			{
				LeafNode->GetAllActorsInNode_Debugging(ActorsInCell);
			}
		}
	}

	ClientCellInfo(CellLocation, CellExtent, ActorsInCell);
}

void AReplicationGraphDebugActor::ClientCellInfo_Implementation(FVector CellLocation, FVector CellExtent, const TArray<AActor*>& Actors)
{
	DrawDebugBox(GetWorld(), CellLocation, CellExtent, FColor::Blue, true, 10.f);

	int32 NullActors=0;
	for (const AActor* Actor : Actors)
	{
		if (Actor)
		{
			DrawDebugLine(GetWorld(), CellLocation, Actor->GetActorLocation(), FColor::Blue, true, 10.f);
		}
		else
		{
			NullActors++;
		}
	}

	UE_LOG(LogReplicationGraph, Display, TEXT("NullActors: %d"), NullActors);
}

FAutoConsoleCommandWithWorldAndArgs NetRepGraphCellInfo(TEXT("Net.RepGraph.Spatial.CellInfo"),TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TActorIterator<AReplicationGraphDebugActor> It(World); It; ++It)
		{
			It->ServerCellInfo();
		}
	})
);

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

#if !(UE_BUILD_SHIPPING | UE_BUILD_TEST)
FAutoConsoleCommandWithWorldAndArgs NetRepGraphForceRebuild(TEXT("Net.RepGraph.Spatial.ForceRebuild"),TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TObjectIterator<UReplicationGraphNode_GridSpatialization2D> It; It; ++It)
		{
			UReplicationGraphNode_GridSpatialization2D* Node = *It;
			if (Node && Node->HasAnyFlags(RF_ClassDefaultObject) == false)
			{
				Node->ForceRebuild();
				Node->DebugActorNames.Append(Args);
			}
		}
	})
);

FAutoConsoleCommandWithWorldAndArgs NetRepGraphSetCellSize(TEXT("Net.RepGraph.Spatial.SetCellSize"),TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		float NewGridSize = 0.f;
		if (Args.Num() > 0 )
		{
			Lex::FromString(NewGridSize, *Args[0]);
		}

		if (NewGridSize <= 0.f)
		{
			return;
		}

		for (TObjectIterator<UReplicationGraphNode_GridSpatialization2D> It; It; ++It)
		{
			UReplicationGraphNode_GridSpatialization2D* Node = *It;
			if (Node && Node->HasAnyFlags(RF_ClassDefaultObject) == false)
			{
				Node->CellSize = NewGridSize;
				Node->ForceRebuild();
			}
		}
	})
);




// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------



FAutoConsoleCommand RepDriverListsAddTestmd(TEXT("Net.RepGraph.Lists.AddTest"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args) 
{ 
	static FActorRepListRefView List;
	List.PrepareForWrite(true);

	int32 Num = 1;
	if (Args.Num() > 0 )
	{
		Lex::FromString(Num,*Args[0]);
	}
	
	while(Num-- > 0)
	{
		List.Add(nullptr);
	}
}));

FAutoConsoleCommand RepDriverListsStatsCmd(TEXT("Net.RepGraph.Lists.Stats"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args) 
{ 
	int32 Mode = 0;
	if (Args.Num() > 0 )
	{
		Lex::FromString(Mode,*Args[0]);
	}

	PrintRepListStats(Mode);
}));

FAutoConsoleCommand RepDriverListDetailsCmd(TEXT("Net.RepGraph.Lists.Details"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args) 
{ 
	int32 PoolIdx = 0;
	int32 BlockIdx = 0;
	int32 ListIdx = -1;
	
	if (Args.Num() > 0 )
	{
		Lex::FromString(PoolIdx,*Args[0]);
	}

	if (Args.Num() > 1 )
	{
		Lex::FromString(BlockIdx,*Args[1]);
	}

	if (Args.Num() > 2 )
	{
		Lex::FromString(ListIdx,*Args[2]);
	}

	PrintRepListDetails(PoolIdx, BlockIdx, ListIdx);
}));

FAutoConsoleCommand RepDriverListsDisplayDebugCmd(TEXT("Net.RepGraph.Lists.DisplayDebug"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args) 
{ 
	static FDelegateHandle Handle;
	static int32 Mode = 0;
	if (Args.Num() > 0 )
	{
		Lex::FromString(Mode,*Args[0]);
	}

	if (Handle.IsValid())
	{
		FCoreDelegates::OnGetOnScreenMessages.Remove(Handle);
		Handle.Reset();
	}
	else
	{
		Handle = FCoreDelegates::OnGetOnScreenMessages.AddLambda([](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
		{
			FStringOutputDevice Str;
			Str.SetAutoEmitLineTerminator(true);
			PrintRepListStatsAr(Mode, Str);

			TArray<FString> Lines;
			Str.ParseIntoArrayLines(Lines, true);

			for (int32 idx=Lines.Num()-1; idx>=0; --idx)
			{
				OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(Lines[idx]));
			}
		});
	}
}));

#endif

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

FAutoConsoleCommand RepDriverStarvListCmd(TEXT("Net.RepGraph.StarvedList"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args) 
{ 
	static FDelegateHandle Handle;
	static int32 ConnectionIdx = 0;
	if (Args.Num() > 0 )
	{
		Lex::FromString(ConnectionIdx, *Args[0]);
	}
	if (Handle.IsValid())
	{
		FCoreDelegates::OnGetOnScreenMessages.Remove(Handle);
		Handle.Reset();
	}
	else
	{
		Handle = FCoreDelegates::OnGetOnScreenMessages.AddLambda([](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
		{
			UNetDriver* BestNetDriver = nullptr;
			for (TObjectIterator<UNetDriver> It; It; ++It)
			{
				if (It->NetDriverName == NAME_GameNetDriver)
				{
					if (It->ClientConnections.Num() > 0)
					{
						if (UReplicationGraph* RepGraph = Cast<UReplicationGraph>(It->GetReplicationDriver()))
						{
							UNetConnection* Connection = It->ClientConnections[ FMath::Min(ConnectionIdx, It->ClientConnections.Num()-1) ];
						
							for (TObjectIterator<UNetReplicationGraphConnection> ConIt; ConIt; ++ConIt)
							{
								if (ConIt->NetConnection == Connection)
								{
									struct FStarveStruct
									{
										FStarveStruct(AActor* InActor, uint32 InStarvedCount) : Actor(InActor), StarveCount(InStarvedCount) { }
										AActor* Actor = nullptr;
										uint32 StarveCount = 0;
										bool operator<(const FStarveStruct& Other) const { return StarveCount < Other.StarveCount; }
									};
								
									TArray<FStarveStruct> TheList;

									for (auto MapIt = ConIt->ActorInfoMap.CreateIterator(); MapIt; ++MapIt)
									{
										TheList.Emplace(MapIt.Key(), RepGraph->GetReplicationGraphFrame() - MapIt.Value().LastRepFrameNum);
									}
									TheList.Sort();
								
									for (int32 i=TheList.Num()-1; i >= 0 ; --i)
									{
										OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString( FString::Printf(TEXT("[%d] %s"), TheList[i].StarveCount, *GetNameSafe(TheList[i].Actor)) ) );
									}
								}
							}
						}
					}
				}
			}
			

			//
		});
	}
}));

UReplicationGraph* FindReplicationGraphHelper()
{
	UReplicationGraph* Graph = nullptr;
	for (TObjectIterator<UReplicationGraph> It; It; ++It)
	{
		if (It->NetDriver && It->NetDriver->GetNetMode() != NM_Client)
		{
			Graph = *It;
			break;
		}
	}
	return Graph;
}

// --------------------------------------------------------------------------------------------------------------------------------------------
//	Graph Debugging: help log/debug the state of the Replication Graph
// --------------------------------------------------------------------------------------------------------------------------------------------
void LogGraphHelper(FOutputDevice& Ar, const TArray< FString >& Args)
{
	UReplicationGraph* Graph = nullptr;
	for (TObjectIterator<UReplicationGraph> It; It; ++It)
	{
		if (It->NetDriver && It->NetDriver->GetNetMode() != NM_Client)
		{
			Graph = *It;
			break;
		}
	}

	if (!Graph)
	{
		UE_LOG(LogReplicationGraph, Warning, TEXT("Could not find valid Replication Graph."));
		return;
	}

	FReplicationGraphDebugInfo DebugInfo(Ar);
	if (Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("nativeclass")) || Str.Contains(TEXT("nclass")) ; }) )
	{
		DebugInfo.Flags = FReplicationGraphDebugInfo::ShowNativeClasses;
	}
	else if (Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("class")); }) )
	{
		DebugInfo.Flags = FReplicationGraphDebugInfo::ShowClasses;
	}
	else if (Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("num")); }) )
	{
		DebugInfo.Flags = FReplicationGraphDebugInfo::ShowTotalCount;
	}
	else
	{
		DebugInfo.Flags = FReplicationGraphDebugInfo::ShowActors;
	}

	Graph->LogGraph(DebugInfo);
}

FAutoConsoleCommand RepGraphPrintGraph(TEXT("Net.RepGraph.PrintGraph"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args) 
{ 
	LogGraphHelper(*GLog, Args);
	
}));

FAutoConsoleCommand RepGraphDrawGraph(TEXT("Net.RepGraph.DrawGraph"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& InArgs) 
{ 
	static FDelegateHandle Handle;
	static TArray< FString > Args;
	Args = InArgs;

	if (Handle.IsValid())
	{
		FCoreDelegates::OnGetOnScreenMessages.Remove(Handle);
		Handle.Reset();
	}
	else
	{
		Handle = FCoreDelegates::OnGetOnScreenMessages.AddLambda([](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
		{
			FStringOutputDevice Str;
			Str.SetAutoEmitLineTerminator(true);

			LogGraphHelper(Str, Args);

			TArray<FString> Lines;
			Str.ParseIntoArrayLines(Lines, true);

			for (int32 idx=0; idx < Lines.Num(); ++idx)
			{
				OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(Lines[idx]));
			}
		});
	}
}));

// ===========================================================================================================


void FGlobalActorReplicationInfo::LogDebugString(FOutputDevice& Ar) const
{
	Ar.Logf(TEXT("  LastPreReplicationFrame: %d. ForceNetUpdateFrame: %d. WorldLocation: %s. bWantsToBeDormant %d"), LastPreReplicationFrame, ForceNetUpdateFrame, *WorldLocation.ToString(), bWantsToBeDormant);
	Ar.Logf(TEXT("  Settings: %s"), *Settings.BuildDebugStringDelta());

	if (DependentActorList.Num() > 0)
	{
		FString DependentActorStr = TEXT("DependentActors: ");
		for (FActorRepListType Actor : DependentActorList)
		{
			DependentActorStr += GetActorRepListTypeDebugString(Actor) + ' ';
		}

		Ar.Logf(TEXT("  %s"), *DependentActorStr);
	}
}

void FConnectionReplicationActorInfo::LogDebugString(FOutputDevice& Ar) const
{
	Ar.Logf(TEXT("  Channel: %s"), *GetNameSafe(Channel));
	Ar.Logf(TEXT("  CullDistSq: %.2f (%.2f)"), CullDistanceSquared, FMath::Sqrt(CullDistanceSquared));
	Ar.Logf(TEXT("  NextReplicationFrameNum: %d. ReplicationPeriodFrame: %d. LastRepFrameNum: %d. StarvedFrameNum: %d. ActorChannelCloseFrameNum: %d. IsDormantOnConnection: %d. TearOff: %d"), NextReplicationFrameNum, ReplicationPeriodFrame, LastRepFrameNum, StarvedFrameNum, ActorChannelCloseFrameNum, bDormantOnConnection, bTearOff);
}

void UReplicationGraph::LogGraph(FReplicationGraphDebugInfo& DebugInfo) const
{
	for (const UReplicationGraphNode* Node : GlobalGraphNodes)
	{
		Node->LogNode(DebugInfo, Node->GetDebugString());
	}

	for (const UNetReplicationGraphConnection* ConnectionManager: Connections)
	{
		DebugInfo.Log(FString::Printf(TEXT("Connection: %s"), *ConnectionManager->NetConnection->GetPlayerOnlinePlatformName().ToString()));

		DebugInfo.PushIndent();
		for (UReplicationGraphNode* Node : ConnectionManager->ConnectionGraphNodes)
		{
			Node->LogNode(DebugInfo, Node->GetDebugString());
		}
		DebugInfo.PopIndent();
	}
}

void UReplicationGraphNode::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);

	DebugInfo.PushIndent();
	for (const UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		ChildNode->LogNode(DebugInfo, ChildNode->GetDebugString());
	}
	DebugInfo.PopIndent();
}

void LogActorRepList(FReplicationGraphDebugInfo& DebugInfo, FString Prefix, const FActorRepListRefView& List)
{
	if (List.IsValid() == false || List.Num() <= 0)
	{
		return;
	}

	FString ActorListStr = FString::Printf(TEXT("%s [%d Actors] "), *Prefix, List.Num());

	if (DebugInfo.Flags == FReplicationGraphDebugInfo::ShowActors)
	{
		for (FActorRepListType Actor : List)
		{
			ActorListStr += GetActorRepListTypeDebugString(Actor);
			ActorListStr += TEXT(" ");
		}
	}
	else if (DebugInfo.Flags == FReplicationGraphDebugInfo::ShowClasses || DebugInfo.Flags == FReplicationGraphDebugInfo::ShowNativeClasses )
	{
		TMap<UClass*, int32> ClassCount;
		for (FActorRepListType Actor : List)
		{
			UClass* ActorClass = GetActorRepListTypeClass(Actor);
			if (DebugInfo.Flags == FReplicationGraphDebugInfo::ShowNativeClasses)
			{
				while (ActorClass && !ActorClass->HasAllClassFlags(CLASS_Native))
				{
					// We lie: don't show AActor. If its blueprinted from AActor just return the blueprint class.
					if (ActorClass->GetSuperClass() == AActor::StaticClass())
					{
						break;
					}
					ActorClass = ActorClass->GetSuperClass();
				}
			}

			ClassCount.FindOrAdd(ActorClass)++;
		}
		for (auto& MapIt : ClassCount)
		{
			ActorListStr += FString::Printf(TEXT("%s:[%d] "), *GetNameSafe(MapIt.Key), MapIt.Value);
		}
	}
	DebugInfo.Log(ActorListStr);
}

void UReplicationGraphNode_GridCell::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);

	DebugInfo.PushIndent();

	Super::LogNode(DebugInfo, TEXT("Static"));
	if (DynamicNode)
	{
		DynamicNode->LogNode(DebugInfo, TEXT("Dynamic"));
	}
	if (DormancyNode)
	{
		DormancyNode->LogNode(DebugInfo, TEXT("Dormant"));
	}
	DebugInfo.PopIndent();
}

void UReplicationGraphNode_ClassCategories::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);

	DebugInfo.PushIndent();
	for(const FCategoryMapping& Mapping : Categories)
	{
		if (Mapping.Node)
		{
			Mapping.Node->LogNode(DebugInfo, Mapping.Category.GetDebugStringSlow());
		}
	}
	DebugInfo.PopIndent();
}

void UReplicationGraphNode_TearOff_ForConnection::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);
	DebugInfo.PushIndent();
	LogActorRepList(DebugInfo, TEXT("TearOff"), ReplicationActorList);
	DebugInfo.PopIndent();
}

// --------------------------------------------------------------------------------------------------------------------------------------------
//	Prioritization Debugging: help log/debug 
// --------------------------------------------------------------------------------------------------------------------------------------------

void PrintPrioritizedList(FOutputDevice& Ar, UNetReplicationGraphConnection* ConnectionManager, const TArrayView<FPrioritizedRepList>& List)
{
	UReplicationGraph* RepGraph = ConnectionManager->NetConnection->Driver->GetReplicationDriver<UReplicationGraph>();
	uint32 RepFrameNum = RepGraph->GetReplicationGraphFrame();
	for (const FPrioritizedRepList& PrioritizedList : List)
	{
		// Skipped actors
#if REPGRAPH_DETAILS
		Ar.Logf(TEXT("%s [%d Skipped Actors]"), *PrioritizedList.ListCategory.GetDebugStringSlow(), PrioritizedList.Items.Num());

		FNativeClassAccumulator DormantClasses;
		FNativeClassAccumulator CulledClasses;

		for (const FSkippedActorFullDebugDetails& SkippedDetails : *PrioritizedList.SkippedDebugDetails)
		{
			FString SkippedStr;
			if (SkippedDetails.bWasDormant)
			{
				SkippedStr = TEXT("Dormant");
				DormantClasses.Increment(SkippedDetails.Actor->GetClass());
			}
			else if (SkippedDetails.DistanceCulled > 0.f)
			{
				SkippedStr = FString::Printf(TEXT("Dist Culled %.2f"), SkippedDetails.DistanceCulled);
				CulledClasses.Increment(SkippedDetails.Actor->GetClass());
			}
			else if (SkippedDetails.FramesTillNextReplication > 0)
			{
				SkippedStr = FString::Printf(TEXT("Not ready (%d frames left)"), SkippedDetails.FramesTillNextReplication);
			}
			else
			{
				SkippedStr = TEXT("Unknown???");
			}

			Ar.Logf(TEXT("%-40s %s"), *GetActorRepListTypeDebugString(SkippedDetails.Actor), *SkippedStr);
		}

		Ar.Logf(TEXT(" Dormant Classes: %s"), *DormantClasses.BuildString());
		Ar.Logf(TEXT(" Culled Classes: %s"), *CulledClasses.BuildString());

#endif

		// Passed (not skipped) actors
		Ar.Logf(TEXT("%s [%d Passed Actors]"), *PrioritizedList.ListCategory.GetDebugStringSlow(), PrioritizedList.Items.Num());
		for (const FPrioritizedRepList::FItem& Item : PrioritizedList.Items)
		{
			const FConnectionReplicationActorInfo& ActorInfo = ConnectionManager->ActorInfoMap.FindOrAdd(Item.Actor);
			const bool bWasStarved = ActorInfo.StarvedFrameNum > 0;
			FString StarvedString = bWasStarved ? FString::Printf(TEXT(" (Starved %d) "), RepFrameNum - ActorInfo.LastRepFrameNum) : TEXT("");

#if REPGRAPH_DETAILS
			
			if (FPrioritizedActorFullDebugDetails* FullDetails = PrioritizedList.FullDebugDetails.Get()->FindByKey(Item.Actor))
			{
				Ar.Logf(TEXT("%-40s %.4f %s %s"), *GetActorRepListTypeDebugString(Item.Actor), Item.Priority, *FullDetails->BuildString(), *StarvedString);
				continue;
			}
#endif

			// Simplified version without full details
			UClass* Class = Item.Actor->GetClass();
			while (Class && !Class->IsNative())
			{
				Class = Class->GetSuperClass();
			}

			Ar.Logf(TEXT("%-40s %-20s %.4f %s"), *GetActorRepListTypeDebugString(Item.Actor), *GetNameSafe(Class), Item.Priority, *StarvedString);
		}

		Ar.Logf(TEXT(""));
	}
}

TFunction<void()> LogPrioritizedListHelper(FOutputDevice& Ar, const TArray< FString >& Args, bool bAutoUnregister)
{
	static TWeakObjectPtr<UNetReplicationGraphConnection> WeakConnectionManager;
	static FDelegateHandle Handle;

	static TFunction<void()> ResetFunc = []()
	{
		if (Handle.IsValid() && WeakConnectionManager.IsValid())
		{
			WeakConnectionManager->OnPostReplicatePrioritizeLists.Remove(Handle);
		}
	};

	UReplicationGraph* Graph = FindReplicationGraphHelper();
	if (!Graph)
	{
		UE_LOG(LogReplicationGraph, Warning, TEXT("Could not find valid Replication Graph."));
		return ResetFunc;
	}

	static int32 ConnectionIdx = 0;
	if (Args.Num() > 0 ) 
	{
		Lex::FromString(ConnectionIdx, *Args[0]);
	}

	if (Graph->Connections.IsValidIndex(ConnectionIdx) == false)
	{
		UE_LOG(LogReplicationGraph, Warning, TEXT("Invalid ConnectionIdx %d"), ConnectionIdx);
		return ResetFunc;
	}

	// Reset if we already have delegates bound
	ResetFunc();
	
	UNetReplicationGraphConnection* ConnectionManager = Graph->Connections[ConnectionIdx];
	WeakConnectionManager = ConnectionManager;

	DO_REPGRAPH_DETAILS(ConnectionManager->bEnableFullActorPrioritizationDetails = true);
	Handle = ConnectionManager->OnPostReplicatePrioritizeLists.AddLambda([&Ar, bAutoUnregister](UNetReplicationGraphConnection* InConnectionManager, TArrayView<FPrioritizedRepList> List)
	{
		PrintPrioritizedList(Ar, InConnectionManager, List);
		if (bAutoUnregister)
		{
			DO_REPGRAPH_DETAILS(InConnectionManager->bEnableFullActorPrioritizationDetails = false);
			InConnectionManager->OnPostReplicatePrioritizeLists.Remove(Handle);
		}
	});

	return ResetFunc;
}

FAutoConsoleCommand RepGraphPrintPrioritizedList(TEXT("Net.RepGraph.PrioritizedLists.Print"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args) 
{
	LogPrioritizedListHelper(*GLog, Args, true);
	
}));

FAutoConsoleCommand RepGraphDrawPrioritizedList(TEXT("Net.RepGraph.PrioritizedLists.Draw"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& InArgs) 
{
	static FDelegateHandle Handle;
	static TArray< FString > Args;
	static FStringOutputDevice Str;
	
	Args = InArgs;
	Str.SetAutoEmitLineTerminator(true);

	const bool bClear = InArgs.ContainsByPredicate([](const FString& InStr) { return InStr.Contains(TEXT("clear")); });

	if (Handle.IsValid())
	{
		FCoreDelegates::OnGetOnScreenMessages.Remove(Handle);
		Handle.Reset();
		return;
	}

	if (Handle.IsValid() == false)
	{
		Str.Reset();
		LogPrioritizedListHelper(Str, Args, true);

		Handle = FCoreDelegates::OnGetOnScreenMessages.AddLambda([](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
		{
			TArray<FString> Lines;
			Str.ParseIntoArrayLines(Lines, true);

			for (int32 idx=0; idx < Lines.Num(); ++idx)
			{
				OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(Lines[idx]));
			}
		});
	}
}));

// --------------------------------------------------------------------------------------------------------------------------------------------
//	Print/Logging for everything (Replication Graph, Prioritized List, Packet Budget [TODO])
// --------------------------------------------------------------------------------------------------------------------------------------------

FAutoConsoleCommand RepGraphPrintAllCmd(TEXT("Net.RepGraph.PrintAll"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& InArgs) 
{
	static TWeakObjectPtr<UNetReplicationGraphConnection> WeakConnectionManager;
	static TArray< FString > Args;

	Args = InArgs;

	UReplicationGraph* Graph = FindReplicationGraphHelper();
	if (!Graph)
	{
		UE_LOG(LogReplicationGraph, Warning, TEXT("Could not find valid Replication Graph."));
		return;
	}

	int32 FrameCount = 1;
	if (Args.Num() > 0 )
	{
		Lex::FromString(FrameCount, *Args[0]);
	}

	int32 ConnectionIdx = 0;
	if (Args.Num() > 1 )
	{
		Lex::FromString(ConnectionIdx, *Args[1]);
	}

	if (Graph->Connections.IsValidIndex(ConnectionIdx) == false)
	{
		UE_LOG(LogReplicationGraph, Warning, TEXT("Invalid ConnectionIdx %d"), ConnectionIdx);
		return;
	}
	UNetReplicationGraphConnection* ConnectionManager = Graph->Connections[ConnectionIdx];

	TSharedPtr<FDelegateHandle> Handle = MakeShareable<FDelegateHandle>(new FDelegateHandle());
	TSharedPtr<int32> FrameCountPtr = MakeShareable<int32>(new int32);
	*FrameCountPtr = FrameCount;

	DO_REPGRAPH_DETAILS(ConnectionManager->bEnableFullActorPrioritizationDetails = true);
	*Handle = ConnectionManager->OnPostReplicatePrioritizeLists.AddLambda([Handle, FrameCountPtr, Graph](UNetReplicationGraphConnection* InConnectionManager, TArrayView<FPrioritizedRepList> List)
	{
		GLog->Logf(TEXT(""));
		GLog->Logf(TEXT("===================================================="));
		GLog->Logf(TEXT("Replication Frame %d"), Graph->GetReplicationGraphFrame());
		GLog->Logf(TEXT("===================================================="));

		LogGraphHelper(*GLog, Args);

		PrintPrioritizedList(*GLog, InConnectionManager, List);
		if (*FrameCountPtr >= 0)
		{
			if (--*FrameCountPtr <= 0)
			{
				DO_REPGRAPH_DETAILS(InConnectionManager->bEnableFullActorPrioritizationDetails = false);
				InConnectionManager->OnPostReplicatePrioritizeLists.Remove(*Handle);
			}
		}
	});
	
}));


// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------


TWeakObjectPtr<UNetReplicationGraphConnection> PacketBudgetHUDNetConnection;

FPacketBudgetRecordBuffer DebugPacketBudgetBuffer;
FPacketBudgetRecordBuffer** CurrentDebugPacketBudgetBufferPtr = nullptr;

static void PacketBudgetOnHUDPostRender(AHUD* HUD, UCanvas* Canvas)
{
	if (PacketBudgetHUDNetConnection.IsValid() == false)
	{
		DebugPacketBudgetBuffer.Reset();
		CurrentDebugPacketBudgetBufferPtr = nullptr;
		return;
	}

	static float StartX = 100.f;
	static float StartYOffset = -100.f;

	static float BudgetWidth = 100.f;
	static float BudgetHeightScale = 0.05f;

	static float SpacingX = 10.f;
	static float SpacingY = 5.f;

	float CurrentX = StartX;
	float StartY = Canvas->SizeY + StartYOffset;

	for (int32 idx = DebugPacketBudgetBuffer.Num()-1; idx >= 0; --idx)
	{
		FPacketBudgetRecord& Record = DebugPacketBudgetBuffer.GetAtIndex(idx);
		check(Record.Budget);

		float CurrentY = StartY;

		Canvas->SetDrawColor(FColor::White);
		CurrentY -= Canvas->DrawText(GEngine->GetTinyFont(), Record.Budget->DebugName, CurrentX, CurrentY);
		CurrentY -= SpacingY;

		float BarStartY = CurrentY;

		// -----------------------------------
		// Draw budget
		// -----------------------------------
		static bool bDrawBudget = true;
		if (bDrawBudget)
		{
			static float BudgetOverdraw = 10.f;

			int64 BudgetTotalSize = 0;
			for (const FPacketBudget::FItem& BudgetItem : Record.Budget->BudgetItems)
			{
				BudgetTotalSize += BudgetItem.MaxBits;

				float Height = ((float)BudgetItem.MaxBits * BudgetHeightScale);
				float LineY = CurrentY - Height;
				Canvas->K2_DrawLine(FVector2D(CurrentX, LineY), FVector2D(CurrentX + BudgetWidth + BudgetOverdraw, LineY), 1.f, FColor::White);
				
				CurrentY -= Height;
			}
		}

		CurrentY = BarStartY;

		// -----------------------------------
		// Draw packet
		// -----------------------------------
		
		for (int32 ItemIdx=0; ItemIdx < Record.Items.Num(); ++ItemIdx)
		{
			FPacketBudgetRecord::FItem Item = Record.Items[ItemIdx];
		
			if (Item.BitsWritten <= 0)
			{
				continue;
			}

			float Height = (float)Item.BitsWritten * BudgetHeightScale;

			Canvas->K2_DrawBox( FVector2D(CurrentX, CurrentY-Height), FVector2D(BudgetWidth, Height), 1.f, FColor::Red );
			Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("%s"), *Record.Budget->BudgetItems[ItemIdx].ListCategory.GetDebugStringSlow()), CurrentX, CurrentY - (Height / 2.f));
			CurrentY -= Height;
		}

		CurrentX += BudgetWidth + SpacingX;
	}

}

static void NetPacketBudgetHUDFunc(const TArray<FString>& Args, UWorld* World)
{
	static FDelegateHandle HUDDelegateHandle;
	if (HUDDelegateHandle.IsValid())
	{
		AHUD::OnShowDebugInfo.Remove(HUDDelegateHandle);
		HUDDelegateHandle.Reset();
	}

	UNetDriver* NetDriver = World->GetNetDriver();
	int32 ConnectionIdx = 0;

	// Force examine server for PIE
	if (Args.FindByPredicate([](const FString& Str){ return Str.Contains(TEXT("SERVER")); }))
	{
		for (TObjectIterator<UWorld> It; It; ++It)
		{		
			UWorld* FoundWorld = *It;
			if (FoundWorld && (FoundWorld->GetNetMode() == ENetMode::NM_DedicatedServer || FoundWorld->GetNetMode() == ENetMode::NM_ListenServer))
			{
				NetDriver = FoundWorld->GetNetDriver();
				break;
			}
		}

		for (const FString& Str : Args)
		{
			Lex::TryParseString<int32>(ConnectionIdx, *Str);
		}
	}

	// Stop recording previous run
	if (CurrentDebugPacketBudgetBufferPtr)
	{
		*CurrentDebugPacketBudgetBufferPtr = nullptr;
		CurrentDebugPacketBudgetBufferPtr = nullptr;
		DebugPacketBudgetBuffer.Reset();
	}

	UNetConnection* NetConnection = nullptr;

	if (NetDriver)
	{
		if (NetDriver->ServerConnection)
		{
			NetConnection = NetDriver->ServerConnection;
		}
		else if (NetDriver->ClientConnections.IsValidIndex(ConnectionIdx))
		{
			NetConnection = NetDriver->ClientConnections[ConnectionIdx];
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("Could Not find a valid connection for %d."), ConnectionIdx);
		}


		if (NetConnection)
		{			
			for (TObjectIterator<UNetReplicationGraphConnection> It; It; ++It)
			{
				if( It->NetConnection == NetConnection)
				{
					CurrentDebugPacketBudgetBufferPtr = &It->PacketRecordBuffer;
					It->PacketRecordBuffer = &DebugPacketBudgetBuffer;
					PacketBudgetHUDNetConnection = *It;
					break;
				}
			}

			if (CurrentDebugPacketBudgetBufferPtr)
			{
				HUDDelegateHandle = AHUD::OnHUDPostRender.AddStatic(&PacketBudgetOnHUDPostRender);
			}
			else
			{
				UE_LOG(LogNet, Warning, TEXT("Could Not find a valid ConnectionManager for %d."), ConnectionIdx);
			}
		}
	}
}

FAutoConsoleCommandWithWorldAndArgs NetPacketBudgetHUDCmd(
	TEXT("Net.PacketBudget.HUD"),
	TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NetPacketBudgetHUDFunc)
);

static void NetPacketBudgetHUDToggleFunc(const TArray<FString>& Args, UWorld* World)
{
	if (CurrentDebugPacketBudgetBufferPtr)
	{
		if (*CurrentDebugPacketBudgetBufferPtr)
		{
			*CurrentDebugPacketBudgetBufferPtr = nullptr;
		}
		else
		{
			*CurrentDebugPacketBudgetBufferPtr = &DebugPacketBudgetBuffer;
		}
	}

}

FAutoConsoleCommandWithWorldAndArgs NetPacketBudgetHUDToggleCmd(
	TEXT("Net.PacketBudget.HUD.Toggle"),
	TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NetPacketBudgetHUDToggleFunc)
);

// ------------------------------------------------------------------------------


#if USE_REPCSVPROFILER
void FReplicationGraphProfiler::OnClientConnect()
{
	if (bEnabled && !bStarted)
	{
		bStarted = true;
		FCsvProfiler::Get()->BeginCapture();
		GEngine->Exec(nullptr, TEXT("stat startfile"));
		StartTime = FPlatformTime::Seconds();

	}
}

void FReplicationGraphProfiler::End()
{
	if (bStarted)
	{
		bStarted = false;
		GEngine->Exec(nullptr, TEXT("stat stopfile"));
		FCsvProfiler::Get()->EndCapture();
	}
}

void FReplicationGraphProfiler::StartRepFrame()
{

}

void FReplicationGraphProfiler::EndRepFrame()
{
	if (bStarted)
	{
		double DeltaTime = FPlatformTime::Seconds() - StartTime;
		if (DeltaTime > TimeLimit)
		{
			End();
			KillFrame = 60;
		}
	}

	if (KillFrame > 0 && --KillFrame == 0)
	{
		GLog->PanicFlushThreadedLogs();
		FPlatformMisc::RequestExit(1);
	}
}
#endif