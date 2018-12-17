// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Assets/ClothingAsset.h"
#include "ClothingSimulationInterface.h"
#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/PBDEvolution.h"
#include "Chaos/Transform.h"
#include "Chaos/TriangleMesh.h"

#include <memory>

namespace Chaos
{

class ClothingSimulationContext : public IClothingSimulationContext
{
  public:
    ClothingSimulationContext() {}
    ~ClothingSimulationContext() {}
    float DeltaTime;
    TArray<FMatrix> RefToLocals;
    TArray<FTransform> BoneTransforms;
    FTransform LocalToWorld;
};

class ClothingSimulation : public IClothingSimulation
{
  public:
    ClothingSimulation()
        : NumIterations(1), EdgeStiffness(1.f), BendingStiffness(1.f), AreaStiffness(1.f), VolumeStiffness(0.f), StrainLimitingStiffness(1.f), ShapeTargetStiffness(0.f),
          SelfCollisionThickness(2.f), CollisionThickness(1.2f), GravityMagnitude(490.f),
          bUseBendingElements(false), bUseTetrahedralConstraints(false), bUseThinShellVolumeConstraints(false), bUseSelfCollisions(false), bUseContinuousCollisionDetection(false)
    {
    }
    virtual ~ClothingSimulation() {}

  protected:
    void CreateActor(USkeletalMeshComponent* InOwnerComponent, UClothingAssetBase* InAsset, int32 SimDataIndex);
    IClothingSimulationContext* CreateContext() { return new ClothingSimulationContext(); }
    void FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext);
    void Initialize();
    void Shutdown() {}
    bool ShouldSimulate() const { return true; }
    void Simulate(IClothingSimulationContext* InContext);
    void DestroyActors() {}
    void DestroyContext(IClothingSimulationContext* InContext) { delete InContext; }
    void GetSimulationData(TMap<int32, FClothSimulData>& OutData, USkeletalMeshComponent* InOwnerComponent, USkinnedMeshComponent* InOverrideComponent) const;
    FBoxSphereBounds GetBounds(const USkeletalMeshComponent* InOwnerComponent) const
    {
        return FBoxSphereBounds(Evolution->Particles().X().GetData(), Evolution->Particles().Size());
    }
    void AddExternalCollisions(const FClothCollisionData& InData);
    void ClearExternalCollisions();
    void GetCollisions(FClothCollisionData& OutCollisions, bool bIncludeExternal = true) const;

  private:
    // UE Collision Data (Needed only for GetCollisions)
    TArray<Pair<uint32, FClothCollisionPrim_Sphere>> IndexAndSphereCollisionMap;
    TArray<Pair<uint32, FClothCollisionPrim_Convex>> IndexAndConvexCollisionMap;
    // Animation Data
    UClothingAsset* Asset;
    TArray<Chaos::TRigidTransform<float, 3>> OldAnimationTransforms;
    TArray<Chaos::TRigidTransform<float, 3>> AnimationTransforms;
    TArray<Chaos::TVector<float, 3>> OldAnimationPositions;
    TArray<Chaos::TVector<float, 3>> AnimationPositions;
    TArray<Chaos::TVector<float, 3>> AnimationNormals;
    Chaos::TArrayCollectionArray<float> BoneIndices;
    Chaos::TArrayCollectionArray<Chaos::TRigidTransform<float, 3>> BaseTransforms;
    // Sim Data
    TArray<Chaos::TVector<uint32, 2>> IndexToRangeMap;
    TUniquePtr<Chaos::TTriangleMesh<float>> Mesh;
    TUniquePtr<Chaos::TPBDEvolution<float, 3>> Evolution;
    float Time;
    float DeltaTime;
    float MaxDeltaTime;
    float ClampDeltaTime;
    // Parameters that should be set in the ui
    int32 NumIterations;
    float EdgeStiffness;
    float BendingStiffness;
    float AreaStiffness;
    float VolumeStiffness;
    float StrainLimitingStiffness;
    float ShapeTargetStiffness;
    float SelfCollisionThickness;
    float CollisionThickness;
    float CoefficientOfFriction;
    float Damping;
    float GravityMagnitude;
    bool bUseBendingElements;
    bool bUseTetrahedralConstraints;
    bool bUseThinShellVolumeConstraints;
    bool bUseSelfCollisions;
    bool bUseContinuousCollisionDetection;
};
}
