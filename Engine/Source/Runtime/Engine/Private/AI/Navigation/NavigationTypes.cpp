// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "AI/NavigationSystemBase.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "EngineStats.h"
#include "Components/ShapeComponent.h"
#include "AI/Navigation/NavAreaBase.h"


DEFINE_STAT(STAT_Navigation_MetaAreaTranslation);

static const uint32 MAX_NAV_SEARCH_NODES = 2048;

namespace FNavigationSystem
{
	// these are totally arbitrary values, and it should haven happen these are ever used.
	// in any reasonable case UNavigationSystemV1::SupportedAgents should be filled in ini file
	// and only those values will be used
	const float FallbackAgentRadius = 35.f;
	const float FallbackAgentHeight = 144.f;
}

//----------------------------------------------------------------------//
// FNavigationQueryFilter
//----------------------------------------------------------------------//
const uint32 FNavigationQueryFilter::DefaultMaxSearchNodes = MAX_NAV_SEARCH_NODES;

//----------------------------------------------------------------------//
// FNavPathType
//----------------------------------------------------------------------//
uint32 FNavPathType::NextUniqueId = 0;

//----------------------------------------------------------------------//
// FNavDataConfig
//----------------------------------------------------------------------//
FNavDataConfig::FNavDataConfig(float Radius, float Height)
	: FNavAgentProperties(Radius, Height)
	, Name(TEXT("Default"))
	, Color(140, 255, 0, 164)
	, DefaultQueryExtent(DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_VERTICAL)
	, NavigationDataClass(FNavigationSystem::GetDefaultNavDataClass())
	, NavigationDataClassName(NavigationDataClass)
{
}

FNavDataConfig::FNavDataConfig(const FNavDataConfig& Other)
	: FNavAgentProperties(Other)
	, Name(Other.Name)
	, Color(Other.Color)
	, DefaultQueryExtent(Other.DefaultQueryExtent)
	, NavigationDataClass(Other.NavigationDataClass)
	, NavigationDataClassName(NavigationDataClass)
{

}

//----------------------------------------------------------------------//
// FNavigationRelevantData
//----------------------------------------------------------------------//
bool FNavigationRelevantData::FCollisionDataHeader::IsValid(const uint8* RawData, int32 RawDataSize)
{
	const int32 HeaderSize = sizeof(FCollisionDataHeader);
	return (RawDataSize == 0) || ((RawDataSize >= HeaderSize) && (((const FCollisionDataHeader*)RawData)->DataSize == RawDataSize));
}

bool FNavigationRelevantData::HasPerInstanceTransforms() const
{
	return NavDataPerInstanceTransformDelegate.IsBound();
}

bool FNavigationRelevantData::IsMatchingFilter(const FNavigationRelevantDataFilter& Filter) const
{
	return (Filter.bIncludeGeometry && HasGeometry()) ||
		(Filter.bIncludeOffmeshLinks && (Modifiers.HasPotentialLinks() || Modifiers.HasLinks())) ||
		(Filter.bIncludeAreas && Modifiers.HasAreas()) ||
		(Filter.bIncludeMetaAreas && Modifiers.HasMetaAreas());
}

void FNavigationRelevantData::Shrink()
{
	CollisionData.Shrink();
	VoxelData.Shrink();
	Modifiers.Shrink();
}

bool FNavigationRelevantData::IsCollisionDataValid() const
{
	const bool bIsValid = FCollisionDataHeader::IsValid(CollisionData.GetData(), CollisionData.Num());
	if (!ensure(bIsValid))
	{
		UE_LOG(LogNavigation, Error, TEXT("NavOctree element has corrupted collision data! Owner:%s Bounds:%s"), *GetNameSafe(GetOwner()), *Bounds.ToString());
		return false;
	}

	return true;
}

//----------------------------------------------------------------------//
// FNavigationQueryFilter
//----------------------------------------------------------------------//
FNavigationQueryFilter::FNavigationQueryFilter(const FNavigationQueryFilter& Source)
{
	Assign(Source);
}

FNavigationQueryFilter::FNavigationQueryFilter(const FNavigationQueryFilter* Source)
	: MaxSearchNodes(DefaultMaxSearchNodes)
{
	if (Source != NULL)
	{
		Assign(*Source);
	}
}

FNavigationQueryFilter::FNavigationQueryFilter(const FSharedNavQueryFilter Source)
	: MaxSearchNodes(DefaultMaxSearchNodes)
{
	if (Source.IsValid())
	{
		SetFilterImplementation(Source->GetImplementation());
	}
}

FNavigationQueryFilter& FNavigationQueryFilter::operator=(const FNavigationQueryFilter& Source)
{
	Assign(Source);
	return *this;
}

void FNavigationQueryFilter::Assign(const FNavigationQueryFilter& Source)
{
	if (Source.GetImplementation() != NULL)
	{
		QueryFilterImpl = Source.QueryFilterImpl;
	}
	MaxSearchNodes = Source.GetMaxSearchNodes();
}

FSharedNavQueryFilter FNavigationQueryFilter::GetCopy() const
{
	FSharedNavQueryFilter Copy = MakeShareable(new FNavigationQueryFilter());
	Copy->QueryFilterImpl = MakeShareable(QueryFilterImpl->CreateCopy());
	Copy->MaxSearchNodes = MaxSearchNodes;

	return Copy;
}

void FNavigationQueryFilter::SetAreaCost(uint8 AreaType, float Cost)
{
	check(QueryFilterImpl.IsValid());
	QueryFilterImpl->SetAreaCost(AreaType, Cost);
}

void FNavigationQueryFilter::SetFixedAreaEnteringCost(uint8 AreaType, float Cost)
{
	check(QueryFilterImpl.IsValid());
	QueryFilterImpl->SetFixedAreaEnteringCost(AreaType, Cost);
}

void FNavigationQueryFilter::SetExcludedArea(uint8 AreaType)
{
	QueryFilterImpl->SetExcludedArea(AreaType);
}

void FNavigationQueryFilter::SetAllAreaCosts(const TArray<float>& CostArray)
{
	SetAllAreaCosts(CostArray.GetData(), CostArray.Num());
}

void FNavigationQueryFilter::SetAllAreaCosts(const float* CostArray, const int32 Count)
{
	QueryFilterImpl->SetAllAreaCosts(CostArray, Count);
}

void FNavigationQueryFilter::GetAllAreaCosts(float* CostArray, float* FixedCostArray, const int32 Count) const
{
	QueryFilterImpl->GetAllAreaCosts(CostArray, FixedCostArray, Count);
}

void FNavigationQueryFilter::SetIncludeFlags(uint16 Flags)
{
	QueryFilterImpl->SetIncludeFlags(Flags);
}

uint16 FNavigationQueryFilter::GetIncludeFlags() const
{
	return QueryFilterImpl->GetIncludeFlags();
}

void FNavigationQueryFilter::SetExcludeFlags(uint16 Flags)
{
	QueryFilterImpl->SetExcludeFlags(Flags);
}

uint16 FNavigationQueryFilter::GetExcludeFlags() const
{
	return QueryFilterImpl->GetExcludeFlags();
}

//----------------------------------------------------------------------//
// FNavAgentSelector
//----------------------------------------------------------------------//
FNavAgentSelector::FNavAgentSelector() : PackedBits(0x7fffffff)
{
}

bool FNavAgentSelector::Serialize(FArchive& Ar)
{
	Ar << PackedBits;
	return true;
}

//----------------------------------------------------------------------//
// FNavHeightfieldSample
//----------------------------------------------------------------------//
FNavHeightfieldSamples::FNavHeightfieldSamples()
{
#if WITH_PHYSX
	//static_assert(sizeof(physx::PxI16) == sizeof(Heights.GetTypeSize()), "FNavHeightfieldSamples::Heights' type needs to be kept in sync with physx::PxI16");
#endif // WITH_PHYSX
}

//----------------------------------------------------------------------//
// FNavAgentProperties
//----------------------------------------------------------------------//
const FNavAgentProperties FNavAgentProperties::DefaultProperties;

FNavAgentProperties::FNavAgentProperties(const FNavAgentProperties& Other)
	: AgentRadius(Other.AgentRadius)
	, AgentHeight(Other.AgentHeight)
	, AgentStepHeight(Other.AgentStepHeight)
	, NavWalkingSearchHeightScale(Other.NavWalkingSearchHeightScale)
	, PreferredNavData(Other.PreferredNavData)
{

}

void FNavAgentProperties::UpdateWithCollisionComponent(UShapeComponent* CollisionComponent)
{
	check(CollisionComponent != NULL);
	AgentRadius = CollisionComponent->Bounds.SphereRadius;
}

bool FNavAgentProperties::IsNavDataMatching(const FNavAgentProperties& Other) const
{
	return (PreferredNavData == Other.PreferredNavData || PreferredNavData == nullptr || Other.PreferredNavData == nullptr);
}

//----------------------------------------------------------------------//
// UNavAreaBase
//----------------------------------------------------------------------//
UNavAreaBase::UNavAreaBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsMetaArea = false;
}

TSubclassOf<UNavAreaBase> UNavAreaBase::PickAreaClassForAgent(const AActor& Actor, const FNavAgentProperties& NavAgent) const
{
	UE_CLOG(IsMetaArea(), LogNavigation, Warning, TEXT("UNavAreaBase::PickAreaClassForAgent called for meta class %s. Please override PickAreaClass.")
		, *(GetClass()->GetName()));

	return GetClass();
}
