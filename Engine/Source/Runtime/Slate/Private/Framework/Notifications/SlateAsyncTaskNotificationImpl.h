// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreAsyncTaskNotificationImpl.h"

class SNotificationItem;
class SSlateAsyncTaskNotificationWidget;

/**
 * Slate asynchronous task notification that uses a notification item.
 */
class FSlateAsyncTaskNotificationImpl : public FCoreAsyncTaskNotificationImpl
{
public:
	//~ IAsyncTaskNotificationImpl
	virtual void Initialize(const FAsyncTaskNotificationConfig& InConfig) override;
	virtual void SetCanCancel(const TAttribute<bool>& InCanCancel) override;
	virtual void SetKeepOpenOnSuccess(const TAttribute<bool>& InKeepOpenOnSuccess) override;
	virtual void SetKeepOpenOnFailure(const TAttribute<bool>& InKeepOpenOnFailure) override;
	virtual bool ShouldCancel() const override;

private:
	//~ FCoreAsyncTaskNotificationImpl
	virtual void UpdateNotification() override;

	/** Active Slate notification widget */
	TSharedPtr<SSlateAsyncTaskNotificationWidget> NotificationItemWidget;
};
