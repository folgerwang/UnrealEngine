// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include <chrono>

class FThread final : public FRunnable
{
public:
	using FCallback = TFunction<void()>;

	explicit FThread(TCHAR const* ThreadName, const FCallback& Callback) :
		Callback(Callback)
	{
		Thread = FRunnableThread::Create(this, ThreadName);
	}

	void Join()
	{
		Thread->WaitForCompletion();
	}

	virtual uint32 Run() override
	{
		Callback();
		return 0;
	}

private:
	FCallback Callback;
	FRunnableThread* Thread;

private:
	FThread(const FThread&) = delete;
	FThread& operator=(const FThread&) = delete;
};

// uses chrono library to have comparable timestamps between UE4 and webrtc app
inline uint64 NowMs()
{
	//return static_cast<uint64>(FPlatformTime::Cycles64() * FPlatformTime::GetSecondsPerCycle64() * 1000);

	//double secs = FPlatformTime::Seconds();
	//// for the trick look at `FWindowsPlatformTime::Seconds()`
	//return static_cast<uint64>((secs - 16777216) * 1000);

	using namespace std::chrono;
	system_clock::duration now = system_clock::now().time_since_epoch();
	return duration_cast<milliseconds>(now - duration_cast<minutes>(now)).count();
}
