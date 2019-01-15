// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"

/**
 * Flags controlling the behavior of struct serializer backends.
 */
enum class EStructSerializerBackendFlags
{
	/**
	 * No special behavior.
	 */
	None = 0,

	/**
	 * Write text in its complex exported format (eg, NSLOCTEXT(...)) rather than as a simple string.
	 * @note This is required to correctly support localization
	 */
	WriteTextAsComplexString = 1<<0,

	/**
	 * Legacy settings for backwards compatibility with code compiled prior to 4.22.
	 */
	Legacy = None,

	/**
	 * Default settings for code compiled for 4.22 onwards.
	 */
	Default = WriteTextAsComplexString,
};
ENUM_CLASS_FLAGS(EStructSerializerBackendFlags);


/**
 * Structure for the write state stack.
 */
struct FStructSerializerState
{
	/** Holds a flag indicating whether the property has been processed. */
	bool HasBeenProcessed;

	/** Holds a pointer to the key property's data. */
	const void* KeyData;

	/** Holds the key property's meta data (only used for TMap). */
	UProperty* KeyProperty;

	/** Holds a pointer to the property value's data. */
	const void* ValueData;

	/** Holds the property value's meta data. */
	UProperty* ValueProperty;

	/** Holds a pointer to the UStruct describing the data. */
	UStruct* ValueType;
};


/**
 * Interface for UStruct serializer backends.
 */
class IStructSerializerBackend
{
public:

	/**
	 * Signals the beginning of an array.
	 *
	 * State.ValueProperty points to the property that holds the array.
	 *
	 * @param State The serializer's current state.
	 * @see BeginStructure, EndArray
	 */
	virtual void BeginArray(const FStructSerializerState& State) = 0;

	/**
	 * Signals the beginning of a child structure.
	 *
	 * State.ValueProperty points to the property that holds the struct.
	 *
	 * @param State The serializer's current state.
	 * @see BeginArray, EndStructure
	 */
	virtual void BeginStructure(const FStructSerializerState& State) = 0;

	/**
	 * Signals the end of an array.
	 *
	 * State.ValueProperty points to the property that holds the array.
	 *
	 * @param State The serializer's current state.
	 * @see BeginArray, EndStructure
	 */
	virtual void EndArray(const FStructSerializerState& State) = 0;

	/**
	 * Signals the end of an object.
	 *
	 * State.ValueProperty points to the property that holds the struct.
	 *
	 * @param State The serializer's current state.
	 * @see BeginStructure, EndArray
	 */
	virtual void EndStructure(const FStructSerializerState& State) = 0;

	/**
	 * Writes a comment to the output stream.
	 *
	 * @param Comment The comment text.
	 * @see BeginArray, BeginStructure, EndArray, EndStructure, WriteProperty
	 */
	virtual void WriteComment(const FString& Comment) = 0;

	/**
	 * Writes a property to the output stream.
	 *
	 * Depending on the context, properties to be written can be either object properties or array elements.
	 *
	 * State.KeyProperty points to the key property that holds the data to write.
	 * State.KeyData points to the key property's data.
	 * State.ValueProperty points to the property that holds the value to write.
	 * State.ValueData points to the actual data to write.
	 * State.TypeInfo contains the data's type information
	 * State.ArrayIndex is the optional index if the data is a value in an array.
	 *
	 * @param State The serializer's current state.
	 * @see BeginArray, BeginStructure, EndArray, EndStructure, WriteComment
	 */
	virtual void WriteProperty(const FStructSerializerState& State, int32 ArrayIndex = 0) = 0;

public:

	/** Virtual destructor. */
	virtual ~IStructSerializerBackend() { }
};
