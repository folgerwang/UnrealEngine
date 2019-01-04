// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDContactGraph.h"

#include "ProfilingDebugging/ScopedTimers.h"
#include "ChaosStats.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/PBDRigidParticles.h"
#include "Containers/Queue.h"
#include "Chaos/PBDRigidParticles.h"

using namespace Chaos;

template<class FRigidBodyContactConstraint, class T, int d>
TPBDContactGraph<FRigidBodyContactConstraint, T, d>::TPBDContactGraph(TPBDRigidParticles<T, d>& InParticles)
{
	Initialize(InParticles.Size());
}

template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::Initialize(int32 Size)
{
	MNodes.SetNum(Size);
	PhysicsParallelFor(Size, [&](int32 BodyIndex) {
		MNodes[BodyIndex].BodyIndex = BodyIndex;
		MNodes[BodyIndex].Island = -1;
		MNodes[BodyIndex].NextColor = 0;
	});
}

template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::Reset(const TPBDRigidParticles<T, d>& InParticles, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	// @todo(mlentine): Investigate if it's faster to update instead of recompute; seems unlikely
	double Time = 0;
	FDurationTimer Timer(Time);
	MEdges.Reset();

	for (IslandData& Island : MIslandData)
	{
		Island.IslandConstraints.Reset();	//@todo(ocohen): Should we reset more than just the constraints? Color and Level are recomputed outside of this, what about IsIslandPersistant?
	}

	MNodes.Reset();
	Initialize(InParticles.Size());
	ComputeGraph(InParticles, Constraints);

	for (int32 BodyIdx = 0, NumBodies = InParticles.Size(); BodyIdx < NumBodies; ++BodyIdx)	//@todo(ocohen): could go wide per island if we can get at the sets
	{
		const int32 Island = InParticles.Island(BodyIdx);
		if (Island >= 0)
		{
			GraphNode& Node = MNodes[BodyIdx];
			Node.Island = Island;
			for (int32 Edge : Node.Edges)
			{
				MIslandData[Island].IslandConstraints.Add(Edge);
			}
		}
	}

	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("\tPBDContactGraphIslands Update Graph %f"), Time);
}

template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ComputeGraph(const TPBDRigidParticles<T, d>& InParticles, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	double Time = 0;
	FDurationTimer Timer(Time);
	MEdges.Reserve(MEdges.Num() + Constraints.Num());
	for (const auto& Constraint : Constraints)
	{
		GraphEdge<FRigidBodyContactConstraint> NewEdge;
		NewEdge.FirstNode = &MNodes[Constraint.ParticleIndex];
		NewEdge.SecondNode = &MNodes[Constraint.LevelsetIndex];
		NewEdge.Data = Constraint;
		NewEdge.FirstNode->Edges.Add(MEdges.Num());
		NewEdge.SecondNode->Edges.Add(MEdges.Num());
		NewEdge.Color = -1;
		NewEdge.FirstNode->BodyIndex = Constraint.ParticleIndex;
		NewEdge.SecondNode->BodyIndex = Constraint.LevelsetIndex;
#ifdef USE_CONTACT_LEVELS
		NewEdge.Level = -1;
#endif

		MEdges.Add(MoveTemp(NewEdge));
	}
	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("\tPBDContactGraph Construct Graph from Constraints %f"), Time);
}

template<class FRigidBodyContactConstraint, class T, int d>
TArray<typename TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ContactMap>&
TPBDContactGraph<FRigidBodyContactConstraint, T, d>::GetContactMapAt(int32 Index)
{
	if (Index < MIslandData.Num())
	{
		return MIslandData[Index].LevelToColorToContactMap;
	}
	return EmptyContactMapArray;
}

template<class FRigidBodyContactConstraint, class T, int d>
const TArray<typename TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ContactMap>&
TPBDContactGraph<FRigidBodyContactConstraint, T, d>::GetContactMapAt(int32 Index) const
{
	if (Index < MIslandData.Num())
	{
		return MIslandData[Index].LevelToColorToContactMap;
	}
	return EmptyContactMapArray;
}

template<class FRigidBodyContactConstraint, class T, int d>
int TPBDContactGraph<FRigidBodyContactConstraint, T, d>::GetMaxColorAt(int32 Index) const
{
	if (Index < MIslandData.Num())
	{
		return MIslandData[Index].MaxColor;
	}
	return -1;
}

template<class FRigidBodyContactConstraint, class T, int d>
int TPBDContactGraph<FRigidBodyContactConstraint, T, d>::GetMaxLevelAt(int32 Index) const
{
	if (Index < MIslandData.Num())
	{
#ifdef USE_CONTACT_LEVELS
		return MIslandData[Index].MaxLevel;
#else
		return 0;
#endif
	}
	return -1;
}

template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TArray<int32>& IslandSleepCounts,
    TSet<int32>& ActiveIndices, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	for (uint32 i = 0; i < InParticles.Size(); ++i)
	{
		InParticles.Island(i) = INDEX_NONE;
	}
	ComputeIslands(InParticles, IslandParticles, IslandSleepCounts, ActiveIndices, Constraints);
}

DECLARE_CYCLE_STAT(TEXT("IslandGeneration"), STAT_IslandGeneration, STATGROUP_Chaos);

template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ComputeIslands(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TArray<int32>& IslandSleepCounts,
    TSet<int32>& ActiveIndices, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	SCOPE_CYCLE_COUNTER(STAT_IslandGeneration);

	int32 NextIsland = 0;
	TArray<TSet<int32>> NewIslandParticles;
	TArray<int32> NewIslandSleepCounts;
	for (uint32 i = 0; i < InParticles.Size(); ++i)
	{
		if (MNodes[i].Island >= 0 || InParticles.InvM(i) == 0 || InParticles.Disabled(i))
		{
			continue;
		}
		TSet<int32> SingleIslandParticles;
		TSet<int32> SingleIslandParticleSet;
		ComputeIsland(InParticles, &MNodes[i], NextIsland, SingleIslandParticles, SingleIslandParticleSet, Constraints);
		for (const auto& StaticParticle : SingleIslandParticleSet)
		{
			SingleIslandParticles.Add(StaticParticle);
		}
		if (SingleIslandParticles.Num())
		{
			NewIslandParticles.SetNum(NextIsland + 1);
			NewIslandParticles[NextIsland] = MoveTemp(SingleIslandParticles);
			NextIsland++;
		}
	}
	MIslandData.SetNum(NextIsland);
	for (int32 i = 0; i < MEdges.Num(); ++i)
	{
		const auto& Edge = MEdges[i];
		check(Edge.FirstNode->Island == Edge.SecondNode->Island || Edge.FirstNode->Island == -1 || Edge.SecondNode->Island == -1);
		int32 Island = Edge.FirstNode->Island;
		if (Edge.FirstNode->Island == -1)
		{
			Island = Edge.SecondNode->Island;
		}
		check(Island >= 0);

#ifdef USE_CONTACT_LEVELS
		MIslandData[Island].IslandConstraints.Add(i);
#endif
	}
	NewIslandSleepCounts.SetNum(NewIslandParticles.Num());
	if (NewIslandParticles.Num())
	{
		for (int32 Island = 0; Island < NewIslandParticles.Num(); ++Island)
		{
			NewIslandSleepCounts[Island] = 0;
			for (const auto& Index : NewIslandParticles[Island])
			{
				if (InParticles.InvM(Index))
				{
					InParticles.Island(Index) = Island;
				}
				else
				{
					InParticles.Island(Index) = INDEX_NONE;
				}
			}
		}
		// Force consistent state if no previous islands
		if (!IslandParticles.Num())
		{
			for (int32 Island = 0; Island < NewIslandParticles.Num(); ++Island)
			{
				MIslandData[Island].IsIslandPersistant = true;
				bool SleepState = true;
				for (const auto& Index : NewIslandParticles[Island])
				{
					if (!InParticles.Sleeping(Index))
					{
						SleepState = false;
						break;
					}
				}
				for (const auto& Index : NewIslandParticles[Island])
				{
					//@todo(DEMO_HACK) : Need to fix, remove the !InParticles.Disabled(Index)
					if (InParticles.Sleeping(Index) && !SleepState && !InParticles.Disabled(Index))
					{
						ActiveIndices.Add(Index);
					}
					if (!InParticles.Sleeping(Index) && SleepState && InParticles.InvM(Index))
					{
						ActiveIndices.Remove(Index);
						InParticles.V(Index) = TVector<T, d>(0);
						InParticles.W(Index) = TVector<T, d>(0);
					}
					if (InParticles.InvM(Index))
					{
						InParticles.SetSleeping(Index, SleepState);
					}
					if ((InParticles.Sleeping(Index) || InParticles.Disabled(Index)) && ActiveIndices.Contains(Index))
					{
						ActiveIndices.Remove(Index);
					}
				}
			}
		}
		for (int32 Island = 0; Island < IslandParticles.Num(); ++Island)
		{
			bool bIsSameIsland = true;
			// Objects were removed from the island
			int32 OtherIsland = -1;
			for (const auto Index : IslandParticles[Island])
			{
				int32 TmpIsland = InParticles.Island(Index);

				if (OtherIsland == INDEX_NONE && TmpIsland >= 0)
				{
					OtherIsland = TmpIsland;
				}
				else
				{
					if (TmpIsland >= 0 && OtherIsland != TmpIsland)
					{
						bIsSameIsland = false;
						break;
					}
				}
			}
			// A new object entered the island or the island is entirely new particles
			if (bIsSameIsland && (OtherIsland == INDEX_NONE || NewIslandParticles[OtherIsland].Num() != IslandParticles[Island].Num()))
			{
				bIsSameIsland = false;
			}
			// Find out if we need to activate island
			if (bIsSameIsland)
			{
				NewIslandSleepCounts[OtherIsland] = IslandSleepCounts[Island];
			}
			else
			{
				for (const auto Index : IslandParticles[Island])
				{
					if (!InParticles.Disabled(Index))
					{
						InParticles.SetSleeping(Index, false);
						ActiveIndices.Add(Index);
					}
				}
			}

			// #BG Necessary? Should we ever not find an island?
			if(OtherIsland != INDEX_NONE)
			{
				MIslandData[OtherIsland].IsIslandPersistant = bIsSameIsland;
			}
		}

		//if any particles are awake, make sure the entire island is awake
#if 0
		for (int32 Island = 0; Island < NewIslandParticles.Num(); ++Island)
		{
			int32 OtherIsland = -1;
			for (const auto Index : NewIslandParticles[Island])
			{
				if (!InParticles.Sleeping(Index) && !InParticles.InvM(Index))
				{
					for (const auto Index : NewIslandParticles[Island])
					{
						if (!InParticles.InvM(Index))
						{
							InParticles.Sleeping(Index) = false;
							ActiveIndices.Add(Index);
						}
					}
					MIslandData[Island].IsIslandPersistant = false;
					break;
				}
			}
		}
#endif
	}
	IslandParticles = MoveTemp(NewIslandParticles);
	IslandSleepCounts = MoveTemp(NewIslandSleepCounts);
}

template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ComputeIsland(const TPBDRigidParticles<T, d>& InParticles, GraphNode* InNode, const int32 Island,
    TSet<int32>& DynamicParticlesInIsland, TSet<int32>& StaticParticlesInIsland, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	TQueue<GraphNode*> NodeQueue;
	NodeQueue.Enqueue(InNode);
	while (!NodeQueue.IsEmpty())
	{
		GraphNode* Node;
		NodeQueue.Dequeue(Node);
		if (Node->Island >= 0)
		{
			check(Node->Island == Island);
			continue;
		}
		if (!InParticles.InvM(Node->BodyIndex))
		{
			if (!StaticParticlesInIsland.Contains(Node->BodyIndex))
			{
				StaticParticlesInIsland.Add(Node->BodyIndex);
			}
			continue;
		}
		DynamicParticlesInIsland.Add(Node->BodyIndex);
		Node->Island = Island;
		for (auto EdgeIndex : Node->Edges)
		{
			const auto& Edge = MEdges[EdgeIndex];
			GraphNode* OtherNode = nullptr;
			if (Edge.FirstNode == Node)
			{
				OtherNode = Edge.SecondNode;
			}
			if (Edge.SecondNode == Node)
			{
				OtherNode = Edge.FirstNode;
			}
			check(OtherNode != nullptr);
			NodeQueue.Enqueue(OtherNode);
		}
	}
}

template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ComputeGraphColoring(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& NodeIndices,
    int32& MaxColor, TArray<TMap<int32, TArray<FRigidBodyContactConstraint>>>& LevelToColorToContactMap)
{
	MaxColor = -1;
	TSet<GraphNode*> ProcessedNodes;
	TArray<GraphNode*> NodesToProcess;
	for (const auto& NodeIndex : NodeIndices)
	{
		if (ProcessedNodes.Contains(&MNodes[NodeIndex]) || !InParticles.InvM(MNodes[NodeIndex].BodyIndex))
		{
			continue;
		}
		NodesToProcess.Add(&MNodes[NodeIndex]);
		while (NodesToProcess.Num())
		{
			const auto Node = NodesToProcess.Last();
			NodesToProcess.SetNum(NodesToProcess.Num() - 1, /*bAllowShrinking=*/false);
			ProcessedNodes.Add(Node);
			for (const auto& EdgeIndex : Node->Edges)
			{
				auto& Edge = MEdges[EdgeIndex];
				// If edge has been colored skip it
				if (Edge.Color >= 0)
				{
					continue;
				}
				// Find next usable color
				while (Node->UsedColors.Contains(Node->NextColor))
				{
					Node->NextColor++;
				}
				// Assign color
				if (Node->NextColor > MaxColor)
				{
					MaxColor = Node->NextColor;
				}
				Edge.Color = Node->NextColor;
				if (InParticles.InvM(Node->BodyIndex))
				{
					Node->NextColor++;
				}
#ifdef USE_CONTACT_LEVELS
				int32 Level = Edge.Level;
#else
				int32 Level = 0;
#endif
				if (!LevelToColorToContactMap[Level].Contains(Edge.Color))
				{
					LevelToColorToContactMap[Level].Add(Edge.Color, {});
				}
				LevelToColorToContactMap[Level][Edge.Color].Add(Edge.Data);
				// Mark Other Node as not allowing use of this color
				GraphNode* OtherNode = nullptr;
				if (Edge.FirstNode == Node)
				{
					OtherNode = Edge.SecondNode;
				}
				if (Edge.SecondNode == Node)
				{
					OtherNode = Edge.FirstNode;
				}
				check(OtherNode != nullptr);
				if (InParticles.InvM(OtherNode->BodyIndex))
				{
					OtherNode->UsedColors.Add(Edge.Color);
				}
				// Add other node as needs processing
				if (!ProcessedNodes.Contains(OtherNode) && InParticles.InvM(OtherNode->BodyIndex))
				{
					check(OtherNode->Island == Node->Island);
					checkSlow(NodeIndices.Find(OtherNode->BodyIndex) != INDEX_NONE);
					NodesToProcess.Add(OtherNode);
				}
			}
		}
	}
}

template<class FRigidBodyContactConstraint, class T, int d>
bool TPBDContactGraph<FRigidBodyContactConstraint, T, d>::SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, int32& IslandSleepCount, const int32 Island, const T LinearSleepThreshold, const T AngularSleepThreshold) const
{
	check(ActiveIndices.Num());
	if (!MIslandData[Island].IsIslandPersistant)
	{
		return false;
	}
	TVector<T, d> X(0);
	TVector<T, d> V(0);
	TVector<T, d> W(0);
	T M = 0;
	for (const auto& Index : ActiveIndices)
	{
		if (!InParticles.InvM(Index))
		{
			continue;
		}
		X += InParticles.X(Index);
		M += InParticles.M(Index);
		V += InParticles.V(Index);
	}
	X /= M;
	V /= M;
	for (const auto& Index : ActiveIndices)
	{
		if (!InParticles.InvM(Index))
		{
			continue;
		}
		W += TVector<T, d>::CrossProduct(InParticles.X(Index) - X, InParticles.M(Index) * InParticles.V(Index)) + InParticles.W(Index);
	}
	W /= M;
	const T VSize = V.SizeSquared();
	const T WSize = W.SizeSquared();
	if (VSize < LinearSleepThreshold /*&& WSize < AngularSleepThreshold*/)
	{
		if (IslandSleepCount > 5)
		{
			for (const auto& Index : ActiveIndices)
			{
				if (!InParticles.InvM(Index))
				{
					continue;
				}
				InParticles.SetSleeping(Index, true);
				InParticles.V(Index) = TVector<T, d>(0);
				InParticles.W(Index) = TVector<T, d>(0);
			}
			return true;
		}
		else
		{
			IslandSleepCount++;
		}
	}
	return false;
}

#ifdef USE_CONTACT_LEVELS
template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ComputeContactGraph(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32> NodeIndices, const TSet<int32> EdgeIndices, int32& MaxLevel, TArray<TMap<int32, TArray<FRigidBodyContactConstraint>>>& LevelToColorToContactMap)
{
	MaxLevel = EdgeIndices.Num() ? 0 : -1;
	std::queue<std::pair<int32, GraphNode*>> QueueToProcess;
	for (const auto& Index : NodeIndices)
	{
		if (!InParticles.InvM(Index))
		{
			QueueToProcess.push(std::make_pair(0, &MNodes[Index]));
		}
	}
	while (!QueueToProcess.empty())
	{
		auto Elem = QueueToProcess.front();
		QueueToProcess.pop();
		for (auto EdgeIndex : Elem.second->Edges)
		{
			auto& Edge = MEdges[EdgeIndex];
			if (!EdgeIndices.Contains(EdgeIndex))
			{
				continue;
			}
			if (Edge.Level >= 0)
			{
				continue;
			}
			Edge.Level = Elem.first;
			// Update max level
			MaxLevel = FGenericPlatformMath::Max(MaxLevel, Edge.Level);
			// Find adjacent node and recurse
			GraphNode* OtherNode = nullptr;
			if (Edge.FirstNode == Elem.second)
			{
				OtherNode = Edge.SecondNode;
			}
			if (Edge.SecondNode == Elem.second)
			{
				OtherNode = Edge.FirstNode;
			}
			check(OtherNode != nullptr);
			QueueToProcess.push(std::make_pair(Edge.Level + 1, OtherNode));
		}
	}
	for (const auto& EdgeIndex : EdgeIndices)
	{
		check(MEdges[EdgeIndex].Level <= MaxLevel);
		if (MEdges[EdgeIndex].Level < 0)
		{
			MEdges[EdgeIndex].Level = 0;
		}
	}
	check(MaxLevel >= 0 || !EdgeIndices.Num());
	LevelToColorToContactMap.SetNum(MaxLevel + 1);
}
#endif

template<class FRigidBodyContactConstraint, class T, int d>
void TPBDContactGraph<FRigidBodyContactConstraint, T, d>::UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island)
{
#ifdef USE_CONTACT_LEVELS
	auto& LevelToColorToContactMap = MIslandData[Island].LevelToColorToContactMap;
	LevelToColorToContactMap.Empty();

	auto& IslandConstraints = MIslandData[Island].IslandConstraints;
	auto& MaxLevel = MIslandData[Island].MaxLevel;
	ComputeContactGraph(InParticles, ActiveIndices, IslandConstraints, MaxLevel, LevelToColorToContactMap);
	auto& MaxColor = MIslandData[Island].MaxColor;
	ComputeGraphColoring(InParticles, ActiveIndices, MaxColor, LevelToColorToContactMap);
#else
	auto& LevelToColorToContactMap = MIslandData[Island].LevelToColorToContactMap;
	LevelToColorToContactMap.Empty();

	LevelToColorToContactMap.SetNum(1);
	auto& MaxColor = MIslandData[Island].MaxColor;
	ComputeGraphColoring(InParticles, ActiveIndices, MaxColor, LevelToColorToContactMap);
#endif
}

template class Chaos::TPBDContactGraph<TRigidBodyContactConstraint<float, 3>, float, 3>;
template class Chaos::TPBDContactGraph<TRigidBodyContactConstraintPGS<float, 3>, float, 3>;
