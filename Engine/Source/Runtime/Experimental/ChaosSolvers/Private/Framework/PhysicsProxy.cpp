// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "Framework/PhysicsProxy.h"

#include "Modules/ModuleManager.h"
#include "ChaosSolversModule.h"

namespace Chaos
{
	FPhysicsProxy::FPhysicsProxy()
		: Solver(nullptr)
		, Callbacks(nullptr)
	{
	}

	FSolverCallbacks* FPhysicsProxy::GetCallbacks()
	{
		if(!Callbacks)
		{
			Callbacks = OnCreateCallbacks();
		}

		return Callbacks;
	}

	void FPhysicsProxy::DestroyCallbacks()
	{
		if(Callbacks)
		{
			OnDestroyCallbacks(Callbacks);
			Callbacks = nullptr;
		}
	}

	void FPhysicsProxy::SetSolver(PBDRigidsSolver* InSolver)
	{
		Solver = InSolver;
	}

	bool FPhysicsProxy::IsMultithreaded() const
	{
		FChaosSolversModule* Module = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");

		if(Module)
		{
			return Module->IsPersistentTaskEnabled() && Module->IsPersistentTaskRunning();
		}

		return false;
	}

}

#endif
