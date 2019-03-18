// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "Math/RandomStream.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "UObject/Stack.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Misc/QualifiedFrameTime.h"

#include "KismetMathLibrary.generated.h"

// Whether to inline functions at all
#define KISMET_MATH_INLINE_ENABLED	(!UE_BUILD_DEBUG)

/** Provides different easing functions that can be used in blueprints */
UENUM(BlueprintType)
namespace EEasingFunc
{
	enum Type
	{
		/** Simple linear interpolation. */
		Linear,

		/** Simple step interpolation. */
		Step,

		/** Sinusoidal in interpolation. */
		SinusoidalIn,

		/** Sinusoidal out interpolation. */
		SinusoidalOut,

		/** Sinusoidal in/out interpolation. */
		SinusoidalInOut,

		/** Smoothly accelerates, but does not decelerate into the target.  Ease amount controlled by BlendExp. */
		EaseIn,

		/** Immediately accelerates, but smoothly decelerates into the target.  Ease amount controlled by BlendExp. */
		EaseOut,

		/** Smoothly accelerates and decelerates.  Ease amount controlled by BlendExp. */
		EaseInOut,

		/** Easing in using an exponential */
		ExpoIn,

		/** Easing out using an exponential */
		ExpoOut,

		/** Easing in/out using an exponential method */
		ExpoInOut,

		/** Easing is based on a half circle. */
		CircularIn,

		/** Easing is based on an inverted half circle. */
		CircularOut,

		/** Easing is based on two half circles. */
		CircularInOut,

	};
}

/** Different methods for interpolating rotation between transforms */
UENUM(BlueprintType)
namespace ELerpInterpolationMode
{
	enum Type
	{
		/** Shortest Path or Quaternion interpolation for the rotation. */
		QuatInterp,

		/** Rotor or Euler Angle interpolation. */
		EulerInterp,

		/** Dual quaternion interpolation, follows helix or screw-motion path between keyframes.   */
		DualQuatInterp
	};
}

USTRUCT(BlueprintType)
struct ENGINE_API FFloatSpringState
{
	GENERATED_BODY()

	float PrevError;
	float Velocity;

	FFloatSpringState()
	: PrevError(0.f)
	, Velocity(0.f)
	{

	}

	void Reset()
	{
		PrevError = Velocity = 0.f;
	}
};

USTRUCT(BlueprintType)
struct ENGINE_API FVectorSpringState
{
	GENERATED_BODY()

	FVector PrevError;
	FVector Velocity;

	FVectorSpringState()
	: PrevError(FVector::ZeroVector)
	, Velocity(FVector::ZeroVector)
	{

	}

	void Reset()
	{
		PrevError = Velocity = FVector::ZeroVector;
	}
};

UCLASS(meta=(BlueprintThreadSafe, ScriptName = "MathLibrary"))
class ENGINE_API UKismetMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//
	// Boolean functions.
	//
	
	/** Returns a uniformly distributed random bool*/
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static bool RandomBool();

	/** 
	 * Get a random chance with the specified weight. Range of weight is 0.0 - 1.0 E.g.,
	 *		Weight = .6 return value = True 60% of the time
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(Weight = "0.5", NotBlueprintThreadSafe))
	static bool RandomBoolWithWeight(float Weight);

	/** 
	 * Get a random chance with the specified weight. Range of weight is 0.0 - 1.0 E.g.,
	*		Weight = .6 return value = True 60% of the time
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(Weight = "0.5"))
	static bool RandomBoolWithWeightFromStream(float Weight, const FRandomStream& RandomStream);

	/** Returns the logical complement of the Boolean value (NOT A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NOT Boolean", CompactNodeTitle = "NOT", Keywords = "! not negate"), Category="Math|Boolean")
	static bool Not_PreBool(bool A);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal Boolean", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Boolean")
	static bool EqualEqual_BoolBool(bool A, bool B);

	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual Boolean", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Boolean")
	static bool NotEqual_BoolBool(bool A, bool B);

	/** Returns the logical AND of two values (A AND B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AND Boolean", CompactNodeTitle = "AND", Keywords = "& and", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Boolean")
	static bool BooleanAND(bool A, bool B);

	/** Returns the logical NAND of two values (A AND B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NAND Boolean", CompactNodeTitle = "NAND", Keywords = "!& nand", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Boolean")
	static bool BooleanNAND(bool A, bool B);

	/** Returns the logical OR of two values (A OR B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "OR Boolean", CompactNodeTitle = "OR", Keywords = "| or", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Boolean")
	static bool BooleanOR(bool A, bool B);
		
	/** Returns the logical eXclusive OR of two values (A XOR B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "XOR Boolean", CompactNodeTitle = "XOR", Keywords = "^ xor"), Category="Math|Boolean")
	static bool BooleanXOR(bool A, bool B);

	/** Returns the logical Not OR of two values (A NOR B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NOR Boolean", CompactNodeTitle = "NOR", Keywords = "!^ nor"), Category="Math|Boolean")
	static bool BooleanNOR(bool A, bool B);

	//
	// Byte functions.
	//

	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte * Byte", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Byte")
	static uint8 Multiply_ByteByte(uint8 A, uint8 B);

	/** Division (A / B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte / Byte", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Byte")
	static uint8 Divide_ByteByte(uint8 A, uint8 B = 1);

	/** Modulo (A % B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "% (Byte)", CompactNodeTitle = "%", Keywords = "% modulus"), Category="Math|Byte")
	static uint8 Percent_ByteByte(uint8 A, uint8 B = 1);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte + Byte", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Byte")
	static uint8 Add_ByteByte(uint8 A, uint8 B = 1);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte - Byte", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Byte")
	static uint8 Subtract_ByteByte(uint8 A, uint8 B = 1);

	/** Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Min (Byte)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Byte")
	static uint8 BMin(uint8 A, uint8 B);

	/** Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Max (Byte)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Byte")
	static uint8 BMax(uint8 A, uint8 B);
	
	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte < Byte", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Byte")
	static bool Less_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte > Byte", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Byte")
	static bool Greater_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte <= Byte", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Byte")
	static bool LessEqual_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte >= Byte", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Byte")
	static bool GreaterEqual_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is equal to B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Byte)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Byte")
	static bool EqualEqual_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is not equal to B (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (Byte)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Byte")
	static bool NotEqual_ByteByte(uint8 A, uint8 B);

	//
	// Integer functions.
	//

	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer * integer", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Multiply_IntInt(int32 A, int32 B);

	/** Division (A / B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer / integer", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Integer")
	static int32 Divide_IntInt(int32 A, int32 B = 1);

	/** Modulo (A % B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "% (integer)", CompactNodeTitle = "%", Keywords = "% modulus"), Category="Math|Integer")
	static int32 Percent_IntInt(int32 A, int32 B = 1);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer + integer", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Add_IntInt(int32 A, int32 B = 1);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer - integer", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Integer")
	static int32 Subtract_IntInt(int32 A, int32 B = 1);

	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer < integer", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Integer")
	static bool Less_IntInt(int32 A, int32 B);

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer > integer", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Integer")
	static bool Greater_IntInt(int32 A, int32 B);

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer <= integer", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Integer")
	static bool LessEqual_IntInt(int32 A, int32 B);

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer >= integer", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Integer")
	static bool GreaterEqual_IntInt(int32 A, int32 B);

	/** Returns true if A is equal to B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (integer)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Integer")
	static bool EqualEqual_IntInt(int32 A, int32 B);

	/** Returns true if A is not equal to B (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (integer)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Integer")
	static bool NotEqual_IntInt(int32 A, int32 B);

	/** Returns true if value is between Min and Max (V >= Min && V <= Max)
	 * If InclusiveMin is true, value needs to be equal or larger than Min, else it needs to be larger
	 * If InclusiveMax is true, value needs to be smaller or equal than Max, else it needs to be smaller
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "InRange (integer)", Min = "0", Max = "10"), Category = "Math|Integer")
	static bool InRange_IntInt(int32 Value, int32 Min, int32 Max, bool InclusiveMin = true, bool InclusiveMax = true);

	/** Bitwise AND (A & B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise AND", CompactNodeTitle = "&", Keywords = "& and", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 And_IntInt(int32 A, int32 B);

	/** Bitwise XOR (A ^ B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise XOR", CompactNodeTitle = "^", Keywords = "^ xor"), Category="Math|Integer")
	static int32 Xor_IntInt(int32 A, int32 B);

	/** Bitwise OR (A | B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise OR", CompactNodeTitle = "|", Keywords = "| or", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Or_IntInt(int32 A, int32 B);

	/** Bitwise NOT (~A) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitwise NOT", CompactNodeTitle = "~", Keywords = "~ not"), Category = "Math|Integer")
	static int32 Not_Int(int32 A);

	/** Sign (integer, returns -1 if A < 0, 0 if A is zero, and +1 if A > 0) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sign (integer)"), Category="Math|Integer")
	static int32 SignOfInteger(int32 A);

	/** Returns a uniformly distributed random number between 0 and Max - 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static int32 RandomInteger(int32 Max);

	/** Return a random integer between Min and Max (>= Min and <= Max) */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta = (NotBlueprintThreadSafe))
	static int32 RandomIntegerInRange(int32 Min, int32 Max);

	/** Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Min (integer)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Min(int32 A, int32 B);

	/** Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Max (integer)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Max(int32 A, int32 B);

	/** Returns Value clamped to be between A and B (inclusive) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp (integer)"), Category="Math|Integer")
	static int32 Clamp(int32 Value, int32 Min, int32 Max);

	/** Returns the absolute (positive) value of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Absolute (integer)", CompactNodeTitle = "ABS"), Category="Math|Integer")
	static int32 Abs_Int(int32 A);

	//
	// Integer64 functions.
	//
	
	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer64 * integer64", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer64")
	static int64 Multiply_Int64Int64(int64 A, int64 B);

	/** Division (A / B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer64 / integer64", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Integer")
	static int64 Divide_Int64Int64(int64 A, int64 B = 1);
	
	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer64 + integer64", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer64")
	static int64 Add_Int64Int64(int64 A, int64 B = 1);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer64 - integer64", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Integer64")
	static int64 Subtract_Int64Int64(int64 A, int64 B = 1);

	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer64 < integer64", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Integer64")
	static bool Less_Int64Int64(int64 A, int64 B);

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer64 > integer64", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Integer64")
	static bool Greater_Int64Int64(int64 A, int64 B);

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer64 <= integer64", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Integer64")
	static bool LessEqual_Int64Int64(int64 A, int64 B);

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer64 >= integer64", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Integer64")
	static bool GreaterEqual_Int64Int64(int64 A, int64 B);

	/** Returns true if A is equal to B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (integer64)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Integer64")
	static bool EqualEqual_Int64Int64(int64 A, int64 B);

	/** Returns true if A is not equal to B (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (integer64)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Integer64")
	static bool NotEqual_Int64Int64(int64 A, int64 B);
	
	/** Returns true if value is between Min and Max (V >= Min && V <= Max)
	 * If InclusiveMin is true, value needs to be equal or larger than Min, else it needs to be larger
	 * If InclusiveMax is true, value needs to be smaller or equal than Max, else it needs to be smaller
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "InRange (integer64)", Min = "0", Max = "10"), Category = "Math|Integer64")
	static bool InRange_Int64Int64(int64 Value, int64 Min, int64 Max, bool InclusiveMin = true, bool InclusiveMax = true);

	/** Bitwise AND (A & B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise AND", CompactNodeTitle = "&", Keywords = "& and", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer64")
	static int64 And_Int64Int64(int64 A, int64 B);

	/** Bitwise XOR (A ^ B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise XOR", CompactNodeTitle = "^", Keywords = "^ xor"), Category="Math|Integer64")
	static int64 Xor_Int64Int64(int64 A, int64 B);

	/** Bitwise OR (A | B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise OR", CompactNodeTitle = "|", Keywords = "| or", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer64")
	static int64 Or_Int64Int64(int64 A, int64 B);

	/** Bitwise NOT (~A) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitwise NOT", CompactNodeTitle = "~", Keywords = "~ not"), Category = "Math|Integer64")
	static int64 Not_Int64(int64 A);

	/** Sign (integer64, returns -1 if A < 0, 0 if A is zero, and +1 if A > 0) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sign (integer64)"), Category="Math|Integer64")
	static int64 SignOfInteger64(int64 A);

	/** Returns a uniformly distributed random number between 0 and Max - 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static int64 RandomInteger64(int64 Max);

	/** Return a random integer64 between Min and Max (>= Min and <= Max) */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta = (NotBlueprintThreadSafe))
	static int64 RandomInteger64InRange(int64 Min, int64 Max);

	/** Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Min (integer64)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer64")
	static int64 MinInt64(int64 A, int64 B);

	/** Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Max (integer64)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer64")
	static int64 MaxInt64(int64 A, int64 B);

	/** Returns Value clamped to be between A and B (inclusive) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp (integer64)"), Category="Math|Integer64")
	static int64 ClampInt64(int64 Value, int64 Min, int64 Max);

	/** Returns the absolute (positive) value of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Absolute (integer64)", CompactNodeTitle = "ABS"), Category="Math|Integer64")
	static int64 Abs_Int64(int64 A);

	//
	// Float functions.
	//

	/** Power (Base to the Exp-th power) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Power" ), Category="Math|Float")
	static float MultiplyMultiply_FloatFloat(float Base, float Exp);

	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float * float", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float Multiply_FloatFloat(float A, float B);

	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int * float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Float")
	static float Multiply_IntFloat(int32 A, float B);

	/** Division (A / B) */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "float / float", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Float")
	static float Divide_FloatFloat(float A, float B = 1.f);
	
	static float GenericDivide_FloatFloat(float A, float B);

	/** Custom thunk to allow script stack trace in case of divide by zero */
	DECLARE_FUNCTION(execDivide_FloatFloat)
	{
		P_GET_PROPERTY(UFloatProperty, A);
		P_GET_PROPERTY(UFloatProperty, B);

		P_FINISH;

		if (B == 0.f)
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Divide by zero detected: %f / 0\n%s"), A, *Stack.GetStackTrace()), ELogVerbosity::Warning);
			*(float*)RESULT_PARAM = 0;
			return;
		}

		*(float*)RESULT_PARAM = GenericDivide_FloatFloat(A, B);
	}

	/** Modulo (A % B) */
	UFUNCTION(BlueprintPure, CustomThunk, meta = (DisplayName = "% (float)", CompactNodeTitle = "%", Keywords = "% modulus"), Category = "Math|Float")
	static float Percent_FloatFloat(float A, float B = 1.f);

	static float GenericPercent_FloatFloat(float A, float B);

	/** Custom thunk to allow script stack trace in case of modulo by zero */
	DECLARE_FUNCTION(execPercent_FloatFloat)
	{
		P_GET_PROPERTY(UFloatProperty, A);
		P_GET_PROPERTY(UFloatProperty, B);

		P_FINISH;

		if (B == 0.f)
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Modulo by zero detected: %f %% 0\n%s"), A, *Stack.GetStackTrace()), ELogVerbosity::Warning);
			*(float*)RESULT_PARAM = 0;
			return;
		}

		*(float*)RESULT_PARAM = GenericPercent_FloatFloat(A, B);
	}

	/** Returns the fractional part of a float. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Fraction(float A);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float + float", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float Add_FloatFloat(float A, float B = 1.f);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float - float", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Float")
	static float Subtract_FloatFloat(float A, float B = 1.f);

	/** Returns true if A is Less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float < float", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Float")
	static bool Less_FloatFloat(float A, float B);

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float > float", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Float")
	static bool Greater_FloatFloat(float A, float B);

	/** Returns true if A is Less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float <= float", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Float")
	static bool LessEqual_FloatFloat(float A, float B);

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float >= float", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Float")
	static bool GreaterEqual_FloatFloat(float A, float B);

	/** Returns true if A is exactly equal to B (A == B)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (float)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Float")
	static bool EqualEqual_FloatFloat(float A, float B);

	/** Returns true if A is nearly equal to B (|A - B| < ErrorTolerance) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Nearly Equal (float)", Keywords = "== equal"), Category="Math|Float")
	static bool NearlyEqual_FloatFloat(float A, float B, float ErrorTolerance = 1.e-6f);

	/** Returns true if A does not equal B (A != B)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (float)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Float")
	static bool NotEqual_FloatFloat(float A, float B);

	/** Returns true if value is between Min and Max (V >= Min && V <= Max)
	 * If InclusiveMin is true, value needs to be equal or larger than Min, else it needs to be larger
	 * If InclusiveMax is true, value needs to be smaller or equal than Max, else it needs to be smaller
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "InRange (float)", Min="0.0", Max="1.0"), Category="Math|Float")
	static bool InRange_FloatFloat(float Value, float Min, float Max, bool InclusiveMin = true, bool InclusiveMax = true);

	/** Returns the hypotenuse of a right-angled triangle given the width and height. */
	UFUNCTION(BlueprintPure, meta=(Keywords = "pythagorean theorem"), Category = "Math|Float")
	static float Hypotenuse(float Width, float Height);
	
	/** Snaps a value to the nearest grid multiple. E.g.,
	 *		Location = 5.1, GridSize = 10.0 : return value = 10.0
	 * If GridSize is 0 Location is returned
	 * if GridSize is very small precision issues may occur.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Snap to grid (float)"), Category = "Math|Float")
	static float GridSnap_Float(float Location, float GridSize);

	/** Returns the absolute (positive) value of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Absolute (float)", CompactNodeTitle = "ABS"), Category="Math|Float")
	static float Abs(float A);

	/** Returns the sine of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sin (Radians)", CompactNodeTitle = "SIN", Keywords = "sine"), Category="Math|Trig")
	static float Sin(float A);

	/** Returns the inverse sine (arcsin) of A (result is in Radians) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Asin (Radians)", CompactNodeTitle = "ASIN", Keywords = "sine"), Category="Math|Trig")
	static float Asin(float A);

	/** Returns the cosine of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Cos (Radians)", CompactNodeTitle = "COS"), Category="Math|Trig")
	static float Cos(float A);

	/** Returns the inverse cosine (arccos) of A (result is in Radians) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Acos (Radians)", CompactNodeTitle = "ACOS"), Category="Math|Trig")
	static float Acos(float A);

	/** Returns the tan of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Tan (Radians)", CompactNodeTitle = "TAN"), Category="Math|Trig")
	static float Tan(float A);

	/** Returns the inverse tan (atan) (result is in Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Atan (Radians)"), Category="Math|Trig")
	static float Atan(float A);

	/** Returns the inverse tan (atan2) of A/B (result is in Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Atan2 (Radians)"), Category="Math|Trig")
	static float Atan2(float A, float B);

	/** Returns exponential(e) to the power A (e^A)*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(CompactNodeTitle = "e"))
	static float Exp(float A);

	/** Returns log of A base B (if B^R == A, returns R)*/
	UFUNCTION(BlueprintPure, Category = "Math|Float")
	static float Log(float A, float Base = 1.f);

	/** Returns natural log of A (if e^R == A, returns R)*/
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Loge(float A);

	/** Returns square root of A*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(Keywords = "square root", CompactNodeTitle = "SQRT"))
	static float Sqrt(float A);

	/** Returns square of A (A*A)*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(CompactNodeTitle = "^2"))
	static float Square(float A);

	/** Returns a random float between 0 and 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static float RandomFloat();

	/** Generate a random number between Min and Max */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static float RandomFloatInRange(float Min, float Max);

	/** Returns the value of PI */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get PI", CompactNodeTitle = "PI"), Category="Math|Trig")
	static float GetPI();

	/** Returns the value of TAU (= 2 * PI) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get TAU", CompactNodeTitle = "TAU"), Category="Math|Trig")
	static float GetTAU();

	/** Returns radians value based on the input degrees */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Degrees To Radians", CompactNodeTitle = "D2R"), Category="Math|Trig")
	static float DegreesToRadians(float A);

	/** Returns degrees value based on the input radians */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Radians To Degrees", CompactNodeTitle = "R2D"), Category="Math|Trig")
	static float RadiansToDegrees(float A);

	/** Returns the sin of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sin (Degrees)", CompactNodeTitle = "SINd", Keywords = "sine"), Category="Math|Trig")
	static float DegSin(float A);

	/** Returns the inverse sin (arcsin) of A (result is in Degrees) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Asin (Degrees)", CompactNodeTitle = "ASINd", Keywords = "sine"), Category="Math|Trig")
	static float DegAsin(float A);

	/** Returns the cos of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Cos (Degrees)", CompactNodeTitle = "COSd"), Category="Math|Trig")
	static float DegCos(float A);

	/** Returns the inverse cos (arccos) of A (result is in Degrees) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Acos (Degrees)", CompactNodeTitle = "ACOSd"), Category="Math|Trig")
	static float DegAcos(float A);

	/** Returns the tan of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Tan (Degrees)", CompactNodeTitle = "TANd"), Category="Math|Trig")
	static float DegTan(float A);

	/** Returns the inverse tan (atan) (result is in Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Atan (Degrees)"), Category="Math|Trig")
	static float DegAtan(float A);

	/** Returns the inverse tan (atan2) of A/B (result is in Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Atan2 (Degrees)"), Category="Math|Trig")
	static float DegAtan2(float A, float B);

	/** 
	 * Clamps an arbitrary angle to be between the given angles.  Will clamp to nearest boundary.
	 * 
	 * @param MinAngleDegrees	"from" angle that defines the beginning of the range of valid angles (sweeping clockwise)
	 * @param MaxAngleDegrees	"to" angle that defines the end of the range of valid angles
	 * @return Returns clamped angle in the range -180..180.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp Angle"), Category="Math|Float")
	static float ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees);

	/** Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Min (float)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float FMin(float A, float B);

	/** Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Max (float)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float FMax(float A, float B);

	/** Returns Value clamped between A and B (inclusive) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp (float)", Min="0.0", Max="1.0"), Category="Math|Float")
	static float FClamp(float Value, float Min, float Max);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static void MaxOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMaxValue, int32& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static void MinOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMinValue, int32& MinValue);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static void MaxOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMaxValue, float& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static void MinOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMinValue, float& MinValue);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Byte")
	static void MaxOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMaxValue, uint8& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Byte")
	static void MinOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMinValue, uint8& MinValue);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Lerp(float A, float B, float Alpha);
	
	UE_DEPRECATED(4.19, "Use NormalizeToRange instead")
	static float InverseLerp(float A, float B, float Value);

	/** Easeing  between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Ease", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static float Ease(float A, float B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Rounds A to the nearest integer */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static int32 Round(float A);

	/** Rounds A to the largest previous integer */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Floor"), Category="Math|Float")
	static int32 FFloor(float A);
	
	/** Rounds A to an integer with truncation towards zero.  (e.g. -1.7 truncated to -1, 2.8 truncated to 2) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Truncate", BlueprintAutocast), Category="Math|Float")
	static int32 FTrunc(float A);

	/** Rounds A to the nearest 32 bit integer then upconverts to 64 bit integer */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Round to Int64"), Category = "Math|Float")
	static int64 Round64(float A);

	/** Rounds A to the largest previous 32 bit integer then upconverts to 64 bit integer */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Floor to Int64"), Category = "Math|Float")
	static int64 FFloor64(float A);

	/** Rounds A to an 32 bit integer with truncation towards zero then upconverts to 64 bit integer.  (e.g. -1.7 truncated to -1, 2.8 truncated to 2) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Truncate to Int64", BlueprintAutocast), Category = "Math|Float")
	static int64 FTrunc64(float A);

	/** Rounds A to the smallest following 32 bit integer then upconverts to 64 bit integer */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Floor to Int64"), Category = "Math|Float")
	static int64 FCeil64(float A);

	/** Rounds A to the smallest following integer */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static int32 FCeil(float A);

	/** Returns the number of times Divisor will go into Dividend (i.e., Dividend divided by Divisor), as well as the remainder */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Division (whole and remainder)"), Category="Math|Float")
	static int32 FMod(float Dividend, float Divisor, float& Remainder);

	/** Sign (float, returns -1 if A < 0, 0 if A is zero, and +1 if A > 0) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sign (float)"), Category="Math|Float")
	static float SignOfFloat(float A);

	/** Returns Value normalized to the given range.  (e.g. 20 normalized to the range 10->50 would result in 0.25) */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float NormalizeToRange(float Value, float RangeMin, float RangeMax);

	/** Returns Value mapped from one range into another.  (e.g. 20 normalized from the range 10->50 to 20->40 would result in 25) */
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(Keywords = "get mapped value"))
	static float MapRangeUnclamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB);

	/** Returns Value mapped from one range into another where the Value is clamped to the Input Range.  (e.g. 0.5 normalized from the range 0->1 to 0->50 would result in 25) */
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(Keywords = "get mapped value"))
	static float MapRangeClamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB);
	
	/** Multiplies the input value by pi. */
	UFUNCTION(BlueprintPure, meta=(Keywords = "* multiply"), Category="Math|Float")
	static float MultiplyByPi(float Value);

	/** Interpolate between A and B, applying an ease in/out function.  Exp controls the degree of the curve. */
	UFUNCTION(BlueprintPure, Category = "Math|Float")
	static float FInterpEaseInOut(float A, float B, float Alpha, float Exponent);

	/**
	* Simple function to create a pulsating scalar value
	*
	* @param  InCurrentTime  Current absolute time
	* @param  InPulsesPerSecond  How many full pulses per second?
	* @param  InPhase  Optional phase amount, between 0.0 and 1.0 (to synchronize pulses)
	*
	* @return  Pulsating value (0.0-1.0)
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Float")
	static float MakePulsatingValue(float InCurrentTime, float InPulsesPerSecond = 1.0f, float InPhase = 0.0f);

	/** 
	 * Returns a new rotation component value
	 *
	 * @param InCurrent is the current rotation value
	 * @param InDesired is the desired rotation value
	 * @param  is the rotation amount to apply
	 *
	 * @return a new rotation component value clamped in the range (-360,360)
	 */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float FixedTurn(float InCurrent, float InDesired, float InDeltaRate);


	//
	// Vector2D constants - exposed for scripting
	//

	/** 2D one vector constant (1,1) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "One", ScriptConstantHost = "Vector2D"), Category = "Math|Vector2D")
	static FVector2D Vector2D_One();

	/** 2D unit vector constant along the 45 degree angle or symmetrical positive axes (sqrt(.5),sqrt(.5)) or (.707,.707). https://en.wikipedia.org/wiki/Unit_vector */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Unit45Deg", ScriptConstantHost = "Vector2D"), Category = "Math|Vector2D")
	static FVector2D Vector2D_Unit45Deg();

	/** 2D zero vector constant (0,0) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Zero", ScriptConstantHost = "Vector2D"), Category = "Math|Vector2D")
	static FVector2D Vector2D_Zero();


	//
	// Vector2D functions
	//

	/** Makes a 2d vector {X, Y} */
	UFUNCTION(BlueprintPure, Category = "Math|Vector2D", meta = (Keywords = "construct build", NativeMakeFunc))
	static FVector2D MakeVector2D(float X, float Y);

	/** Breaks a 2D vector apart into X, Y. */
	UFUNCTION(BlueprintPure, Category = "Math|Vector2D", meta = (NativeBreakFunc))
	static void BreakVector2D(FVector2D InVec, float& X, float& Y);

	/** Convert a Vector2D to a Vector */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Vector (Vector2D)", CompactNodeTitle = "->", ScriptMethod = "Vector", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FVector Conv_Vector2DToVector(FVector2D InVector2D, float Z = 0);

	/** Convert a Vector2D to a Vector */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To IntPoint (Vector2D)", CompactNodeTitle = "->", ScriptMethod = "IntPoint", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FIntPoint Conv_Vector2DToIntPoint(FVector2D InVector2D);

	/** Returns addition of Vector A and Vector B (A + B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d + vector2d", CompactNodeTitle = "+", ScriptMethod = "Add", ScriptOperator = "+;+=", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Vector2D")
	static FVector2D Add_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns Vector A added by B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d + float", CompactNodeTitle = "+", ScriptMethod = "AddFloat", ScriptOperator = "+;+=", Keywords = "+ add plus"), Category = "Math|Vector2D")
	static FVector2D Add_Vector2DFloat(FVector2D A, float B);

	/** Returns subtraction of Vector B from Vector A (A - B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d - vector2d", CompactNodeTitle = "-", ScriptMethod = "Subtract", ScriptOperator = "-;-=", Keywords = "- subtract minus"), Category = "Math|Vector2D")
	static FVector2D Subtract_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns Vector A subtracted by B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d - float", CompactNodeTitle = "-", ScriptMethod = "SubtractFloat", ScriptOperator = "-;-=", Keywords = "- subtract minus"), Category = "Math|Vector2D")
	static FVector2D Subtract_Vector2DFloat(FVector2D A, float B);

	/** Element-wise Vector multiplication (Result = {A.x*B.x, A.y*B.y}) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d * vector2d", CompactNodeTitle = "*", ScriptMethod = "Multiply", ScriptOperator = "*;*=", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Vector2D")
	static FVector2D Multiply_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns Vector A scaled by B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d * float", CompactNodeTitle = "*", ScriptMethod = "MultiplyFloat", ScriptOperator = "*;*=", Keywords = "* multiply"), Category = "Math|Vector2D")
	static FVector2D Multiply_Vector2DFloat(FVector2D A, float B);

	/** Element-wise Vector divide (Result = {A.x/B.x, A.y/B.y}) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d / vector2d", CompactNodeTitle = "/", ScriptMethod = "Divide", ScriptOperator = "/;/=", Keywords = "/ divide division"), Category = "Math|Vector2D")
	static FVector2D Divide_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns Vector A divided by B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d / float", CompactNodeTitle = "/", ScriptMethod = "DivideFloat", ScriptOperator = "/;/=", Keywords = "/ divide division"), Category = "Math|Vector2D")
	static FVector2D Divide_Vector2DFloat(FVector2D A, float B = 1.f);

	/** Returns true if vector A is equal to vector B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal Exactly (Vector2D)", CompactNodeTitle = "===", ScriptMethod = "Equals", ScriptOperator = "==", Keywords = "== equal"), Category="Math|Vector")
	static bool EqualExactly_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns true if vector2D A is equal to vector2D B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (Vector2D)", CompactNodeTitle = "==", ScriptMethod = "IsNearEqual", Keywords = "== equal"), Category = "Math|Vector2D")
	static bool EqualEqual_Vector2DVector2D(FVector2D A, FVector2D B, float ErrorTolerance = 1.e-4f);

	/** Returns true if vector2D A is not equal to vector2D B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal Exactly (Vector2D)", CompactNodeTitle = "!==", ScriptMethod = "NotEqual", ScriptOperator = "!=", Keywords = "!= not equal"), Category = "Math|Vector2D")
	static bool NotEqualExactly_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns true if vector2D A is not equal to vector2D B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal (Vector2D)", CompactNodeTitle = "!=", ScriptMethod = "IsNotNearEqual", Keywords = "!= not equal"), Category = "Math|Vector2D")
	static bool NotEqual_Vector2DVector2D(FVector2D A, FVector2D B, float ErrorTolerance = 1.e-4f);

	/** Gets a negated copy of the vector. */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "Negated", ScriptOperator = "neg"), Category = "Math|Vector2D")
	static FVector2D Negated2D(const FVector2D& A);

	/**
	 * Set the values of the vector directly.
	 *
	 * @param InX New X coordinate.
	 * @param InY New Y coordinate.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "Set"), Category = "Math|Vector2D")
	static void Set2D(UPARAM(ref) FVector2D& A, float X, float Y);

	/**
	 * Creates a copy of this vector with both axes clamped to the given range.
	 * @return New vector with clamped axes.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "ClampedAxes"), Category = "Math|Vector2D")
	static FVector2D ClampAxes2D(FVector2D A, float MinAxisVal, float MaxAxisVal);

	/** Returns the cross product of two 2d vectors - see  http://mathworld.wolfram.com/CrossProduct.html */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Cross Product (2D)", CompactNodeTitle = "cross", ScriptMethod = "Cross", ScriptOperator = "^"), Category = "Math|Vector2D")
	static float CrossProduct2D(FVector2D A, FVector2D B);

	/**
	 * Distance between two 2D points.
	 *
	 * @param V1 The first point.
	 * @param V2 The second point.
	 * @return The distance between two 2D points.
	 */
	UFUNCTION(BlueprintPure, meta = (Keywords = "magnitude", ScriptMethod = "Distance"), Category = "Math|Vector2D")
	static float Distance2D(FVector2D V1, FVector2D V2);

	/**
	 * Squared distance between two 2D points.
	 *
	 * @param V1 The first point.
	 * @param V2 The second point.
	 * @return The squared distance between two 2D points.
	 */
	UFUNCTION(BlueprintPure, meta = (Keywords = "magnitude", ScriptMethod = "DistanceSquared"), Category = "Math|Vector2D")
	static float DistanceSquared2D(FVector2D V1, FVector2D V2);

	/** Returns the dot product of two 2d vectors - see http://mathworld.wolfram.com/DotProduct.html */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Dot Product (2D)", CompactNodeTitle = "dot", ScriptMethod = "Dot", ScriptOperator = "|"), Category = "Math|Vector2D")
	static float DotProduct2D(FVector2D A, FVector2D B);

	/**
	* Get a copy of this vector with absolute value of each component.
	*
	* @return A copy of this vector with absolute value of each component.
	*/
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "GetAbs"), Category = "Math|Vector2D")
	static FVector2D GetAbs2D(FVector2D A);

	/**
	 * Get the maximum absolute value of the vector's components.
	 *
	 * @return The maximum absolute value of the vector's components.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "GetAbsMax"), Category = "Math|Vector2D")
	static float GetAbsMax2D(FVector2D A);

	/**
	 * Get the maximum value of the vector's components.
	 *
	 * @return The maximum value of the vector's components.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "GetMax"), Category = "Math|Vector2D")
	static float GetMax2D(FVector2D A);

	/**
	 * Get the minimum value of the vector's components.
	 *
	 * @return The minimum value of the vector's components.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "GetMin"), Category = "Math|Vector2D")
	static float GetMin2D(FVector2D A);

	/**
	 * Rotates around axis (0,0,1)
	 *
	 * @param AngleDeg Angle to rotate (in degrees)
	 * @return Rotated Vector
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "GetRotated"), Category = "Math|Vector2D")
	static FVector2D GetRotated2D(FVector2D A, float AngleDeg);

	/**
	 * Checks whether vector is near to zero within a specified tolerance.
	 *
	 * @param Tolerance Error tolerance.
	 * @return true if vector is in tolerance to zero, otherwise false.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "IsNearlyZero"), Category = "Math|Vector2D")
	static bool IsNearlyZero2D(const FVector2D& A, float Tolerance = 1.e-4f);

	/**
	 * Checks whether all components of the vector are exactly zero.
	 *
	 * @return true if vector is exactly zero, otherwise false.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "IsZero"), Category = "Math|Vector2D")
	static bool IsZero2D(const FVector2D& A);

	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(ScriptMethod="InterpTo", Keywords="position"))
	static FVector2D Vector2DInterpTo(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed);
	
	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(ScriptMethod="InterpToConstant", Keywords="position"))
	static FVector2D Vector2DInterpTo_Constant(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed);

	/**
	 * Gets a normalized copy of the vector, checking it is safe to do so based on the length.
	 * Returns zero vector if vector length is too small to safely normalize.
	 *
	 * @param Tolerance Minimum squared length of vector for normalization.
	 * @return A normalized copy of the vector if safe, (0,0) otherwise.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Normal Safe (Vector2D)", Keywords = "Unit Vector", ScriptMethod = "Normal"), Category = "Math|Vector2D")
	static FVector2D NormalSafe2D(FVector2D A, float Tolerance = 1.e-8f);

	/** Returns a unit normal version of the 2D vector */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Normalize2D", Keywords = "Unit Vector", ScriptMethod = "NormalUnsafe"), Category = "Math|Vector2D")
	static FVector2D Normal2D(FVector2D A);

	/**
	 * Normalize this vector in-place if it is large enough, set it to (0,0) otherwise.
	 *
	 * @param Tolerance Minimum squared length of vector for normalization.
	 * @see NormalSafe2D()
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Normalize In Place (Vector2D)", Keywords = "Unit Vector", ScriptMethod = "Normalize"), Category = "Math|Vector2D")
	static void Normalize2D(UPARAM(ref) FVector2D& A, float Tolerance = 1.e-8);

	/** Converts spherical coordinates on the unit sphere into a Cartesian unit length vector. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Spherical2D To Unit Cartesian", Keywords = "Unit Vector", ScriptMethod = "SphericalToUnitCartesian"), Category = "Math|Vector2D")
	static FVector Spherical2DToUnitCartesian(FVector2D A);

	/**
	 * Util to convert this vector into a unit direction vector and its original length.
	 *
	 * @param OutDir Reference passed in to store unit direction vector.
	 * @param OutLength Reference passed in to store length of the vector.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Direction And Length", ScriptMethod = "ToDirectionAndLength"), Category = "Math|Vector2D")
	static void ToDirectionAndLength2D(FVector2D A, FVector2D &OutDir, float &OutLength);

	/**
	 * Get this vector as a vector where each component has been rounded to the nearest int.
	 *
	 * @return New FVector2D from this vector that is rounded.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Rounded (Vector2D)", ScriptMethod = "ToRounded"), Category = "Math|Vector2D")
	static FVector2D ToRounded2D(FVector2D A);

	/**
	* Get a copy of the vector as sign only.
	* Each component is set to +1 or -1, with the sign of zero treated as +1.
	*
	* @return A copy of the vector with each component set to +1 or -1
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To sign (+1/-1) 2D", ScriptMethod = "ToSign"), Category = "Math|Vector2D")
	static FVector2D ToSign2D(FVector2D A);

	/** Returns the length of a 2D Vector. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector2dLength", Keywords = "magnitude", ScriptMethod = "Length"), Category = "Math|Vector2D")
	static float VSize2D(FVector2D A);

	/** Returns the squared length of a 2D Vector. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector2dLengthSquared", Keywords = "magnitude", ScriptMethod = "LengthSquared"), Category = "Math|Vector2D")
	static float VSize2DSquared(FVector2D A);


	//
	// Vector (3D) constants - exposed for scripting
	//

	/** 3D vector zero constant (0,0,0) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Zero", ScriptConstantHost = "Vector"), Category = "Math|Vector")
	static FVector Vector_Zero();

	/** 3D vector one constant (1,1,1) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "One", ScriptConstantHost = "Vector"), Category = "Math|Vector")
	static FVector Vector_One();

	/** 3D vector Unreal forward direction constant (1,0,0) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Forward", ScriptConstantHost = "Vector"), Category = "Math|Vector")
	static FVector Vector_Forward();

	/** 3D vector Unreal backward direction constant (-1,0,0) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Backward", ScriptConstantHost = "Vector"), Category = "Math|Vector")
	static FVector Vector_Backward();

	/** 3D vector Unreal up direction constant (0,0,1) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Up", ScriptConstantHost = "Vector"), Category = "Math|Vector")
	static FVector Vector_Up();

	/** 3D vector Unreal down direction constant (0,0,-1) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Down", ScriptConstantHost = "Vector"), Category = "Math|Vector")
	static FVector Vector_Down();

	/** 3D vector Unreal right direction constant (0,1,0) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Right", ScriptConstantHost = "Vector"), Category = "Math|Vector")
	static FVector Vector_Right();

	/** 3D vector Unreal left direction constant (0,-1,0) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Left", ScriptConstantHost = "Vector"), Category = "Math|Vector")
	static FVector Vector_Left();

	//
	// Vector (3D) functions.
	//

	/** Makes a vector {X, Y, Z} */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="construct build", NativeMakeFunc))
	static FVector MakeVector(float X, float Y, float Z);

	/** Creates a directional vector from rotation values {Pitch, Yaw} supplied in degrees with specified Length*/	
	UFUNCTION(BlueprintPure, Category = "Math|Vector", meta = (Keywords = "rotation rotate"))
	static FVector CreateVectorFromYawPitch(float Yaw, float Pitch, float Length = 1.0f );

	/**
	 * Assign the values of the supplied vector.
	 *
	 * @param InVector Vector to copy values from.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "Assign"), Category = "Math|Vector")
	static void Vector_Assign(UPARAM(ref) FVector& A, const FVector& InVector);

	/**
	 * Set the values of the vector directly.
	 *
	 * @param InX New X coordinate.
	 * @param InY New Y coordinate.
	 * @param InZ New Z coordinate.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "Set"), Category = "Math|Vector")
	static void Vector_Set(UPARAM(ref) FVector& A, float X, float Y, float Z);

	/** Breaks a vector apart into X, Y, Z */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(NativeBreakFunc))
	static void BreakVector(FVector InVec, float& X, float& Y, float& Z);

	/** Converts a vector to LinearColor */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToLinearColor (Vector)", CompactNodeTitle = "->", ScriptMethod = "LinearColor", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FLinearColor Conv_VectorToLinearColor(FVector InVec);

	/** Convert a vector to a transform. Uses vector as location */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToTransform (Vector)", CompactNodeTitle = "->", ScriptMethod = "Transform", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FTransform Conv_VectorToTransform(FVector InLocation);
	
	/** Convert a Vector to a Vector2D */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToVector2D (Vector)", CompactNodeTitle = "->", ScriptMethod = "Vector2D", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FVector2D Conv_VectorToVector2D(FVector InVector);

	/**
	 * Return the FRotator orientation corresponding to the direction in which the vector points.
	 * Sets Yaw and Pitch to the proper numbers, and sets Roll to zero because the roll can't be determined from a vector.
	 *
	 * @return FRotator from the Vector's direction, without any roll.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotationFromXVector", ScriptMethod = "Rotator", Keywords="rotation rotate cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FRotator Conv_VectorToRotator(FVector InVec);

	/** Create a rotation from an this axis and supplied angle (in degrees) */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "RotatorFromAxisAndAngle", Keywords="make construct build rotate rotation"), Category="Math|Vector")
	static FRotator RotatorFromAxisAndAngle(FVector Axis, float Angle);

	/**
	 * Return the Quaternion orientation corresponding to the direction in which the vector points.
	 * Similar to the FRotator version, returns a result without roll such that it preserves the up vector.
	 *
	 * @note If you don't care about preserving the up vector and just want the most direct rotation, you can use the faster
	 * 'FQuat::FindBetweenVectors(FVector::ForwardVector, YourVector)' or 'FQuat::FindBetweenNormals(...)' if you know the vector is of unit length.
	 *
	 * @return Quaternion from the Vector's direction, without any roll.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "To Quaterion (Vector)", ScriptMethod = "Quaternion", Keywords="rotation rotate cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FQuat Conv_VectorToQuaterion(FVector InVec);

	/** Vector addition */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector + vector", CompactNodeTitle = "+", ScriptMethod = "Add", ScriptOperator = "+;+=", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Vector")
	static FVector Add_VectorVector(FVector A, FVector B);

	/** Adds a float to each component of a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector + float", CompactNodeTitle = "+", ScriptMethod = "AddFloat", Keywords = "+ add plus"), Category="Math|Vector")
	static FVector Add_VectorFloat(FVector A, float B);
	
	/** Adds an integer to each component of a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector + int", CompactNodeTitle = "+", ScriptMethod = "AddInt", Keywords = "+ add plus"), Category="Math|Vector")
	static FVector Add_VectorInt(FVector A, int32 B);

	/** Vector subtraction */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector - vector", CompactNodeTitle = "-", ScriptMethod = "Subtract", ScriptOperator = "-;-=", Keywords = "- subtract minus"), Category="Math|Vector")
	static FVector Subtract_VectorVector(FVector A, FVector B);

	/** Subtracts a float from each component of a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector - float", CompactNodeTitle = "-", ScriptMethod = "SubtractFloat", Keywords = "- subtract minus"), Category="Math|Vector")
	static FVector Subtract_VectorFloat(FVector A, float B);

	/** Subtracts an integer from each component of a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector - int", CompactNodeTitle = "-", ScriptMethod = "SubtractInt", Keywords = "- subtract minus"), Category="Math|Vector")
	static FVector Subtract_VectorInt(FVector A, int32 B);

	/** Element-wise Vector multiplication (Result = {A.x*B.x, A.y*B.y, A.z*B.z}) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector * vector", CompactNodeTitle = "*", ScriptMethod = "Multiply", ScriptOperator = "*;*=", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Vector")
	static FVector Multiply_VectorVector(FVector A, FVector B);

	/** Scales Vector A by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector * float", CompactNodeTitle = "*", ScriptMethod = "MultiplyFloat", ScriptOperator = "*;*=", Keywords = "* multiply"), Category="Math|Vector")
	static FVector Multiply_VectorFloat(FVector A, float B);
	
	/** Scales Vector A by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector * int", CompactNodeTitle = "*", ScriptMethod = "MultiplyInt", Keywords = "* multiply"), Category="Math|Vector")
	static FVector Multiply_VectorInt(FVector A, int32 B);

	/** Element-wise Vector division (Result = {A.x/B.x, A.y/B.y, A.z/B.z}) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector / vector", CompactNodeTitle = "/", ScriptMethod = "Divide", ScriptOperator = "/;/=", Keywords = "/ divide division"), Category="Math|Vector")
	static FVector Divide_VectorVector(FVector A, FVector B = FVector(1.f,1.f,1.f));

	/** Vector divide by a float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector / float", CompactNodeTitle = "/", ScriptMethod = "DivideFloat", ScriptOperator = "/;/=", Keywords = "/ divide division"), Category="Math|Vector")
	static FVector Divide_VectorFloat(FVector A, float B = 1.f);

	/** Vector divide by an integer */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector / int", CompactNodeTitle = "/", ScriptMethod = "DivideInt", Keywords = "/ divide division"), Category="Math|Vector")
	static FVector Divide_VectorInt(FVector A, int32 B = 1);

	/** Negate a vector. */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "Negated", ScriptOperator = "neg"), Category="Math|Vector")
	static FVector NegateVector(FVector A);

	/** Returns true if vector A is equal to vector B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal Exactly (Vector)", CompactNodeTitle = "===", ScriptMethod = "Equals", ScriptOperator = "==", Keywords = "== equal"), Category="Math|Vector")
	static bool EqualExactly_VectorVector(FVector A, FVector B);

	/** Returns true if vector A is equal to vector B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Vector)", CompactNodeTitle = "==", ScriptMethod = "IsNearEqual", Keywords = "== equal"), Category="Math|Vector")
	static bool EqualEqual_VectorVector(FVector A, FVector B, float ErrorTolerance = 1.e-4f);

	/** Returns true if vector A is not equal to vector B (A != B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal Exactly (Vector)", CompactNodeTitle = "!==", ScriptMethod = "NotEqual", ScriptOperator = "!=", Keywords = "!= not equal"), Category = "Math|Vector2D")
	static bool NotEqualExactly_VectorVector(FVector A, FVector B);

	/** Returns true if vector A is not equal to vector B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Not Equal (Vector)", CompactNodeTitle = "!=", ScriptMethod = "IsNotNearEqual"), Category="Math|Vector")
	static bool NotEqual_VectorVector(FVector A, FVector B, float ErrorTolerance = 1.e-4f);

	/** Returns the dot product of two 3d vectors - see http://mathworld.wolfram.com/DotProduct.html */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Dot Product", CompactNodeTitle = "dot", ScriptMethod = "Dot", ScriptOperator = "|"), Category="Math|Vector" )
	static float Dot_VectorVector(FVector A, FVector B);

	/** Returns the cross product of two 3d vectors - see http://mathworld.wolfram.com/CrossProduct.html */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Cross Product", CompactNodeTitle = "cross", ScriptMethod = "Cross", ScriptOperator = "^"), Category="Math|Vector" )
	static FVector Cross_VectorVector(FVector A, FVector B);

	/** Returns result of vector A rotated by Rotator B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotateVector", ScriptMethod = "Rotate"), Category="Math|Vector")
	static FVector GreaterGreater_VectorRotator(FVector A, FRotator B);

	/** Returns result of vector A rotated by AngleDeg around Axis */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotateVectorAroundAxis", ScriptMethod = "RotateAngleAxis"), Category="Math|Vector")
	static FVector RotateAngleAxis(FVector InVect, float AngleDeg, FVector Axis);

	/** Returns result of vector A rotated by the inverse of Rotator B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "UnrotateVector", ScriptMethod = "Unrotate"), Category="Math|Vector")
	static FVector LessLess_VectorRotator(FVector A, FRotator B);

	/** When this vector contains Euler angles (degrees), ensure that angles are between +/-180 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "UnwindEuler"), Category = "Math|Vector")
	static void Vector_UnwindEuler(UPARAM(ref) FVector& A);

	/** Create a copy of this vector, with its magnitude/size/length clamped between Min and Max. */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ClampedSize"), Category="Math|Vector")
	static FVector ClampVectorSize(FVector A, float Min, float Max);

	/** Create a copy of this vector, with the 2D magnitude/size/length clamped between Min and Max. Z is unchanged. */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ClampedSize2D"), Category="Math|Vector")
	static FVector Vector_ClampSize2D(FVector A, float Min, float Max);

	/** Create a copy of this vector, with its maximum magnitude/size/length clamped to MaxSize. */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ClampedSizeMax"), Category="Math|Vector")
	static FVector Vector_ClampSizeMax(FVector A, float Max);

	/** Create a copy of this vector, with the maximum 2D magnitude/size/length clamped to MaxSize. Z is unchanged. */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ClampedSizeMax2D"), Category="Math|Vector")
	static FVector Vector_ClampSizeMax2D(FVector A, float Max);

	/** Find the minimum element (X, Y or Z) of a vector */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetMinElement"), Category="Math|Vector")
	static float GetMinElement(FVector A);

	/** Find the maximum element (X, Y or Z) of a vector */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetMaxElement"), Category="Math|Vector")
	static float GetMaxElement(FVector A);

	/** Find the maximum absolute element (abs(X), abs(Y) or abs(Z)) of a vector */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetAbsMax"), Category="Math|Vector")
	static float Vector_GetAbsMax(FVector A);

	/** Find the minimum absolute element (abs(X), abs(Y) or abs(Z)) of a vector */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetAbsMin"), Category="Math|Vector")
	static float Vector_GetAbsMin(FVector A);

	/**
	 * Get a copy of this vector with absolute value of each component.
	 *
	 * @return A copy of this vector with absolute value of each component.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetAbs"), Category="Math|Vector")
	static FVector Vector_GetAbs(FVector A);

	/** Find the minimum elements (X, Y and Z) between the two vector's components */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetMin"), Category="Math|Vector")
	static FVector Vector_ComponentMin(FVector A, FVector B);

	/** Find the maximum elements (X, Y and Z) between the two vector's components */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetMax"), Category="Math|Vector")
	static FVector Vector_ComponentMax(FVector A, FVector B);

	/**
	 * Get a copy of the vector as sign only.
	 * Each component is set to +1 or -1, with the sign of zero treated as +1.
	 *
	 * @param A copy of the vector with each component set to +1 or -1
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetSignVector"), Category="Math|Vector")
	static FVector Vector_GetSignVector(FVector A);

	/**
	 * Projects 2D components of vector based on Z.
	 *
	 * @return Projected version of vector based on Z.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetProjection"), Category="Math|Vector")
	static FVector Vector_GetProjection(FVector A);

	/**
	 * Convert a direction vector into a 'heading' angle.
	 *
	 * @return 'Heading' angle between +/-PI radians. 0 is pointing down +X.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "HeadingAngle"), Category="Math|Vector")
	static float Vector_HeadingAngle(FVector A);

	/**
	 * Returns the cosine of the angle between this vector and another projected onto the XY plane (no Z).
	 *
	 * @param B the other vector to find the 2D cosine of the angle with.
	 * @return The cosine.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "CosineAngle2D"), Category="Math|Vector")
	static float Vector_CosineAngle2D(FVector A, FVector B);

	/**
	 * Converts a vector containing degree values to a vector containing radian values.
	 *
	 * @return Vector containing radian values
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ToRadians"), Category="Math|Vector")
	static FVector Vector_ToRadians(FVector A);

	/**
	 * Converts a vector containing radian values to a vector containing degree values.
	 *
	 * @return Vector  containing degree values
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ToDegrees"), Category="Math|Vector")
	static FVector Vector_ToDegrees(FVector A);

	/** 
	 * Converts a Cartesian unit vector into spherical coordinates on the unit sphere.
	 * @return Output Theta will be in the range [0, PI], and output Phi will be in the range [-PI, PI]. 
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "UnitCartesianToSpherical"), Category="Math|Vector")
	static FVector2D Vector_UnitCartesianToSpherical(FVector A);

	/** Find the unit direction vector from one position to another or (0,0,0) if positions are the same. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Unit Direction (Vector)", ScriptMethod = "DirectionUnitTo", Keywords = "Unit Vector"), Category="Math|Vector")
	static FVector GetDirectionUnitVector(FVector From, FVector To);

	/** Breaks a vector apart into Yaw, Pitch rotation values given in degrees. (non-clamped) */
	UFUNCTION(BlueprintPure, Category = "Math|Vector", meta = (ScriptMethod = "GetYawPitch", NativeBreakFunc))
	static void GetYawPitchFromVector(FVector InVec, float& Yaw, float& Pitch);

	/** Breaks a direction vector apart into Azimuth (Yaw) and Elevation (Pitch) rotation values given in degrees. (non-clamped)
	 Relative to the provided reference frame (an Actor's WorldTransform for example) */
	UFUNCTION(BlueprintPure, Category = "Math|Vector", meta = (ScriptMethod = "GetAzimuthElevation", NativeBreakFunc))
	static void GetAzimuthAndElevation(FVector InDirection, const FTransform& ReferenceFrame, float& Azimuth, float& Elevation);

	/** Find the average of an array of vectors */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector GetVectorArrayAverage(const TArray<FVector>& Vectors);

	/** Rounds A to an integer with truncation towards zero for each element in a vector.  (e.g. -1.7 truncated to -1, 2.8 truncated to 2) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Truncate (Vector)", ScriptMethod = "Truncated", BlueprintAutocast), Category = "Math|Float")
	static FIntVector FTruncVector(const FVector& InVector);

	/**
	 * Distance between two points.
	 *
	 * @param V1 The first point.
	 * @param V2 The second point.
	 * @return The distance between two points.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Distance (Vector)", ScriptMethod = "Distance", Keywords = "magnitude"), Category = "Math|Vector")
	static float Vector_Distance(FVector V1, FVector V2);

	/**
	 * Squared distance between two points.
	 *
	 * @param V1 The first point.
	 * @param V2 The second point.
	 * @return The squared distance between two points.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Distance Squared (Vector)", ScriptMethod = "DistanceSquared", Keywords = "magnitude"), Category = "Math|Vector")
	static float Vector_DistanceSquared(FVector V1, FVector V2);

	/**
	* Euclidean distance between two points in the XY plane (ignoring Z).
	*
	* @param V1 The first point.
	* @param V2 The second point.
	* @return The distance between two points in the XY plane.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Distance2D (Vector)", ScriptMethod = "Distance2D", Keywords = "magnitude"), Category = "Math|Vector")
	static float Vector_Distance2D(FVector V1, FVector V2);

	/**
	* Squared euclidean distance between two points in the XY plane (ignoring Z).
	*
	* @param V1 The first point.
	* @param V2 The second point.
	* @return The distance between two points in the XY plane.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Distance2D Squared (Vector)", ScriptMethod = "Distance2DSquared", Keywords = "magnitude"), Category = "Math|Vector")
	static float Vector_Distance2DSquared(FVector V1, FVector V2);

	/** Returns the length of the vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorLength", ScriptMethod = "Length", Keywords="magnitude"), Category="Math|Vector")
	static float VSize(FVector A);

	/** Returns the squared length of the vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorLengthSquared", ScriptMethod = "LengthSquared", Keywords="magnitude"), Category="Math|Vector")
	static float VSizeSquared(FVector A);

	/** Returns the length of the vector's XY components. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorLengthXY", ScriptMethod = "Length2D", Keywords="magnitude"), Category="Math|Vector")
	static float VSizeXY(FVector A);

	/** Returns the squared length of the vector's XY components. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorLengthXYSquared", ScriptMethod = "Length2DSquared", Keywords="magnitude"), Category="Math|Vector")
	static float VSizeXYSquared(FVector A);

	/**
	 * Checks whether vector is near to zero within a specified tolerance.
	 *
	 * @param Tolerance Error tolerance.
	 * @return true if vector is in tolerance to zero, otherwise false.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "IsNearlyZero"), Category = "Math|Vector")
	static bool Vector_IsNearlyZero(const FVector& A, float Tolerance = 1.e-4f);

	/**
	 * Checks whether all components of the vector are exactly zero.
	 *
	 * @return true if vector is exactly zero, otherwise false.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "IsZero"), Category = "Math|Vector")
	static bool Vector_IsZero(const FVector& A);

	/**
	 * Determines if any component is not a number (NAN)
	 *
	 * @return true if one or more components is NAN, otherwise false.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "IsNAN"), Category = "Math|Vector")
	static bool Vector_IsNAN(const FVector& A);

	/**
	 * Checks whether all components of this vector are the same, within a tolerance.
	 *
	 * @param Tolerance Error tolerance.
	 * @return true if the vectors are equal within tolerance limits, false otherwise.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Uniform (Vector)", ScriptMethod = "IsUniform"), Category="Math|Vector")
	static bool Vector_IsUniform(const FVector& A, float Tolerance = 1.e-4f);

	/**
	 * Determines if vector is normalized / unit (length 1) within specified squared tolerance.
	 *
	 * @return true if unit, false otherwise.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Unit (Vector)", ScriptMethod = "IsUnit", Keywords="Unit Vector"), Category="Math|Vector")
	static bool Vector_IsUnit(const FVector& A, float SquaredLenthTolerance = 1.e-4f);

	/**
	 * Determines if vector is normalized / unit (length 1).
	 *
	 * @return true if normalized, false otherwise.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Normal (Vector)", ScriptMethod = "IsNormal", Keywords="Unit Vector"), Category="Math|Vector")
	static bool Vector_IsNormal(const FVector& A);

	/**
	 * Gets a normalized unit copy of the vector, ensuring it is safe to do so based on the length.
	 * Returns zero vector if vector length is too small to safely normalize.
	 *
	 * @param Tolerance Minimum squared vector length.
	 * @return A normalized copy if safe, (0,0,0) otherwise.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normalize", ScriptMethod = "Normal", Keywords="Unit Vector"), Category="Math|Vector")
	static FVector Normal(FVector A, float Tolerance = 1.e-4f);

	/**
	 * Gets a normalized unit copy of the 2D components of the vector, ensuring it is safe to do so. Z is set to zero. 
	 * Returns zero vector if vector length is too small to normalize.
	 *
	 * @param Tolerance Minimum squared vector length.
	 * @return Normalized copy if safe, (0,0,0) otherwise.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normalize 2D (Vector)", ScriptMethod = "Normal2D", Keywords="Unit Vector"), Category="Math|Vector")
	static FVector Vector_Normal2D(FVector A, float Tolerance = 1.e-4f);

	/**
	 * Calculates normalized unit version of vector without checking for zero length.
	 *
	 * @return Normalized version of vector.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normal unsafe (Vector)", ScriptMethod = "NormalUnsafe", Keywords="Unit Vector"), Category="Math|Vector")
	static FVector Vector_NormalUnsafe(const FVector& A);

	/**
	 * Normalize this vector in-place if it is large enough or set it to (0,0,0) otherwise.
	 *
	 * @param Tolerance Minimum squared length of vector for normalization.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Normalize In Place (Vector)", ScriptMethod = "Normalize", Keywords = "Unit Vector"), Category = "Math|Vector")
	static void Vector_Normalize(UPARAM(ref) FVector& A, float Tolerance = 1.e-8);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp (Vector)", ScriptMethod = "LerpTo"), Category="Math|Vector")
	static FVector VLerp(FVector A, FVector B, float Alpha);

	/** Easing between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Ease (Vector)", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static FVector VEase(FVector A, FVector B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(ScriptMethod = "InterpTo", Keywords="position"))
	static FVector VInterpTo(FVector Current, FVector Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Interpolation", meta = (ScriptMethod = "InterpToConstant", Keywords = "position"))
	static FVector VInterpTo_Constant(FVector Current, FVector Target, float DeltaTime, float InterpSpeed);

	/**
	* Uses a simple spring model to interpolate a vector from Current to Target.
	*
	* @param Current				Current value
	* @param Target					Target value
	* @param SpringState			Data related to spring model (velocity, error, etc..) - Create a unique variable per spring
	* @param Stiffness				How stiff the spring model is (more stiffness means more oscillation around the target value)
	* @param CriticalDampingFactor	How much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)
	* @param Mass					Multiplier that acts like mass on a spring
	*/
	UFUNCTION(BlueprintCallable,  meta = (ScriptMethod = "InterpSpringTo", Keywords = "position"), Category = "Math|Interpolation")
	static FVector VectorSpringInterp(FVector Current, FVector Target, UPARAM(ref) FVectorSpringState& SpringState, float Stiffness, float CriticalDampingFactor, float DeltaTime, float Mass = 1.f);

	/**
	 * Gets the reciprocal of this vector, avoiding division by zero.
	 * Zero components are set to BIG_NUMBER.
	 *
	 * @return Reciprocal of this vector.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Reciprocal (Vector)", ScriptMethod = "Reciprocal"), Category="Math|Vector")
	static FVector Vector_Reciprocal(const FVector& A);

	/** 
	 * Given a direction vector and a surface normal, returns the vector reflected across the surface normal.
	 * Produces a result like shining a laser at a mirror!
	 *
	 * @param Direction Direction vector the ray is coming from.
	 * @param SurfaceNormal A normal of the surface the ray should be reflected on.
	 *
	 * @returns Reflected vector.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "MirrorByVector", Keywords = "Reflection"), Category="Math|Vector")
	static FVector GetReflectionVector(FVector Direction, FVector SurfaceNormal);

	/** 
	 * Given a direction vector and a surface normal, returns the vector reflected across the surface normal.
	 * Produces a result like shining a laser at a mirror!
	 *
	 * @param InVect Direction vector the ray is coming from.
	 * @param InNormal A normal of the surface the ray should be reflected on.
	 *
	 * @returns Reflected vector.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector MirrorVectorByNormal(FVector InVect, FVector InNormal);

	/**
	 * Mirrors a vector about a plane.
	 *
	 * @param Plane Plane to mirror about.
	 * @return Mirrored vector.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "MirrorByPlane", Keywords = "Reflection"), Category="Math|Vector")
	static FVector Vector_MirrorByPlane(FVector A, const FPlane& InPlane);

	/**
	 * Gets a copy of this vector snapped to a grid.
	 *
	 * @param InGridSize Grid dimension / step.
	 * @return A copy of this vector snapped to a grid.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "SnappedToGrid", Keywords = "Bounding"), Category="Math|Vector")
	static FVector Vector_SnappedToGrid(FVector InVect, float InGridSize);

	/**
	 * Get a copy of this vector, clamped inside of an axis aligned cube centered at the origin.
	 *
	 * @param InRadius Half size of the cube (or radius of sphere circumscribed in the cube).
	 * @return A copy of this vector, bound by cube.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "BoundedToCube", Keywords = "Bounding"), Category="Math|Vector")
	static FVector Vector_BoundedToCube(FVector InVect, float InRadius);

	/**
	 * Add a vector to this and clamp the result to an axis aligned cube centered at the origin.
	 *
	 * @param InAddVect Vector to add.
	 * @param InRadius Half size of the cube.
	 */
	UFUNCTION(BlueprintCallable, meta=(ScriptMethod = "AddBounded", Keywords = "Bounding"), Category="Math|Vector")
	static void Vector_AddBounded(UPARAM(ref) FVector& A, FVector InAddVect, float InRadius);

	/** Get a copy of this vector, clamped inside of the specified axis aligned cube. */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "BoundedToBox", Keywords = "Bounding"), Category="Math|Vector")
	static FVector Vector_BoundedToBox(FVector InVect, FVector InBoxMin, FVector InBoxMax);

	/**
	 * Gets a copy of this vector projected onto the input vector, which is assumed to be unit length.
	 * 
	 * @param  InNormal Vector to project onto (assumed to be unit length).
	 * @return Projected vector.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ProjectOnToNormal", Keywords = "Project"), Category="Math|Vector")
	static FVector Vector_ProjectOnToNormal(FVector V, FVector InNormal);

	/**
	* Projects one vector (V) onto another (Target) and returns the projected vector.
	* If Target is nearly zero in length, returns the zero vector.
	*
	* @param  V Vector to project.
	* @param  Target Vector on which we are projecting.
	* @return V projected on to Target.
	*/
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ProjectOnTo", Keywords = "Project"), Category="Math|Vector")
	static FVector ProjectVectorOnToVector(FVector V, FVector Target);

	/**
	 * Projects/snaps a point onto a plane defined by a point on the plane and a plane normal.
	 *
	 * @param  Point Point to project onto the plane.
	 * @param  PlaneBase A point on the plane.
	 * @param  PlaneNormal Normal of the plane.
	 * @return Point projected onto the plane.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ProjectPointOnToPlane", Keywords = "Project"), Category = "Math|Vector")
	static FVector ProjectPointOnToPlane(FVector Point, FVector PlaneBase, FVector PlaneNormal);

	/**
	* Projects a vector onto a plane defined by a normalized vector (PlaneNormal).
	*
	* @param  V Vector to project onto the plane.
	* @param  PlaneNormal Normal of the plane.
	* @return Vector projected onto the plane.
	*/
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ProjectOnToPlane", Keywords = "Project"), Category="Math|Vector")
	static FVector ProjectVectorOnToPlane(FVector V, FVector PlaneNormal);

	/**
	 * Find closest points between 2 segments.
	 *
	 * @param	Segment1Start	Start of the 1st segment.
	 * @param	Segment1End		End of the 1st segment.
	 * @param	Segment2Start	Start of the 2nd segment.
	 * @param	Segment2End		End of the 2nd segment.
	 * @param	Segment1Point	Closest point on segment 1 to segment 2.
	 * @param	Segment2Point	Closest point on segment 2 to segment 1.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static void FindNearestPointsOnLineSegments(FVector Segment1Start, FVector Segment1End, FVector Segment2Start, FVector Segment2End, FVector& Segment1Point, FVector& Segment2Point);
	
	/**
	 * Find the closest point on a segment to a given point.
	 *
	 * @param Point			Point for which we find the closest point on the segment.
	 * @param SegmentStart	Start of the segment.
	 * @param SegmentEnd	End of the segment.
	 * @return The closest point on the segment to the given point.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static FVector FindClosestPointOnSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd);

	/**
	 * Find the closest point on an infinite line to a given point.
	 *
	 * @param Point			Point for which we find the closest point on the line.
	 * @param LineOrigin	Point of reference on the line.
	 * @param LineDirection Direction of the line. Not required to be normalized.
	 * @return The closest point on the line to the given point.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static FVector FindClosestPointOnLine(FVector Point, FVector LineOrigin, FVector LineDirection);

	/**
	* Find the distance from a point to the closest point on a segment.
	*
	* @param Point			Point for which we find the distance to the closest point on the segment.
	* @param SegmentStart	Start of the segment.
	* @param SegmentEnd		End of the segment.
	* @return The distance from the given point to the closest point on the segment.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static float GetPointDistanceToSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd);

	/**
	* Find the distance from a point to the closest point on an infinite line.
	*
	* @param Point			Point for which we find the distance to the closest point on the line.
	* @param LineOrigin		Point of reference on the line.
	* @param LineDirection	Direction of the line. Not required to be normalized.
	* @return The distance from the given point to the closest point on the line.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static float GetPointDistanceToLine(FVector Point, FVector LineOrigin, FVector LineDirection);

	/** Returns a random vector with length of 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static FVector RandomUnitVector();

	/** Returns a random point within the specified bounding box using the first vector as an origin and the second as the box extents. */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(ScriptMethod = "RandomPointInBoxExtents", NotBlueprintThreadSafe))
	static FVector RandomPointInBoundingBox(FVector Origin, FVector BoxExtent);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	 * @param ConeDir					The base "center" direction of the cone.
	 * @param ConeHalfAngleInRadians	The half-angle of the cone (from ConeDir to edge), in radians.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(NotBlueprintThreadSafe))
	static FVector RandomUnitVectorInConeInRadians(FVector ConeDir, float ConeHalfAngleInRadians);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	 * @param ConeDir					The base "center" direction of the cone.
	 * @param ConeHalfAngleInDegrees	The half-angle of the cone (from ConeDir to edge), in degrees.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(NotBlueprintThreadSafe))
	static inline FVector RandomUnitVectorInConeInDegrees(FVector ConeDir, float ConeHalfAngleInDegrees)
	{
		return RandomUnitVectorInConeInRadians(ConeDir, FMath::DegreesToRadians(ConeHalfAngleInDegrees));
	}

	/**
	* Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	* The shape of the cone can be modified according to the yaw and pitch angles.
	*
	* @param MaxYawInRadians	The yaw angle of the cone (from ConeDir to horizontal edge), in radians.
	* @param MaxPitchInRadians	The pitch angle of the cone (from ConeDir to vertical edge), in radians.	
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (Keywords = "RandomVector Pitch Yaw", NotBlueprintThreadSafe))
	static FVector RandomUnitVectorInEllipticalConeInRadians(FVector ConeDir, float MaxYawInRadians, float MaxPitchInRadians);

	/**
	* Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	* The shape of the cone can be modified according to the yaw and pitch angles.
	*
	* @param MaxYawInDegrees	The yaw angle of the cone (from ConeDir to horizontal edge), in degrees.
	* @param MaxPitchInDegrees	The pitch angle of the cone (from ConeDir to vertical edge), in degrees.	
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (Keywords = "RandomVector Pitch Yaw", NotBlueprintThreadSafe))
	static inline FVector RandomUnitVectorInEllipticalConeInDegrees(FVector ConeDir, float MaxYawInDegrees, float MaxPitchInDegrees)
	{
		return RandomUnitVectorInEllipticalConeInRadians(ConeDir, FMath::DegreesToRadians(MaxYawInDegrees), FMath::DegreesToRadians(MaxPitchInDegrees));
	}


	//
	// Vector4 constants - exposed for scripting
	//

	/** 4D vector zero constant (0,0,0) */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Zero", ScriptConstantHost = "Vector4"), Category = "Math|Vector4")
	static FVector4 Vector4_Zero();

	//
	// Vector4 functions
	//

	/** Makes a 4D vector {X, Y, Z, W} */
	UFUNCTION(BlueprintPure, meta = (Keywords = "construct build", NativeMakeFunc), Category = "Math|Vector4")
	static FVector4 MakeVector4(float X, float Y, float Z, float W);

	/** Breaks a 4D vector apart into X, Y, Z, W. */
	UFUNCTION(BlueprintPure, meta = (NativeBreakFunc), Category = "Math|Vector4")
	static void BreakVector4(const FVector4& InVec, float& X, float& Y, float& Z, float& W);

	/** Convert a Vector4 to a Vector (dropping the W element) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Vector (Vector4)", CompactNodeTitle = "->", ScriptMethod = "Vector", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FVector Conv_Vector4ToVector(const FVector4& InVector4);

	/**
	 * Return the FRotator orientation corresponding to the direction in which the vector points.
	 * Sets Yaw and Pitch to the proper numbers, and sets Roll to zero because the roll can't be determined from a vector.
	 *
	 * @return FRotator from the Vector's direction, without any roll.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "To Rotation (Vector4)", ScriptMethod = "Rotator", Keywords = "rotation rotate cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FRotator Conv_Vector4ToRotator(const FVector4& InVec);

	/**
	 * Return the Quaternion orientation corresponding to the direction in which the vector points.
	 * Similar to the FRotator version, returns a result without roll such that it preserves the up vector.
	 *
	 * @note If you don't care about preserving the up vector and just want the most direct rotation, you can use the faster
	 * 'FQuat::FindBetweenVectors(FVector::ForwardVector, YourVector)' or 'FQuat::FindBetweenNormals(...)' if you know the vector is of unit length.
	 *
	 * @return Quaternion from the Vector's direction, without any roll.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "To Quaterion (Vector4)", ScriptMethod = "Quaternion", Keywords = "rotation rotate cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FQuat Conv_Vector4ToQuaterion(const FVector4& InVec);

	/** Returns addition of Vector A and Vector B (A + B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector4 + Vector4", CompactNodeTitle = "+", ScriptMethod = "Add", ScriptOperator = "+;+=", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Vector4")
	static FVector4 Add_Vector4Vector4(const FVector4& A, const FVector4& B);

	/** Returns subtraction of Vector B from Vector A (A - B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector4 - Vector4", CompactNodeTitle = "-", ScriptMethod = "Subtract", ScriptOperator = "-;-=", Keywords = "- subtract minus"), Category = "Math|Vector4")
	static FVector4 Subtract_Vector4Vector4(const FVector4& A, const FVector4& B);

	/** Element-wise Vector multiplication (Result = {A.x*B.x, A.y*B.y, A.z*B.z, A.w*B.w}) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector4 * Vector4", CompactNodeTitle = "*", ScriptMethod = "Multiply", ScriptOperator = "*;*=", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Vector4")
	static FVector4 Multiply_Vector4Vector4(const FVector4& A, const FVector4& B);

	/** Element-wise Vector divide (Result = {A.x/B.x, A.y/B.y, A.z/B.z, A.w/B.w}) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector4 / Vector4", CompactNodeTitle = "/", ScriptMethod = "Divide", ScriptOperator = "/;/=", Keywords = "/ divide division"), Category = "Math|Vector4")
	static FVector4 Divide_Vector4Vector4(const FVector4& A, const FVector4& B);

	/** Returns true if vector A is equal to vector B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal Exactly (Vector4)", CompactNodeTitle = "===", ScriptMethod = "Equals", ScriptOperator = "==", Keywords = "== equal"), Category = "Math|Vector4")
	static bool EqualExactly_Vector4Vector4(const FVector4& A, const FVector4& B);

	/** Returns true if vector A is equal to vector B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (Vector4)", CompactNodeTitle = "==", ScriptMethod = "IsNearEqual", Keywords = "== equal"), Category = "Math|Vector4")
	static bool EqualEqual_Vector4Vector4(const FVector4& A, const FVector4& B, float ErrorTolerance = 1.e-4f);

	/** Returns true if vector A is not equal to vector B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal Exactly (Vector4)", CompactNodeTitle = "!==", ScriptMethod = "NotEqual", ScriptOperator = "!=", Keywords = "!= not equal"), Category = "Math|Vector4")
	static bool NotEqualExactly_Vector4Vector4(const FVector4& A, const FVector4& B);

	/** Returns true if vector A is not equal to vector B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal (Vector4)", CompactNodeTitle = "!=", ScriptMethod = "IsNotNearEqual", Keywords = "!= not equal"), Category = "Math|Vector4")
	static bool NotEqual_Vector4Vector4(const FVector4& A, const FVector4& B, float ErrorTolerance = 1.e-4f);

	/** Gets a negated copy of the vector. Equivalent to -Vector for scripts. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Negated (Vector4)", ScriptMethod = "Negated", ScriptOperator = "neg"), Category = "Math|Vector4")
	static FVector4 Vector4_Negated(const FVector4& A);

	/**
	 * Assign the values of the supplied vector.
	 *
	 * @param InVector Vector to copy values from.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "Assign"), Category = "Math|Vector4")
	static void Vector4_Assign(UPARAM(ref) FVector4& A, const FVector4& InVector);

	/**
	 * Set the values of the vector directly.
	 *
	 * @param InX New X coordinate.
	 * @param InY New Y coordinate.
	 * @param InZ New Z coordinate.
	 * @param InW New W coordinate.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "Set"), Category = "Math|Vector4")
	static void Vector4_Set(UPARAM(ref) FVector4& A, float X, float Y, float Z, float W);

	/** Returns the cross product of two vectors - see  http://mathworld.wolfram.com/CrossProduct.html */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Cross Product XYZ (Vector4)", CompactNodeTitle = "cross3", ScriptMethod = "Cross3"), Category = "Math|Vector4")
	static FVector4 Vector4_CrossProduct3(const FVector4& A, const FVector4& B);

	/** Returns the dot product of two vectors - see http://mathworld.wolfram.com/DotProduct.html */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Dot Product (Vector4)", CompactNodeTitle = "dot", ScriptMethod = "Dot", ScriptOperator = "|"), Category = "Math|Vector4")
	static float Vector4_DotProduct(const FVector4& A, const FVector4& B);

	/** Returns the dot product of two vectors - see http://mathworld.wolfram.com/DotProduct.html The W element is ignored.*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Dot Product XYZ (Vector4)", CompactNodeTitle = "dot3", ScriptMethod = "Dot3"), Category = "Math|Vector4")
	static float Vector4_DotProduct3(const FVector4& A, const FVector4& B);

	/**
	 * Determines if any component is not a number (NAN)
	 *
	 * @return true if one or more components is NAN, otherwise false.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "IsNAN"), Category = "Math|Vector4")
	static bool Vector4_IsNAN(const FVector4& A);

	/**
	 * Checks whether vector is near to zero within a specified tolerance. The W element is ignored.
	 *
	 * @param Tolerance Error tolerance.
	 * @return true if vector is in tolerance to zero, otherwise false.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "IsNearlyZero3"), Category = "Math|Vector4")
	static bool Vector4_IsNearlyZero3(const FVector4& A, float Tolerance = 1.e-4f);

	/**
	 * Checks whether all components of the vector are exactly zero.
	 *
	 * @return true if vector is exactly zero, otherwise false.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "IsZero"), Category = "Math|Vector4")
	static bool Vector4_IsZero(const FVector4& A);

	/** Returns the length of the vector. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Length (Vector4)", ScriptMethod = "Length", Keywords = "magnitude"), Category = "Math|Vector4")
	static float Vector4_Size(const FVector4& A);

	/** Returns the squared length of the vector. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Length Squared (Vector4)", ScriptMethod = "LengthSquared", Keywords = "magnitude"), Category = "Math|Vector4")
	static float Vector4_SizeSquared(const FVector4& A);

	/** Returns the length of the vector. The W element is ignored. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "LengthXYZ (Vector4)", ScriptMethod = "Length3", Keywords = "magnitude"), Category = "Math|Vector4")
	static float Vector4_Size3(const FVector4& A);

	/** Returns the squared length of the vector. The W element is ignored. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "LengthXYZ Squared (Vector4)", ScriptMethod = "LengthSquared3", Keywords = "magnitude"), Category = "Math|Vector4")
	static float Vector4_SizeSquared3(const FVector4& A);

	/**
	 * Determines if vector is normalized / unit (length 1) within specified squared tolerance. The W element is ignored.
	 *
	 * @return true if unit, false otherwise.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Unit XYZ (Vector4)", ScriptMethod = "IsUnit3", Keywords = "Unit Vector"), Category = "Math|Vector4")
	static bool Vector4_IsUnit3(const FVector4& A, float SquaredLenthTolerance = 1.e-4f);

	/**
	 * Determines if vector is normalized / unit (length 1). The W element is ignored.
	 *
	 * @return true if normalized, false otherwise.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Normal XYZ (Vector4)", ScriptMethod = "IsNormal3", Keywords = "Unit Vector"), Category = "Math|Vector4")
	static bool Vector4_IsNormal3(const FVector4& A);

	/**
	 * Gets a normalized unit copy of the vector, ensuring it is safe to do so based on the length. The W element is ignored and the returned vector has W=0.
	 * Returns zero vector if vector length is too small to safely normalize.
	 *
	 * @param Tolerance Minimum squared vector length.
	 * @return A normalized copy if safe, (0,0,0) otherwise.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normalize XYZ (Vector 4)", ScriptMethod = "Normal3", Keywords = "Unit Vector"), Category = "Math|Vector4")
	static FVector4 Vector4_Normal3(const FVector4& A, float Tolerance = 1.e-4f);

	/**
	 * Calculates normalized unit version of vector without checking for zero length. The W element is ignored and the returned vector has W=0.
	 *
	 * @return Normalized version of vector.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normal unsafe XYZ (Vector4)", ScriptMethod = "NormalUnsafe3", Keywords = "Unit Vector"), Category = "Math|Vector4")
	static FVector4 Vector4_NormalUnsafe3(const FVector4& A);

	/**
	 * Normalize this vector in-place if it is large enough or set it to (0,0,0,0) otherwise. The W element is ignored and the returned vector has W=0.
	 *
	 * @param Tolerance Minimum squared length of vector for normalization.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Normalize In Place XYZ (Vector4)", ScriptMethod = "Normalize3", Keywords = "Unit Vector"), Category = "Math|Vector4")
	static void Vector4_Normalize3(UPARAM(ref) FVector4& A, float Tolerance = 1.e-8);

	/** 
	 * Given a direction vector and a surface normal, returns the vector reflected across the surface normal.
	 * Produces a result like shining a laser at a mirror!
	 * The W element is ignored.
	 *
	 * @param Direction Direction vector the ray is coming from.
	 * @param SurfaceNormal A normal of the surface the ray should be reflected on.
	 *
	 * @returns Reflected vector.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "MirrorByVector3", Keywords = "Reflection"), Category = "Math|Vector4")
	static FVector4 Vector4_MirrorByVector3(const FVector4& Direction, const FVector4& SurfaceNormal);


	//
	// Rotator functions.
	//

	/** Makes a rotator {Roll, Pitch, Yaw} from rotation values supplied in degrees */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator", NativeMakeFunc))
	static FRotator MakeRotator(
		UPARAM(DisplayName="X (Roll)") float Roll,	
		UPARAM(DisplayName="Y (Pitch)") float Pitch,
		UPARAM(DisplayName="Z (Yaw)") float Yaw);

	/** Builds a rotator given only a XAxis. Y and Z are unspecified but will be orthonormal. XAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromX(const FVector& X);

	/** Builds a rotation matrix given only a YAxis. X and Z are unspecified but will be orthonormal. YAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromY(const FVector& Y);

	/** Builds a rotation matrix given only a ZAxis. X and Y are unspecified but will be orthonormal. ZAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZ(const FVector& Z);

	/** Builds a matrix with given X and Y axes. X will remain fixed, Y may be changed minimally to enforce orthogonality. Z will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromXY(const FVector& X, const FVector& Y);

	/** Builds a matrix with given X and Z axes. X will remain fixed, Z may be changed minimally to enforce orthogonality. Y will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromXZ(const FVector& X, const FVector& Z);

	/** Builds a matrix with given Y and X axes. Y will remain fixed, X may be changed minimally to enforce orthogonality. Z will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromYX(const FVector& Y, const FVector& X);

	/** Builds a matrix with given Y and Z axes. Y will remain fixed, Z may be changed minimally to enforce orthogonality. X will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromYZ(const FVector& Y, const FVector& Z);

	/** Builds a matrix with given Z and X axes. Z will remain fixed, X may be changed minimally to enforce orthogonality. Y will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZX(const FVector& Z, const FVector& X);

	/** Builds a matrix with given Z and Y axes. Z will remain fixed, Y may be changed minimally to enforce orthogonality. X will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZY(const FVector& Z, const FVector& Y);

	// Build a reference frame from three axes
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate"))
	static FRotator MakeRotationFromAxes(FVector Forward, FVector Right, FVector Up);

	/** Find a rotation for an object at Start location to point at Target location. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate"))
	static FRotator FindLookAtRotation(const FVector& Start, const FVector& Target);

	/** Breaks apart a rotator into {Roll, Pitch, Yaw} angles in degrees */
	UFUNCTION(BlueprintPure, Category = "Math|Rotator", meta = (Keywords = "rotation rotate rotator breakrotator", NativeBreakFunc))
	static void BreakRotator(
		UPARAM(DisplayName="Rotation") FRotator InRot,
		UPARAM(DisplayName="X (Roll)") float& Roll,
		UPARAM(DisplayName="Y (Pitch)") float& Pitch,
		UPARAM(DisplayName="Z (Yaw)") float& Yaw);

	/** Breaks apart a rotator into its component axes */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate rotator breakrotator"))
	static void BreakRotIntoAxes(const FRotator& InRot, FVector& X, FVector& Y, FVector& Z);

	/** Returns true if rotator A is equal to rotator B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Rotator)", CompactNodeTitle = "==", ScriptMethod = "IsNearEqual", ScriptOperator = "==", Keywords = "== equal"), Category="Math|Rotator")
	static bool EqualEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance = 1.e-4f);

	/** Returns true if rotator A is not equal to rotator B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Not Equal (Rotator)", CompactNodeTitle = "!=", ScriptMethod = "IsNotNearEqual", ScriptOperator = "!=", Keywords = "!= not equal"), Category="Math|Rotator")
	static bool NotEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance = 1.e-4f);

	/** Returns rotator representing rotator A scaled by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ScaleRotator", CompactNodeTitle = "*", ScriptMethod = "Scale", Keywords = "* multiply rotate rotation"), Category="Math|Rotator")
	static FRotator Multiply_RotatorFloat(FRotator A, float B);
	
	/** Returns rotator representing rotator A scaled by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ScaleRotator (integer)", CompactNodeTitle = "*", ScriptMethod = "ScaleInteger", Keywords = "* multiply rotate rotation"), Category="Math|Rotator")
	static FRotator Multiply_RotatorInt(FRotator A, int32 B);

	/** Combine 2 rotations to give you the resulting rotation of first applying A, then B. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "CombineRotators", ScriptMethod = "Combine", Keywords="rotate rotation add"), Category="Math|Rotator")
	static FRotator ComposeRotators(FRotator A, FRotator B);

	/** Negate a rotator*/
	UFUNCTION(BlueprintPure, meta=(DisplayName="InvertRotator", ScriptMethod = "Inversed", ScriptOperator = "neg", Keywords="rotate rotation"), Category="Math|Rotator")
	static FRotator NegateRotator(FRotator A);

	/** Rotate the world forward vector by the given rotation */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetForwardVector", Keywords="rotation rotate"), Category="Math|Vector")
	static FVector GetForwardVector(FRotator InRot);

	/** Rotate the world right vector by the given rotation */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetRightVector", Keywords="rotation rotate"), Category="Math|Vector")
	static FVector GetRightVector(FRotator InRot);

	/** Rotate the world up vector by the given rotation */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetUpVector", Keywords="rotation rotate"), Category="Math|Vector")
	static FVector GetUpVector(FRotator InRot);

	/** Get the X direction vector after this rotation */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "ToVector", DisplayName = "GetRotationXVector", Keywords="rotation rotate cast convert", BlueprintAutocast), Category="Math|Rotator")
	static FVector Conv_RotatorToVector(FRotator InRot);

	/** Convert Rotator to Transform */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToTransform (Rotator)", CompactNodeTitle = "->", ScriptMethod = "Transform", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FTransform Conv_RotatorToTransform(const FRotator& InRotator);

	/** Get the reference frame direction vectors (axes) described by this rotation */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "GetAxes", Keywords="rotate rotation"), Category="Math|Rotator")
	static void GetAxes(FRotator A, FVector& X, FVector& Y, FVector& Z);

	/** Generates a random rotation, with optional random roll. */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(Keywords="rotate rotation", NotBlueprintThreadSafe))
	static FRotator RandomRotator(bool bRoll = false);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp (Rotator)", ScriptMethod = "Lerp"), Category="Math|Rotator")
	static FRotator RLerp(FRotator A, FRotator B, float Alpha, bool bShortestPath);

	/** Easing between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Ease (Rotator)", BlueprintInternalUseOnly = "true", ScriptMethod = "Ease"), Category = "Math|Interpolation")
	static FRotator REase(FRotator A, FRotator B, float Alpha, bool bShortestPath, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Normalized A-B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Delta (Rotator)", ScriptMethod = "Delta"), Category="Math|Rotator")
	static FRotator NormalizedDeltaRotator(FRotator A, FRotator B);

	/**
	* Clamps an angle to the range of [0, 360].
	*
	* @param Angle The angle to clamp.
	* @return The clamped angle.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Rotator")
	static float ClampAxis(float Angle);

	/**
	* Clamps an angle to the range of [-180, 180].
	*
	* @param Angle The Angle to clamp.
	* @return The clamped angle.
	*/
	UFUNCTION(BlueprintPure, Category="Math|Rotator")
	static float NormalizeAxis(float Angle);


	//
	// Matrix functions
	//

	/** Convert a Matrix to a Transform */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Transform (Matrix)", CompactNodeTitle = "->", ScriptMethod = "Transform", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FTransform Conv_MatrixToTransform(const FMatrix& InMatrix);

	/** Convert a Matrix to a Rotator */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Rotator (Matrix)", CompactNodeTitle = "->", ScriptMethod = "Rotator", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FRotator Conv_MatrixToRotator(const FMatrix& InMatrix);

	/**
	 * Get the origin of the co-ordinate system
	 *
	 * @return co-ordinate system origin
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Origin (Matrix)", ScriptMethod = "GetOrigin"), Category = "Math|Matrix")
	static FVector Matrix_GetOrigin(const FMatrix& InMatrix);


	//
	// Quat constants - exposed for scripting
	//

	/** Identity quaternion constant */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Identity", ScriptConstantHost = "Quat"), Category = "Math|Quat")
	static FQuat Quat_Identity();

	//
	// Quat functions
	//

	/** Returns true if Quaternion A is equal to Quaternion B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (Quat)", CompactNodeTitle = "==", ScriptMethod = "Equals", ScriptOperator = "==", Keywords = "== equal"), Category = "Math|Quat")
	static bool EqualEqual_QuatQuat(const FQuat& A, const FQuat& B, float Tolerance = 1.e-4f);

	/** Returns true if Quat A is not equal to Quat B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal (Quat)", CompactNodeTitle = "!=", ScriptMethod = "NotEqual", ScriptOperator = "!=", Keywords = "!= not equal"), Category = "Math|Quat")
	static bool NotEqual_QuatQuat(const FQuat& A, const FQuat& B, float ErrorTolerance = 1.e-4f);

	/** Returns addition of Vector A and Vector B (A + B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Quat + Quat", CompactNodeTitle = "+", ScriptMethod = "Add", ScriptOperator = "+;+=", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Quat")
	static FQuat Add_QuatQuat(const FQuat& A, const FQuat& B);

	/** Returns subtraction of Vector B from Vector A (A - B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Quat - Quat", CompactNodeTitle = "-", ScriptMethod = "Subtract", ScriptOperator = "-;-=", Keywords = "- subtract minus"), Category = "Math|Quat")
	static FQuat Subtract_QuatQuat(const FQuat& A, const FQuat& B);

	/**
	 * Gets the result of multiplying two quaternions (A * B).
	 *
	 * Order matters when composing quaternions: C = A * B will yield a quaternion C that logically
	 * first applies B then A to any subsequent transformation (right first, then left).
	 *
	 * @param B The Quaternion to multiply by.
	 * @return The result of multiplication (A * B).
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Quat * Quat", CompactNodeTitle = "*", ScriptMethod = "Multiply", ScriptOperator = "*;*=", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Quat")
	static FQuat Multiply_QuatQuat(const FQuat& A, const FQuat& B);

	/**
	 * Checks whether this Quaternion is an Identity Quaternion.
	 * Assumes Quaternion tested is normalized.
	 *
	 * @param Tolerance Error tolerance for comparison with Identity Quaternion.
	 * @return true if Quaternion is a normalized Identity Quaternion.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Identity (Quat)", ScriptMethod = "IsIdentity"), Category = "Math|Quat")
	static bool Quat_IsIdentity(const FQuat& Q, float Tolerance = 1.e-4f);

	/**	Return true if this quaternion is normalized */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Normalized (Quat)", ScriptMethod = "IsNormalized"), Category = "Math|Quat")
	static bool Quat_IsNormalized(const FQuat& Q);

	/** Determine if all the values  are finite (not NaN nor Inf) in this Quat.	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Finite (Quat)", ScriptMethod = "IsFinite"), Category = "Math|Quat")
	static bool Quat_IsFinite(const FQuat& Q);

	/** Determine if there are any non-finite values (NaN or Inf) in this Quat.	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Non-Finite (Quat)", ScriptMethod = "IsNonFinite"), Category = "Math|Quat")
	static bool Quat_IsNonFinite(const FQuat& Q);

	/**
	 * Find the angular distance/difference between two rotation quaternions.
	 *
	 * @param B Quaternion to find angle distance to
	 * @return angular distance in radians
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Angular Distance (Quat)", ScriptMethod = "AngularDistance"), Category = "Math|Quat")
	static float Quat_AngularDistance(const FQuat& A, const FQuat& B);

	/** Modify the quaternion to ensure that the delta between it and B represents the shortest possible rotation angle. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Ensure shortest arc to (Quat)", ScriptMethod = "EnsureShortestArcTo"), Category = "Math|Quat")
	static void Quat_EnforceShortestArcWith(UPARAM(ref) FQuat& A, const FQuat& B);

	/**	Convert a Quaternion into floating-point Euler angles (in degrees). */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Euler (Quat)", ScriptMethod = "Euler"), Category = "Math|Quat")
	static FVector Quat_Euler(const FQuat& Q);

	/**
	 * Used in combination with Log().
	 * Assumes a quaternion with W=0 and V=theta*v (where |v| = 1).
	 * Exp(q) = (sin(theta)*v, cos(theta))
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Exp (Quat)", ScriptMethod = "Exp"), Category = "Math|Quat")
	static FQuat Quat_Exp(const FQuat& Q);

	/** Get the angle of this quaternion */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Angle (Quat)", ScriptMethod = "GetAngle"), Category = "Math|Quat")
	static float Quat_GetAngle(const FQuat& Q);

	/** Get the forward direction (X axis) after it has been rotated by this Quaternion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Axis X (Quat)", ScriptMethod = "GetAxisX"), Category = "Math|Quat")
	static FVector Quat_GetAxisX(const FQuat& Q);

	/** Get the right direction (Y axis) after it has been rotated by this Quaternion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Axis Y (Quat)", ScriptMethod = "GetAxisY"), Category = "Math|Quat")
	static FVector Quat_GetAxisY(const FQuat& Q);

	/** Get the up direction (Z axis) after it has been rotated by this Quaternion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Axis Z (Quat)", ScriptMethod = "GetAxisZ"), Category = "Math|Quat")
	static FVector Quat_GetAxisZ(const FQuat& Q);

	/** Get the forward direction (X axis) after it has been rotated by this Quaternion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector Forward (Quat)", ScriptMethod = "VectorForward"), Category = "Math|Quat")
	static FVector Quat_VectorForward(const FQuat& Q);

	/** Get the right direction (Y axis) after it has been rotated by this Quaternion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector Right (Quat)", ScriptMethod = "VectorRight"), Category = "Math|Quat")
	static FVector Quat_VectorRight(const FQuat& Q);

	/** Get the up direction (Z axis) after it has been rotated by this Quaternion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Vector Up (Quat)", ScriptMethod = "VectorUp"), Category = "Math|Quat")
	static FVector Quat_VectorUp(const FQuat& Q);

	/**
	 * Normalize this quaternion if it is large enough as compared to the supplied tolerance.
	 * If it is too small then set it to the identity quaternion.
	 *
	 * @param Tolerance Minimum squared length of quaternion for normalization.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Normalize (Quat)", ScriptMethod = "Normalize"), Category = "Math|Quat")
	static void Quat_Normalize(UPARAM(ref) FQuat& Q, float Tolerance = 1.e-4f);

	/**
	 * Get a normalized copy of this quaternion.
	 * If it is too small, returns an identity quaternion.
	 *
	 * @param Tolerance Minimum squared length of quaternion for normalization.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Normalized (Quat)", ScriptMethod = "Normalized"), Category = "Math|Quat")
	static FQuat Quat_Normalized(const FQuat& Q, float Tolerance = 1.e-4f);

	/**
	 * Get the axis of rotation of the Quaternion.
	 * This is the axis around which rotation occurs to transform the canonical coordinate system to the target orientation.
	 * For the identity Quaternion which has no such rotation, FVector(1,0,0) is returned.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Rotation Axis (Quat)", ScriptMethod = "GetRotationAxis"), Category = "Math|Quat")
	static FVector Quat_GetRotationAxis(const FQuat& Q);

	/**	Return an inversed copy of this quaternion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Inversed (Quat)", ScriptMethod = "Inversed"), Category = "Math|Quat")
	static FQuat Quat_Inversed(const FQuat& Q);

	/**	Quaternion with W=0 and V=theta*v. Used in combination with Exp(). */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Log (Quat)", ScriptMethod = "Log"), Category = "Math|Quat")
	static FQuat Quat_Log(const FQuat& Q);

	/** Set X, Y, Z, W components of Quaternion. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set Components (Quat)", ScriptMethod = "SetComponents"), Category = "Math|Quat")
	static void Quat_SetComponents(UPARAM(ref) FQuat& Q, float X, float Y, float Z, float W);

	/**
	 * Convert a vector of floating-point Euler angles (in degrees) into a Quaternion.
	 * 
	 * @param Q Quaternion to update
	 * @param Euler the Euler angles
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set from Euler (Quat)", ScriptMethod = "SetFromEuler"), Category = "Math|Quat")
	static void Quat_SetFromEuler(UPARAM(ref) FQuat& Q, const FVector& Euler);

	/**
	 * Convert a vector of floating-point Euler angles (in degrees) into a Quaternion.
	 * 
	 * @param Euler the Euler angles
	 * @return constructed Quat
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Make from Euler (Quat)"), Category = "Math|Quat")
	static FQuat Quat_MakeFromEuler(const FVector& Euler);

	/** Convert to Rotator representation of this Quaternion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToRotator (Quat)", CompactNodeTitle = "->", ScriptMethod = "Rotator", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FRotator Quat_Rotator(const FQuat& Q);

	/**
	 * Get the length of the quaternion.
	 *
	 * @return The length of the quaternion.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Size (Quat)", ScriptMethod = "Size"), Category = "Math|Quat")
	static float Quat_Size(const FQuat& Q);

	/**
	 * Get the squared length of the quaternion.
	 *
	 * @return The squared length of the quaternion.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Size Squared (Quat)", ScriptMethod = "SizeSquared"), Category = "Math|Quat")
	static float Quat_SizeSquared(const FQuat& Q);

	/**
	 * Rotate a vector by this quaternion.
	 *
	 * @param V the vector to be rotated
	 * @return vector after rotation
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Rotate Vector (Quat)", ScriptMethod = "RotateVector"), Category = "Math|Quat")
	static FVector Quat_RotateVector(const FQuat& Q, const FVector& V);

	/**
	 * Rotate a vector by the inverse of this quaternion.
	 *
	 * @param V the vector to be rotated
	 * @return vector after rotation by the inverse of this quaternion.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Unrotate Vector (Quat)", ScriptMethod = "UnrotateVector"), Category = "Math|Quat")
	static FVector Quat_UnrotateVector(const FQuat& Q, const FVector& V);


	//
	// LinearColor constants - exposed for scripting
	//

	/** White linear color */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "White", ScriptConstantHost = "LinearColor"), Category = "Math|Color")
	static FLinearColor LinearColor_White();

	/** Grey linear color */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Gray", ScriptConstantHost = "LinearColor"), Category = "Math|Color")
	static FLinearColor LinearColor_Gray();

	/** Black linear color */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Black", ScriptConstantHost = "LinearColor"), Category = "Math|Color")
	static FLinearColor LinearColor_Black();

	/** Red linear color */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Red", ScriptConstantHost = "LinearColor"), Category = "Math|Color")
	static FLinearColor LinearColor_Red();

	/** Green linear color */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Green", ScriptConstantHost = "LinearColor"), Category = "Math|Color")
	static FLinearColor LinearColor_Green();

	/** Blue linear color */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Blue", ScriptConstantHost = "LinearColor"), Category = "Math|Color")
	static FLinearColor LinearColor_Blue();

	/** Yellow linear color */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Yellow", ScriptConstantHost = "LinearColor"), Category = "Math|Color")
	static FLinearColor LinearColor_Yellow();

	/** Transparent linear color - black with 0 opacity/alpha */
	UFUNCTION(BlueprintPure, meta = (ScriptConstant = "Tansparent", ScriptConstantHost = "LinearColor"), Category = "Math|Color")
	static FLinearColor LinearColor_Transparent();


	//
	//	LinearColor functions
	//

	/** Make a color from individual color components (RGB space) */
	UFUNCTION(BlueprintPure, Category = "Math|Color", meta = (Keywords = "construct build", NativeMakeFunc))
	static FLinearColor MakeColor(float R, float G, float B, float A = 1.0f);

	/** Breaks apart a color into individual RGB components (as well as alpha) */
	UFUNCTION(BlueprintPure, Category = "Math|Color")
	static void BreakColor(FLinearColor InColor, float& R, float& G, float& B, float& A);

	/** Assign contents of InColor */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "Set"), Category = "Math|Color")
	static void LinearColor_Set(UPARAM(ref) FLinearColor& InOutColor, FLinearColor InColor);

	/** Assign individual linear RGBA components. */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "SetRGBA"), Category = "Math|Color")
	static void LinearColor_SetRGBA(UPARAM(ref) FLinearColor& InOutColor, float R, float G, float B, float A = 1.0f);

	/** Assigns an HSV color to a linear space RGB color */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "SetFromHSV"), Category = "Math|Color")
	static void LinearColor_SetFromHSV(UPARAM(ref) FLinearColor& InOutColor, float H, float S, float V, float A = 1.0f);

	/**
	 * Assigns an FColor coming from an observed sRGB output, into a linear color.
	 * @param InSRGB The sRGB color that needs to be converted into linear space.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "SetFromSRGB"), Category = "Math|Color")
	static void LinearColor_SetFromSRGB(UPARAM(ref) FLinearColor& InOutColor, const FColor& InSRGB);

	/**
	 * Assigns an FColor coming from an observed Pow(1/2.2) output, into a linear color.
	 * @param InColor The Pow(1/2.2) color that needs to be converted into linear space.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "SetFromPow22"), Category = "Math|Color")
	static void LinearColor_SetFromPow22(UPARAM(ref) FLinearColor& InOutColor, const FColor& InColor);

	/** Converts temperature in Kelvins of a black body radiator to RGB chromaticity. */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "SetTemperature"), Category = "Math|Color")
	static void LinearColor_SetTemperature(UPARAM(ref) FLinearColor& InOutColor, float InTemperature);

	/** Sets to a random color. Choses a quite nice color based on a random hue. */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod = "SetRandomHue"), Category = "Math|Color")
	static void LinearColor_SetRandomHue(UPARAM(ref) FLinearColor& InOutColor);

	/** Convert a float into a LinearColor, where each element is that float */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToLinearColor (float)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FLinearColor Conv_FloatToLinearColor(float InFloat);

	/** Make a color from individual color components (HSV space; Hue is [0..360) while Saturation and Value are 0..1) */
	UFUNCTION(BlueprintPure, Category = "Math|Color", meta = (DisplayName = "HSV to RGB"))
	static FLinearColor HSVToRGB(float H, float S, float V, float A = 1.0f);

	/** Converts a HSV linear color (where H is in R (0..360), S is in G (0..1), and V is in B (0..1)) to RGB */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "HSV to RGB (Vector)", ScriptMethod = "HSVIntoRGB", Keywords = "cast convert"), Category = "Math|Color")
	static void HSVToRGB_Vector(FLinearColor HSV, FLinearColor& RGB);

	/** Converts a HSV linear color (where H is in R, S is in G, and V is in B) to linear RGB */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "HSV to RGB linear color", ScriptMethod = "HSVToRGB", Keywords = "cast convert"), Category = "Math|Color")
	static FLinearColor HSVToRGBLinear(FLinearColor HSV);

	/** Breaks apart a color into individual HSV components (as well as alpha) (Hue is [0..360) while Saturation and Value are 0..1) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "RGB to HSV", ScriptMethod = "RGBIntoHSVComponents"), Category = "Math|Color")
	static void RGBToHSV(FLinearColor InColor, float& H, float& S, float& V, float& A);

	/** Converts a RGB linear color to HSV (where H is in R (0..360), S is in G (0..1), and V is in B (0..1)) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "RGB to HSV (Vector)", ScriptMethod = "RGBIntoHSV", Keywords = "cast convert"), Category = "Math|Color")
	static void RGBToHSV_Vector(FLinearColor RGB, FLinearColor& HSV);

	/** Converts a RGB linear color to HSV (where H is in R, S is in G, and V is in B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "RGB to HSV linear color", ScriptMethod = "RGBToHSV", Keywords = "cast convert"), Category = "Math|Color")
	static FLinearColor RGBLinearToHSV(FLinearColor RGB);

	/** Converts a LinearColor to a vector */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToVector (LinearColor)", ScriptMethod = "ToRGBVector", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FVector Conv_LinearColorToVector(FLinearColor InLinearColor);

	/** Convert from linear to 8-bit RGBE as outlined in Gregory Ward's Real Pixels article, Graphics Gems II, page 80. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToRGBE (LinearColor)", ScriptMethod = "ToRGBE", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Color")
	static FColor LinearColor_ToRGBE(FLinearColor InLinearColor);

	/** Quantizes the linear color and returns the result as a FColor with optional sRGB conversion and quality as goal. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToColor (LinearColor)", ScriptMethod = "ToColor", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FColor Conv_LinearColorToColor(FLinearColor InLinearColor, bool InUseSRGB = true);

	/** Quantizes the linear color and returns the result as an 8-bit color.  This bypasses the SRGB conversion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Quantize to 8-bit (LinearColor)", ScriptMethod = "Quantize", Keywords = "cast convert"), Category = "Math|Color")
	static FColor LinearColor_Quantize(FLinearColor InColor);

	/** Quantizes the linear color with rounding and returns the result as an 8-bit color.  This bypasses the SRGB conversion. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Quantize with rounding to 8-bit (LinearColor)", ScriptMethod = "QuantizeRound", Keywords = "cast convert"), Category = "Math|Color")
	static FColor LinearColor_QuantizeRound(FLinearColor InColor);

	/**
	 * Returns a desaturated color, with 0 meaning no desaturation and 1 == full desaturation
	 *
	 * @param	Desaturation	Desaturation factor in range [0..1]
	 * @return	Desaturated color
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Desaturate (LinearColor)", ScriptMethod = "Desaturated"), Category = "Math|Color")
	static FLinearColor LinearColor_Desaturated(FLinearColor InColor, float InDesaturation);

	/** Euclidean distance between two color points. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Distance (LinearColor)", ScriptMethod = "Distance", Keywords = "magnitude"), Category = "Math|Color")
	static float LinearColor_Distance(FLinearColor C1, FLinearColor C2);
	
	/** Returns a copy of this color using the specified opacity/alpha.	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "New Opacity (LinearColor)", ScriptMethod = "ToNewOpacity"), Category = "Math|Color")
	static FLinearColor LinearColor_ToNewOpacity(FLinearColor InColor, float InOpacity);

	/**	Returns the perceived brightness of a color on a display taking into account the impact on the human eye per color channel: green > red > blue. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Luminance (LinearColor)", ScriptMethod = "GetLuminance"), Category = "Math|Color")
	static float LinearColor_GetLuminance(FLinearColor InColor);

	/**
	 * Returns the maximum color channel value in this color structure
	 *
	 * @return The maximum color channel value
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Max (LinearColor)", ScriptMethod = "GetMax"), Category = "Math|Color")
	static float LinearColor_GetMax(FLinearColor InColor);

	/**
	 * Returns the minimum color channel value in this color structure
	 *
	 * @return The minimum color channel value
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Min (LinearColor)", ScriptMethod = "GetMin"), Category = "Math|Color")
	static float LinearColor_GetMin(FLinearColor InColor);

	/**
	 * Interpolate Linear Color from Current to Target. Scaled by distance to Target, so it has a strong start speed and ease out.
	 *
	 * @param		Current			Current Color
	 * @param		Target			Target Color
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated Color
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Interpolate (LinearColor)", ScriptMethod = "InterpolateTo", Keywords = "color"), Category = "Math|Interpolation")
	static FLinearColor CInterpTo(FLinearColor Current, FLinearColor Target, float DeltaTime, float InterpSpeed);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp (LinearColor)", ScriptMethod = "LerpTo"), Category="Math|Color")
	static FLinearColor LinearColorLerp(FLinearColor A, FLinearColor B, float Alpha);

	/**
	 * Linearly interpolates between two colors by the specified Alpha amount (100% of A when Alpha=0 and 100% of B when Alpha=1).  The interpolation is performed in HSV color space taking the shortest path to the new color's hue.  This can give better results than a normal lerp, but is much more expensive.  The incoming colors are in RGB space, and the output color will be RGB.  The alpha value will also be interpolated.
	 * 
	 * @param	A		The color and alpha to interpolate from as linear RGBA
	 * @param	B		The color and alpha to interpolate to as linear RGBA
	 * @param	Alpha	Scalar interpolation amount (usually between 0.0 and 1.0 inclusive)
	 * 
	 * @return	The interpolated color in linear RGB space along with the interpolated alpha value
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp Using HSV (LinearColor)", ScriptMethod = "LerpUsingHSVTo"), Category="Math|Color")
	static FLinearColor LinearColorLerpUsingHSV(FLinearColor A, FLinearColor B, float Alpha);

	/** Returns true if linear color A is equal to linear color B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Near Equal (LinearColor)", CompactNodeTitle = "~==", ScriptMethod = "IsNearEqual", ScriptOperator = "==", Keywords = "== equal"), Category = "Math|Color")
	static bool LinearColor_IsNearEqual(FLinearColor A, FLinearColor B, float Tolerance = 1.e-4f);

	/** Returns true if linear color A is equal to linear color B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (LinearColor)", CompactNodeTitle = "==", ScriptMethod = "Equals", ScriptOperator = "==", Keywords = "== equal"), Category = "Math|Color")
	static bool EqualEqual_LinearColorLinearColor(FLinearColor A, FLinearColor B);

	/** Returns true if linear color A is not equal to linear color B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal (LinearColor)", CompactNodeTitle = "!=", ScriptMethod = "NotEqual", ScriptOperator = "!=", Keywords = "!= not equal"), Category = "Math|Color")
	static bool NotEqual_LinearColorLinearColor(FLinearColor A, FLinearColor B);

	/** Element-wise addition of two linear colors (R+R, G+G, B+B, A+A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "LinearColor + LinearColor", CompactNodeTitle = "+", ScriptMethod = "Add", ScriptOperator = "+;+=", Keywords = "+ add plus"), Category="Math|Color")
	static FLinearColor Add_LinearColorLinearColor(FLinearColor A, FLinearColor B);

	/** Element-wise subtraction of two linear colors (R-R, G-G, B-B, A-A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "LinearColor - LinearColor", CompactNodeTitle = "-", ScriptMethod = "Subtract", ScriptOperator = "-;-=", Keywords = "- subtract minus"), Category="Math|Color")
	static FLinearColor Subtract_LinearColorLinearColor(FLinearColor A, FLinearColor B);

	/** Element-wise multiplication of two linear colors (R*R, G*G, B*B, A*A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "LinearColor * LinearColor", CompactNodeTitle = "*", ScriptMethod = "Multiply", ScriptOperator = "*;*=", Keywords = "* multiply"), Category="Math|Color")
	static FLinearColor Multiply_LinearColorLinearColor(FLinearColor A, FLinearColor B);

	/** Element-wise multiplication of a linear color by a float (F*R, F*G, F*B, F*A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "LinearColor * Float", CompactNodeTitle = "*", ScriptMethod = "MultiplyFloat", Keywords = "* multiply"), Category="Math|Color")
	static FLinearColor Multiply_LinearColorFloat(FLinearColor A, float B);

	/** Element-wise multiplication of two linear colors (R/R, G/G, B/B, A/A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "LinearColor / LinearColor", CompactNodeTitle = "/", ScriptMethod = "Divide", ScriptOperator = "/;/=", Keywords = "/ divide"), Category="Math|Color")
	static FLinearColor Divide_LinearColorLinearColor(FLinearColor A, FLinearColor B);


	//
	// Plane functions.
	//
	
	/** 
	* Creates a plane with a facing direction of Normal at the given Point
	* 
	* @param Point	A point on the plane
	* @param Normal  The Normal of the plane at Point
	* @return Plane instance
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Plane", meta=(Keywords="make plane"))
	static FPlane MakePlaneFromPointAndNormal(FVector Point, FVector Normal);
	

	//
	// DateTime functions.
	//

	/** Makes a DateTime struct */
	UFUNCTION(BlueprintPure, Category="Math|DateTime", meta=(NativeMakeFunc, AdvancedDisplay = "3"))
	static FDateTime MakeDateTime(int32 Year, int32 Month, int32 Day, int32 Hour = 0, int32 Minute = 0, int32 Second = 0, int32 Millisecond = 0);

	/** Breaks a DateTime into its components */
	UFUNCTION(BlueprintPure, Category="Math|DateTime", meta=(NativeBreakFunc))
	static void BreakDateTime(FDateTime InDateTime, int32& Year, int32& Month, int32& Day, int32& Hour, int32& Minute, int32& Second, int32& Millisecond);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime + Timespan", CompactNodeTitle="+", Keywords="+ add plus"), Category="Math|DateTime")
	static FDateTime Add_DateTimeTimespan( FDateTime A, FTimespan B );

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime - Timespan", CompactNodeTitle="-", Keywords="- subtract minus"), Category="Math|DateTime")
	static FDateTime Subtract_DateTimeTimespan(FDateTime A, FTimespan B);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "DateTime - DateTime", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category = "Math|DateTime")
	static FTimespan Subtract_DateTimeDateTime(FDateTime A, FDateTime B);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (DateTime)", CompactNodeTitle="==", Keywords="== equal"), Category="Math|DateTime")
	static bool EqualEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Not Equal (DateTime)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="Math|DateTime")
	static bool NotEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime > DateTime", CompactNodeTitle=">", Keywords="> greater"), Category="Math|DateTime")
	static bool Greater_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime >= DateTime", CompactNodeTitle=">=", Keywords=">= greater"), Category="Math|DateTime")
	static bool GreaterEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime < DateTime", CompactNodeTitle="<", Keywords="< less"), Category="Math|DateTime")
	static bool Less_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime <= DateTime", CompactNodeTitle="<=", Keywords="<= less"), Category="Math|DateTime")
	static bool LessEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns the date component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDate"), Category="Math|DateTime")
	static FDateTime GetDate( FDateTime A );

	/** Returns the day component of A (1 to 31) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDay"), Category="Math|DateTime")
	static int32 GetDay( FDateTime A );

	/** Returns the day of year of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDayOfYear"), Category="Math|DateTime")
	static int32 GetDayOfYear( FDateTime A );

	/** Returns the hour component of A (24h format) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetHour"), Category="Math|DateTime")
	static int32 GetHour( FDateTime A );

	/** Returns the hour component of A (12h format) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetHour12"), Category="Math|DateTime")
	static int32 GetHour12( FDateTime A );

	/** Returns the millisecond component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMillisecond"), Category="Math|DateTime")
	static int32 GetMillisecond( FDateTime A );

	/** Returns the minute component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMinute"), Category="Math|DateTime")
	static int32 GetMinute( FDateTime A );

	/** Returns the month component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMonth"), Category="Math|DateTime")
	static int32 GetMonth( FDateTime A );

	/** Returns the second component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetSecond"), Category="Math|DateTime")
	static int32 GetSecond( FDateTime A );

	/** Returns the time elapsed since midnight of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTimeOfDay"), Category="Math|DateTime")
	static FTimespan GetTimeOfDay( FDateTime A );

	/** Returns the year component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetYear"), Category="Math|DateTime")
	static int32 GetYear( FDateTime A );

	/** Returns whether A's time is in the afternoon */
	UFUNCTION(BlueprintPure, meta=(DisplayName="IsAfternoon"), Category="Math|DateTime")
	static bool IsAfternoon( FDateTime A );

	/** Returns whether A's time is in the morning */
	UFUNCTION(BlueprintPure, meta=(DisplayName="IsMorning"), Category="Math|DateTime")
	static bool IsMorning( FDateTime A );

	/** Returns the number of days in the given year and month */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DaysInMonth"), Category="Math|DateTime")
	static int32 DaysInMonth( int32 Year, int32 Month );

	/** Returns the number of days in the given year */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DaysInYear"), Category="Math|DateTime")
	static int32 DaysInYear( int32 Year );

	/** Returns whether given year is a leap year */
	UFUNCTION(BlueprintPure, meta=(DisplayName="IsLeapYear"), Category="Math|DateTime")
	static bool IsLeapYear( int32 Year );

	/** Returns the maximum date and time value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="MaxValue"), Category="Math|DateTime")
	static FDateTime DateTimeMaxValue( );

	/** Returns the minimum date and time value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="MinValue"), Category="Math|DateTime")
	static FDateTime DateTimeMinValue( );

	/** Returns the local date and time on this computer */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Now"), Category="Math|DateTime")
	static FDateTime Now( );

	/** Returns the local date on this computer */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Today"), Category="Math|DateTime")
	static FDateTime Today( );

	/** Returns the UTC date and time on this computer */
	UFUNCTION(BlueprintPure, meta=(DisplayName="UtcNow"), Category="Math|DateTime")
	static FDateTime UtcNow( );

	/** Converts a date string in ISO-8601 format to a DateTime object */
	UFUNCTION(BlueprintPure, Category="Math|DateTime")
	static bool DateTimeFromIsoString(FString IsoString, FDateTime& Result);

	/** Converts a date string to a DateTime object */
	UFUNCTION(BlueprintPure, Category="Math|DateTime")
	static bool DateTimeFromString(FString DateTimeString, FDateTime& Result);


	//
	// Timespan constants
	//

	/** Returns the maximum time span value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="MaxValue", ScriptConstant = "MaxValue", ScriptConstantHost = "Timespan"), Category="Math|Timespan")
	static FTimespan TimespanMaxValue( );

	/** Returns the minimum time span value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="MinValue", ScriptConstant = "MinValue", ScriptConstantHost = "Timespan"), Category="Math|Timespan")
	static FTimespan TimespanMinValue( );

	/** Returns a zero time span value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="ZeroValue", ScriptConstant = "Zero", ScriptConstantHost = "Timespan"), Category="Math|Timespan")
	static FTimespan TimespanZeroValue( );

	//
	// Timespan functions.
	//

	/** Makes a Timespan struct */
	UFUNCTION(BlueprintPure, Category="Math|Timespan", meta=(NativeMakeFunc))
	static FTimespan MakeTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 Milliseconds);

	/** Makes a Timespan struct */
	UFUNCTION(BlueprintPure, Category="Math|Timespan", meta=(NativeMakeFunc))
	static FTimespan MakeTimespan2(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 FractionNano);

	/** Breaks a Timespan into its components */
	UFUNCTION(BlueprintPure, Category="Math|Timespan", meta=(NativeBreakFunc))
	static void BreakTimespan(FTimespan InTimespan, int32& Days, int32& Hours, int32& Minutes, int32& Seconds, int32& Milliseconds);

	/** Breaks a Timespan into its components */
	UFUNCTION(BlueprintPure, Category="Math|Timespan", meta=(NativeBreakFunc))
	static void BreakTimespan2(FTimespan InTimespan, int32& Days, int32& Hours, int32& Minutes, int32& Seconds, int32& FractionNano);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan + Timespan", CompactNodeTitle="+", Keywords="+ add plus"), Category="Math|Timespan")
	static FTimespan Add_TimespanTimespan( FTimespan A, FTimespan B );

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan - Timespan", CompactNodeTitle="-", Keywords="- subtract minus"), Category="Math|Timespan")
	static FTimespan Subtract_TimespanTimespan( FTimespan A, FTimespan B );

	/** Scalar multiplication (A * s) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan * float", CompactNodeTitle="*", Keywords="* multiply"), Category="Math|Timespan")
	static FTimespan Multiply_TimespanFloat( FTimespan A, float Scalar );

	/** Scalar division (A * s) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan * float", CompactNodeTitle="/", Keywords="/ divide"), Category="Math|Timespan")
	static FTimespan Divide_TimespanFloat( FTimespan A, float Scalar );

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (Timespan)", CompactNodeTitle="==", Keywords="== equal"), Category="Math|Timespan")
	static bool EqualEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Not Equal (Timespan)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="Math|Timespan")
	static bool NotEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan > Timespan", CompactNodeTitle=">", Keywords="> greater"), Category="Math|Timespan")
	static bool Greater_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan >= Timespan", CompactNodeTitle=">=", Keywords=">= greater"), Category="Math|Timespan")
	static bool GreaterEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan < Timespan", CompactNodeTitle="<", Keywords="< less"), Category="Math|Timespan")
	static bool Less_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan <= Timespan", CompactNodeTitle="<=", Keywords="<= less"), Category="Math|Timespan")
	static bool LessEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns the days component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDays"), Category="Math|Timespan")
	static int32 GetDays( FTimespan A );

	/** Returns the absolute value of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDuration"), Category="Math|Timespan")
	static FTimespan GetDuration( FTimespan A );

	/** Returns the hours component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetHours"), Category="Math|Timespan")
	static int32 GetHours( FTimespan A );

	/** Returns the milliseconds component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMilliseconds"), Category="Math|Timespan")
	static int32 GetMilliseconds( FTimespan A );

	/** Returns the minutes component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMinutes"), Category="Math|Timespan")
	static int32 GetMinutes( FTimespan A );

	/** Returns the seconds component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetSeconds"), Category="Math|Timespan")
	static int32 GetSeconds( FTimespan A );

	/** Returns the total number of days in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalDays"), Category="Math|Timespan")
	static float GetTotalDays( FTimespan A );

	/** Returns the total number of hours in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalHours"), Category="Math|Timespan")
	static float GetTotalHours( FTimespan A );

	/** Returns the total number of milliseconds in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalMilliseconds"), Category="Math|Timespan")
	static float GetTotalMilliseconds( FTimespan A );

	/** Returns the total number of minutes in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalMinutes"), Category="Math|Timespan")
	static float GetTotalMinutes( FTimespan A );

	/** Returns the total number of seconds in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalSeconds"), Category="Math|Timespan")
	static float GetTotalSeconds( FTimespan A );

	/** Returns a time span that represents the specified number of days */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromDays"), Category="Math|Timespan")
	static FTimespan FromDays( float Days );

	/** Returns a time span that represents the specified number of hours */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromHours"), Category="Math|Timespan")
	static FTimespan FromHours( float Hours );

	/** Returns a time span that represents the specified number of milliseconds */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromMilliseconds"), Category="Math|Timespan")
	static FTimespan FromMilliseconds( float Milliseconds );

	/** Returns a time span that represents the specified number of minutes */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromMinutes"), Category="Math|Timespan")
	static FTimespan FromMinutes( float Minutes );

	/** Returns a time span that represents the specified number of seconds */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromSeconds"), Category="Math|Timespan")
	static FTimespan FromSeconds( float Seconds );

	/** Returns the ratio between two time spans (A / B), handles zero values */
	UFUNCTION(BlueprintPure, meta=(DisplayName="TimespanRatio"), Category="Math|Timespan")
	static float TimespanRatio( FTimespan A, FTimespan B );

	/** Converts a time span string to a Timespan object */
	UFUNCTION(BlueprintPure, Category="Math|Timespan")
	static bool TimespanFromString(FString TimespanString, FTimespan& Result);

	//
	// Frame Time and Frame Rate Functions
	//

	/** Creates a FQualifiedFrameTime out of a frame number, frame rate, and optional 0-1 clamped subframe. */
	UFUNCTION(BlueprintPure, Category = "Time Management", meta = (NativeMakeFunc))
	static FQualifiedFrameTime MakeQualifiedFrameTime(FFrameNumber Frame, FFrameRate FrameRate, float SubFrame = 0.f);

	/** Breaks a FQualifiedFrameTime into its component parts again. */
	UFUNCTION(BlueprintPure, Category = "Time Management", meta = (NativeBreakFunc))
	static void BreakQualifiedFrameTime(const FQualifiedFrameTime& InFrameTime, FFrameNumber& Frame, FFrameRate& FrameRate, float& SubFrame);

	/** Creates a FFrameRate from a Numerator and a Denominator. Enforces that the Denominator is at least one. */
	UFUNCTION(BlueprintPure, Category = "Time Management", meta = (NativeMakeFunc))
	static FFrameRate MakeFrameRate(int32 Numerator, int32 Denominator = 1);

	/** Breaks a FFrameRate into a numerator and denominator. */
	UFUNCTION(BlueprintPure, Category = "Time Management", meta = (NativeBreakFunc))
	static void BreakFrameRate(const FFrameRate& InFrameRate, int32& Numerator, int32& Denominator);
	
	// -- Begin K2 utilities

	/** Converts a byte to a float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToFloat (byte)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static float Conv_ByteToFloat(uint8 InByte);

	/** Converts an integer to a float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToFloat (integer)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static float Conv_IntToFloat(int32 InInt);

	/** Converts an integer to a 64 bit integer */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToInt64 (integer)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static int64 Conv_IntToInt64(int32 InInt);

	/** Converts an integer to a byte (if the integer is too large, returns the low 8 bits) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToByte (integer)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static uint8 Conv_IntToByte(int32 InInt);

	/** Converts an integer to an IntVector*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToIntVector (integer)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FIntVector Conv_IntToIntVector(int32 InInt);

	/** Converts a int to a bool*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToBool (integer)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static bool Conv_IntToBool(int32 InInt);

	/** Converts a bool to an int */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToInt (bool)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static int32 Conv_BoolToInt(bool InBool);

	/** Converts a bool to a float (0.0f or 1.0f) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToFloat (bool)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static float Conv_BoolToFloat(bool InBool);

	/** Converts a bool to a byte */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToByte (bool)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static uint8 Conv_BoolToByte(bool InBool);
	
	/** Converts a byte to an integer */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToInt (byte)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static int32 Conv_ByteToInt(uint8 InByte);

	/** Converts a color to LinearColor */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToLinearColor (Color)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FLinearColor Conv_ColorToLinearColor(FColor InColor);

	/** Convert an IntVector to a vector */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToVector (IntVector)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FVector Conv_IntVectorToVector(const FIntVector& InIntVector);

	/** Convert a float into a vector, where each element is that float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToVector (float)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FVector Conv_FloatToVector(float InFloat);


	//
	// Box functions
	//

	/** Makes an FBox from Min and Max and sets IsValid to true */
	UFUNCTION(BlueprintPure, Category="Math|Box", meta=(Keywords="construct build", NativeMakeFunc))
	static FBox MakeBox(FVector Min, FVector Max);


	//
	// Box2D functions
	//

	/** Makes an FBox2D from Min and Max and sets IsValid to true */
	UFUNCTION(BlueprintPure, Category = "Math|Box2D", meta = (Keywords = "construct build", NativeMakeFunc))
	static FBox2D MakeBox2D(FVector2D Min, FVector2D Max);


	//
	// Misc functions
	//

	/** Makes a SRand-based random number generator */
	UFUNCTION(BlueprintPure, meta = (Keywords = "construct build", NativeMakeFunc), Category = "Math|Random")
	static FRandomStream MakeRandomStream(int32 InitialSeed);

	/** Breaks apart a random number generator */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (NativeBreakFunc))
	static void BreakRandomStream(const FRandomStream& InRandomStream, int32& InitialSeed);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Utilities|String")
	static FString SelectString(const FString& A, const FString& B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static int32 SelectInt(int32 A, int32 B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float SelectFloat(float A, float B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector SelectVector(FVector A, FVector B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate"))
	static FRotator SelectRotator(FRotator A, FRotator B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Color")
	static FLinearColor SelectColor(FLinearColor A, FLinearColor B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FTransform SelectTransform(const FTransform& A, const FTransform& B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Utilities")
	static UObject* SelectObject(UObject* A, UObject* B, bool bSelectA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static UClass* SelectClass(UClass* A, UClass* B, bool bSelectA);


	//
	// Object operators and functions.
	//
	
	/** Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Object)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities")
	static bool EqualEqual_ObjectObject(class UObject* A, class UObject* B);

	/** Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (Object)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities")
	static bool NotEqual_ObjectObject(class UObject* A, class UObject* B);

	//
	// Class operators and functions.
	//

	/** Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Class)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities")
	static bool EqualEqual_ClassClass(class UClass* A, class UClass* B);

	/** Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (Class)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities")
	static bool NotEqual_ClassClass(class UClass* A, class UClass* B);

	/**
	 * Determine if a class is a child of another class.
	 *
	 * @return	true if TestClass == ParentClass, or if TestClass is a child of ParentClass; false otherwise, or if either
	 *			the value for either parameter is 'None'.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities")
	static bool ClassIsChildOf(TSubclassOf<class UObject> TestClass, TSubclassOf<class UObject> ParentClass);


	//
	// Name operators.
	//
	
	/** Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Name)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities|Name")
	static bool EqualEqual_NameName(FName A, FName B);

	/** Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (Name)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities|Name")
	static bool NotEqual_NameName(FName A, FName B);


	//
	// Transform functions
	//
	
	/** Make a transform from location, rotation and scale */
	UFUNCTION(BlueprintPure, meta = (Scale = "1,1,1", Keywords = "construct build", NativeMakeFunc), Category = "Math|Transform")
	static FTransform MakeTransform(FVector Location, FRotator Rotation, FVector Scale);

	/** Breaks apart a transform into location, rotation and scale */
	UFUNCTION(BlueprintPure, Category = "Math|Transform", meta = (NativeBreakFunc))
	static void BreakTransform(const FTransform& InTransform, FVector& Location, FRotator& Rotation, FVector& Scale);

	/** Returns true if transform A is equal to transform B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal Transform", CompactNodeTitle = "==", ScriptMethod = "Equals", ScriptOperator = "==", Keywords = "== equal"), Category="Math|Transform")
	static bool EqualEqual_TransformTransform(const FTransform& A, const FTransform& B);

	/** 
	 *	Returns true if transform A is nearly equal to B 
	 *	@param LocationTolerance	How close position of transforms need to be to be considered equal
	 *	@param RotationTolerance	How close rotations of transforms need to be to be considered equal
	 *	@param Scale3DTolerance		How close scale of transforms need to be to be considered equal
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Nearly Equal (Transform)", ScriptMethod = "IsNearEqual", Keywords = "== equal"), Category = "Math|Transform")
	static bool NearlyEqual_TransformTransform(const FTransform& A, const FTransform& B, float LocationTolerance = 1.e-4f, float RotationTolerance = 1.e-4f, float Scale3DTolerance = 1.e-4f);

	/**
	 * Compose two transforms in order: A * B.
	 *
	 * Order matters when composing transforms:
	 * A * B will yield a transform that logically first applies A then B to any subsequent transformation.
	 *
	 * Example: LocalToWorld = ComposeTransforms(DeltaRotation, LocalToWorld) will change rotation in local space by DeltaRotation.
	 * Example: LocalToWorld = ComposeTransforms(LocalToWorld, DeltaRotation) will change rotation in world space by DeltaRotation.
	 *
	 * @return New transform: A * B
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "Multiply", ScriptOperator = "*;*=", CompactNodeTitle = "*", Keywords="multiply *"), Category="Math|Transform")
	static FTransform ComposeTransforms(const FTransform& A, const FTransform& B);

	/** 
	 *	Transform a position by the supplied transform.
	 *	For example, if T was an object's transform, this would transform a position from local space to world space.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod="TransformLocation", Keywords="location"), Category="Math|Transform")
	static FVector TransformLocation(const FTransform& T, FVector Location);

	/** 
	 *	Transform a direction vector by the supplied transform - will not change its length. 
	 *	For example, if T was an object's transform, this would transform a direction from local space to world space.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod="TransformDirection"), Category="Math|Transform")
	static FVector TransformDirection(const FTransform& T, FVector Direction);

	/** 
	 *	Transform a rotator by the supplied transform. 
	 *	For example, if T was an object's transform, this would transform a rotation from local space to world space.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod="TransformRotation"), Category="Math|Transform")
	static FRotator TransformRotation(const FTransform& T, FRotator Rotation);

	/** 
	 *	Transform a position by the inverse of the supplied transform.
	 *	For example, if T was an object's transform, this would transform a position from world space to local space.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod="InverseTransformLocation", Keywords="location"), Category="Math|Transform")
	static FVector InverseTransformLocation(const FTransform& T, FVector Location);

	/** 
	 *	Transform a direction vector by the inverse of the supplied transform - will not change its length.
	 *	For example, if T was an object's transform, this would transform a direction from world space to local space.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod="InverseTransformDirection"), Category="Math|Transform")
	static FVector InverseTransformDirection(const FTransform& T, FVector Direction);

	/** 
	 *	Transform a rotator by the inverse of the supplied transform. 
	 *	For example, if T was an object's transform, this would transform a rotation from world space to local space.
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod="InverseTransformRotation"), Category="Math|Transform")
	static FRotator InverseTransformRotation(const FTransform& T, FRotator Rotation);

	/** 
	 * Computes a relative transform of one transform compared to another.
	 *
	 * Example: ChildOffset = MakeRelativeTransform(Child.GetActorTransform(), Parent.GetActorTransform())
	 * This computes the relative transform of the Child from the Parent.
	 *
	 * @param		A				The object's transform
	 * @param		RelativeTo		The transform the result is relative to (in the same space as A)
	 * @return		The new relative transform
	 */
	UFUNCTION(BlueprintPure, meta=(ScriptMethod = "MakeRelative", Keywords="convert torelative"), Category="Math|Transform")
	static FTransform MakeRelativeTransform(const FTransform& A, const FTransform& RelativeTo);

	UE_DEPRECATED(4.22, "Use MakeRelativeTransform instead, with reversed order of arguments.")
	UFUNCTION(BlueprintPure, meta=(Keywords="cast convert"), Category="Math|Transform")
	static FTransform ConvertTransformToRelative(const FTransform& Transform, const FTransform& ParentTransform);

	/** 
	 * Returns the inverse of the given transform T.
	 * 
	 * Example: Given a LocalToWorld transform, WorldToLocal will be returned.
	 *
	 * @param	T	The transform you wish to invert
	 * @return	The inverse of T.
	 */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "Inverse", Keywords = "inverse"), Category = "Math|Transform")
	static FTransform InvertTransform(const FTransform& T);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp (Transform)", AdvancedDisplay = "3", ScriptMethod = "Lerp"), Category="Math|Transform")
	static FTransform TLerp(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<ELerpInterpolationMode::Type> InterpMode = ELerpInterpolationMode::QuatInterp);

	/** Ease between A and B using a specified easing function. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Ease (Transform)", BlueprintInternalUseOnly = "true", ScriptMethod = "Ease"), Category = "Math|Interpolation")
	static FTransform TEase(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Tries to reach a target transform. */
	UFUNCTION(BlueprintPure, meta = (ScriptMethod = "InterpTo"), Category="Math|Interpolation")
	static FTransform TInterpTo(const FTransform& Current, const FTransform& Target, float DeltaTime, float InterpSpeed);

	/** Calculates the determinant of the transform (converts to FMatrix internally) */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta = (DisplayName = "Determinant", ScriptMethod = "Determinant"))
	static float Transform_Determinant(const FTransform& Transform);


	//
	// Interpolation functions
	//

	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation")
	static float FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation")
	static float FInterpTo_Constant(float Current, float Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target rotation based on Current rotation, giving a nice smooth feeling when rotating to Target rotation.
	 *
	 * @param		Current			Actual rotation
	 * @param		Target			Target rotation
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="rotation rotate"))
	static FRotator RInterpTo(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target rotation at a constant rate.
	 *
	 * @param		Current			Actual rotation
	 * @param		Target			Target rotation
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="rotation rotate"))
	static FRotator RInterpTo_Constant(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed);

	/**
	 * Uses a simple spring model to interpolate a float from Current to Target.
	 *
	 * @param Current				Current value
	 * @param Target				Target value
	 * @param SpringState			Data related to spring model (velocity, error, etc..) - Create a unique variable per spring
	 * @param Stiffness				How stiff the spring model is (more stiffness means more oscillation around the target value)
	 * @param CriticalDampingFactor	How much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)
	 * @param Mass					Multiplier that acts like mass on a spring
	 */
	UFUNCTION(BlueprintCallable, Category = "Math|Interpolation")
	static float FloatSpringInterp(float Current, float Target, UPARAM(ref) FFloatSpringState& SpringState, float Stiffness, float CriticalDampingFactor, float DeltaTime, float Mass = 1.f);

	/** Resets the state of a given spring */
	UFUNCTION(BlueprintCallable, Category = "Math|Interpolation")
	static void ResetFloatSpringState(UPARAM(ref) FFloatSpringState& SpringState);

	/** Resets the state of a given spring */
	UFUNCTION(BlueprintCallable, Category = "Math|Interpolation")
	static void ResetVectorSpringState(UPARAM(ref) FVectorSpringState& SpringState);


	//
	// Random stream functions
	//

	/** Returns a uniformly distributed random number between 0 and Max - 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static int32 RandomIntegerFromStream(int32 Max, const FRandomStream& Stream);

	/** Return a random integer between Min and Max (>= Min and <= Max) */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static int32 RandomIntegerInRangeFromStream(int32 Min, int32 Max, const FRandomStream& Stream);

	/** Returns a random bool */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static bool RandomBoolFromStream(const FRandomStream& Stream);

	/** Returns a random float between 0 and 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float RandomFloatFromStream(const FRandomStream& Stream);

	/** Generate a random number between Min and Max */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float RandomFloatInRangeFromStream(float Min, float Max, const FRandomStream& Stream);

	/** Returns a random vector with length of 1.0 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static FVector RandomUnitVectorFromStream(const FRandomStream& Stream);

	/** Create a random rotation */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static FRotator RandomRotatorFromStream(bool bRoll, const FRandomStream& Stream);

	/** Reset a random stream */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void ResetRandomStream(const FRandomStream& Stream);

	/** Create a new random seed for a random stream */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void SeedRandomStream(UPARAM(ref) FRandomStream& Stream);

	/** Set the seed of a random stream to a specific number */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void SetRandomStreamSeed(UPARAM(ref) FRandomStream& Stream, int32 NewSeed);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	 * @param ConeDir					The base "center" direction of the cone.
	 * @param ConeHalfAngleInRadians	The half-angle of the cone (from ConeDir to edge), in radians.
	 * @param Stream					The random stream from which to obtain the vector.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta = (Keywords = "RandomVector"))
	static FVector RandomUnitVectorInConeInRadiansFromStream(const FVector& ConeDir, float ConeHalfAngleInRadians, const FRandomStream& Stream);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	 * @param ConeDir					The base "center" direction of the cone.
	 * @param ConeHalfAngleInDegrees	The half-angle of the cone (from ConeDir to edge), in degrees.
	 * @param Stream					The random stream from which to obtain the vector.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta = (Keywords = "RandomVector"))
	static inline FVector RandomUnitVectorInConeInDegreesFromStream(const FVector& ConeDir, float ConeHalfAngleInDegrees, const FRandomStream& Stream)
	{
		return RandomUnitVectorInConeInRadiansFromStream(ConeDir, FMath::DegreesToRadians(ConeHalfAngleInDegrees), Stream);
	}

	/**
	* Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	* The shape of the cone can be modified according to the yaw and pitch angles.
	*
	* @param MaxYawInRadians	The yaw angle of the cone (from ConeDir to horizontal edge), in radians.
	* @param MaxPitchInRadians	The pitch angle of the cone (from ConeDir to vertical edge), in radians.
	* @param Stream				The random stream from which to obtain the vector.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (Keywords = "RandomVector"))
	static FVector RandomUnitVectorInEllipticalConeInRadiansFromStream(const FVector& ConeDir, float MaxYawInRadians, float MaxPitchInRadians, const FRandomStream& Stream);

	/**
	* Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	* The shape of the cone can be modified according to the yaw and pitch angles.
	*
	* @param MaxYawInDegrees	The yaw angle of the cone (from ConeDir to horizontal edge), in degrees.
	* @param MaxPitchInDegrees	The pitch angle of the cone (from ConeDir to vertical edge), in degrees.
	* @param Stream				The random stream from which to obtain the vector.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (Keywords = "RandomVector"))
	static inline FVector RandomUnitVectorInEllipticalConeInDegreesFromStream(const FVector& ConeDir, float MaxYawInDegrees, float MaxPitchInDegrees, const FRandomStream& Stream)
	{
		return RandomUnitVectorInEllipticalConeInRadiansFromStream(ConeDir, FMath::DegreesToRadians(MaxYawInDegrees), FMath::DegreesToRadians(MaxPitchInDegrees), Stream);
	}

	/**
	 * Generates a 1D Perlin noise from the given value.  Returns a continuous random value between -1.0 and 1.0.
	 *
	 * @param	Value	The input value that Perlin noise will be generated from.  This is usually a steadily incrementing time value.
	 *
	 * @return	Perlin noise in the range of -1.0 to 1.0
	 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float PerlinNoise1D(const float Value);

	//
	// Geometry
	//

	/**  
	 * Finds the minimum area rectangle that encloses all of the points in InVerts
	 * Uses algorithm found in http://www.geometrictools.com/Documentation/MinimumAreaRectangle.pdf
	 *	
	 * @param		InVerts	- Points to enclose in the rectangle
	 * @outparam	OutRectCenter - Center of the enclosing rectangle
	 * @outparam	OutRectSideA - Vector oriented and sized to represent one edge of the enclosing rectangle, orthogonal to OutRectSideB
	 * @outparam	OutRectSideB - Vector oriented and sized to represent one edge of the enclosing rectangle, orthogonal to OutRectSideA
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Math|Geometry", meta=(WorldContext="WorldContextObject", CallableWithoutWorldContext))
	static void MinimumAreaRectangle(UObject* WorldContextObject, const TArray<FVector>& InVerts, const FVector& SampleSurfaceNormal, FVector& OutRectCenter, FRotator& OutRectRotation, float& OutSideLengthX, float& OutSideLengthY, bool bDebugDraw = false);

	/**
	 * Determines whether a given set of points are coplanar, with a tolerance. Any three points or less are always coplanar.
	 *
	 * @param Points - The set of points to determine coplanarity for.
	 * @param Tolerance - Larger numbers means more variance is allowed.
	 *
	 * @return Whether the points are relatively coplanar, based on the tolerance
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Geometry")
	static bool PointsAreCoplanar(const TArray<FVector>& Points, float Tolerance = 0.1f);

	/**
	 * Determines whether the given point is in a box. Includes points on the box.
	 *
	 * @param Point			Point to test
	 * @param BoxOrigin		Origin of the box
	 * @param BoxExtent		Extents of the box (distance in each axis from origin)
	 * @return Whether the point is in the box.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Geometry")
	static bool IsPointInBox(FVector Point, FVector BoxOrigin, FVector BoxExtent);

	/**
	* Determines whether a given point is in a box with a given transform. Includes points on the box.
	*
	* @param Point				Point to test
	* @param BoxWorldTransform	Component-to-World transform of the box.
	* @param BoxExtent			Extents of the box (distance in each axis from origin), in component space.
	* @return Whether the point is in the box.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Geometry")
	static bool IsPointInBoxWithTransform(FVector Point, const FTransform& BoxWorldTransform, FVector BoxExtent);

	/**
	* Returns Slope Pitch and Roll angles in degrees based on the following information: 
	*
	* @param	MyRightYAxis				Right (Y) direction unit vector of Actor standing on Slope.
	* @param	FloorNormal					Floor Normal (unit) vector.
	* @param	UpVector					UpVector of reference frame.
	* @outparam OutSlopePitchDegreeAngle	Slope Pitch angle (degrees)
	* @outparam OutSlopeRollDegreeAngle		Slope Roll angle (degrees)
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Geometry")
	static void GetSlopeDegreeAngles(const FVector& MyRightYAxis, const FVector& FloorNormal, const FVector& UpVector, float& OutSlopePitchDegreeAngle, float& OutSlopeRollDegreeAngle);

	//
	// Intersection
	//

	/**
	 * Computes the intersection point between a line and a plane.
	 * @param		T - The t of the intersection between the line and the plane
	 * @param		Intersection - The point of intersection between the line and the plane
	 * @return		True if the intersection test was successful.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Intersection")
	static bool LinePlaneIntersection(const FVector& LineStart, const FVector& LineEnd, const FPlane& APlane, float& T, FVector& Intersection);

	/**
	 * Computes the intersection point between a line and a plane.
	 * @param		T - The t of the intersection between the line and the plane
	 * @param		Intersection - The point of intersection between the line and the plane
	 * @return		True if the intersection test was successful.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Intersection", meta = (DisplayName = "Line Plane Intersection (Origin & Normal)"))
	static bool LinePlaneIntersection_OriginNormal(const FVector& LineStart, const FVector& LineEnd, FVector PlaneOrigin, FVector PlaneNormal, float& T, FVector& Intersection);


private:

	static void ReportError_Divide_ByteByte();
	static void ReportError_Percent_ByteByte();
	static void ReportError_Divide_IntInt();
	static void ReportError_Divide_Int64Int64();
	static void ReportError_Percent_IntInt();
	static void ReportError_Sqrt();
	static void ReportError_Divide_VectorFloat();
	static void ReportError_Divide_VectorInt();
	static void ReportError_Divide_VectorVector();
	static void ReportError_ProjectVectorOnToVector();
	static void ReportError_Divide_Vector2DFloat();
	static void ReportError_Divide_Vector2DVector2D();
	static void ReportError_DaysInMonth();
};


// Conditionally inlined
#if KISMET_MATH_INLINE_ENABLED
#include "KismetMathLibrary.inl"
#endif

