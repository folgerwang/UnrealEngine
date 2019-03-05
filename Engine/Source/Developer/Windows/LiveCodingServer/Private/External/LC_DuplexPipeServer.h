// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_DuplexPipe.h"


class DuplexPipeServer : public DuplexPipe
{
public:
	bool Create(const wchar_t* name);
	bool WaitForClient(void);
	void Disconnect(void);
};
