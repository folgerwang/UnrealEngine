// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Field/FieldSystem.h"
#include "GeometryCollection/RecordedTransformTrack.h"
#include "GeometryCollectionSimulationTypes.h"
#include "GeometryCollection/GeometryCollection.h"

/**
*  Simulation Parameters
*/
struct FSimulationParameters
{
	FSimulationParameters()
		: Name("")
		, RestCollection(nullptr)
		, DynamicCollection(nullptr)
		, RecordedTrack(nullptr)
		, bOwnsTrack(false)
		, Simulating(false)
		, FieldSystem(nullptr)
		, WorldTransform(FTransform::Identity)
		, ObjectType(EObjectTypeEnum::Chaos_Object_Dynamic)
		, EnableClustering(true)
		, MaxClusterLevel(100)
		, DamageThreshold({250.f})
		, CollisionType(ECollisionTypeEnum::Chaos_Surface_Volumetric)
		, ImplicitType(EImplicitTypeEnum::Chaos_Implicit_Sphere)
		, MinLevelSetResolution(5)
		, MaxLevelSetResolution(10)
		, MassAsDensity(false)
		, Mass(1.0)
		, MinimumMassClamp(0.1)
		, CollisionParticlesFraction(1.0)
		, Friction(0.3)
		, Bouncyness(0.0)
		, InitialVelocityType(EInitialVelocityTypeEnum::Chaos_Initial_Velocity_None)
		, InitialLinearVelocity(FVector(0))
		, InitialAngularVelocity(FVector(0))
		, CacheType(EGeometryCollectionCacheType::None)
		, CacheBeginTime(0.0f)
		, ReverseCacheBeginTime(0.0f)
		, bClearCache(false)
		, SaveCollisionData(true)
		, CollisionDataMaxSize(1024)
		, DoCollisionDataSpatialHash(true)
		, SpatialHashRadius(15.f)
		, MaxCollisionPerCell(1)
		, SaveTrailingData(true)
		, TrailingDataSizeMax(1024)
		, TrailingMinSpeedThreshold(100.f)
		, TrailingMinVolumeThreshold(10000.f)
	{}

	FSimulationParameters(
		FString Name
		, FGeometryCollection * RestCollectionIn
		, FGeometryCollection * DynamicCollectionIn
		, const FRecordedTransformTrack* InRecordedTrack
		, bool OwnsTrackIn
		, bool SimulatingIn
		, const FFieldSystem* FieldSystemIn
		, FTransform& WorldTransformIn
		, EObjectTypeEnum ObjectTypeIn
		, bool EnableClusteringIn
		, int32 MaxClusterLevelIn
		, TArray<float> DamageThresholdIn
		, ECollisionTypeEnum CollisionTypeIn
		, EImplicitTypeEnum ImplicitTypeIn
		, int32 MinLevelSetResolutionIn
		, int32 MaxLevelSetResolutionIn
		, bool MassAsDensityIn
		, float MassIn
		, float MinimumMassClampIn
		, float CollisionParticlesFractionIn
		, float FrictionIn
		, float BouncynessIn
		, EInitialVelocityTypeEnum InitialVelocityTypeIn
		, FVector InitialLinearVelocityIn
		, FVector InitialAngularVelocityIn
		, bool bClearCacheIn
		, bool SaveCollisionDataIn
		, int32 CollisionDataMaxSizeIn
		, bool DoCollisionDataSpatialHashIn
		, float SpatialHashRadiusIn
		, int32 MaxCollisionPerCellIn
		, bool SaveTrailingDataIn
		, int32 TrailingDataSizeMaxIn
		, float TrailingMinSpeedThresholdIn
		, float TrailingMinVolumeThresholdIn
		, EGeometryCollectionCacheType InCacheType
		, float InCacheBeginTime
		, float InReverseCacheBeginTime)
		: RestCollection(RestCollectionIn)
		, DynamicCollection(DynamicCollectionIn)
		, RecordedTrack(InRecordedTrack)
		, bOwnsTrack(OwnsTrackIn)
		, Simulating(SimulatingIn)
		, FieldSystem(FieldSystemIn)
		, WorldTransform(WorldTransformIn)
		, ObjectType(ObjectTypeIn)
		, EnableClustering(EnableClusteringIn)
		, MaxClusterLevel(MaxClusterLevelIn)
		, DamageThreshold(DamageThresholdIn)
		, CollisionType(CollisionTypeIn)
		, ImplicitType(ImplicitTypeIn)
		, MinLevelSetResolution(MinLevelSetResolutionIn)
		, MaxLevelSetResolution(MaxLevelSetResolutionIn)
		, MassAsDensity(MassAsDensityIn)
		, Mass(MassIn)
		, MinimumMassClamp(MinimumMassClampIn)
		, CollisionParticlesFraction(CollisionParticlesFractionIn)
		, Friction(FrictionIn)
		, Bouncyness(BouncynessIn)
		, InitialVelocityType(InitialVelocityTypeIn)
		, InitialLinearVelocity(InitialLinearVelocityIn)
		, InitialAngularVelocity(InitialAngularVelocityIn)
		, CacheType(InCacheType)
		, CacheBeginTime(InCacheBeginTime)
		, ReverseCacheBeginTime(InReverseCacheBeginTime)
		, bClearCache(bClearCacheIn)
		, SaveCollisionData(SaveCollisionDataIn)
		, CollisionDataMaxSize(CollisionDataMaxSizeIn)
		, DoCollisionDataSpatialHash(DoCollisionDataSpatialHashIn)
		, SpatialHashRadius(SpatialHashRadiusIn)
		, MaxCollisionPerCell(MaxCollisionPerCellIn)
		, SaveTrailingData(SaveTrailingDataIn)
		, TrailingDataSizeMax(TrailingDataSizeMaxIn)
		, TrailingMinSpeedThreshold(TrailingMinSpeedThresholdIn)
		, TrailingMinVolumeThreshold(TrailingMinVolumeThresholdIn)
	{}

	FSimulationParameters(const FSimulationParameters& Other)
		: RestCollection(Other.RestCollection)
		, DynamicCollection(Other.DynamicCollection)
		, RecordedTrack(Other.RecordedTrack)
		, bOwnsTrack(false)
		, Simulating(Other.Simulating)
		, FieldSystem(Other.FieldSystem)
		, WorldTransform(Other.WorldTransform)
		, ObjectType(Other.ObjectType)
		, EnableClustering(Other.EnableClustering)
		, MaxClusterLevel(Other.MaxClusterLevel)
		, DamageThreshold(Other.DamageThreshold)
		, CollisionType(Other.CollisionType)
		, ImplicitType(Other.ImplicitType)
		, MinLevelSetResolution(Other.MinLevelSetResolution)
		, MaxLevelSetResolution(Other.MaxLevelSetResolution)
		, MassAsDensity(Other.MassAsDensity)
		, Mass(Other.Mass)
		, MinimumMassClamp(Other.MinimumMassClamp)
		, CollisionParticlesFraction(Other.CollisionParticlesFraction)
		, Friction(Other.Friction)
		, Bouncyness(Other.Bouncyness)
		, InitialVelocityType(Other.InitialVelocityType)
		, InitialLinearVelocity(Other.InitialLinearVelocity)
		, InitialAngularVelocity(Other.InitialAngularVelocity)
		, CacheType(Other.CacheType)
		, CacheBeginTime(Other.CacheBeginTime)
		, ReverseCacheBeginTime(Other.ReverseCacheBeginTime)
		, bClearCache(Other.bClearCache)
		, SaveCollisionData(Other.SaveCollisionData)
		, CollisionDataMaxSize(Other.CollisionDataMaxSize)
		, DoCollisionDataSpatialHash(Other.DoCollisionDataSpatialHash)
		, SpatialHashRadius(Other.SpatialHashRadius)
		, MaxCollisionPerCell(Other.MaxCollisionPerCell)
		, SaveTrailingData(Other.SaveTrailingData)
		, TrailingDataSizeMax(Other.TrailingDataSizeMax)
		, TrailingMinSpeedThreshold(Other.TrailingMinSpeedThreshold)
		, TrailingMinVolumeThreshold(Other.TrailingMinVolumeThreshold)
	{}

	~FSimulationParameters()
	{
		if (bOwnsTrack)
		{
			delete const_cast<FRecordedTransformTrack*>(RecordedTrack);
		}
	}

	bool IsCacheRecording() { return CacheType == EGeometryCollectionCacheType::Record || CacheType == EGeometryCollectionCacheType::RecordAndPlay; }
	bool IsCachePlaying() { return CacheType == EGeometryCollectionCacheType::Play || CacheType == EGeometryCollectionCacheType::RecordAndPlay; }

	FString Name;
	FGeometryCollection * RestCollection;
	FGeometryCollection * DynamicCollection;
	const FRecordedTransformTrack* RecordedTrack;
	bool bOwnsTrack;

	bool Simulating;

	const FFieldSystem* FieldSystem;

	FTransform WorldTransform;

	EObjectTypeEnum ObjectType;

	bool EnableClustering;
	int32 MaxClusterLevel;
	TArray<float> DamageThreshold;

	ECollisionTypeEnum CollisionType;
	EImplicitTypeEnum ImplicitType;
	int32 MinLevelSetResolution;
	int32 MaxLevelSetResolution;
	bool MassAsDensity;
	float Mass;
	float MinimumMassClamp;
	float CollisionParticlesFraction;
	float Friction;
	float Bouncyness;

	EInitialVelocityTypeEnum InitialVelocityType;
	FVector InitialLinearVelocity;
	FVector InitialAngularVelocity;

	EGeometryCollectionCacheType CacheType;
	float CacheBeginTime;
	float ReverseCacheBeginTime;
	bool bClearCache;
	bool SaveCollisionData;
	int32 CollisionDataMaxSize;
	bool DoCollisionDataSpatialHash;
	float SpatialHashRadius;
	int32 MaxCollisionPerCell;
	bool SaveTrailingData;
	int32 TrailingDataSizeMax;
	float TrailingMinSpeedThreshold;
	float TrailingMinVolumeThreshold;
};