// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IncludePython.h"
#include "PyPtr.h"
#include "PyConversionMethod.h"

#if WITH_PYTHON

/** Owner context information for wrapped types */
class FPyWrapperOwnerContext
{
public:
	/** Default constructor */
	FPyWrapperOwnerContext();

	/** Construct this context from the given Python object and optional property (will create a new reference to the given object) */
	explicit FPyWrapperOwnerContext(PyObject* InOwner, const UProperty* InProp = nullptr);

	/** Construct this context from the given Python object and optional property */
	explicit FPyWrapperOwnerContext(const FPyObjectPtr& InOwner, const UProperty* InProp = nullptr);

	/** Reset this context back to its default state */
	void Reset();

	/** Check to see if this context has an owner set */
	bool HasOwner() const;

	/** Get the Python object that owns the instance being wrapped (if any, borrowed reference) */
	PyObject* GetOwnerObject() const;

	/** Get the property on the owner object that owns the instance being wrapped (if known) */
	const UProperty* GetOwnerProperty() const;

	/** Assert that the given conversion method is valid for this owner context */
	void AssertValidConversionMethod(const EPyConversionMethod InMethod) const;

private:
	/** The Python object that owns the instance being wrapped (if any) */
	FPyObjectPtr OwnerObject;

	/** The property on the owner object that owns the instance being wrapped (if known) */
	const UProperty* OwnerProperty;
};

#endif	// WITH_PYTHON
