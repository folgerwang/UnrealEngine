// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreBinarySampleBase.h"

#include "AjaMediaPrivate.h"

/**
 * Implements a media binary data sample for AjaMedia.
 */
class FAjaMediaBinarySample
	: public FMediaIOCoreBinarySampleBase
{
public:

	/**
	 * Initialize the sample.
	 *
	 * @param InReceiverInstance The receiver instance that generated the sample.
	 * @param InFrame The metadata frame data.
	 * @param InTime The sample time (in the player's own clock).
	 */
	bool Initialize(const AJA::AJAAncillaryFrameData& InAncillaryData, FTimespan InTime)
	{
		if (InAncillaryData.AncBuffer == nullptr && InAncillaryData.AncF2Buffer)
		{
			Buffer.Reset();
			return false;
		}

		int32 TotalSize = InAncillaryData.AncBuffer ? InAncillaryData.AncBufferSize : 0;
		TotalSize += InAncillaryData.AncF2Buffer ? InAncillaryData.AncF2BufferSize : 0;
		Buffer.Reset(TotalSize);

		if (InAncillaryData.AncBuffer)
		{
			Buffer.Append(InAncillaryData.AncBuffer, InAncillaryData.AncBufferSize);
		}

		if (InAncillaryData.AncF2Buffer)
		{
			Buffer.Append(InAncillaryData.AncF2Buffer, InAncillaryData.AncF2BufferSize);
		}

		Time = InTime;

		return true;
	}
};

/*
 * Implements a pool for AJA audio sample objects. 
 */

class FAjaMediaBinarySamplePool : public TMediaObjectPool<FAjaMediaBinarySample> { };
