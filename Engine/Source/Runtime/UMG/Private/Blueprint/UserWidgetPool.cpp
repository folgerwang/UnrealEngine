// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/UserWidgetPool.h"

FUserWidgetPool::FUserWidgetPool(UWidget& InOwningWidget)
	: OwningWidget(&InOwningWidget)
{}

FUserWidgetPool& FUserWidgetPool::operator=(FUserWidgetPool&& Other)
{
	OwningWidget = Other.OwningWidget;

	ActiveWidgets = MoveTemp(Other.ActiveWidgets);
	InactiveWidgets = MoveTemp(Other.InactiveWidgets);
	CachedSlateByWidgetObject = MoveTemp(Other.CachedSlateByWidgetObject);

	Other.OwningWidget.Reset();
	Other.ResetPool();

	return *this;
}

FUserWidgetPool::~FUserWidgetPool()
{
	ResetPool();
}

void FUserWidgetPool::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects<UUserWidget>(ActiveWidgets, OwningWidget.Get());
	Collector.AddReferencedObjects<UUserWidget>(InactiveWidgets, OwningWidget.Get());
}

void FUserWidgetPool::Release(UUserWidget* Widget, bool bReleaseSlate)
{
	const int32 ActiveWidgetIdx = ActiveWidgets.Find(Widget);
	if (ActiveWidgetIdx != INDEX_NONE)
	{
		InactiveWidgets.Push(Widget);
		ActiveWidgets.RemoveAt(ActiveWidgetIdx);

		if (bReleaseSlate)
		{
			CachedSlateByWidgetObject.Remove(Widget);
		}
	}
}

void FUserWidgetPool::ReleaseAll(bool bReleaseSlate)
{
	InactiveWidgets.Append(ActiveWidgets);
	ActiveWidgets.Empty();

	if (bReleaseSlate)
	{
		CachedSlateByWidgetObject.Reset();
	}
}

void FUserWidgetPool::ResetPool()
{
	InactiveWidgets.Reset();
	ActiveWidgets.Reset();
	CachedSlateByWidgetObject.Reset();
}
