// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Field/FieldSystem.h"

#if INCLUDE_CHAOS

#include "PBDRigidsSolver.h"

class FIELDSYSTEMSIMULATIONCORE_API FFieldSystemSolverCallbacks : public FSolverFieldCallbacks
{
public:
	static int32 Invalid; 

	FFieldSystemSolverCallbacks() = delete;
	FFieldSystemSolverCallbacks(const FFieldSystem & System);
	virtual ~FFieldSystemSolverCallbacks() { }

	/**/
	virtual bool IsSimulating() const override;

	/**/
	virtual void CommandUpdateCallback(FSolverCallbacks::FParticlesType& Particles, 
		Chaos::TArrayCollectionArray<FVector> & LinearForces, const float Time) override;

	/**/
	virtual void BufferCommand(const FFieldSystemCommand& Command) { Commands.Add(Command); }

private:
	TArray<FFieldSystemCommand> Commands;
};

#else
// Stub solver callbacks for non Chaos builds. 
class FIELDSYSTEMSIMULATIONCORE_API FFieldSystemSolverCallbacks
{
public:
	FFieldSystemSolverCallbacks();
};
#endif
