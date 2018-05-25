// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Logging/TokenizedMessage.h"

class FEditorViewportClient;
class IPinnedCommandList;

/** Opaque state interface for saving and restoring viewport state */
struct IPersonaViewportState
{
};

/** Abstract viewport that can save and restore state */
class IPersonaViewport : public SCompoundWidget
{
public:
	/** Save the viewport state */
	virtual TSharedRef<IPersonaViewportState> SaveState() const = 0;

	/** Restore the viewport state */
	virtual void RestoreState(TSharedRef<IPersonaViewportState> InState) = 0;

	/** Get the viewport client contained within this viewport */
	virtual FEditorViewportClient& GetViewportClient() const = 0;

	/** Get the pinned commands list for this viewport */
	virtual TSharedRef<IPinnedCommandList> GetPinnedCommandList() const = 0;

	/** 
	 * Add a notification widget 
	 * @param	InSeverity				The severity of the message
	 * @param	InCanBeDismissed		Whether the message can be manually dismissed
	 * @param	InNotificationWidget	The widget showing the notification
	 * @return the widget containing the notification
	 */
	virtual TWeakPtr<SWidget> AddNotification(TAttribute<EMessageSeverity::Type> InSeverity, TAttribute<bool> InCanBeDismissed, const TSharedRef<SWidget>& InNotificationWidget) = 0;

	/** 
	 * Remove a notification widget 
	 * @param	InContainingWidget		The containing widget returned from AddNotification()
	 */
	virtual void RemoveNotification(const TWeakPtr<SWidget>& InContainingWidget) = 0;
};
