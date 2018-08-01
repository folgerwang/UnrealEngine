// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Physics/Experimental/PhysScene_LLImmediate.h"
#include "Async/ParallelFor.h"


using namespace ImmediatePhysics;

FPhysScene_LLImmediate::FPhysScene_LLImmediate()
	: Simulation(nullptr)
	, SimulationTime(0)
{
	Init();
}

FPhysScene_LLImmediate::~FPhysScene_LLImmediate()
{
	if(Simulation)
	{
		delete Simulation;
		Simulation = nullptr;
	}
}

void FPhysScene_LLImmediate::Init()
{
	if(Simulation)
	{
		delete Simulation;
		Simulation = nullptr;
	}
	
	Simulation = new FSimulation();
}

void FPhysScene_LLImmediate::Tick(float InDeltaSeconds)
{
	check(Simulation);

	if(CreateBodiesFunction)
	{
		CreateBodiesFunction(Simulation->GetActorHandles());
	}

	ParallelFor(Simulation->NumActors(), [&](const int32& Index)
	{
		if(ParameterUpdateFunction)
		{
			ParameterUpdateFunction(Simulation->GetActorHandles(), InDeltaSeconds, Index);
		}
		
		for(TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float, const int32)>& ForceFunction : ForceFunctions)
		{
			if(ensure(ForceFunction))
			{
				ForceFunction(Simulation->GetActorHandles(), InDeltaSeconds, Index);
			}
		}
	});

	if(DisableCollisionsUpdateFunction)
	{
		TSet<TTuple<int32, int32>> Temp;
		DisableCollisionsUpdateFunction(Temp);
	}
	
	if(StartFrameFunction)
	{
		StartFrameFunction(InDeltaSeconds);
	}

	Simulation->Simulate(InDeltaSeconds, FVector(0.0f, 0.0f, -980.0f));

	if(EndFrameFunction)
	{
		EndFrameFunction(InDeltaSeconds);
	}
	
	SimulationTime += InDeltaSeconds;
}

template class FPhysScene_Base<FPhysScene_LLImmediate>;
