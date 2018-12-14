// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"

UENUM()
enum class ECollisionTypeEnum : uint8
{
	Chaos_Volumetric         UMETA(DisplayName = "Implicit-Implicit"),
	Chaos_Surface_Volumetric UMETA(DisplayName = "Particle-Implicit"),
	//
	Chaos_Max                UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EImplicitTypeEnum : uint8
{
	Chaos_Implicit_Cube UMETA(DisplayName = "Cube"),
	Chaos_Implicit_Sphere UMETA(DisplayName = "Sphere"),
	Chaos_Implicit_LevelSet UMETA(DisplayName = "Level Set"),
	//
	Chaos_Max                UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EObjectTypeEnum : uint8
{
	Chaos_Object_Dynamic UMETA(DisplayName = "Dynamic"),
	Chaos_Object_Kinematic UMETA(DisplayName = "Kinematic"),
	Chaos_Object_Sleeping UMETA(DisplayName = "Sleeping"),
	//
	Chaos_Max                UMETA(Hidden)
};


UENUM(BlueprintType)
enum class EInitialVelocityTypeEnum : uint8
{
	//Chaos_Initial_Velocity_Animation UMETA(DisplayName = "Animation"),
	Chaos_Initial_Velocity_User_Defined UMETA(DisplayName = "User Defined"),
	//Chaos_Initial_Velocity_Field UMETA(DisplayName = "Field"),
	Chaos_Initial_Velocity_None UMETA(DisplayName = "None"),
	//
	Chaos_Max                UMETA(Hidden)
};


UENUM(BlueprintType)
enum class EEmissionPatternTypeEnum : uint8
{
	Chaos_Emission_Pattern_First_Frame UMETA(DisplayName = "First Frame"),
	Chaos_Emission_Pattern_On_Demand UMETA(DisplayName = "On Demand"),
	//
	Chaos_Max                UMETA(Hidden)
};