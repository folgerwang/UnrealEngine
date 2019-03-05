// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Commands.h"


class DuplexPipe
{
public:
	DuplexPipe(void);
	~DuplexPipe(void);

	void Close(void);
	bool IsValid(void) const volatile;

	// send command, synchronous
	template <typename T>
	void SendCommandAndWaitForAck(const T& command) const
	{
		// send the command ID and the command, and wait for ACK to come in
		const uint32_t commandId = T::ID;
		Send(&commandId, sizeof(commandId));
		Send(&command, sizeof(command));

		commands::Acknowledge ack = {};
		Read(&ack, sizeof(commands::Acknowledge));
	}

	// receive command ID
	bool ReceiveCommandId(uint32_t* id) const;

	template <typename T>
	bool ReceiveCommand(T* command) const
	{
		// receive command
		const bool success = Read(command, sizeof(T));
		return success;
	}

	// send acknowledge command
	void SendAck(void) const
	{
		commands::Acknowledge ack = {};
		Send(&ack, sizeof(commands::Acknowledge));
	}

protected:
	HANDLE m_pipe;

private:
	void Send(const void* buffer, size_t size) const;
	bool Read(void* buffer, size_t size) const;
};
