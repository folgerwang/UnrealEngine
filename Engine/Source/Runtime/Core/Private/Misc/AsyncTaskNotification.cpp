// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Misc/AsyncTaskNotification.h"
#include "Misc/CoreAsyncTaskNotificationImpl.h"

FAsyncTaskNotification::FAsyncTaskNotification(const FAsyncTaskNotificationConfig& InConfig)
	: NotificationImpl(FAsyncTaskNotificationFactory::Get().InvokeFactory())
{
	check(NotificationImpl);
	NotificationImpl->Initialize(InConfig);
}

FAsyncTaskNotification::~FAsyncTaskNotification()
{
	delete NotificationImpl;
}

FAsyncTaskNotification::FAsyncTaskNotification(FAsyncTaskNotification&& InOther)
	: NotificationImpl(InOther.NotificationImpl)
{
	InOther.NotificationImpl = nullptr;
}

FAsyncTaskNotification& FAsyncTaskNotification::operator=(FAsyncTaskNotification&& InOther)
{
	if (this != &InOther)
	{
		delete NotificationImpl;
		NotificationImpl = InOther.NotificationImpl;
		InOther.NotificationImpl = nullptr;
	}
	return *this;
}

void FAsyncTaskNotification::SetTitleText(const FText& InTitleText, const bool bClearProgressText)
{
	NotificationImpl->SetTitleText(InTitleText, bClearProgressText);
}

void FAsyncTaskNotification::SetProgressText(const FText& InProgressText)
{
	NotificationImpl->SetProgressText(InProgressText);
}

void FAsyncTaskNotification::SetComplete(const bool bSuccess)
{
	NotificationImpl->SetComplete(bSuccess);
}

void FAsyncTaskNotification::SetComplete(const FText& InTitleText, const FText& InProgressText, const bool bSuccess)
{
	NotificationImpl->SetComplete(InTitleText, InProgressText, bSuccess);
}

void FAsyncTaskNotification::SetCanCancel(const TAttribute<bool>& InCanCancel)
{
	NotificationImpl->SetCanCancel(InCanCancel);
}

void FAsyncTaskNotification::SetKeepOpenOnSuccess(const TAttribute<bool>& InKeepOpenOnSuccess)
{
	NotificationImpl->SetKeepOpenOnSuccess(InKeepOpenOnSuccess);
}

void FAsyncTaskNotification::SetKeepOpenOnFailure(const TAttribute<bool>& InKeepOpenOnFailure)
{
	NotificationImpl->SetKeepOpenOnFailure(InKeepOpenOnFailure);
}

bool FAsyncTaskNotification::ShouldCancel() const
{
	return NotificationImpl->ShouldCancel();
}
