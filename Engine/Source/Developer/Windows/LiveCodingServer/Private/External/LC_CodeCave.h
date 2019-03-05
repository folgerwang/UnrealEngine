// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Process.h"
#include "LC_Types.h"

// nifty helper to let all threads of a process except one make "progress" by being held inside a jump-to-self cave.
class CodeCave
{
public:
	CodeCave(process::Handle processHandle, unsigned int processId, unsigned int commandThreadId);

	void Install(void);
	void Uninstall(void);

private:
	process::Handle m_processHandle;
	unsigned int m_processId;
	unsigned int m_commandThreadId;

	void* m_cave;

	struct PerThreadData
	{
		unsigned int id;
		const void* originalIp;
		int priority;
	};

	types::vector<PerThreadData> m_perThreadData;
};
