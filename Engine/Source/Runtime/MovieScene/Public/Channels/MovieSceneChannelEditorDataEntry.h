// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Misc/InlineValue.h"
#include "Algo/Sort.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "Channels/MovieSceneChannelEditorData.h"

#if WITH_EDITOR

/**
 * Base entry type for use in FMovieSceneChannelProxy that stores a piece of editor data for each channel or a given type (one entry per type).
 */
struct FMovieSceneChannelEditorDataEntry
{
	/**
	 * Templated constructor that uses the channel parameter to construct
	 * the editor data array from the correct editor data type
	 */
	template<typename ChannelType>
	explicit FMovieSceneChannelEditorDataEntry(const ChannelType& Channel)
	{
		ConstructSpecializedEditorDataArray<ChannelType>((typename TMovieSceneChannelTraits<ChannelType>::EditorDataType*)nullptr);
	}

	/**
	 * Get the common editor data for all channels
	 */
	TArrayView<const FMovieSceneChannelEditorData> GetCommonEditorData() const
	{
		return CommonEditorDataArray;
	}

	/**
	 * Access the specialized editor data for a specific channel
	 */
	const void* GetSpecializedEditorData(int32 ChannelIndex) const
	{
		if (SpecializedEditorDataArray.IsValid())
		{
			return SpecializedEditorDataArray->GetChannel(ChannelIndex);
		}
		return nullptr;
	}

protected:

	/**
	 * Add new editor data for the specified channel type at the last index in the array
	 */
	template<typename ChannelType>
	void AddEditorData(const FMovieSceneChannelEditorData& CommonData)
	{
		static_assert(TIsSame<typename TMovieSceneChannelTraits<ChannelType>::EditorDataType, void>::Value, "Must supply typed editor data according to the channel's traits.");

		// Add the common editor data
		CommonEditorDataArray.Add(CommonData);
	}

	/**
	 * Add new editor data for the specified channel type at the last index in the arrays
	 */
	template<typename ChannelType, typename SpecializedEditorData>
	void AddEditorData(const FMovieSceneChannelEditorData& CommonData, SpecializedEditorData&& InTypeSpecificEditorData)
	{
		// Add the common editor data
		CommonEditorDataArray.Add(CommonData);

		auto& TypedImpl = static_cast<TMovieSceneEditorDataArray<ChannelType>&>(SpecializedEditorDataArray.GetValue());
		TypedImpl.Data.Add(Forward<SpecializedEditorData>(InTypeSpecificEditorData));
	}

	/**
	 * Access the specialized editor data for channels stored in this entry
	 */
	template<typename ChannelType>
	TArrayView<const typename TMovieSceneChannelTraits<ChannelType>::EditorDataType> GetAllSpecializedEditorData() const
	{
		static_assert(!TIsSame<typename TMovieSceneChannelTraits<ChannelType>::EditorDataType, void>::Value, "This channel type does not define any additional editor data.");

		const auto& TypedImpl = static_cast<const TMovieSceneEditorDataArray<ChannelType>&>(SpecializedEditorDataArray.GetValue());
		return TypedImpl.Data;
	}

private:

	/** Construct the specialized editor data container for channel types that require it */
	template<typename ChannelType, typename EditorDataType>
	void ConstructSpecializedEditorDataArray(EditorDataType*)
	{
		SpecializedEditorDataArray = TMovieSceneEditorDataArray<ChannelType>();
	}

	template<typename ChannelType>
	void ConstructSpecializedEditorDataArray(void*)
	{
	}

private:

	/** Base editor data, one per channel */
	TArray<FMovieSceneChannelEditorData, TInlineAllocator<1>> CommonEditorDataArray;

	/**
	 * We store the array behind an interface whose access is via void*
	 * Typed access is only permitted using the original ChannelType templated methods
	 * to ensure safe casting of the array
	 */
	struct IMovieSceneEditorDataArray
	{
		virtual ~IMovieSceneEditorDataArray() {}
		virtual const void* GetChannel(int32 Index) const = 0;
	};

	template<typename ChannelType>
	struct TMovieSceneEditorDataArray : IMovieSceneEditorDataArray
	{
		typedef typename TMovieSceneChannelTraits<ChannelType>::EditorDataType EditorDataType;

		virtual const void* GetChannel(int32 Index) const { return &Data[Index]; }

		/** The actual editor data */
		TArray<EditorDataType> Data;
	};

	/** Typed editor data, one per channel, defined by TMovieSceneChannelTraits::EditorDataType */
	TInlineValue<IMovieSceneEditorDataArray, 8> SpecializedEditorDataArray;
};

#else // WITH_EDITOR

/** Empty stub in non-editor bulds */
struct FMovieSceneChannelEditorDataEntry
{
	template<typename ChannelType> FMovieSceneChannelEditorDataEntry(const ChannelType&){}
};

#endif // WITH_EDITOR
