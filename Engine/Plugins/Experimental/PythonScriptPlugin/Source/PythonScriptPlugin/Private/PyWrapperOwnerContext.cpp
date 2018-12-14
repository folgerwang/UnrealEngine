// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PyWrapperOwnerContext.h"

#if WITH_PYTHON

FPyWrapperOwnerContext::FPyWrapperOwnerContext()
	: OwnerObject()
	, OwnerProperty(nullptr)
{
}

FPyWrapperOwnerContext::FPyWrapperOwnerContext(PyObject* InOwner, const UProperty* InProp)
	: OwnerObject(FPyObjectPtr::NewReference(InOwner))
	, OwnerProperty(InProp)
{
	checkf(!OwnerProperty || OwnerObject.IsValid(), TEXT("Owner context cannot have an owner property without an owner object"));
}

FPyWrapperOwnerContext::FPyWrapperOwnerContext(const FPyObjectPtr& InOwner, const UProperty* InProp)
	: OwnerObject(InOwner)
	, OwnerProperty(InProp)
{
	checkf(!OwnerProperty || OwnerObject.IsValid(), TEXT("Owner context cannot have an owner property without an owner object"));
}

void FPyWrapperOwnerContext::Reset()
{
	OwnerObject.Reset();
	OwnerProperty = nullptr;
}

bool FPyWrapperOwnerContext::HasOwner() const
{
	return OwnerObject.IsValid();
}

PyObject* FPyWrapperOwnerContext::GetOwnerObject() const
{
	return (PyObject*)OwnerObject.GetPtr();
}

const UProperty* FPyWrapperOwnerContext::GetOwnerProperty() const
{
	return OwnerProperty;
}

void FPyWrapperOwnerContext::AssertValidConversionMethod(const EPyConversionMethod InMethod) const
{
	::AssertValidPyConversionOwner(GetOwnerObject(), InMethod);
}

#endif	// WITH_PYTHON
