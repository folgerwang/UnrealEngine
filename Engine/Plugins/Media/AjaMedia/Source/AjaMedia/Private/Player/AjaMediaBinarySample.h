// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreBinarySampleBase.h"

#include "AjaMediaPrivate.h"

/*
 * Implements a pool for AJA audio sample objects. 
 */

class FAjaMediaBinarySamplePool : public TMediaObjectPool<FMediaIOCoreBinarySampleBase> { };
