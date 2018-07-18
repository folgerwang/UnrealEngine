// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"
#include "Containers/Array.h"

// @todo mesheditor: Comment these classes and enums!

class CORE_API FChange
{

public:

	/** Makes the change to the object, returning a new change that can be used to perfectly roll back this change */
	virtual TUniquePtr<FChange> Execute( UObject* Object ) = 0;

	/** Describes this change (for debugging) */
	virtual FString ToString() const = 0;

	/** Prints this change to the log, including sub-changes if there are any.  For compound changes, there might be multiple lines.  You should not need to override this function. */
	virtual void PrintToLog( class FFeedbackContext& FeedbackContext, const int32 IndentLevel = 0 );

	/** Virtual destructor */
	virtual ~FChange()
	{
	}

protected:

	/** Protected default constructor */
	FChange()
	{
	}

private:
	// Non-copyable
	FChange( const FChange& ) = delete;
	FChange& operator=( const FChange& ) = delete;

};



struct CORE_API FCompoundChangeInput
{
	FCompoundChangeInput()
	{
	}

	explicit FCompoundChangeInput( FCompoundChangeInput&& RHS )
		: Subchanges( MoveTemp( RHS.Subchanges ) )
	{
	}

	/** Ordered list of changes that comprise everything needed to describe this change */
	TArray<TUniquePtr<FChange>> Subchanges;

private:
	// Non-copyable
	FCompoundChangeInput( const FCompoundChangeInput& ) = delete;
	FCompoundChangeInput& operator=( const FCompoundChangeInput& ) = delete;
};


class CORE_API FCompoundChange : public FChange
{

public:

	/** Constructor */
	explicit FCompoundChange( FCompoundChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;
	virtual void PrintToLog( class FFeedbackContext& FeedbackContext, const int32 IndentLevel = 0 ) override;


private:
	
	/** The data we need to make this change */
	FCompoundChangeInput Input;

private:
	// Non-copyable
	FCompoundChange( const FCompoundChange& ) = delete;
	FCompoundChange& operator=( const FCompoundChange& ) = delete;

};


