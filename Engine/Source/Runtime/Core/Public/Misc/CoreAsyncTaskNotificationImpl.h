// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AsyncTaskNotification.h"

/**
 * Implementation interface of notifications for an on-going asynchronous task.
 */
class IAsyncTaskNotificationImpl
{
public:
	virtual ~IAsyncTaskNotificationImpl() = default;

	/**
	 * Initialize this notification based on the given config.
	 */
	virtual void Initialize(const FAsyncTaskNotificationConfig& InConfig) = 0;

	/**
	 * Set the title text of this notification.
	 */
	virtual void SetTitleText(const FText& InTitleText, const bool bClearProgressText) = 0;

	/**
	 * Set the progress text of this notification.
	 */
	virtual void SetProgressText(const FText& InProgressText) = 0;

	/**
	 * Set the task as complete.
	 */
	virtual void SetComplete(const bool bSuccess) = 0;

	/**
	 * Update the text and set the task as complete.
	 */
	virtual void SetComplete(const FText& InTitleText, const FText& InProgressText, const bool bSuccess) = 0;

	/**
	 * Set whether this task be canceled.
	 */
	virtual void SetCanCancel(const TAttribute<bool>& InCanCancel) = 0;

	/**
	 * Set whether to keep this notification open on success.
	*/
	virtual void SetKeepOpenOnSuccess(const TAttribute<bool>& InKeepOpenOnSuccess) = 0;

	/**
	 * Set whether to keep this notification open on failure.
	 */
	virtual void SetKeepOpenOnFailure(const TAttribute<bool>& InKeepOpenOnFailure) = 0;

	/**
	 * True if the user has requested that the task be canceled
	 */
	virtual bool ShouldCancel() const = 0;
};

/**
 * Basic asynchronous task notification that just logs status changes.
 */
class CORE_API FCoreAsyncTaskNotificationImpl : public IAsyncTaskNotificationImpl
{
public:
	FCoreAsyncTaskNotificationImpl();
	virtual ~FCoreAsyncTaskNotificationImpl();

	//~ IAsyncTaskNotificationImpl
	virtual void Initialize(const FAsyncTaskNotificationConfig& InConfig) override;
	virtual void SetTitleText(const FText& InTitleText, const bool bClearProgressText) override;
	virtual void SetProgressText(const FText& InProgressText) override;
	virtual void SetComplete(const bool bSuccess) override;
	virtual void SetComplete(const FText& InTitleText, const FText& InProgressText, const bool bSuccess) override;
	virtual void SetCanCancel(const TAttribute<bool>& InCanCancel) override;
	virtual void SetKeepOpenOnSuccess(const TAttribute<bool>& InKeepOpenOnSuccess) override;
	virtual void SetKeepOpenOnFailure(const TAttribute<bool>& InKeepOpenOnFailure) override;
	virtual bool ShouldCancel() const override;

protected:
	/** Update the notification (the critical section is held while this function is called) */
	virtual void UpdateNotification();

	enum class ENotificationState : uint8
	{
		Pending,
		Success,
		Failure,
	};

	/** The current state of this notification */
	ENotificationState State = ENotificationState::Pending;

	/** The title text displayed in the notification (if any) */
	FText TitleText;

	/** The progress text displayed in the notification (if any) */
	FText ProgressText;

private:
	/** Log the current notification state (if any, and if enabled) */
	void LogNotification();

	/** Category this task should log its notifications under, or null to skip logging */
	const FLogCategoryBase* LogCategory = nullptr;

	/** Critical section protecting concurrent access to this object state */
	mutable FCriticalSection SynchronizationObject;
};

/**
 * Factory to allow other systems (such as Slate) to override the default asynchronous task notification implementation.
 */
class CORE_API FAsyncTaskNotificationFactory
{
	friend class FAsyncTaskNotification;

public:
	typedef IAsyncTaskNotificationImpl* FImplPointerType;
	typedef TFunction<FImplPointerType()> FFactoryFunc;

	/**
	 * Get the factory singleton.
	 */
	static FAsyncTaskNotificationFactory& Get();

	/**
	 * Register a factory function.
	 */
	void RegisterFactory(const FName InName, const FFactoryFunc& InFunc);

	/**
	 * Unregister a factory function.
	 */
	void UnregisterFactory(const FName InName);

private:
	/**
	 * Invoke the active factory function (if any), or return a default instance.
	 */
	FImplPointerType InvokeFactory() const;

	/** Registered factories */
	TArray<TTuple<FName, FFactoryFunc>> Factories;
};
