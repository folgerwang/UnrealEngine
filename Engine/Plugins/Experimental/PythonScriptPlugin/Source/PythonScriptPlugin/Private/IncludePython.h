// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Stats/Stats.h"

#if WITH_PYTHON

THIRD_PARTY_INCLUDES_START
#include "Python.h"
#include "structmember.h"
THIRD_PARTY_INCLUDES_END

DECLARE_STATS_GROUP(TEXT("Python"), STATGROUP_Python, STATCAT_Advanced);

#endif	// WITH_PYTHON
