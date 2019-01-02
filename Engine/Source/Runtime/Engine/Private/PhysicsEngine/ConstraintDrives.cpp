// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/ConstraintDrives.h"
#include "PhysicsPublic.h"
#include "PhysXIncludes.h"

const bool bIsAccelerationDrive = true;

FConstraintDrive::FConstraintDrive()
	: Stiffness(50.f)
	, Damping(1.f)
	, MaxForce(0.f)
	, bEnablePositionDrive(false)
	, bEnableVelocityDrive(false)
{
}

FLinearDriveConstraint::FLinearDriveConstraint()
	: PositionTarget(ForceInit)
	, VelocityTarget(ForceInit)
	, bEnablePositionDrive(false)
{
}

FAngularDriveConstraint::FAngularDriveConstraint()
	: OrientationTarget(ForceInit)
	, AngularVelocityTarget(ForceInit)
	, AngularDriveMode(EAngularDriveMode::SLERP)
{
}

void FLinearDriveConstraint::SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
	XDrive.bEnablePositionDrive = bEnableXDrive;
	YDrive.bEnablePositionDrive = bEnableYDrive;
	ZDrive.bEnablePositionDrive = bEnableZDrive;
}

void FLinearDriveConstraint::SetLinearVelocityDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
	XDrive.bEnableVelocityDrive = bEnableXDrive;
	YDrive.bEnableVelocityDrive = bEnableYDrive;
	ZDrive.bEnableVelocityDrive = bEnableZDrive;
}

void FAngularDriveConstraint::SetOrientationDriveTwistAndSwing(bool InEnableTwistDrive, bool InEnableSwingDrive)
{
	SwingDrive.bEnablePositionDrive = InEnableSwingDrive;
	TwistDrive.bEnablePositionDrive = InEnableTwistDrive;
}

void FAngularDriveConstraint::SetOrientationDriveSLERP(bool InEnableSLERP)
{
	SlerpDrive.bEnablePositionDrive = InEnableSLERP;
}

void FAngularDriveConstraint::SetAngularVelocityDriveTwistAndSwing(bool InEnableTwistDrive, bool InEnableSwingDrive)
{
	SwingDrive.bEnableVelocityDrive = InEnableSwingDrive;
	TwistDrive.bEnableVelocityDrive = InEnableTwistDrive;
}

void FAngularDriveConstraint::SetAngularVelocityDriveSLERP(bool InEnableSLERP)
{
	SlerpDrive.bEnableVelocityDrive = InEnableSLERP;
}

void FConstraintDrive::SetDriveParams(float InStiffness, float InDamping, float InForceLimit)
{
	Stiffness = InStiffness;
	Damping = InDamping;
	MaxForce = InForceLimit;
}

void FLinearDriveConstraint::SetDriveParams(float InStiffness, float InDamping, float InForceLimit)
{
	XDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
	YDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
	ZDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
}

void FAngularDriveConstraint::SetAngularDriveMode(EAngularDriveMode::Type DriveMode)
{
	AngularDriveMode = DriveMode;
}

void FAngularDriveConstraint::SetDriveParams(float InStiffness, float InDamping, float InForceLimit)
{
	SwingDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
	TwistDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
	SlerpDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
}
