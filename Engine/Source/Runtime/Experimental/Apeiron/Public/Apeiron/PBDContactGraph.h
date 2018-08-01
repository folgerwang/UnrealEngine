// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PBDCollisionTypes.h"
#include "Apeiron/PerParticleRule.h"

#include <memory>
#include <queue>
#include <sstream>

#define USE_CONTACT_LEVELS 1

namespace Apeiron
{
template<class T, int d>
struct TRigidBodyContactConstraint;

/**/
template<class T, int d>
class TPBDContactGraph
{
  public:
	typedef TRigidBodyContactConstraint<T, d> FRigidBodyContactConstraint;
	typedef TArray<FRigidBodyContactConstraint> ContactList;
	typedef TMap<int32, ContactList> ContactMap;

	struct IslandData
	{
		TArray<TMap<int32, TArray<FRigidBodyContactConstraint>>> LevelToColorToContactMap;
		int32 MaxColor;
		int32 MaxLevel;
#ifdef USE_CONTACT_LEVELS
		TSet<int32> IslandConstraints;
#endif
		bool IsIslandPersistant;
	};

	TPBDContactGraph(TPBDRigidParticles<T, d>& InParticles);
	virtual ~TPBDContactGraph() {}

	const TArray<ContactMap>& GetContactMapAt(int32 Index) const;
	int GetMaxColorAt(int32 Index) const;
	int GetMaxLevelAt(int32 Index) const;

	void Initialize(int32 size);
	void Reset(const TPBDRigidParticles<T, d>& InParticles, const TArray<FRigidBodyContactConstraint>& Constraints);
	void ComputeGraph(const TPBDRigidParticles<T, d>& InParticles, const TArray<FRigidBodyContactConstraint>& Constraints);

	void UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island);
	void SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, TSet<int32>& GlobalActiveIndices, const int32 Island) const;
	void UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TSet<int32>& ActiveIndices, const TArray<FRigidBodyContactConstraint>& Constraints);

  private:
	struct GraphNode
	{
		TArray<int32> Edges;
		int32 BodyIndex;
		int32 NextColor;
		TSet<int32> UsedColors;
		int32 Island;
	};

	template<class T_DATA>
	struct GraphEdge
	{
		GraphNode* FirstNode;
		GraphNode* SecondNode;
		int32 Color;
#ifdef USE_CONTACT_LEVELS
		int32 Level;
#endif
		T_DATA Data;
	};

	void ComputeContactGraph(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32> NodeIndices, const TSet<int32> EdgeIndices, int32& MaxLevel, TArray<TMap<int32, TArray<FRigidBodyContactConstraint>>>& LevelToColorToContactMap);
	void ComputeGraphColoring(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& NodeIndices, int32& MaxColor, TArray<TMap<int32, TArray<FRigidBodyContactConstraint>>>& LevelToColorToContactMap);
	void ComputeIslands(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TSet<int32>& ActiveIndices, const TArray<FRigidBodyContactConstraint>& Constraints);
	void ComputeIsland(const TPBDRigidParticles<T, d>& InParticles, GraphNode* Node, const int32 Island,
	    TSet<int32>& DynamicParticlesInIsland, TSet<int32>& StaticParticlesInIsland, const TArray<FRigidBodyContactConstraint>& Constraints);

	TArray<GraphNode> MNodes;
	TArray<GraphEdge<FRigidBodyContactConstraint>> MEdges;
	TArray<IslandData> MIslandData;

	TArray<ContactMap> EmptyContactMapArray;
};
}
