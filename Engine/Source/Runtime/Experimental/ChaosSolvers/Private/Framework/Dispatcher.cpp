// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "Framework/Dispatcher.h"

#include "PBDRigidsSolver.h"

namespace Chaos
{
	template<>
	void Dispatcher<DispatcherMode::DedicatedThread>::EnqueueCommand(PBDRigidsSolver* InSolver, TFunction<void(PBDRigidsSolver *)> InCommand) const
	{
		check(InSolver && InCommand);
		InSolver->GetCommandQueue().Enqueue(InCommand);
	}

	template<>
	void Dispatcher<DispatcherMode::DedicatedThread>::EnqueueCommand(TFunction<void(FPersistentPhysicsTask*)> InCommand)
	{
		check(Owner);
		TaskCommandQueue.Enqueue(InCommand);
	}

	template<>
	void Dispatcher<DispatcherMode::DedicatedThread>::EnqueueCommand(TFunction<void()> InCommand)
	{
		check(Owner);
		GlobalCommandQueue.Enqueue(InCommand);
	}

	//////////////////////////////////////////////////////////////////////////

	template<>
	void Dispatcher<DispatcherMode::SingleThread>::EnqueueCommand(PBDRigidsSolver* InSolver, TFunction<void(PBDRigidsSolver *)> InCommand) const
	{
		check(InSolver && InCommand);
		InCommand(InSolver);
	}

	template<>
	void Dispatcher<DispatcherMode::SingleThread>::EnqueueCommand(TFunction<void(FPersistentPhysicsTask*)> InCommand)
	{
		check(Owner);
		InCommand(nullptr);
	}

	template<>
	void Dispatcher<DispatcherMode::SingleThread>::EnqueueCommand(TFunction<void()> InCommand)
	{
		check(Owner);
		InCommand();
	}
}

#endif
