// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IMediaModule.h"

#include "CoreMinimal.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"

#include "IMediaCaptureSupport.h"
#include "IMediaPlayerFactory.h"
#include "IMediaTimeSource.h"
#include "MediaClock.h"
#include "MediaTicker.h"


/**
 * Implements the Media module.
 */
class FMediaModule
	: public IMediaModule
{
public:

	//~ IMediaModule interface

	virtual const TArray<IMediaCaptureSupport*>& GetCaptureSupports() const override
	{
		return CaptureSupports;
	}

	virtual IMediaClock& GetClock() override
	{
		return Clock;
	}

	virtual const TArray<IMediaPlayerFactory*>& GetPlayerFactories() const override
	{
		return PlayerFactories;
	}

	virtual IMediaPlayerFactory* GetPlayerFactory(const FName& FactoryName) const override
	{
		for (IMediaPlayerFactory* Factory : PlayerFactories)
		{
			if (Factory->GetPlayerName() == FactoryName)
			{
				return Factory;
			}
		}

		return nullptr;
	}

	virtual IMediaTicker& GetTicker() override
	{
		return Ticker;
	}

	virtual FSimpleMulticastDelegate& GetOnTickPreEngineCompleted() override
	{
		return OnTickPreEngineCompleted;
	}

	virtual void LockToTimecode(bool Locked) override
	{
		TimecodeLocked = Locked;
	}

	virtual void RegisterCaptureSupport(IMediaCaptureSupport& Support) override
	{
		CaptureSupports.AddUnique(&Support);
	}

	virtual void RegisterPlayerFactory(IMediaPlayerFactory& Factory) override
	{
		PlayerFactories.AddUnique(&Factory);
	}

	virtual void SetTimeSource(const TSharedPtr<IMediaTimeSource, ESPMode::ThreadSafe>& NewTimeSource) override
	{
		TimeSource = NewTimeSource;
	}

	virtual void TickPostEngine() override
	{
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_Media_TickFetch);
			Clock.TickFetch();
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_Media_TickRender);
			Clock.TickRender();
		}
	}

	virtual void TickPostRender() override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Media_TickOutput);
		Clock.TickOutput();
	}

	virtual void TickPreEngine() override
	{
		if (TimeSource.IsValid())
		{
			Clock.UpdateTimecode(TimeSource->GetTimecode(), TimecodeLocked);
		}

		QUICK_SCOPE_CYCLE_COUNTER(STAT_Media_TickInput);
		Clock.TickInput();

		OnTickPreEngineCompleted.Broadcast();
	}

	virtual void TickPreSlate() override
	{
		// currently not used
	}

	virtual void UnregisterCaptureSupport(IMediaCaptureSupport& Support) override
	{
		CaptureSupports.Remove(&Support);
	}

	virtual void UnregisterPlayerFactory(IMediaPlayerFactory& Factory) override
	{
		PlayerFactories.Remove(&Factory);
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		if (!IsRunningDedicatedServer())
		{
			TickerThread = FRunnableThread::Create(&Ticker, TEXT("FMediaTicker"));
		}
	}

	virtual void ShutdownModule() override
	{
		if (TickerThread != nullptr)
		{
			TickerThread->Kill(true);
			delete TickerThread;
			TickerThread = nullptr;
		}

		CaptureSupports.Reset();
		PlayerFactories.Reset();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

private:

	/** The registered capture device support objects. */
	TArray<IMediaCaptureSupport*> CaptureSupports;

	/** The media clock. */
	FMediaClock Clock;

	/** Time code of the current frame. */
	FTimespan CurrentTimecode;

	/** The registered video player factories. */
	TArray<IMediaPlayerFactory*> PlayerFactories;

	/** High-frequency ticker runnable. */
	FMediaTicker Ticker;

	/** High-frequency ticker thread. */
	FRunnableThread* TickerThread;

	/** Delegate to receive TickPreEngine */
	FSimpleMulticastDelegate OnTickPreEngineCompleted;

	/** Whether media objects should lock to the media clock's time code. */
	bool TimecodeLocked;

	/** The media clock's time source. */
	TSharedPtr<IMediaTimeSource, ESPMode::ThreadSafe> TimeSource;
};


IMPLEMENT_MODULE(FMediaModule, Media);
