// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ISequencerSection.h"
#include "MovieSceneSection.h"
#include "ISectionLayoutBuilder.h"
#include "SequencerSectionPainter.h"
#include "IKeyArea.h"
#include "Channels/MovieSceneChannelProxy.h"

/** Structure used during key area creation to group channels by their group name */
struct FChannelData
{
	/** The channel's type ID (FMovieSceneChannelEntry::RegisterNewID) */
	uint32 ChannelID;

	/** Handle to the channel */
	TMovieSceneChannelHandle<void> Channel;

	/** The channel's common editor data */
	const FMovieSceneChannelEditorData& EditorData;

	/** Handle to the channel's specialized editor data */
	TMovieSceneChannelHandle<const void> SpecializedData;

	/**
	 * Make a key area out of this data
	 */
	TSharedRef<IKeyArea> MakeKeyArea(UMovieSceneSection& InSection) const
	{
		return MakeShared<IKeyArea>(ChannelID, InSection, Channel, EditorData, SpecializedData);
	}
};

/** Data pertaining to a group of channels */
struct FGroupData
{
	FGroupData(FText InGroupText)
		: GroupText(InGroupText)
		, SortOrder(-1)
	{}

	void AddChannel(FChannelData&& InChannel)
	{
		if (InChannel.EditorData.SortOrder < SortOrder)
		{
			SortOrder = InChannel.EditorData.SortOrder;
		}

		Channels.Add(MoveTemp(InChannel));
	}

	/** Text to display for the group */
	FText GroupText;

	/** Sort order of the group */
	uint8 SortOrder;

	/** Array of channels within this group */
	TArray<FChannelData, TInlineAllocator<4>> Channels;
};

void ISequencerSection::GenerateSectionLayout( ISectionLayoutBuilder& LayoutBuilder )
{
	UMovieSceneSection* Section = GetSectionObject();
	if (!Section)
	{
		return;
	}

	// Group channels by their group name
	TMap<FName, FGroupData> GroupToChannelsMap;

	FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
	for (const FMovieSceneChannelEntry& Entry : Section->GetChannelProxy().GetAllEntries())
	{
		const uint32 ChannelID = Entry.GetChannelID();

		// One editor data ptr per channel
		TArrayView<void* const>                        Channels         = Entry.GetChannels();
		TArrayView<const FMovieSceneChannelEditorData> CommonEditorData = Entry.GetCommonEditorData();

		for (int32 Index = 0; Index < Channels.Num(); ++Index)
		{
			TMovieSceneChannelHandle<void>       Channel         = ChannelProxy.MakeHandle(Channels[Index]);
			TMovieSceneChannelHandle<const void> SpecializedData = ChannelProxy.MakeHandle(Entry.GetSpecializedEditorData(Index));

			const FMovieSceneChannelEditorData& EditorData = CommonEditorData[Index];
			if (EditorData.bEnabled)
			{
				FName GroupName = *EditorData.Group.ToString();

				FGroupData* ExistingGroup = GroupToChannelsMap.Find(GroupName);
				if (!ExistingGroup)
				{
					ExistingGroup = &GroupToChannelsMap.Add(GroupName, FGroupData(EditorData.Group));
				}

				FChannelData NewChannel{ ChannelID, Channel, EditorData, SpecializedData };
				ExistingGroup->AddChannel(MoveTemp(NewChannel));
			}
		}
	}

	if (GroupToChannelsMap.Num() == 0)
	{
		return;
	}

	// Collapse single channels to the top level track node if allowed
	if (GroupToChannelsMap.Num() == 1)
	{
		const TTuple<FName, FGroupData>& Pair = *GroupToChannelsMap.CreateIterator();
		if (Pair.Value.Channels.Num() == 1 && Pair.Value.Channels[0].EditorData.bCanCollapseToTrack)
		{
			LayoutBuilder.SetSectionAsKeyArea(Pair.Value.Channels[0].MakeKeyArea(*Section));
			return;
		}
	}

	// Sort the channels in each group by its sort order and name
	TArray<FName, TInlineAllocator<6>> SortedGroupNames;
	for (auto& Pair : GroupToChannelsMap)
	{
		SortedGroupNames.Add(Pair.Key);

		// Sort by sort order then name
		Pair.Value.Channels.Sort([](const FChannelData& A, const FChannelData& B){
			if (A.EditorData.SortOrder == B.EditorData.SortOrder)
			{
				return A.EditorData.Name < B.EditorData.Name;
			}
			return A.EditorData.SortOrder < B.EditorData.SortOrder;
		});
	}

	// Sort groups by the lowest sort order in each group
	auto SortPredicate = [&GroupToChannelsMap](FName A, FName B)
	{
		if (A.IsNone())
		{
			return false;
		}
		else if (B.IsNone())
		{
			return true;
		}

		const int32 SortOrderA = GroupToChannelsMap.FindChecked(A).SortOrder;
		const int32 SortOrderB = GroupToChannelsMap.FindChecked(B).SortOrder;
		return SortOrderA < SortOrderB;
	};
	SortedGroupNames.Sort(SortPredicate);

	// Create key areas for each group name
	for (FName GroupName : SortedGroupNames)
	{
		auto& ChannelData = GroupToChannelsMap.FindChecked(GroupName);

		if (!GroupName.IsNone())
		{
			LayoutBuilder.PushCategory(GroupName, ChannelData.GroupText);
		}

		for (const FChannelData& ChannelAndData : ChannelData.Channels)
		{
			TSharedRef<IKeyArea> KeyArea = ChannelAndData.MakeKeyArea(*Section);
			LayoutBuilder.AddKeyArea(ChannelAndData.EditorData.Name, ChannelAndData.EditorData.DisplayText, KeyArea);
		}

		if (!GroupName.IsNone())
		{
			LayoutBuilder.PopCategory();
		}
	}
}

void ISequencerSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeFrameNumber)
{
	UMovieSceneSection* SectionObject = GetSectionObject();
	if (ResizeMode == ESequencerSectionResizeMode::SSRM_LeadingEdge)
	{
		FFrameNumber MaxFrame = SectionObject->HasEndFrame() ? SectionObject->GetExclusiveEndFrame()-1 : TNumericLimits<int32>::Max();
		ResizeFrameNumber = FMath::Min( ResizeFrameNumber, MaxFrame );

		SectionObject->SetRange(TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Inclusive(ResizeFrameNumber), SectionObject->GetRange().GetUpperBound()));
	}
	else
	{
		FFrameNumber MinFrame = SectionObject->HasStartFrame() ? SectionObject->GetInclusiveStartFrame() : TNumericLimits<int32>::Lowest();
		ResizeFrameNumber = FMath::Max( ResizeFrameNumber, MinFrame );

		SectionObject->SetRange(TRange<FFrameNumber>(SectionObject->GetRange().GetLowerBound(), TRangeBound<FFrameNumber>::Exclusive(ResizeFrameNumber)));
	}
}

int32 FSequencerSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	return Painter.PaintSectionBackground();
}