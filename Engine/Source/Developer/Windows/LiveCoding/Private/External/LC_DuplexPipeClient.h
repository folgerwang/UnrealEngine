// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_DuplexPipe.h"


class DuplexPipeClient : public DuplexPipe
{
public:
	bool Connect(const wchar_t* name);
};
