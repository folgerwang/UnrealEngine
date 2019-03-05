// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Commands.h"


class DuplexPipe;

class CommandMap
{
	template <class T>
	static bool ReceiveAndCallAction(const DuplexPipe* pipe, void* context)
	{
		// receive command and call user action
		typename T::CommandType command = {};
		const bool success = pipe->ReceiveCommand(&command);
		if (!success)
		{
			pipe->SendAck();
			return false;
		}

		return T::Execute(&command, pipe, context);
	}

public:
	// an action returns whether execution should continue
	typedef bool (*Action)(const DuplexPipe*, void*);

	CommandMap(void);
	~CommandMap(void);

	template <class T>
	void RegisterAction(void)
	{
		m_actions[T::CommandType::ID] = &ReceiveAndCallAction<T>;
	}

	bool HandleCommands(const DuplexPipe* pipe, void* context);

private:
	Action m_actions[commands::COUNT];
};
