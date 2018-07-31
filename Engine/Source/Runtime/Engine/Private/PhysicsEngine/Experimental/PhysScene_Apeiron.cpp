// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_APEIRON

#include "Physics/Experimental/PhysScene_Apeiron.h"

#include "Async/AsyncWork.h"
#include "Async/ParallelFor.h"

class AdvanceOneTimeStepTask : public FNonAbandonableTask
{
    friend class FAutoDeleteAsyncTask<AdvanceOneTimeStepTask>;
public:
    AdvanceOneTimeStepTask(FPhysScene_Apeiron* Scene, const float DeltaTime, TSharedPtr<FCriticalSection> PrevFrameLock, TSharedPtr<FEvent> PrevFrameEvent, TSharedPtr<FCriticalSection> CurrentFrameLock, TSharedPtr<FEvent> CurrentFrameEvent)
        : MScene(Scene), MDeltaTime(DeltaTime), PrevLock(PrevFrameLock), PrevEvent(PrevFrameEvent), CurrentLock(CurrentFrameLock), CurrentEvent(CurrentFrameEvent)
    {
        CurrentFrameLock->Lock();
    }

    void DoWork()
    {
        while (PrevLock.IsValid() && !PrevLock->TryLock())
        {
            PrevEvent->Wait(1);
        }
        MScene->MCreateBodies(MScene->MEvolution->Particles());
        ParallelFor(MScene->GetRigidParticles().Size(), [&](const int32 Index) {
            MScene->MParameterUpdate(MScene->MEvolution->Particles(), MScene->MTime, Index);
        });
        MScene->MDisableCollisionsUpdate(MScene->MEvolution->DisabledCollisions());
        MScene->MStartFrame(MDeltaTime);
        while (MDeltaTime > MScene->MMaxDeltaTime)
        {
            MScene->MEvolution->AdvanceOneTimeStep(MScene->MMaxDeltaTime);
            MDeltaTime -= MScene->MMaxDeltaTime;
        }
        MScene->MEvolution->AdvanceOneTimeStep(MDeltaTime);
		MScene->MEndFrame(MDeltaTime);
        MScene->MTime += MDeltaTime;
        CurrentLock->Unlock();
        CurrentEvent->Trigger();
    }

protected:

	TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(AdvanceOneTimeStepTask, STATGROUP_ThreadPoolAsyncTasks);
    }

    FPhysScene_Apeiron* MScene;
    float MDeltaTime;
    TSharedPtr<FCriticalSection> PrevLock, CurrentLock;
    TSharedPtr<FEvent> PrevEvent, CurrentEvent;
};

FPhysScene_Apeiron::FPhysScene_Apeiron()
    : MCurrentLock(nullptr), MCurrentEvent(nullptr)
{
    Apeiron::TPBDRigidParticles<float, 3> TRigidParticles;
    MEvolution.Reset(new Apeiron::TPBDRigidsEvolution<float, 3>(MoveTemp(TRigidParticles)));
    MMaxDeltaTime = 1;
    MTime = 0;
}

void FPhysScene_Apeiron::Tick(float DeltaTime)
{
	TSharedPtr<FCriticalSection> NewFrameLock(new FCriticalSection());
	TSharedPtr<FEvent> NewFrameEvent(FPlatformProcess::CreateSynchEvent());
	//(new FAutoDeleteAsyncTask<AdvanceOneTimeStepTask>(this, DeltaTime, MCurrentLock, MCurrentEvent, NewFrameLock, NewFrameEvent))->StartBackgroundTask();
	AdvanceOneTimeStepTask(this, DeltaTime, MCurrentLock, MCurrentEvent, NewFrameLock, NewFrameEvent).DoWork();
	MCurrentLock = NewFrameLock;
	MCurrentEvent = NewFrameEvent;
}

#endif

