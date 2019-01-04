// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/** Accumulated error and info messages for a source control operation.  */
struct FSourceControlResultInfo
{
	/** Append any messages from another FSourceControlResultInfo, ensuring to keep any already accumulated info. */
	void Append(const FSourceControlResultInfo& InResultInfo)
	{
		InfoMessages.Append(InResultInfo.InfoMessages);
		ErrorMessages.Append(InResultInfo.ErrorMessages);
	}

	/** Info and/or warning message storage */
	TArray<FText> InfoMessages;

	/** Potential error message storage */
	TArray<FText> ErrorMessages;
};


class ISourceControlOperation : public TSharedFromThis<ISourceControlOperation, ESPMode::ThreadSafe>
{
public:
	/**
	 * Virtual destructor
	 */
	virtual ~ISourceControlOperation() {}

	/** Get the name of this operation, used as a unique identifier */
	virtual FName GetName() const = 0;

	/** Get the string to display when this operation is in progress */
	virtual FText GetInProgressString() const
	{
		return FText();
	}

	/** Retrieve any info or error messages that may have accumulated during the operation. */
	virtual const FSourceControlResultInfo& GetResultInfo() const
	{
		// Implemented in subclasses
		static const FSourceControlResultInfo ResultInfo = FSourceControlResultInfo();
		return ResultInfo;
	}

	/** Add info/warning message. */
	virtual void AddInfoMessge(const FText& InInfo)
	{
		// Implemented in subclasses
	}

	/** Add error message. */
	virtual void AddErrorMessge(const FText& InError)
	{
		// Implemented in subclasses
	}

	/**
	 * Append any info or error messages that may have accumulated during the operation prior
	 * to returning a result, ensuring to keep any already accumulated info.
	 */
	virtual void AppendResultInfo(const FSourceControlResultInfo& InResultInfo)
	{
		// Implemented in subclasses
	}

	/** Factory method for easier operation creation */
	template<typename Type>
	static TSharedRef<Type, ESPMode::ThreadSafe> Create()
	{
		return MakeShareable( new Type() );
	}

};

typedef TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> FSourceControlOperationRef;
