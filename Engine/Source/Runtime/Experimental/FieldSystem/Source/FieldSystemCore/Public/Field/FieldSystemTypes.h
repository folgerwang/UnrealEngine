// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "UObject/ObjectMacros.h"

class FFieldSystem;

/**
*
*/
UENUM(BlueprintType)
enum ESetMaskConditionType
{
	Field_Set_Always	        UMETA(DisplayName = "Always"),
	Field_Set_IFF_NOT_Interior  UMETA(DisplayName = "IFF NOT Interior"),
	Field_Set_IFF_NOT_Exterior  UMETA(DisplayName = "IFF NOT Exterior"),
	//~~~
	//256th entry
	Field_MaskCondition_Max                 UMETA(Hidden)
};


/**
*
*/
UENUM(BlueprintType)
enum EFieldOperationType
{
	Field_Multiply  UMETA(DisplayName = "Multiply"),
	Field_Divide    UMETA(DisplayName = "Divide"),
	Field_Add       UMETA(DisplayName = "Add"),
	Field_Substract UMETA(DisplayName = "Subtract"),
	//~~~
	//256th entry
	Field_Operation_Max                 UMETA(Hidden)
};


/**
*
*/
UENUM(BlueprintType)
enum EFieldPhysicsType
{
	Field_StayDynamic		UMETA(DisplayName = "StayDynamic"),
	Field_LinearForce		UMETA(DisplayName = "LinearForce"),
	//~~~
	//256th entry
	Field_PhysicsType_Max                 UMETA(Hidden)
};


/**
*
*/
UENUM(BlueprintType)
enum EFieldPhysicsDefaultFields
{
	Field_RadialIntMask				UMETA(DisplayName = "RadialIntMask"),
	Field_RadialFalloff				UMETA(DisplayName = "RadialFalloff"),
	Field_UniformVector				UMETA(DisplayName = "UniformVector"),
	Field_RadialVector				UMETA(DisplayName = "RadialVector"),
	Field_RadialVectorFalloff		UMETA(DisplayName = "RadialVectorFalloff"),
	//~~~
	//256th entry
	Field_EFieldPhysicsDefaultFields_Max                 UMETA(Hidden)
};


/**
* FFieldContext
*/
struct FFieldContext
{
	FFieldContext() = delete;
	FFieldContext(int32 TerminalIn, const TArrayView<int32>& SampleIndicesIn,
		const TArrayView<FVector>& SamplesIn, const FFieldSystem* FieldSystemIn,
		const FVector* PositionIn = nullptr, const FVector* DirectionIn = nullptr,
		const float* RadiusIn = nullptr, const float* MagnitudeIn = nullptr)
		: Terminal(TerminalIn)
		, SampleIndices(SampleIndicesIn)
		, Samples(SamplesIn)
		, FieldSystem(FieldSystemIn)
		, Position(PositionIn)
		, Direction(DirectionIn)
		, Radius(RadiusIn)
		, Magnitude(MagnitudeIn) {}
	FFieldContext(int32 TerminalIn, const FFieldContext & ContextIn)
		: Terminal(TerminalIn)
		, SampleIndices(ContextIn.SampleIndices)
		, Samples(ContextIn.Samples)
		, FieldSystem(ContextIn.FieldSystem)
		, Position(ContextIn.Position)
		, Direction(ContextIn.Direction)
		, Radius(ContextIn.Radius)
		, Magnitude(ContextIn.Magnitude) {}
	FFieldContext(int32 TerminalIn, const TArrayView<int32>& SampleIndicesIn, const FFieldContext & ContextIn)
		: Terminal(TerminalIn)
		, SampleIndices(SampleIndicesIn)
		, Samples(ContextIn.Samples)
		, FieldSystem(ContextIn.FieldSystem)
		, Position(ContextIn.Position)
		, Direction(ContextIn.Direction)
		, Radius(ContextIn.Radius)
		, Magnitude(ContextIn.Magnitude) {}


	const int32 Terminal;
	const TArrayView<int32>& SampleIndices;
	const TArrayView<FVector>& Samples;
	const FFieldSystem* FieldSystem;
	// Node overrides
	const FVector* Position;
	const FVector* Direction;
	const float* Radius;
	const float* Magnitude;
};


/**
* FieldCommand
*/
class FFieldSystemCommand
{
public:
	FFieldSystemCommand()
		: Name("none")
		, Type(EFieldPhysicsType::Field_PhysicsType_Max)
		, Position(FVector(0))
		, Direction(FVector(0))
		, Radius(0.f)
		, Magnitude(0.f)
		, MaxClusterLevel(1000) 
	{}

	FFieldSystemCommand(const FName NameIn,
		const EFieldPhysicsType TypeIn = EFieldPhysicsType::Field_PhysicsType_Max,
		const FVector PositionIn = FVector(0), const FVector DirectionIn = FVector(0),
		const float RadiusIn = 0.f, const float MagnitudeIn = 0.f, const float MaxClusterLevelIn = 1000)
		: Name(NameIn)
		, Type(TypeIn)
		, Position(PositionIn)
		, Direction(DirectionIn)
		, Radius(RadiusIn)
		, Magnitude(MagnitudeIn) 
		, MaxClusterLevel(MaxClusterLevelIn)
	{}

	FFieldSystemCommand(const FFieldSystemCommand & CommandIn)
		: Name(CommandIn.Name)
		, Type(CommandIn.Type)
		, Position(CommandIn.Position)
		, Direction(CommandIn.Direction)
		, Radius(CommandIn.Radius)
		, Magnitude(CommandIn.Magnitude) 
		, MaxClusterLevel(CommandIn.MaxClusterLevel)
	{}

	FFieldSystemCommand &operator =(const FFieldSystemCommand & Other) {
		this->Name = Other.Name;
		this->Type = Other.Type;
		this->Position = Other.Position;
		this->Direction = Other.Direction;
		this->Radius = Other.Radius;
		this->Magnitude = Other.Magnitude;
		this->MaxClusterLevel = Other.MaxClusterLevel;
		return *this;
	}

	 FName  Name;
	 EFieldPhysicsType Type;
	 FVector Position;
	 FVector Direction;
	 float Radius;
	 float Magnitude;
	 int MaxClusterLevel;
};
