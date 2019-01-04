// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STreeView.h"

/** A generic widget for selecting an item from an array of items including optional filtering and categorization. */
template<typename CategoryType, typename ItemType>
class SItemSelector : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(TArray<CategoryType>, FOnGetCategoriesForItem, const ItemType& /* Item */);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnCompareCategoriesForEquality, const CategoryType& /* CategoryA */, const CategoryType& /* CategoryB */);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnCompareCategoriesForSorting, const CategoryType& /* CategoryA */, const CategoryType& /* CateogoryB */);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnCompareItemsForSorting, const ItemType& /* ItemA */, const ItemType& /* ItemB */);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnDoesItemMatchFilterText, const FText& /* Filter text */, const ItemType& /* Item */);
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerateWidgetForCategory, const CategoryType& /* Category */);
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerateWidgetForItem, const ItemType& /* Item */);
	DECLARE_DELEGATE_OneParam(FOnItemActivated, const ItemType& /* Item */);

public:
	SLATE_BEGIN_ARGS(SItemSelector)
		: _AllowMultiselect(false)
	{}

	SLATE_ARGUMENT(TArray<ItemType>, Items)
		/** Whether or not this item selector should allow multiple items to be selected. */
		SLATE_ARGUMENT(bool, AllowMultiselect)

		/** An optional delegate to get an array of categories for the specified item. Each category in the returned array represents one level of nested categories. 
		NOTE: The OnCompareCategoriesForEquality, and OnGenerateWidgetForCategory delegates must be bound if this delegate is bound. */
		SLATE_EVENT(FOnGetCategoriesForItem, OnGetCategoriesForItem)

		/** An optional delegate to compare two categories for equality which must be supplied when generating categories for items.  This equality comparer will be used to collate items into matching categories. */
		SLATE_EVENT(FOnCompareCategoriesForEquality, OnCompareCategoriesForEquality)

		/** An optional delegate which determines the sorting for categories.  If not bound categories will be ordered by the order they're encountered while processing items. */
		SLATE_EVENT(FOnCompareCategoriesForSorting, OnCompareCategoriesForSorting)

		/** An optional delegate which determines the sorting for items within each category. */
		SLATE_EVENT(FOnCompareItemsForSorting, OnCompareItemsForSorting)

		/** An optional delegate which can be used to filter items available for selection.  If not bound the search box will not be shown. */
		SLATE_EVENT(FOnDoesItemMatchFilterText, OnDoesItemMatchFilterText)

		/** An optional delegate which generates widgets for categories which must be bound when generating categories for items. */
		SLATE_EVENT(FOnGenerateWidgetForCategory, OnGenerateWidgetForCategory)

		/** The delegate which is used to generate widgets for the items to be selected. */
		SLATE_EVENT(FOnGenerateWidgetForItem, OnGenerateWidgetForItem)

		/** A delegate which is called when an item is activated by either double clicking on it or by pressing enter while it's selected. */
		SLATE_EVENT(FOnItemActivated, OnItemActivated)
	SLATE_END_ARGS();

private:
	enum class EItemSelectorItemViewModelType
	{
		Category,
		Item
	};

	class IItemSelectorItemViewModelUtilities
	{
	public:
		virtual ~IItemSelectorItemViewModelUtilities()
		{
		}

		virtual bool DoesItemMatchFilterText(const ItemType& InItem) const = 0;
		virtual bool CompareCategoriesForEquality(const CategoryType& CategoryA, const CategoryType& CategoryB) const = 0;
		virtual const FOnCompareCategoriesForSorting& GetOnCompareCategoriesForSorting() const = 0;
		virtual const FOnCompareItemsForSorting& GetOnCompareItemsForSorting() const = 0;
	};

	class FItemSelectorItemViewModel
	{
	public:
		FItemSelectorItemViewModel(TSharedRef<IItemSelectorItemViewModelUtilities> InItemUtilities, EItemSelectorItemViewModelType InType)
			: ItemUtilities(InItemUtilities)
			, Type(InType)
		{
		}

		virtual ~FItemSelectorItemViewModel()
		{
		}

		TSharedRef<IItemSelectorItemViewModelUtilities> GetItemUtilities() const
		{
			TSharedPtr<IItemSelectorItemViewModelUtilities> ItemUtilitiesPinned = ItemUtilities.Pin();
			checkf(ItemUtilities.IsValid(), TEXT("Item utilities deleted before item."));
			return ItemUtilitiesPinned.ToSharedRef();
		}

		EItemSelectorItemViewModelType GetType() const
		{
			return Type;
		}

		void GetChildren(TArray<TSharedRef<FItemSelectorItemViewModel>>& OutChildren) const
		{
			TArray<TSharedRef<FItemSelectorItemViewModel>> Children;
			GetChildrenInternal(Children);
			for (TSharedRef<FItemSelectorItemViewModel> Child : Children)
			{
				if (Child->PassesFilter())
				{
					OutChildren.Add(Child);
				}
			}
		}

		virtual bool PassesFilter() const
		{
			return true;
		}

	protected:
		virtual void GetChildrenInternal(TArray<TSharedRef<FItemSelectorItemViewModel>>& OutChildren) const
		{
		}

	private:
		TWeakPtr<IItemSelectorItemViewModelUtilities> ItemUtilities;
		EItemSelectorItemViewModelType Type;
	};

	class FItemSelectorItemContainerViewModel : public FItemSelectorItemViewModel
	{
	public:
		FItemSelectorItemContainerViewModel(TSharedRef<IItemSelectorItemViewModelUtilities> InItemUtilities, const ItemType& InItem)
			: FItemSelectorItemViewModel(InItemUtilities, EItemSelectorItemViewModelType::Item)
			, Item(InItem)
		{
		}

		const ItemType& GetItem() const
		{
			return Item;
		}

		virtual bool PassesFilter() const
		{
			return this->GetItemUtilities()->DoesItemMatchFilterText(Item);
		}

	private:
		const ItemType& Item;
	};

	class FItemSelectorItemCategoryViewModel : public FItemSelectorItemViewModel
	{
	public:
		FItemSelectorItemCategoryViewModel(TSharedRef<IItemSelectorItemViewModelUtilities> InItemUtilities, const CategoryType& InCategory)
			: FItemSelectorItemViewModel(InItemUtilities, EItemSelectorItemViewModelType::Category)
			, Category(InCategory)
		{
		}

		const CategoryType& GetCategory() const
		{
			return Category;
		}

		TSharedRef<FItemSelectorItemCategoryViewModel> AddCategory(const CategoryType& InCategory)
		{
			TSharedRef<FItemSelectorItemCategoryViewModel> NewCategoryViewModel = MakeShared<FItemSelectorItemCategoryViewModel>(this->GetItemUtilities(), InCategory);
			ChildCategoryViewModels.Add(NewCategoryViewModel);
			return NewCategoryViewModel;
		}

		void AddItem(const ItemType& InItem)
		{
			ChildItemViewModels.Add(MakeShared<FItemSelectorItemContainerViewModel>(this->GetItemUtilities(), InItem));
		}

		TSharedPtr<FItemSelectorItemCategoryViewModel> FindChildCategory(const CategoryType& InCategory)
		{
			for (TSharedRef<FItemSelectorItemCategoryViewModel> ChildCategoryViewModel : ChildCategoryViewModels)
			{
				if (this->GetItemUtilities()->CompareCategoriesForEquality(ChildCategoryViewModel->GetCategory(), InCategory))
				{
					return ChildCategoryViewModel;
				}
			}
			return TSharedPtr<FItemSelectorItemCategoryViewModel>();
		}

		void SortChildren()
		{
			TSharedRef<IItemSelectorItemViewModelUtilities> Utilities = this->GetItemUtilities();
			if (ChildCategoryViewModels.Num() > 0 && Utilities->GetOnCompareCategoriesForSorting().IsBound())
			{
				ChildCategoryViewModels.Sort([Utilities](const TSharedRef<FItemSelectorItemCategoryViewModel>& CategoryViewModelA, const TSharedRef<FItemSelectorItemCategoryViewModel>& CategoryViewModelB)
				{
					return Utilities->GetOnCompareCategoriesForSorting().Execute(CategoryViewModelA->GetCategory(), CategoryViewModelB->GetCategory());
				});

			}
			if (ChildItemViewModels.Num() > 0 && Utilities->GetOnCompareItemsForSorting().IsBound())
			{
				ChildItemViewModels.Sort([Utilities](const TSharedRef<FItemSelectorItemContainerViewModel>& ItemViewModelA, const TSharedRef<FItemSelectorItemContainerViewModel>& ItemViewModelB)
				{
					return Utilities->GetOnCompareItemsForSorting().Execute(ItemViewModelA->GetItem(), ItemViewModelB->GetItem());
				});
			}

			for (TSharedRef<FItemSelectorItemCategoryViewModel> ChildCategoryViewModel : ChildCategoryViewModels)
			{
				ChildCategoryViewModel->SortChildren();
			}
		}

		virtual bool PassesFilter() const
		{
			for (const TSharedRef<FItemSelectorItemContainerViewModel> ChildItemViewModel : ChildItemViewModels)
			{
				if (ChildItemViewModel->PassesFilter())
				{
					return true;
				}
			}
			for (const TSharedRef<FItemSelectorItemCategoryViewModel> ChildCategoryViewModel : ChildCategoryViewModels)
			{
				if (ChildCategoryViewModel->PassesFilter())
				{
					return true;
				}
			}
			return false;
		}

	protected:
		virtual void GetChildrenInternal(TArray<TSharedRef<FItemSelectorItemViewModel>>& OutChildren) const
		{
			OutChildren.Append(ChildCategoryViewModels);
			OutChildren.Append(ChildItemViewModels);
		}

	private:
		const CategoryType& Category;
		TArray<TSharedRef<FItemSelectorItemCategoryViewModel>> ChildCategoryViewModels;
		TArray<TSharedRef<FItemSelectorItemContainerViewModel>> ChildItemViewModels;
	};

	class FItemSelectorViewModel : public IItemSelectorItemViewModelUtilities, public TSharedFromThis<FItemSelectorViewModel>
	{
	public:
		FItemSelectorViewModel(TArray<ItemType> InItems, FOnGetCategoriesForItem InOnGetCategoriesForItem, FOnCompareCategoriesForEquality InOnCompareCategoriesForEquality, FOnCompareCategoriesForSorting InOnCompareCategoriesForSorting, FOnCompareItemsForSorting InOnCompareItemsForSorting, FOnDoesItemMatchFilterText InOnDoesItemMatchFilterText)
			: Items(InItems)
			, OnGetCategoriesForItem(InOnGetCategoriesForItem)
			, OnCompareCategoriesForEquality(InOnCompareCategoriesForEquality)
			, OnCompareCategoriesForSorting(InOnCompareCategoriesForSorting)
			, OnCompareItemsForSorting(InOnCompareItemsForSorting)
			, OnDoesItemMatchFilterText(InOnDoesItemMatchFilterText)
		{
		}

		const TArray<TSharedRef<FItemSelectorItemViewModel>>* GetRootItems()
		{
			if (RootCategoryViewModel.IsValid() == false)
			{
				RootCategoryViewModel = MakeShared<FItemSelectorItemCategoryViewModel>(this->AsShared(), RootCategory);
				for (const ItemType& Item : Items)
				{
					// Cache the category array since the category view models hold a const reference to the generated categories and without this they
					// are deleted.
					TSharedRef<TArray<CategoryType>> ItemCategories = MakeShared<TArray<CategoryType>>();
					ItemCategoriesCache.Add(ItemCategories);

					if (OnGetCategoriesForItem.IsBound())
					{
						ItemCategories->Append(OnGetCategoriesForItem.Execute(Item));
					}

					TSharedPtr<FItemSelectorItemCategoryViewModel> CurrentCategoryViewModel = RootCategoryViewModel;
					if (ItemCategories->Num() > 0)
					{

						for (const CategoryType& ItemCategory : (*ItemCategories))
						{
							TSharedPtr<FItemSelectorItemCategoryViewModel> ExistingCategoryViewModel = CurrentCategoryViewModel->FindChildCategory(ItemCategory);
							if (ExistingCategoryViewModel.IsValid())
							{
								CurrentCategoryViewModel = ExistingCategoryViewModel;
							}
							else
							{
								TSharedRef<FItemSelectorItemCategoryViewModel> NewItemCategoryViewModel = CurrentCategoryViewModel->AddCategory(ItemCategory);
								CurrentCategoryViewModel = NewItemCategoryViewModel;
							}
						}
					}
					CurrentCategoryViewModel->AddItem(Item);
				}
				RootCategoryViewModel->SortChildren();
				RootCategoryViewModel->GetChildren(RootTreeCategories);
			}
			return &RootTreeCategories;
		}

		const FText& GetFilterText() const
		{
			return FilterText;
		}

		void SetFilterText(FText InFilterText)
		{
			FilterText = InFilterText;
			RootTreeCategories.Empty();
			RootCategoryViewModel->GetChildren(RootTreeCategories);
		}

		virtual bool DoesItemMatchFilterText(const ItemType& InItem) const override
		{
			return FilterText.IsEmpty() || OnDoesItemMatchFilterText.IsBound() == false || OnDoesItemMatchFilterText.Execute(FilterText, InItem);
		}

		virtual bool CompareCategoriesForEquality(const CategoryType& CategoryA, const CategoryType& CategoryB) const override
		{
			return OnCompareCategoriesForEquality.Execute(CategoryA, CategoryB);
		}

		virtual const FOnCompareCategoriesForSorting& GetOnCompareCategoriesForSorting() const override
		{
			return OnCompareCategoriesForSorting;
		}

		virtual const FOnCompareItemsForSorting& GetOnCompareItemsForSorting() const override
		{
			return OnCompareItemsForSorting;
		}

		bool CanCompareItems() const
		{
			return OnCompareItemsForSorting.IsBound();
		}

		int32 CompareItems(const ItemType& ItemA, const ItemType& ItemB)
		{
			return OnCompareItemsForSorting.Execute(ItemA, ItemB);
		}

	private:
		CategoryType RootCategory;

		TArray<ItemType> Items;
		TArray<TSharedRef<TArray<CategoryType>>> ItemCategoriesCache;

		FOnGetCategoriesForItem OnGetCategoriesForItem;
		FOnCompareCategoriesForEquality OnCompareCategoriesForEquality;
		FOnCompareCategoriesForSorting OnCompareCategoriesForSorting;
		FOnCompareItemsForSorting OnCompareItemsForSorting;
		FOnDoesItemMatchFilterText OnDoesItemMatchFilterText;

		TSharedPtr<FItemSelectorItemCategoryViewModel> RootCategoryViewModel;
		TArray<TSharedRef<FItemSelectorItemViewModel>> RootTreeCategories;
		FText FilterText;
	};

	typedef STableRow<TSharedRef<FItemSelectorItemViewModel>> SItemSelectorTableRow;

	class SItemSelectorItemContainerTableRow : public SItemSelectorTableRow
	{
	public:
		SLATE_BEGIN_ARGS(SItemSelectorItemContainerTableRow)
		{}
			SLATE_DEFAULT_SLOT(typename SItemSelectorItemContainerTableRow::FArguments, Content)
		SLATE_END_ARGS();

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTree)
		{
			typename SItemSelectorTableRow::FArguments Arguments;
			Arguments = Arguments[InArgs._Content.Widget];
			SItemSelectorTableRow::Construct(Arguments,	OwnerTree);
		}

		virtual int32 GetIndentLevel() const override
		{
			return 0;
		}
	};

public:
	void Construct(const FArguments& InArgs)
	{
		Items = InArgs._Items;
		OnGetCategoriesForItem = InArgs._OnGetCategoriesForItem;
		OnCompareCategoriesForEquality = InArgs._OnCompareCategoriesForEquality;
		OnCompareCategoriesForSorting = InArgs._OnCompareCategoriesForSorting;
		OnCompareItemsForSorting = InArgs._OnCompareItemsForSorting;
		OnDoesItemMatchFilterText = InArgs._OnDoesItemMatchFilterText;
		OnGenerateWidgetForCategory = InArgs._OnGenerateWidgetForCategory;
		OnGenerateWidgetForItem = InArgs._OnGenerateWidgetForItem;
		OnItemActivated = InArgs._OnItemActivated;

		checkf(OnGetCategoriesForItem.IsBound() == false || OnCompareCategoriesForEquality.IsBound(), TEXT("OnCompareCategoriesForEquality must be bound if OnGenerateCategoriesForItem is bound."));
		checkf(OnGetCategoriesForItem.IsBound() == false || OnGenerateWidgetForCategory.IsBound(), TEXT("OnGenerateWidgetForCategory must be bound if OnGenerateCategoriesForItem is bound."));
		checkf(OnGenerateWidgetForItem.IsBound(), TEXT("OnGenerateWidgetForItem must be bound"));

		ViewModel = MakeShared<FItemSelectorViewModel>(Items, OnGetCategoriesForItem, OnCompareCategoriesForEquality, OnCompareCategoriesForSorting, OnCompareItemsForSorting, OnDoesItemMatchFilterText);

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 5)
			[
				SAssignNew(SearchBox, SSearchBox)
				.Visibility(this, &SItemSelector::GetSearchBoxVisibility)
				.OnTextChanged(this, &SItemSelector::OnSearchTextChanged)
			]
			+ SVerticalBox::Slot()
			.Padding(0)
			[
				SAssignNew(ItemTree, STreeView<TSharedRef<FItemSelectorItemViewModel>>)
				.SelectionMode(InArgs._AllowMultiselect ? ESelectionMode::Multi : ESelectionMode::SingleToggle)
				.OnGenerateRow(this, &SItemSelector::OnGenerateRow)
				.OnGetChildren(this, &SItemSelector::OnGetChildren)
				.OnMouseButtonDoubleClick(this, &SItemSelector::OnMouseDoubleClick)
				.TreeItemsSource(ViewModel->GetRootItems())
			]
		];

		ExpandTree();
	}

	TArray<ItemType> GetSelectedItems()
	{
		TArray<TSharedRef<FItemSelectorItemViewModel>> SelectedItemViewModels;
		ItemTree->GetSelectedItems(SelectedItemViewModels);

		TArray<ItemType> SelectedItems;
		for (TSharedRef<FItemSelectorItemViewModel> SelectedItemViewModel : SelectedItemViewModels)
		{
			if (SelectedItemViewModel->GetType() == EItemSelectorItemViewModelType::Item)
			{
				TSharedRef<FItemSelectorItemContainerViewModel> SelectedItemContainerViewModel = StaticCastSharedRef<FItemSelectorItemContainerViewModel>(SelectedItemViewModel);
				SelectedItems.Add(SelectedItemContainerViewModel->GetItem());
			}
		}
		return SelectedItems;
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		if (InKeyEvent.GetKey() == EKeys::Enter && OnItemActivated.IsBound())
		{
			TArray<TSharedRef<FItemSelectorItemViewModel>> SelectedItemViewModels;
			ItemTree->GetSelectedItems(SelectedItemViewModels);
			if (SelectedItemViewModels.Num() == 1 && SelectedItemViewModels[0]->GetType() == EItemSelectorItemViewModelType::Item)
			{
				TSharedRef<FItemSelectorItemContainerViewModel> ItemContainer = StaticCastSharedRef<FItemSelectorItemContainerViewModel>(SelectedItemViewModels[0]);
				OnItemActivated.Execute(ItemContainer->GetItem());
				return FReply::Handled();
			}
		}
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	EVisibility GetSearchBoxVisibility() const
	{
		return OnDoesItemMatchFilterText.IsBound() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	void OnSearchTextChanged(const FText& SearchText)
	{
		if (ViewModel->GetFilterText().CompareTo(SearchText) != 0)
		{
			ViewModel->SetFilterText(SearchText);
			ExpandTree();
			ItemTree->RequestTreeRefresh();
		}
	}

	TSharedRef<ITableRow> OnGenerateRow(TSharedRef<FItemSelectorItemViewModel> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		switch (Item->GetType())
		{
		case EItemSelectorItemViewModelType::Category:
		{
			TSharedRef<FItemSelectorItemCategoryViewModel> ItemCategoryViewModel = StaticCastSharedRef<FItemSelectorItemCategoryViewModel>(Item);
			return SNew(STableRow<TSharedRef<FItemSelectorItemViewModel>>, OwnerTable)
				.ShowSelection(false)
				[
					OnGenerateWidgetForCategory.Execute(ItemCategoryViewModel->GetCategory())
				];
		}
		case EItemSelectorItemViewModelType::Item:
		{
			TSharedRef<FItemSelectorItemContainerViewModel> ItemContainerViewModel = StaticCastSharedRef<FItemSelectorItemContainerViewModel>(Item);
			return SNew(SItemSelectorItemContainerTableRow, OwnerTable)
				[
					OnGenerateWidgetForItem.Execute(ItemContainerViewModel->GetItem())
				];
		}
		default:
			checkf(false, TEXT("Unsupported type"));
			return SNew(STableRow<TSharedRef<FItemSelectorItemViewModel>>, OwnerTable);
		}
	}

	void OnGetChildren(TSharedRef<FItemSelectorItemViewModel> Item, TArray<TSharedRef<FItemSelectorItemViewModel>>& OutChildren)
	{
		Item->GetChildren(OutChildren);
	}

	void OnMouseDoubleClick(TSharedRef<FItemSelectorItemViewModel> ItemDoubleClicked)
	{
		if (OnItemActivated.IsBound() && ItemDoubleClicked->GetType() == EItemSelectorItemViewModelType::Item)
		{
			TSharedRef<FItemSelectorItemContainerViewModel> ItemContainer = StaticCastSharedRef<FItemSelectorItemContainerViewModel>(ItemDoubleClicked);
			OnItemActivated.Execute(ItemContainer->GetItem());
		}
	}

	void ExpandTree()
	{
		TArray<TSharedRef<FItemSelectorItemViewModel>> ItemsToProcess;
		ItemsToProcess.Append(*ViewModel->GetRootItems());
		while (ItemsToProcess.Num() > 0)
		{
			TSharedRef<FItemSelectorItemViewModel> ItemToProcess = ItemsToProcess[0];
			ItemsToProcess.RemoveAtSwap(0);
			ItemTree->SetItemExpansion(ItemToProcess, true);
			ItemToProcess->GetChildren(ItemsToProcess);
		}
	}

private:
	TArray<ItemType> Items;

	FOnGetCategoriesForItem OnGetCategoriesForItem;
	FOnCompareCategoriesForEquality OnCompareCategoriesForEquality;
	FOnCompareCategoriesForSorting OnCompareCategoriesForSorting;
	FOnCompareItemsForSorting OnCompareItemsForSorting;
	FOnDoesItemMatchFilterText OnDoesItemMatchFilterText;
	FOnGenerateWidgetForCategory OnGenerateWidgetForCategory;
	FOnGenerateWidgetForItem OnGenerateWidgetForItem;
	FOnItemActivated OnItemActivated;

	TSharedPtr<FItemSelectorViewModel> ViewModel;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<STreeView<TSharedRef<FItemSelectorItemViewModel>>> ItemTree;
};