// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerGenericKeyStruct.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

template<typename ChannelType>
TSharedPtr<FStructOnScope> TMovieSceneKeyStructCustomization<ChannelType>::GetValueStruct()
{
	ChannelType* Channel = ChannelHandle.Get();
	if (Channel)
	{
		auto ChannelData = Channel->GetData();
		const int32 KeyIndex = ChannelData.GetIndex(KeyHandle);

		if (KeyIndex != INDEX_NONE)
		{
			auto& Value = ChannelData.GetValues()[KeyIndex];

			TSharedPtr<FStructOnScope> NewStruct = MakeShared<FStructOnScope>(Value.StaticStruct());

			// Assign over the existing value
			typedef typename TDecay<decltype(Value)>::Type ValueType;
			*((ValueType*)NewStruct->GetStructMemory()) = Value;

			return NewStruct;
		}
	}
	return nullptr;
}

template<typename ChannelType>
void TMovieSceneKeyStructCustomization<ChannelType>::Extend(IDetailLayoutBuilder& DetailBuilder)
{
	using namespace Sequencer;
	if (!KeyStruct.IsValid())
	{
		KeyStruct = GetValueStruct();
	}

	if (KeyStruct.IsValid())
	{
		DetailBuilder.EditCategory("Key").AddExternalStructure(KeyStruct.ToSharedRef());
	}
}

template<typename ChannelType>
void TMovieSceneKeyStructCustomization<ChannelType>::Apply(FFrameNumber NewTime)
{
	ChannelType* Channel = ChannelHandle.Get();
	if (Channel)
	{
		auto ChannelData = Channel->GetData();
		const int32 KeyIndex = ChannelData.GetIndex(KeyHandle);
		if (KeyIndex != INDEX_NONE)
		{
			if (KeyStruct.IsValid())
			{
				auto& Value = ChannelData.GetValues()[KeyIndex];

				// Assign over the new value
				typedef typename TDecay<decltype(Value)>::Type ValueType;
				Value = *((ValueType*)KeyStruct->GetStructMemory());
			}

			// Move the key to the new time
			ChannelData.MoveKey(KeyIndex, NewTime);
		}
	}
}