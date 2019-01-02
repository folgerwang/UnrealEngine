// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"

class FChaosSolversModule;

namespace Chaos
{
	class PBDRigidsSolver;
	class FPersistentPhysicsTask;
}

namespace Chaos
{
	enum class DispatcherMode : uint8
	{
		DedicatedThread,
		TaskGraph,
		SingleThread
	};

	class IDispatcher
	{
	public:
		virtual ~IDispatcher() {}

		virtual void EnqueueCommand(TFunction<void()> InCommand) = 0;
		virtual void EnqueueCommand(TFunction<void(FPersistentPhysicsTask*)> InCommand) = 0;
		virtual void EnqueueCommand(PBDRigidsSolver* InSolver, TFunction<void(PBDRigidsSolver*)> InCommand) const = 0;
		virtual DispatcherMode GetMode() const = 0;
	};

	template<DispatcherMode Mode>
	class CHAOSSOLVERS_API Dispatcher : public IDispatcher
	{
	public:
		friend class FPersistentPhysicsTask;

		explicit Dispatcher(FChaosSolversModule* InOwnerModule)
			: Owner(InOwnerModule)
		{}

		virtual void EnqueueCommand(TFunction<void()> InCommand) final override;
		virtual void EnqueueCommand(TFunction<void(FPersistentPhysicsTask*)> InCommand) final override;
		virtual void EnqueueCommand(PBDRigidsSolver* InSolver, TFunction<void(PBDRigidsSolver*)> InCommand) const final override;
		virtual DispatcherMode GetMode() const final override { return Mode; }

	private:
		FChaosSolversModule* Owner;

		TQueue<TFunction<void()>, EQueueMode::Mpsc> GlobalCommandQueue;
		TQueue<TFunction<void(FPersistentPhysicsTask*)>, EQueueMode::Mpsc> TaskCommandQueue;
	};

	template<>
	void Dispatcher<DispatcherMode::DedicatedThread>::EnqueueCommand(TFunction<void()> InCommand);
	template<>
	void Dispatcher<DispatcherMode::DedicatedThread>::EnqueueCommand(TFunction<void(FPersistentPhysicsTask*)> InCommand);
	template<>
	void Dispatcher<DispatcherMode::DedicatedThread>::EnqueueCommand(PBDRigidsSolver* InSolver, TFunction<void(PBDRigidsSolver *)> InCommand) const;

	template<>
	void Dispatcher<DispatcherMode::SingleThread>::EnqueueCommand(TFunction<void()> InCommand);
	template<>
	void Dispatcher<DispatcherMode::SingleThread>::EnqueueCommand(TFunction<void(FPersistentPhysicsTask*)> InCommand);
	template<>
	void Dispatcher<DispatcherMode::SingleThread>::EnqueueCommand(PBDRigidsSolver* InSolver, TFunction<void(PBDRigidsSolver *)> InCommand) const;
}

#endif
