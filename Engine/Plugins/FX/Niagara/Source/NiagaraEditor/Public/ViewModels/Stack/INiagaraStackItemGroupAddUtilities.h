// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Represents a single action for adding an item to a group in the stack. */
class INiagaraStackItemGroupAddAction
{
public:
	/** Gets the category for this action. */
	virtual FText GetCategory() const = 0;

	/** Gets the short display name for this action. */
	virtual FText GetDisplayName() const = 0;

	/** Gets a long description of what will happen if this add action is executed. */
	virtual FText GetDescription() const = 0;

	/** Gets a space separated string of keywords which expose additional search terms for this action. */
	virtual FText GetKeywords() const = 0;

	virtual ~INiagaraStackItemGroupAddAction() { }
};

/** Defines utilities for generically handling adding items to groups in the stack. */
class INiagaraStackItemGroupAddUtilities
{
public:
	/** Defines different modes for adding to a stack group. */
	enum EAddMode
	{
		/** The group adds a new item directly. */
		AddDirectly,
		/** The group provides a list of actions to choose from for adding. */
		AddFromAction
	};

public:
	/** Gets the generic name for the type of item to add e.g. "Module" */
	virtual FText GetAddItemName() const = 0;

	/** Gets whether or not the add actions should be automatically expanded in the UI. */
	virtual bool GetAutoExpandAddActions() const = 0;

	/** Gets the add mode supported by these add group utilities. */
	virtual EAddMode GetAddMode() const = 0;

	/** Adds a new item directly. */
	virtual void AddItemDirectly() = 0;

	/** Populates an array with the valid add actions. */
	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions) const = 0;

	/** Executes the specified add action. */
	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) = 0;

	virtual ~INiagaraStackItemGroupAddUtilities() { }
};

class FNiagaraStackItemGroupAddUtilities : public INiagaraStackItemGroupAddUtilities
{
public:
	FNiagaraStackItemGroupAddUtilities(FText InAddItemName, EAddMode InAddMode, bool bInAutoExpandAddActions)
		: AddItemName(InAddItemName)
		, bAutoExpandAddActions(bInAutoExpandAddActions)
		, AddMode(InAddMode)
	{
	}

	virtual FText GetAddItemName() const override { return AddItemName; }
	virtual bool GetAutoExpandAddActions() const override { return bAutoExpandAddActions; }
	virtual EAddMode GetAddMode() const override { return AddMode; }

protected:
	FText AddItemName;
	bool bAutoExpandAddActions;
	EAddMode AddMode;
};

template<typename AddedItemType>
class TNiagaraStackItemGroupAddUtilities : public FNiagaraStackItemGroupAddUtilities
{
public:
	DECLARE_DELEGATE_OneParam(FOnItemAdded, AddedItemType);

public:
	TNiagaraStackItemGroupAddUtilities(FText InAddItemName, EAddMode InAddMode, bool bInAutoExpandAddActions, FOnItemAdded InOnItemAdded)
		: FNiagaraStackItemGroupAddUtilities(InAddItemName, InAddMode, bInAutoExpandAddActions)
		, OnItemAdded(InOnItemAdded)
	{
	}

protected:
	FOnItemAdded OnItemAdded;
};