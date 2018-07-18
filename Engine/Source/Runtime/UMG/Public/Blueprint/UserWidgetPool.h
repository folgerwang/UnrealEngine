// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UserWidget.h"
#include "WidgetTree.h"
#include "Slate/SObjectWidget.h"

/** 
 * Pools UUserWidget instances to minimize UObject allocations for UMG elements with dynamic entries. Optionally retains the underlying slate instances of each UUserWidget as well.
 * 
 * Note that if underlying Slate instances are released when a UserWidget instance becomes inactive, NativeConstruct & NativeDestruct will be called when UUserWidget 
 * instances are made active or inactive, respectively, provided the widget isn't actively referenced in the Slate hierarchy (i.e. if the shared reference count on the widget goes from/to 0).
 *
 * WARNING: Be sure to fully reset the pool within the owning widget's ReleaseSlateResources call to prevent leaking due to circular references (since the pool caches hard references to both the UUserWidget and SObjectWidget instances)
 *
 * @see UListView
 * @see UDynamicEntryBox
 */
class UMG_API FUserWidgetPool : public FGCObject
{
public:
	FUserWidgetPool() = default;
	FUserWidgetPool(UWidget& InOwningWidget);
	FUserWidgetPool& operator=(FUserWidgetPool&& Other);
	~FUserWidgetPool();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	bool IsInitialized() const { return OwningWidget.IsValid(); }
	const TArray<UUserWidget*>& GetActiveWidgets() const { return ActiveWidgets; }

	/**
	 * Gets an instance of a widget of the given class.
	 * The underlying slate is stored automatically as well, so the returned widget is fully constructed and GetCachedWidget will return a valid SWidget.
	 */
	template <typename UserWidgetT = UUserWidget>
	UserWidgetT* GetOrCreateInstance(TSubclassOf<UserWidgetT> WidgetClass)
	{
		// Just make a normal SObjectWidget, same as would happen in TakeWidget
		return AddActiveWidgetInternal(WidgetClass,
			[] (UUserWidget* Widget, TSharedRef<SWidget> Content)
			{
				return SNew(SObjectWidget, Widget)[Content];
			});
	}

	using WidgetConstructFunc = TFunctionRef<TSharedPtr<SObjectWidget>(UUserWidget*, TSharedRef<SWidget>)>;

	/** Gets an instance of the widget this factory is for with a custom underlying SObjectWidget type */
	template <typename UserWidgetT = UUserWidget>
	UserWidgetT* GetOrCreateInstance(TSubclassOf<UserWidgetT> WidgetClass, WidgetConstructFunc ConstructWidgetFunc)
	{
		return AddActiveWidgetInternal(WidgetClass, ConstructWidgetFunc);
	}

	/** Return a widget object to the pool, allowing it to be reused in the future */
	void Release(UUserWidget* Widget, bool bReleaseSlate = false);

	/** Returns all active widget objects to the inactive pool and optionally destroys all cached underlying slate widgets. */
	void ReleaseAll(bool bReleaseSlate = false);

	/** Full reset of all created widget objects (and any cached underlying slate) */
	void ResetPool();

private:
	template <typename UserWidgetT = UUserWidget>
	UserWidgetT* AddActiveWidgetInternal(TSubclassOf<UserWidgetT> WidgetClass, WidgetConstructFunc ConstructWidgetFunc)
	{
		if (!IsInitialized())
		{
			return nullptr;
		}

		UUserWidget* WidgetInstance = nullptr;
		for (UUserWidget* InactiveWidget : InactiveWidgets)
		{
			if (InactiveWidget->GetClass() == WidgetClass)
			{
				WidgetInstance = InactiveWidget;
				InactiveWidgets.RemoveSingleSwap(InactiveWidget);
				break;
			}
		}

		if (!WidgetInstance)
		{
			WidgetInstance = CreateWidget(OwningWidget.Get(), WidgetClass);
		}

		if (WidgetInstance)
		{
			TSharedPtr<SWidget>& CachedSlateWidget = CachedSlateByWidgetObject.FindOrAdd(WidgetInstance);
			if (!CachedSlateWidget.IsValid())
			{
				CachedSlateWidget = WidgetInstance->TakeDerivedWidget(ConstructWidgetFunc);
			}
			ActiveWidgets.Add(WidgetInstance);
		}

		return Cast<UserWidgetT>(WidgetInstance);
	}

	TWeakObjectPtr<UWidget> OwningWidget;
	
	TArray<UUserWidget*> ActiveWidgets;
	TArray<UUserWidget*> InactiveWidgets;
	TMap<UUserWidget*, TSharedPtr<SWidget>> CachedSlateByWidgetObject;
};