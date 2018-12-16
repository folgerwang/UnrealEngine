// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineDefines.h"
#include "PhysxUserData.h"
#include "ShapeElem.generated.h"

namespace EAggCollisionShape
{
	enum Type
	{
		Sphere,
		Box,
		Sphyl,
		Convex,
		TaperedCapsule,

		Unknown
	};
}

/** Sphere shape used for collision */
USTRUCT()
struct FKShapeElem
{
	GENERATED_USTRUCT_BODY()

	FKShapeElem()
	: RestOffset(0.f)
	, ShapeType(EAggCollisionShape::Unknown)
	, bContributeToMass(true)
#if WITH_PHYSX
	, UserData(this)
#endif
	{}

	FKShapeElem(EAggCollisionShape::Type InShapeType)
	: RestOffset(0.f)
	, ShapeType(InShapeType)
	, bContributeToMass(true)
#if WITH_PHYSX
	, UserData(this)
#endif
	{}

	FKShapeElem(const FKShapeElem& Copy)
	: RestOffset(Copy.RestOffset)
	, Name(Copy.Name)
	, ShapeType(Copy.ShapeType)
	, bContributeToMass(Copy.bContributeToMass)
#if WITH_PHYSX
	, UserData(this)
#endif
	{
	}

	virtual ~FKShapeElem(){}

	const FKShapeElem& operator=(const FKShapeElem& Other)
	{
		CloneElem(Other);
		return *this;
	}

	template <typename T>
	T* GetShapeCheck()
	{
		check(T::StaticShapeType == ShapeType);
		return (T*)this;
	}

#if WITH_PHYSX
	const FPhysxUserData* GetUserData() const { FPhysxUserData::Set<FKShapeElem>((void*)&UserData, const_cast<FKShapeElem*>(this));  return &UserData; }
#endif // WITH_PHYSX

	ENGINE_API static EAggCollisionShape::Type StaticShapeType;

	/** Get the user-defined name for this shape */
	ENGINE_API const FName& GetName() const { return Name; }

	/** Set the user-defined name for this shape */
	ENGINE_API void SetName(const FName& InName) { Name = InName; }

	/** Get whether this shape contributes to the mass of the body */
	ENGINE_API bool GetContributeToMass() const { return bContributeToMass; }

	/** Set whether this shape will contribute to the mass of the body */
	ENGINE_API void SetContributeToMass(bool bInContributeToMass) { bContributeToMass = bInContributeToMass; }

	/** Offset used when generating contact points. This allows you to smooth out
		the Minkowski sum by radius R. Useful for making objects slide smoothly
		on top of irregularities  */
	UPROPERTY(Category = Shape, EditAnywhere)
	float RestOffset;

protected:
	/** Helper function to safely clone instances of this shape element */
	void CloneElem(const FKShapeElem& Other)
	{
		ShapeType = Other.ShapeType;
		Name = Other.Name;
		bContributeToMass = Other.bContributeToMass;
	}

private:
	/** User-defined name for this shape */
	UPROPERTY(Category=Shape, EditAnywhere)
	FName Name;

	EAggCollisionShape::Type ShapeType;

	/** True if this shape should contribute to the overall mass of the body it
		belongs to. This lets you create extra collision volumes which do not affect
		the mass properties of an object. */
	UPROPERTY(Category=Shape, EditAnywhere)
	uint8 bContributeToMass : 1;

#if WITH_PHYSX
	FPhysxUserData UserData;
#endif // WITH_PHYSX
};
