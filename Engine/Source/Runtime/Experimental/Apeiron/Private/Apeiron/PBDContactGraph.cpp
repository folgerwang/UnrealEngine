// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Apeiron/PBDContactGraph.h"

#include "Async/ParallelFor.h"
#include "ProfilingDebugging/ScopedTimers.h"

using namespace Apeiron;

template<class T, int d>
TPBDContactGraph<T, d>::TPBDContactGraph(TPBDRigidParticles<T, d>& InParticles)
{
	Initialize(InParticles.Size());
}

template<class T, int d>
void TPBDContactGraph<T, d>::Initialize(int32 Size)
{
	MNodes.SetNum(Size);
	ParallelFor(Size, [&](int32 BodyIndex) {
		MNodes[BodyIndex].BodyIndex = BodyIndex;
		MNodes[BodyIndex].Island = -1;
		MNodes[BodyIndex].NextColor = 0;
	});
}

template<class T, int d>
void TPBDContactGraph<T, d>::Reset(const TPBDRigidParticles<T, d>& InParticles, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	// @todo(mlentine): Investigate if it's faster to update instead of recompute; seems unlikely
	double Time = 0;
	FDurationTimer Timer(Time);
	MEdges.Reset();
	ParallelFor(InParticles.Size(), [&](int32 BodyIndex) {
		MNodes[BodyIndex].Edges.Reset();
		MNodes[BodyIndex].BodyIndex = BodyIndex;
		MNodes[BodyIndex].Island = -1;
		MNodes[BodyIndex].NextColor = 0;
	});
	ComputeGraph(InParticles, Constraints);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("\tPBDContactGraphIslands Update Graph %f"), Time);
}

template<class T, int d>
void TPBDContactGraph<T, d>::ComputeGraph(const TPBDRigidParticles<T, d>& InParticles, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	double Time = 0;
	FDurationTimer Timer(Time);
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
	UE_LOG(LogApeiron, Verbose, TEXT("\tPBDContactGraph Construct Graph from Constraints %f"), Time);
}

template<class T, int d>
const TArray<typename TPBDContactGraph<T, d>::ContactMap>&
TPBDContactGraph<T, d>::GetContactMapAt(int32 Index) const
{
	if (Index < MIslandData.Num())
	{
		return MIslandData[Index].LevelToColorToContactMap;
	}
	return EmptyContactMapArray;
}

template<class T, int d>
int TPBDContactGraph<T, d>::GetMaxColorAt(int32 Index) const
{
	if (Index < MIslandData.Num())
	{
		return MIslandData[Index].MaxColor;
	}
	return -1;
}

template<class T, int d>
int TPBDContactGraph<T, d>::GetMaxLevelAt(int32 Index) const
{
	if (Index < MIslandData.Num())
	{
		return MIslandData[Index].MaxLevel;
	}
	return -1;
}

template<class T, int d>
void TPBDContactGraph<T, d>::UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles,
    TSet<int32>& ActiveIndices, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	ComputeIslands(InParticles, IslandParticles, ActiveIndices, Constraints);
}

template<class T, int d>
void TPBDContactGraph<T, d>::ComputeIslands(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles,
    TSet<int32>& ActiveIndices, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	double Time = 0;
	FDurationTimer Timer(Time);

	int32 NextIsland = 0;
	TArray<TSet<int32>> NewIslandParticles;
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
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("\tPBDContactGraph Construct %d Islands %f"), NextIsland, Time);
	Time = 0;
	Timer.Start();
	if (NewIslandParticles.Num())
	{
		for (int32 Island = 0; Island < NewIslandParticles.Num(); ++Island)
		{
			for (const auto& Index : NewIslandParticles[Island])
			{
				if (InParticles.InvM(Index))
				{
					InParticles.Island(Index) = Island;
				}
				else
				{
					InParticles.Island(Index) = -1;
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
				if (OtherIsland == -1 && TmpIsland >= 0)
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
			// A new object entered the island
			if (bIsSameIsland && NewIslandParticles.Num() != IslandParticles.Num())
			{
				bIsSameIsland = false;
			}
			// Find out if we need to activate island
			if (!bIsSameIsland)
			{
				for (const auto Index : IslandParticles[Island])
				{
					InParticles.Sleeping(Index) = false;
					ActiveIndices.Add(Index);
				}
			}
			MIslandData[OtherIsland].IsIslandPersistant = bIsSameIsland;
		}
	}
	IslandParticles = MoveTemp(NewIslandParticles);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("\tPBDContactGraph Find Motionless Islands %f"), Time);
}

template<class T, int d>
void TPBDContactGraph<T, d>::ComputeIsland(const TPBDRigidParticles<T, d>& InParticles, GraphNode* Node, const int32 Island,
    TSet<int32>& DynamicParticlesInIsland, TSet<int32>& StaticParticlesInIsland, const TArray<FRigidBodyContactConstraint>& Constraints)
{
	if (Node->Island >= 0)
	{
		check(Node->Island == Island);
		return;
	}
	if (!InParticles.InvM(Node->BodyIndex))
	{
		if (!StaticParticlesInIsland.Contains(Node->BodyIndex))
		{
			StaticParticlesInIsland.Add(Node->BodyIndex);
		}
		return;
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
		ComputeIsland(InParticles, OtherNode, Island, DynamicParticlesInIsland, StaticParticlesInIsland, Constraints);
	}
}

template<class T, int d>
void TPBDContactGraph<T, d>::ComputeGraphColoring(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& NodeIndices,
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
			NodesToProcess.SetNum(NodesToProcess.Num() - 1);
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

template<class T, int d>
void TPBDContactGraph<T, d>::SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, TSet<int32>& GlobalActiveIndices, const int32 Island) const
{
	check(ActiveIndices.Num());
	if (!MIslandData[Island].IsIslandPersistant)
	{
		return;
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
	if (V.SizeSquared() < 1e-3 && W.SizeSquared() < 1e-3)
	{
		for (const auto& Index : ActiveIndices)
		{
			GlobalActiveIndices.Remove(Index);
			InParticles.Sleeping(Index) = true;
			InParticles.V(Index) = TVector<T, d>(0);
			InParticles.W(Index) = TVector<T, d>(0);
		}
	}
}

template<class T, int d>
void TPBDContactGraph<T, d>::ComputeContactGraph(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32> NodeIndices, const TSet<int32> EdgeIndices, int32& MaxLevel, TArray<TMap<int32, TArray<FRigidBodyContactConstraint>>>& LevelToColorToContactMap)
{
#ifdef USE_CONTACT_LEVELS
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
#endif
}

template<class T, int d>
void TPBDContactGraph<T, d>::UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island)
{
#ifdef USE_CONTACT_LEVELS
	auto& LevelToColorToContactMap = MIslandData[Island].LevelToColorToContactMap;
	auto& IslandConstraints = MIslandData[Island].IslandConstraints;
	auto& MaxLevel = MIslandData[Island].MaxLevel;
	ComputeContactGraph(InParticles, ActiveIndices, IslandConstraints, MaxLevel, LevelToColorToContactMap);
	auto& MaxColor = MIslandData[Island].MaxColor;
	ComputeGraphColoring(InParticles, ActiveIndices, MaxColor, LevelToColorToContactMap);
#else
	auto& LevelToColorToContactMap = MIslandData[Island].LevelToColorToContactMap;
	LevelToColorToContactMap.SetNum(1);
	auto& MaxColor = MIslandData[Island].MaxColor;
	ComputeGraphColoring(InParticles, ActiveIndices, MaxColor, LevelToColorToContactMap);
#endif
}

template class Apeiron::TPBDContactGraph<float, 3>;
