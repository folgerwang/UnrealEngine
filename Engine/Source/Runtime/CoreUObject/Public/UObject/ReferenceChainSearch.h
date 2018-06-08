// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/GarbageCollection.h"

/** Search mode flags */
enum class EReferenceChainSearchMode
{
	// Returns all reference chains found
	Default = 0,
	// Returns only reference chains from external objects
	ExternalOnly = 1 << 0,
	// Returns only the shortest reference chain for each rooted object
	Shortest = 1 << 1,
	// Returns only the longest reference chain for each rooted object
	Longest = 1 << 2,
	// Returns only the direct referencers
	Direct = 1 << 3,

	// Print results
	PrintResults = 1 << 16,
	// Print ALL results (in some cases there may be thousands of reference chains)
	PrintAllResults = 1 << 17,
};

ENUM_CLASS_FLAGS(EReferenceChainSearchMode);


class FReferenceChainSearch
{
public:

	/** Type of reference */
	enum class EReferenceType
	{
		Unknown = 0,
		Property = 1,
		AddReferencedObjects
	};

	/** Extended information about a reference */
	template <typename T>
	struct TReferenceInfo
	{
		/** Object that is being referenced */
		T* Object;
		/** Type of reference to the object being referenced */
		EReferenceType Type;
		/** Name of the object or property that is referencing this object */
		FName ReferencerName;

		/** Default ctor */
		TReferenceInfo()
			: Object(nullptr)
			, Type(EReferenceType::Unknown)
		{}

		/** Ctor */
		TReferenceInfo(T* InObject, EReferenceType InType = EReferenceType::Unknown, const FName& InReferencerName = NAME_None)
			: Object(InObject)
			, Type(InType)
			, ReferencerName(InReferencerName)
		{}

		bool operator == (const TReferenceInfo& Other) const
		{
			return Object == Other.Object;
		}

		friend uint32 GetTypeHash(const TReferenceInfo& Info)
		{
			return GetTypeHash(Info.Object);
		}

		/** Dumps this reference info to string. Does not include the object being referenced */
		FString ToString() const
		{
			if (Type == EReferenceType::Property)
			{
				return FString::Printf(TEXT("->%s"), *ReferencerName.ToString());
			}
			else if (Type == EReferenceType::AddReferencedObjects)
			{
				return TEXT("::AddReferencedObjects()");
			}
			return FString();
		}
	};

	/** Single node in the reference graph */
	struct FGraphNode
	{
		FGraphNode()
			: Object(nullptr)
			, Visited(0)
		{}

		/** Object pointer */
		UObject* Object;
		/** Objects referenced by this object with reference info */
		TSet< TReferenceInfo<FGraphNode> > ReferencedObjects;
		/** Objects that have references to this object */
		TSet<FGraphNode*> ReferencedByObjects;
		/** Non-zero if this node has been already visited during reference search */
		int32 Visited;
	};

	/** Convenience type definitions to avoid template hell */
	typedef TReferenceInfo<UObject> FObjectReferenceInfo;
	typedef TReferenceInfo<FGraphNode> FNodeReferenceInfo;

	/** Reference chain. The first object in the list is the target object and the last object is a root object */
	class FReferenceChain
	{
		friend class FReferenceChainSearch;

		/** Nodes in this reference chain. The first node is the target object and the last one is a root object */
		TArray<FGraphNode*> Nodes;
		/** Reference information for Nodes */
		TArray<FNodeReferenceInfo> ReferenceInfos;

		/** Fills this chain with extended reference info for each node */
		void FillReferenceInfo();

	public:
		/** Adds a new node to the chain */
		void AddNode(FGraphNode* InNode)
		{
			Nodes.Add(InNode);
		}
		/** Gets a node from the chain */
		FGraphNode* GetNode(int32 NodeIndex) const
		{
			return Nodes[NodeIndex];
		}
		FGraphNode* GetRootNode() const
		{
			return Nodes.Last();
		}
		/** Returns the number of nodes in the chain */
		int32 Num() const
		{
			return Nodes.Num();
		}
		/** Returns a duplicate of this chain */
		FReferenceChain* Split()
		{
			FReferenceChain* NewChain = new FReferenceChain(*this);
			return NewChain;
		}
		/** Checks if this chain contains the specified node */
		bool Contains(const FGraphNode* InNode) const
		{
			return Nodes.Contains(InNode);
		}
		/** Gets extended reference info for the specified node index */
		const FNodeReferenceInfo& GetReferenceInfo(int32 NodeIndex) const
		{
			return ReferenceInfos[NodeIndex];
		}
		/** Check if this reference chain represents an external reference (the root is not in target object) */
		bool IsExternal() const;
	};

	/** Constructs a new search engine and finds references to the specified object */
	FReferenceChainSearch(UObject* InObjectToFindReferencesTo, EReferenceChainSearchMode Mode = EReferenceChainSearchMode::PrintResults);

	/** Destructor */
	~FReferenceChainSearch();

	/** 
	 * Dumps results to log 
	 * @param bDumpAllChains - if set to false, the output will be trimmed to the first 100 reference chains
	 */
	void PrintResults(bool bDumpAllChains = false);

	/** Returns all reference chains */
	const TArray<FReferenceChain*>& GetReferenceChains() const
	{
		return ReferenceChains;
	}

private:

	/** The object we're going to look for references to */
	UObject* ObjectToFindReferencesTo;
	/** All reference chain found during the search */
	TArray<FReferenceChain*> ReferenceChains;
	/** All nodes created during the search */
	TMap<UObject*, FGraphNode*> AllNodes;

	/** Performs the search */
	void PerformSearch(EReferenceChainSearchMode SearchMode);

	/** Performs the search */
	void FindDirectReferencesForObjects();

	/** Frees memory */
	void Cleanup();

	/** Tries to find a node for an object and if it doesn't exists creates a new one and returns it */
	static FGraphNode* FindOrAddNode(TMap<UObject*, FGraphNode*>& AllNodes, UObject* InObjectToFindNodeFor);
	/** Builds reference chains */
	static void BuildReferenceChains(FGraphNode* TargetNode, FReferenceChain* Chain, TArray<FReferenceChain*>& AllChains, const int32 VisitCounter);
	/** Builds reference chains */
	static void BuildReferenceChains(FGraphNode* TargetNode, TArray<FReferenceChain*>& AllChains, EReferenceChainSearchMode SearchMode);
	/** Builds reference chains for direct references only */
	static void BuildReferenceChainsForDirectReferences(FGraphNode* TargetNode, TArray<FReferenceChain*>& AllChains, EReferenceChainSearchMode SearchMode);
	/** Leaves only chains with unique root objects */
	static void RemoveChainsWithDuplicatedRoots(TArray<FReferenceChain*>& AllChains);
	/** Leaves only unique chains */
	static void RemoveDuplicatedChains(TArray<FReferenceChain*>& AllChains);
	/** Tries to complete chains we stopped processing in BuildReferenceChains with chains that are already complete */
	static void TryToCompleteChains(TArray<FReferenceChain*>& IncompleteChains, TArray<FReferenceChain*>& AllChains);
	
	static void FindCompleteChains(TArray<FReferenceChain*>& AllChains, TArray<FReferenceChain*>& CompleteChains, TArray<FReferenceChain*>& IncompleteChains, EReferenceChainSearchMode SearchMode);
	/** Returns a string with all flags (we care about) set on an object */
	static FString GetObjectFlags(UObject* InObject);
	/** Dumps a reference chain to log */
	static void DumpChain(FReferenceChain* Chain);
};

