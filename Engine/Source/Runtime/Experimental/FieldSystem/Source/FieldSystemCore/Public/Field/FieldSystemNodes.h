// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Field/FieldSystem.h"
#include "Math/Vector.h"

/**
* RadialMaskField
**/

class FIELDSYSTEMCORE_API FRadialIntMask : public FFieldNode<int32>
{
	typedef FFieldNode<int32> Super;

public:

	FRadialIntMask() = delete;

	FRadialIntMask(FName NameIn)
		: Super(NameIn)
		, Radius(0)
		, Position(FVector(0, 0, 0))
		, InteriorValue(1.0)
		, ExteriorValue(0.0)
		, SetMaskCondition(ESetMaskConditionType::Field_Set_Always)
	{}

	virtual ~FRadialIntMask() {}

	void Evaluate(const FFieldContext &, TArrayView<int32> & Results) const;

	float Radius;
	FVector Position;
	int32 InteriorValue;
	int32 ExteriorValue;
	ESetMaskConditionType SetMaskCondition;

	virtual FFieldNodeBase* Clone() const override { return new FRadialIntMask(*this); }
};


/**
* RadialFalloff
**/

class FIELDSYSTEMCORE_API FRadialFalloff : public FFieldNode<float>
{
	typedef FFieldNode<float> Super;

public:

	FRadialFalloff() = delete;

	FRadialFalloff(FName NameIn)
		: Super(NameIn)
		, Magnitude(1.0)
		, Radius(0)
		, Position(FVector(0, 0, 0))
	{}

	virtual ~FRadialFalloff() {}

	void Evaluate(const FFieldContext &, TArrayView<float> & Results) const;

	float Magnitude;
	float Radius;
	FVector Position;

	virtual FFieldNodeBase* Clone() const override { return new FRadialFalloff(*this); }
};


/**
* UniformVector
**/

class FIELDSYSTEMCORE_API FUniformVector : public FFieldNode<FVector>
{
	typedef FFieldNode<FVector> Super;

public:

	FUniformVector() = delete;

	FUniformVector(FName NameIn)
		: Super(NameIn)
		, Magnitude(1.0)
		, Direction(FVector(0, 0, 0))
	{}

	virtual ~FUniformVector() {}

	void Evaluate(const FFieldContext &, TArrayView<FVector> & Results) const;

	float Magnitude;
	FVector Direction;

	virtual FFieldNodeBase* Clone() const override { return new FUniformVector(*this); }
};


/**
* RadialVector
**/

class FIELDSYSTEMCORE_API FRadialVector : public FFieldNode<FVector>
{
	typedef FFieldNode<FVector> Super;

public:

	FRadialVector() = delete;

	FRadialVector(FName NameIn)
		: Super(NameIn)
		, Magnitude(1.0)
		, Position(FVector(0, 0, 0))
	{}

	virtual ~FRadialVector() {}

	void Evaluate(const FFieldContext &, TArrayView<FVector> & Results) const;

	float Magnitude;
	FVector Position;


	virtual FFieldNodeBase* Clone() const override { return new FRadialVector(*this); }
};


/**
* SumVector
**/

class FIELDSYSTEMCORE_API FSumVector : public FFieldNode<FVector>
{
	typedef FFieldNode<FVector> Super;

public:

	FSumVector() = delete;

	FSumVector(FName NameIn)
		: Super(NameIn)
		, Magnitude(1.0)
		, Scalar(Invalid)
		, VectorRight(Invalid)
		, VectorLeft(Invalid)
		, Operation(EFieldOperationType::Field_Multiply)
	{}

	virtual ~FSumVector() {}

	void Evaluate(const FFieldContext &, TArrayView<FVector> & Results) const;

	float Magnitude;
	int Scalar;
	int VectorRight;
	int VectorLeft;
	EFieldOperationType Operation;

	virtual FFieldNodeBase* Clone() const override { return new FSumVector(*this); }
};

/**
* SumScalar
**/

class FIELDSYSTEMCORE_API FSumScalar : public FFieldNode<float>
{
	typedef FFieldNode<float> Super;

public:

	FSumScalar() = delete;

	FSumScalar(FName NameIn)
		: Super(NameIn)
		, Magnitude(1.0)
		, ScalarRight(Invalid)
		, ScalarLeft(Invalid)
		, Operation(EFieldOperationType::Field_Multiply)
	{}

	virtual ~FSumScalar() {}

	void Evaluate(const FFieldContext &, TArrayView<float> & Results) const;

	float Magnitude;
	int ScalarRight;
	int ScalarLeft;
	EFieldOperationType Operation;

	virtual FFieldNodeBase* Clone() const override { return new FSumScalar(*this); }
};

