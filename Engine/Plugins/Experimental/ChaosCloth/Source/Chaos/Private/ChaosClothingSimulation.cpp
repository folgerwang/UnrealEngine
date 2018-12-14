// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "ChaosClothingSimulation.h"

#include "Async/ParallelFor.h"
#include "Assets/ClothingAsset.h"
#include "ClothingSimulation.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Chaos/Box.h"
#include "Chaos/Capsule.h"
#include "Chaos/Cylinder.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/ImplicitObjectIntersection.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/Levelset.h"
#include "Chaos/PBDAxialSpringConstraints.h"
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PBDVolumeConstraint.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/PerParticlePBDLongRangeConstraints.h"
#include "Chaos/PerParticlePBDShapeConstraints.h"
#include "Chaos/Plane.h"
#include "Chaos/Sphere.h"
#include "Chaos/TaperedCylinder.h"
#include "Chaos/Transform.h"
#include "Chaos/Utilities.h"
#include "Chaos/Vector.h"
#include "ChaosClothPrivate.h"

#if WITH_PHYSX && !PLATFORM_LUMIN && !PLATFORM_ANDROID
#include "PhysXIncludes.h" 
#endif

#include <functional>
#include <unordered_map>
#include <unordered_set>

using namespace Chaos;

static TAutoConsoleVariable<int32> CVarClothNumIterations(TEXT("physics.ClothNumIterations"), 1, TEXT(""));
static TAutoConsoleVariable<float> CVarClothSelfCollisionThickness(TEXT("physics.ClothSelfCollisionThickness"), 2.f, TEXT(""));
static TAutoConsoleVariable<float> CVarClothCollisionThickness(TEXT("physics.ClothCollisionThickness"), 1.2f, TEXT(""));
static TAutoConsoleVariable<float> CVarClothCoefficientOfFriction(TEXT("physics.ClothCoefficientOfFriction"), 0.f, TEXT(""));
static TAutoConsoleVariable<float> CVarClothDamping(TEXT("physics.ClothDamping"), 0.01f, TEXT(""));
static TAutoConsoleVariable<float> CVarClothGravityMagnitude(TEXT("physics.ClothGravityMagnitude"), 490.f, TEXT(""));

void ClothingSimulation::CreateActor(USkeletalMeshComponent* InOwnerComponent, UClothingAssetBase* InAsset, int32 InSimDataIndex)
{
    EdgeStiffness = InOwnerComponent->EdgeStiffness;
    BendingStiffness = InOwnerComponent->BendingStiffness;
    AreaStiffness = InOwnerComponent->AreaStiffness;
    VolumeStiffness = InOwnerComponent->VolumeStiffness;
    StrainLimitingStiffness = InOwnerComponent->StrainLimitingStiffness;
    ShapeTargetStiffness = InOwnerComponent->ShapeTargetStiffness;
    bUseBendingElements = InOwnerComponent->bUseBendingElements;
    bUseTetrahedralConstraints = InOwnerComponent->bUseTetrahedralConstraints;
    bUseThinShellVolumeConstraints = InOwnerComponent->bUseThinShellVolumeConstraints;
    bUseSelfCollisions = InOwnerComponent->bUseSelfCollisions;
    bUseContinuousCollisionDetection = InOwnerComponent->bUseContinuousCollisionDetection;

    ClothingSimulationContext Context;
    FillContext(InOwnerComponent, 0, &Context);

    // TODO(mlentine): Support multiple assets.
    Asset = Cast<UClothingAsset>(InAsset);
    check(Asset->LodData.Num() == 1);
    FClothLODData& AssetLodData = Asset->LodData[0];
    FClothPhysicalMeshData& PhysMesh = AssetLodData.PhysicalMeshData;

    FTransform RootBoneTransform = Context.BoneTransforms[Asset->ReferenceBoneIndex];
    FClothingSimulationBase::SkinPhysicsMesh(Asset, PhysMesh, RootBoneTransform, Context.RefToLocals.GetData(), Context.RefToLocals.Num(), reinterpret_cast<TArray<FVector>&>(AnimationPositions), reinterpret_cast<TArray<FVector>&>(AnimationNormals));
    FTransform RootBoneWorldTransform = RootBoneTransform * Context.LocalToWorld;
    ParallelFor(AnimationPositions.Num(), [&](int32 Index) {
        AnimationPositions[Index] = RootBoneWorldTransform.TransformPosition(AnimationPositions[Index]);
        AnimationNormals[Index] = RootBoneWorldTransform.TransformVector(AnimationNormals[Index]);
    });

    TPBDParticles<float, 3>& LocalParticles = Evolution->Particles();
    uint32 Size = LocalParticles.Size();
    check(Size == 0);
    LocalParticles.AddParticles(PhysMesh.Vertices.Num());
    if (IndexToRangeMap.Num() <= InSimDataIndex)
        IndexToRangeMap.SetNum(InSimDataIndex + 1);
    IndexToRangeMap[InSimDataIndex] = Chaos::TVector<uint32, 2>({Size, LocalParticles.Size()});
    for (uint32 i = Size; i < LocalParticles.Size(); ++i)
    {
        LocalParticles.X(i) = AnimationPositions[i - Size];
        LocalParticles.V(i) = Chaos::TVector<float, 3>(0.f, 0.f, 0.f);
        LocalParticles.M(i) = 1.f;
        LocalParticles.InvM(i) = PhysMesh.MaxDistances[i - Size] > 0.1 ? 1.f : 0.f;
    }

    TArray<Chaos::TVector<int32, 3>> InputSurfaceElements;
    for (int i = Size; i < PhysMesh.Indices.Num() / 3; ++i)
    {
        const int32 Index = 3 * i;
        InputSurfaceElements.Add({static_cast<int32>(PhysMesh.Indices[Index]), static_cast<int32>(PhysMesh.Indices[Index + 1]), static_cast<int32>(PhysMesh.Indices[Index + 2])});
    }
    Mesh.Reset(new Chaos::TTriangleMesh<float>(MoveTemp(InputSurfaceElements)));
    const auto& SurfaceElements = Mesh->GetSurfaceElements();
    // Add Model
    if (ShapeTargetStiffness)
    {
        check(ShapeTargetStiffness > 0.f && ShapeTargetStiffness <= 1.f);
        Evolution->AddPBDConstraintFunction(std::bind(
            static_cast<void (Chaos::TPerParticlePBDShapeConstraints<float, 3>::*)(TPBDParticles<float, 3> & InParticles, const float Dt) const>(&Chaos::TPerParticlePBDShapeConstraints<float, 3>::Apply),
            Chaos::TPerParticlePBDShapeConstraints<float, 3>(Evolution->Particles(), AnimationPositions, ShapeTargetStiffness), std::placeholders::_1, std::placeholders::_2));
    }
    if (EdgeStiffness)
    {
        check(EdgeStiffness > 0.f && EdgeStiffness <= 1.f);
        Evolution->AddPBDConstraintFunction([SpringConstraints = Chaos::TPBDSpringConstraints<float, 3>(Evolution->Particles(), SurfaceElements, EdgeStiffness)](TPBDParticles<float, 3>& InParticles, const float Dt) {
            SpringConstraints.Apply(InParticles, Dt);
        });
    }
    if (BendingStiffness)
    {
        check(BendingStiffness > 0.f && BendingStiffness <= 1.f);
        if (bUseBendingElements)
        {
            TArray<Chaos::TVector<int32, 4>> BendingConstraints = Mesh->GetUniqueAdjacentElements();
            Evolution->AddPBDConstraintFunction(std::bind(&Chaos::TPBDBendingConstraints<float>::Apply,
                                                          Chaos::TPBDBendingConstraints<float>(Evolution->Particles(), MoveTemp(BendingConstraints)), std::placeholders::_1, std::placeholders::_2));
        }
        else
        {
            TArray<Chaos::TVector<int32, 2>> BendingConstraints = Mesh->GetUniqueAdjacentPoints();
            Evolution->AddPBDConstraintFunction([SpringConstraints = Chaos::TPBDSpringConstraints<float, 3>(Evolution->Particles(), MoveTemp(BendingConstraints), BendingStiffness)](TPBDParticles<float, 3>& InParticles, const float Dt) {
                SpringConstraints.Apply(InParticles, Dt);
            });
        }
    }
    if (AreaStiffness)
    {
        TArray<Chaos::TVector<int32, 3>> SurfaceConstraints = SurfaceElements;
        Evolution->AddPBDConstraintFunction(std::bind(&Chaos::TPBDAxialSpringConstraints<float, 3>::Apply,
                                                      Chaos::TPBDAxialSpringConstraints<float, 3>(Evolution->Particles(), MoveTemp(SurfaceConstraints), AreaStiffness), std::placeholders::_1, std::placeholders::_2));
    }
    if (VolumeStiffness)
    {
        check(VolumeStiffness > 0.f && VolumeStiffness <= 1.f);
        if (bUseTetrahedralConstraints)
        {
            // TODO(mlentine): Need to tetrahedralize surface to support this
            check(false);
        }
        else if (bUseThinShellVolumeConstraints)
        {
            TArray<Chaos::TVector<int32, 2>> BendingConstraints = Mesh->GetUniqueAdjacentPoints();
            TArray<Chaos::TVector<int32, 2>> DoubleBendingConstraints;
            {
                TMap<int32, TArray<int32>> BendingHash;
                for (int32 i = 0; i < BendingConstraints.Num(); ++i)
                {
                    BendingHash.FindOrAdd(BendingConstraints[i][0]).Add(BendingConstraints[i][1]);
                    BendingHash.FindOrAdd(BendingConstraints[i][1]).Add(BendingConstraints[i][0]);
                }
                TSet<Chaos::TVector<int32, 2>> Visited;
                for (auto Elem : BendingHash)
                {
                    for (int32 i = 0; i < Elem.Value.Num(); ++i)
                    {
                        for (int32 j = i + 1; j < Elem.Value.Num(); ++j)
                        {
                            if (Elem.Value[i] == Elem.Value[j])
                                continue;
                            auto NewElem = Chaos::TVector<int32, 2>(Elem.Value[i], Elem.Value[j]);
                            if (!Visited.Contains(NewElem))
                            {
                                DoubleBendingConstraints.Add(NewElem);
                                Visited.Add(NewElem);
                                Visited.Add(Chaos::TVector<int32, 2>(Elem.Value[j], Elem.Value[i]));
                            }
                        }
                    }
                }
            }
            Evolution->AddPBDConstraintFunction([SpringConstraints = Chaos::TPBDSpringConstraints<float, 3>(Evolution->Particles(), MoveTemp(DoubleBendingConstraints), VolumeStiffness)](TPBDParticles<float, 3>& InParticles, const float Dt) {
                SpringConstraints.Apply(InParticles, Dt);
            });
        }
        else
        {
            TArray<Chaos::TVector<int32, 3>> SurfaceConstraints = SurfaceElements;
            Evolution->AddPBDConstraintFunction(std::bind(&Chaos::TPBDVolumeConstraint<float>::Apply,
                                                          Chaos::TPBDVolumeConstraint<float>(Evolution->Particles(), MoveTemp(SurfaceConstraints), VolumeStiffness), std::placeholders::_1, std::placeholders::_2));
        }
    }
    if (StrainLimitingStiffness)
    {
        Evolution->AddPBDConstraintFunction(std::bind(
            static_cast<void (Chaos::TPerParticlePBDLongRangeConstraints<float, 3>::*)(TPBDParticles<float, 3> & InParticles, const float Dt) const>(&Chaos::TPerParticlePBDLongRangeConstraints<float, 3>::Apply),
            Chaos::TPerParticlePBDLongRangeConstraints<float, 3>(Evolution->Particles(), *Mesh, StrainLimitingStiffness), std::placeholders::_1, std::placeholders::_2));
    }
    // Add Self Collisions
    if (bUseSelfCollisions)
    {
        // TODO(mlentine): Parallelize these for multiple meshes
        Evolution->CollisionTriangles().Append(SurfaceElements);
        for (uint32 i = Size; i < LocalParticles.Size(); ++i)
        {
            auto Neighbors = Mesh->GetNRing(i, 5);
            for (const auto& Element : Neighbors)
            {
                check(i != Element);
                Evolution->DisabledCollisionElements().Add(Chaos::TVector<int32, 2>(i, Element));
                Evolution->DisabledCollisionElements().Add(Chaos::TVector<int32, 2>(Element, i));
            }
        }
    }
    // Add Collision Bodies
    TGeometryParticles<float, 3>& CollisionParticles = Evolution->CollisionParticles();
    USkeletalMesh* TargetMesh = InOwnerComponent->SkeletalMesh;
    // TODO(mlentine): Support collision body activation on a per particle basis, preferably using a map but also can be a particle attribute
    if (UPhysicsAsset* PhysAsset = Asset->PhysicsAsset)
    {
        for (const USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
        {
            int32 MeshBoneIndex = TargetMesh->RefSkeleton.FindBoneIndex(BodySetup->BoneName);
            int32 MappedBoneIndex = INDEX_NONE;

            if (MeshBoneIndex != INDEX_NONE)
            {
                MappedBoneIndex = Asset->UsedBoneNames.AddUnique(BodySetup->BoneName);
            }

            {
                uint32 OldSize = CollisionParticles.Size();
                CollisionParticles.AddParticles(BodySetup->AggGeom.SphereElems.Num());
                for (uint32 i = OldSize; i < CollisionParticles.Size(); ++i)
                {
                    const auto& CollisionSphere = BodySetup->AggGeom.SphereElems[i - OldSize];
                    CollisionParticles.Geometry(i) = new Chaos::TSphere<float, 3>(Chaos::TVector<float, 3>(0.f, 0.f, 0.f), CollisionSphere.Radius);
                    BaseTransforms[i] = Chaos::TRigidTransform<float, 3>(CollisionSphere.Center, Chaos::TRotation<float, 3>::MakeFromEuler(Chaos::TVector<float, 3>(0.f, 0.f, 0.f)));
                    BoneIndices[i] = MappedBoneIndex;
                }
            }
            {
                uint32 OldSize = CollisionParticles.Size();
                CollisionParticles.AddParticles(BodySetup->AggGeom.BoxElems.Num());
                for (uint32 i = OldSize; i < CollisionParticles.Size(); ++i)
                {
                    const auto& Box = BodySetup->AggGeom.BoxElems[i - OldSize];
                    Chaos::TVector<float, 3> half_extents(Box.X / 2.f, Box.Y / 2.f, Box.Z / 2.f);
                    CollisionParticles.Geometry(i) = new Chaos::TBox<float, 3>(-half_extents, half_extents);
                    BaseTransforms[i] = Chaos::TRigidTransform<float, 3>(Box.Center, Box.Rotation.Quaternion());
                    BoneIndices[i] = MappedBoneIndex;
                }
            }
            {
                uint32 OldSize = CollisionParticles.Size();
                CollisionParticles.AddParticles(BodySetup->AggGeom.SphylElems.Num());
                for (uint32 i = OldSize; i < CollisionParticles.Size(); ++i)
                {
                    const auto& Capsule = BodySetup->AggGeom.SphylElems[i - OldSize];
                    if (Capsule.Length == 0)
                    {
                        CollisionParticles.Geometry(i) = new Chaos::TSphere<float, 3>(Chaos::TVector<float, 3>(0), Capsule.Radius);
                    }
                    else
                    {
                        Chaos::TVector<float, 3> half_extents(0, 0, Capsule.Length / 2);
                        CollisionParticles.Geometry(i) = new Chaos::TCapsule<float>(-half_extents, half_extents, Capsule.Radius);
                    }
                    BaseTransforms[i] = Chaos::TRigidTransform<float, 3>(Capsule.Center, Capsule.Rotation.Quaternion());
                    BoneIndices[i] = MappedBoneIndex;
                }
            }
            /*{
                int32 OldSize = CollisionParticles.Size();
                CollisionParticles.AddParticles(BodySetup->AggGeom.TaperedCapsuleElems.Num());
                for (int32 i = OldSize; i < CollisionParticles.Size(); ++i)
                {
                    const auto& Capsule = BodySetup->AggGeom.TaperedCapsuleElems[i - OldSize];
                    if (Capsule.Length == 0)
                    {
                        CollisionParticles.Geometry(i) = new Chaos::Sphere<float, 3>(Chaos::TVector<float, 3>(0), Capsule.Radius1 > Capsule.Radius0 ? Capsule.Radius1 : Capsule.Radius0);
                    }
                    else
                    {
                        TArray<TUniquePtr<TImplicitObject<float, 3>>> Objects;
                        Chaos::TVector<float, 3> half_extents(0, 0, Capsule.Length / 2);
                        Objects.Add(TUniquePtr<Chaos::TImplicitObject<float, 3>>(
                            new Chaos::TTaperedCylinder<float>(-half_extents, half_extents, Capsule.Radius1, Capsule.Radius0)));
                        Objects.Add(TUniquePtr<Chaos::TImplicitObject<float, 3>>(
                            new Chaos::Sphere<float, 3>(-half_extents, Capsule.Radius1)));
                        Objects.Add(TUniquePtr<Chaos::TImplicitObject<float, 3>>(
                            new Chaos::Sphere<float, 3>(half_extents, Capsule.Radius0)));
                        CollisionParticles.Geometry(i) = new Chaos::TImplicitObjectUnion<float, 3>(MoveTemp(Objects));
                    }
					BaseTransforms[i] = Chaos::TRigidTransform<float, 3>(Capsule.Center, Capsule.Rotation.Quaternion());
                    BoneIndices[i] = MappedBoneIndex;
                }
			}*/
            {
// Collision bodies are stored in PhysX specific data structures so they can only be imported if we enable PhysX.
#if WITH_PHYSX && !PLATFORM_LUMIN && !PLATFORM_ANDROID
                uint32 OldSize = CollisionParticles.Size();
                CollisionParticles.AddParticles(BodySetup->AggGeom.ConvexElems.Num());
                for (uint32 i = OldSize; i < CollisionParticles.Size(); ++i)
                {
                    const auto& CollisionBody = BodySetup->AggGeom.ConvexElems[i - OldSize];
                    TArray<Chaos::TVector<int32, 3>> CollisionMeshElements;
                    const auto PhysXMesh = CollisionBody.GetConvexMesh();
                    for (int32 j = 0; j < static_cast<int32>(PhysXMesh->getNbPolygons()); ++j)
                    {
                        physx::PxHullPolygon Poly;
                        PhysXMesh->getPolygonData(j, Poly);
                        check(Poly.mNbVerts == 3);
                        const auto Indices = PhysXMesh->getIndexBuffer() + Poly.mIndexBase;
                        CollisionMeshElements.Add(Chaos::TVector<int32, 3>(Indices[0], Indices[1], Indices[2]));
                    }
                    Chaos::TParticles<float, 3> CollisionMeshParticles;
                    CollisionMeshParticles.AddParticles(CollisionBody.VertexData.Num());
                    for (uint32 j = 0; j < CollisionMeshParticles.Size(); ++j)
                    {
                        CollisionMeshParticles.X(j) = CollisionBody.VertexData[j];
                    }
                    Chaos::TBox<float, 3> BoundingBox(CollisionMeshParticles.X(0), CollisionMeshParticles.X(0));
                    for (uint32 j = 1; j < CollisionMeshParticles.Size(); ++j)
                    {
                        BoundingBox.GrowToInclude(CollisionMeshParticles.X(i));
                    }
                    int32 MaxAxisSize = 100;
                    int32 MaxAxis;
                    const auto Extents = BoundingBox.Extents();
                    if (Extents[0] > Extents[1] && Extents[0] > Extents[2])
                    {
                        MaxAxis = 0;
                    }
                    else if (Extents[1] > Extents[2])
                    {
                        MaxAxis = 1;
                    }
                    else
                    {
                        MaxAxis = 2;
                    }
                    Chaos::TUniformGrid<float, 3> Grid(BoundingBox.Min(), BoundingBox.Max(), Chaos::TVector<int32, 3>(100 * Extents[0] / Extents[MaxAxis], 100 * Extents[0] / Extents[MaxAxis], 100 * Extents[0] / Extents[MaxAxis]));
                    Chaos::TTriangleMesh<float> CollisionMesh(MoveTemp(CollisionMeshElements));
                    CollisionParticles.Geometry(i) = new Chaos::TLevelSet<float, 3>(Grid, CollisionMeshParticles, CollisionMesh);
                    BaseTransforms[i] = Chaos::TRigidTransform<float, 3>(Chaos::TVector<float, 3>(0.f, 0.f, 0.f), Chaos::TRotation<float, 3>::MakeFromEuler(Chaos::TVector<float, 3>(0.f, 0.f, 0.f)));
                    BoneIndices[i] = MappedBoneIndex;
                }
#endif
            }
        }
    }
    AnimationTransforms.SetNum(BaseTransforms.Num());
    for (uint32 i = 0; i < CollisionParticles.Size(); ++i)
    {
        if (BoneIndices[i] != INDEX_NONE)
        {
            const int32 MappedIndex = Asset->UsedBoneIndices[BoneIndices[i]];
            if (MappedIndex != INDEX_NONE)
            {
                const FTransform& BoneTransform = Context.BoneTransforms[MappedIndex];
                AnimationTransforms[i] = BaseTransforms[i] * BoneTransform * Context.LocalToWorld;
                CollisionParticles.X(i) = AnimationTransforms[i].GetTranslation();
                CollisionParticles.R(i) = AnimationTransforms[i].GetRotation();
            }
        }
    }
}

void ClothingSimulation::Initialize()
{
    NumIterations = CVarClothNumIterations.GetValueOnGameThread();
    SelfCollisionThickness = CVarClothSelfCollisionThickness.GetValueOnGameThread();
    CollisionThickness = CVarClothCollisionThickness.GetValueOnGameThread();
    CoefficientOfFriction = CVarClothCoefficientOfFriction.GetValueOnGameThread();
    Damping = CVarClothDamping.GetValueOnGameThread();
    GravityMagnitude = CVarClothGravityMagnitude.GetValueOnGameThread();

    Chaos::TPBDParticles<float, 3> LocalParticles;
    Chaos::TKinematicGeometryParticles<float, 3> TRigidParticles;
    Evolution.Reset(new Chaos::TPBDEvolution<float, 3>(MoveTemp(LocalParticles), MoveTemp(TRigidParticles), {}, NumIterations, CollisionThickness, SelfCollisionThickness, CoefficientOfFriction, Damping));
    Evolution->CollisionParticles().AddArray(&BoneIndices);
    Evolution->CollisionParticles().AddArray(&BaseTransforms);
    if (GravityMagnitude)
    {
        Evolution->AddForceFunction(Chaos::Utilities::GetDeformablesGravityFunction(Chaos::TVector<float, 3>(0.f, 0.f, -1.f), GravityMagnitude));
    }
    Evolution->SetKinematicUpdateFunction([&](Chaos::TPBDParticles<float, 3>& ParticlesInput, const float Dt, const float LocalTime, int32 Index) {
        if (ParticlesInput.InvM(Index) > 0)
            return;
        float Alpha = (LocalTime - Time) / DeltaTime;
        ParticlesInput.X(Index) = Alpha * AnimationPositions[Index] + (1 - Alpha) * OldAnimationPositions[Index];
    });
    Evolution->SetCollisionKinematicUpdateFunction([&](Chaos::TKinematicGeometryParticles<float, 3>& ParticlesInput, const float Dt, const float LocalTime, int32 Index) {
        float Alpha = (LocalTime - Time) / DeltaTime;
        Chaos::TVector<float, 3> NewX = Alpha * AnimationTransforms[Index].GetTranslation() + (1 - Alpha) * OldAnimationTransforms[Index].GetTranslation();
        ParticlesInput.V(Index) = (NewX - ParticlesInput.X(Index)) / DeltaTime;
        ParticlesInput.X(Index) = NewX;
        Chaos::TRotation<float, 3> NewR = FQuat::Slerp(OldAnimationTransforms[Index].GetRotation(), AnimationTransforms[Index].GetRotation(), Alpha);
        Chaos::TRotation<float, 3> Delta = NewR * ParticlesInput.R(Index).Inverse();
        Chaos::TVector<float, 3> Axis;
        float Angle;
        Delta.ToAxisAndAngle(Axis, Angle);
        ParticlesInput.W(Index) = Axis * Angle / Dt;
        ParticlesInput.R(Index) = NewR;
    });
    MaxDeltaTime = 1.0f;
    ClampDeltaTime = 0.f;
    Time = 0.f;
}

void ClothingSimulation::FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext)
{
    ClothingSimulationContext* Context = static_cast<ClothingSimulationContext*>(InOutContext);
    Context->DeltaTime = ClampDeltaTime > 0 ? std::min(InDeltaTime, ClampDeltaTime) : InDeltaTime;
    Context->RefToLocals.Reset();
    Context->LocalToWorld = InComponent->GetComponentToWorld();
    InComponent->GetCurrentRefToLocalMatrices(Context->RefToLocals, 0);

    USkeletalMesh* SkelMesh = InComponent->SkeletalMesh;
    if (USkinnedMeshComponent* MasterComponent = InComponent->MasterPoseComponent.Get())
    {
        int32 NumBones = InComponent->GetMasterBoneMap().Num();
        if (NumBones == 0)
        {
            if (InComponent->SkeletalMesh)
            {
                // This case indicates an invalid master pose component (e.g. no skeletal mesh)
                NumBones = InComponent->SkeletalMesh->RefSkeleton.GetNum();
                Context->BoneTransforms.Empty(NumBones);
                Context->BoneTransforms.AddDefaulted(NumBones);
            }
        }
        else
        {
            Context->BoneTransforms.Reset(NumBones);
            Context->BoneTransforms.AddDefaulted(NumBones);
            for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
            {
                bool bFoundMaster = false;
                if (InComponent->GetMasterBoneMap().IsValidIndex(BoneIndex))
                {
                    const int32 MasterIndex = InComponent->GetMasterBoneMap()[BoneIndex];
                    if (MasterIndex != INDEX_NONE)
                    {
                        Context->BoneTransforms[BoneIndex] = MasterComponent->GetComponentSpaceTransforms()[MasterIndex];
                        bFoundMaster = true;
                    }
                }

                if (!bFoundMaster && SkelMesh)
                {
                    const int32 ParentIndex = SkelMesh->RefSkeleton.GetParentIndex(BoneIndex);
                    if (ParentIndex != INDEX_NONE)
                    {
                        Context->BoneTransforms[BoneIndex] = Context->BoneTransforms[ParentIndex] * SkelMesh->RefSkeleton.GetRefBonePose()[BoneIndex];
                    }
                    else
                    {
                        Context->BoneTransforms[BoneIndex] = SkelMesh->RefSkeleton.GetRefBonePose()[BoneIndex];
                    }
                }
            }
        }
    }
    else
    {
        Context->BoneTransforms = InComponent->GetComponentSpaceTransforms();
    }
}

void ClothingSimulation::Simulate(IClothingSimulationContext* InContext)
{
    ClothingSimulationContext* Context = static_cast<ClothingSimulationContext*>(InContext);
    if (Context->DeltaTime == 0)
        return;
    // Get New Animation Positions and Normals
    OldAnimationTransforms = AnimationTransforms;
    OldAnimationPositions = AnimationPositions;
    FClothLODData& AssetLodData = Asset->LodData[0];
    FTransform RootBoneTransform = Context->BoneTransforms[Asset->ReferenceBoneIndex];
    FClothPhysicalMeshData& PhysMesh = AssetLodData.PhysicalMeshData;
    FClothingSimulationBase::SkinPhysicsMesh(Asset, PhysMesh, RootBoneTransform, Context->RefToLocals.GetData(), Context->RefToLocals.Num(), reinterpret_cast<TArray<FVector>&>(AnimationPositions), reinterpret_cast<TArray<FVector>&>(AnimationNormals));
    FTransform RootBoneWorldTransform = RootBoneTransform * Context->LocalToWorld;
    ParallelFor(AnimationPositions.Num(), [&](int32 Index) {
        AnimationPositions[Index] = RootBoneWorldTransform.TransformPosition(AnimationPositions[Index]);
        AnimationNormals[Index] = RootBoneWorldTransform.TransformVector(AnimationNormals[Index]);
    });
    // Collision bodies
    TGeometryParticles<float, 3>& CollisionParticles = Evolution->CollisionParticles();
    for (uint32 i = 0; i < CollisionParticles.Size(); ++i)
    {
        if (BoneIndices[i] != INDEX_NONE)
        {
            const int32 MappedIndex = Asset->UsedBoneIndices[BoneIndices[i]];
            if (MappedIndex != INDEX_NONE)
            {
                const FTransform& BoneTransform = Context->BoneTransforms[MappedIndex];
                AnimationTransforms[i] = BaseTransforms[i] * BoneTransform * Context->LocalToWorld;
            }
        }
    }
    // Advance Sim
    DeltaTime = Context->DeltaTime;
    while (Context->DeltaTime > MaxDeltaTime)
    {
        Evolution->AdvanceOneTimeStep(MaxDeltaTime);
        Context->DeltaTime -= MaxDeltaTime;
    }
    Evolution->AdvanceOneTimeStep(Context->DeltaTime);
    Time += DeltaTime;
}

void ClothingSimulation::GetSimulationData(TMap<int32, FClothSimulData>& OutData, USkeletalMeshComponent* InOwnerComponent, USkinnedMeshComponent* InOverrideComponent) const
{
    if (!Mesh) return;
    TArray<Chaos::TVector<float, 3>> PointNormals = Mesh->GetPointNormals(Evolution->Particles());
    for (int32 i = 0; i < IndexToRangeMap.Num(); ++i)
    {
        FClothSimulData Data;
        Data.Positions.SetNum(IndexToRangeMap[i][1] - IndexToRangeMap[i][0]);
        Data.Normals.SetNum(IndexToRangeMap[i][1] - IndexToRangeMap[i][0]);
        for (uint32 j = IndexToRangeMap[i][0]; j < IndexToRangeMap[i][1]; ++j)
        {
            Data.Positions[j - IndexToRangeMap[i][0]] = Evolution->Particles().X(j);
            Data.Normals[j - IndexToRangeMap[i][0]] = PointNormals[j];
        }
        OutData.Emplace(i, Data);
    }
}

void ClothingSimulation::AddExternalCollisions(const FClothCollisionData& InData)
{
    TGeometryParticles<float, 3>& CollisionParticles = Evolution->CollisionParticles();
    {
        uint32 Size = CollisionParticles.Size();
        CollisionParticles.AddParticles(InData.Spheres.Num());
        for (uint32 i = Size; i < CollisionParticles.Size(); ++i)
        {
            const auto& CollisionSphere = InData.Spheres[i - Size];
            CollisionParticles.X(i) = Chaos::TVector<float, 3>(0.f, 0.f, 0.f);
            CollisionParticles.R(i) = Chaos::TRotation<float, 3>::MakeFromEuler(Chaos::TVector<float, 3>(0.f, 0.f, 0.f));
            CollisionParticles.Geometry(i) = new Chaos::TSphere<float, 3>(CollisionSphere.LocalPosition, CollisionSphere.Radius);
            IndexAndSphereCollisionMap.Add(MakePair(i, CollisionSphere));
        }
    }
    {
        uint32 Size = CollisionParticles.Size();
        CollisionParticles.AddParticles(InData.Convexes.Num());
        for (uint32 i = Size; i < CollisionParticles.Size(); ++i)
        {
            const auto& convex = InData.Convexes[i - Size];
            CollisionParticles.X(i) = Chaos::TVector<float, 3>(0.f, 0.f, 0.f);
            CollisionParticles.R(i) = Chaos::TRotation<float, 3>::MakeFromEuler(Chaos::TVector<float, 3>(0.f, 0.f, 0.f));
            TArray<TUniquePtr<TImplicitObject<float, 3>>> Planes;
            for (int32 j = 0; j < convex.Planes.Num(); ++j)
            {
                Planes.Add(TUniquePtr<Chaos::TImplicitObject<float, 3>>(
                    new Chaos::TPlane<float, 3>(Chaos::TVector<float, 3>(0.f, 0.f, convex.Planes[j].W / convex.Planes[j].Z),
                                                   Chaos::TVector<float, 3>(convex.Planes[j].X, convex.Planes[j].Y, convex.Planes[j].Z))));
            }
            CollisionParticles.Geometry(i) = new Chaos::TImplicitObjectIntersection<float, 3>(MoveTemp(Planes));
            IndexAndConvexCollisionMap.Add(MakePair(i, convex));
        }
    }
}

void ClothingSimulation::ClearExternalCollisions()
{
    Evolution->CollisionParticles().Resize(0);
    IndexAndSphereCollisionMap.Reset();
    IndexAndConvexCollisionMap.Reset();
}

void ClothingSimulation::GetCollisions(FClothCollisionData& OutCollisions, bool bIncludeExternal) const
{
    OutCollisions.Spheres.Reset();
    OutCollisions.SphereConnections.Reset();
    OutCollisions.Convexes.Reset();
    for (const auto& IndexSphere : IndexAndSphereCollisionMap)
    {
        if (Evolution->Collided(IndexSphere.First))
        {
            OutCollisions.Spheres.Add(IndexSphere.Second);
        }
    }
    for (const auto& IndexConvex : IndexAndConvexCollisionMap)
    {
        if (Evolution->Collided(IndexConvex.First))
        {
            OutCollisions.Convexes.Add(IndexConvex.Second);
        }
    }
}
