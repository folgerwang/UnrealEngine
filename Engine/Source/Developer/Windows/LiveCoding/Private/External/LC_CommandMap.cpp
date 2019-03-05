// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_CommandMap.h"
#include "LC_DuplexPipe.h"


namespace
{
	template <class T>
	static bool DefaultAction(const DuplexPipe* pipe, void*)
	{
		// receive command and continue execution
		T command = {};
		const bool success = pipe->ReceiveCommand(&command);
		pipe->SendAck();

		if (!success)
		{
			return false;
		}

		return true;
	}

	template <typename T>
	static void RegisterDefaultAction(CommandMap::Action* actions)
	{
		actions[T::ID] = &DefaultAction<T>;
	}
}


CommandMap::CommandMap(void)
	: m_actions()
{
	// register default handlers that receive command data and continue execution
	RegisterDefaultAction<commands::Acknowledge>(m_actions);
	RegisterDefaultAction<commands::RegisterProcess>(m_actions);
	RegisterDefaultAction<commands::RegisterProcessFinished>(m_actions);

	RegisterDefaultAction<commands::EnableModuleBatchBegin>(m_actions);
	RegisterDefaultAction<commands::EnableModuleBatchEnd>(m_actions);
	RegisterDefaultAction<commands::DisableModuleBatchBegin>(m_actions);
	RegisterDefaultAction<commands::DisableModuleBatchEnd>(m_actions);

	RegisterDefaultAction<commands::EnableModule>(m_actions);
	RegisterDefaultAction<commands::EnableModuleFinished>(m_actions);
	RegisterDefaultAction<commands::DisableModule>(m_actions);
	RegisterDefaultAction<commands::DisableModuleFinished>(m_actions);

	RegisterDefaultAction<commands::EnableAllModules>(m_actions);
	RegisterDefaultAction<commands::EnableAllModulesFinished>(m_actions);
	RegisterDefaultAction<commands::DisableAllModules>(m_actions);
	RegisterDefaultAction<commands::DisableAllModulesFinished>(m_actions);

	RegisterDefaultAction<commands::EnterSyncPoint>(m_actions);
	RegisterDefaultAction<commands::LeaveSyncPoint>(m_actions);
	RegisterDefaultAction<commands::CallHooks>(m_actions);
	RegisterDefaultAction<commands::GetModule>(m_actions);
	RegisterDefaultAction<commands::GetModuleInfo>(m_actions);
	RegisterDefaultAction<commands::LoadPatch>(m_actions);
	RegisterDefaultAction<commands::LoadPatchInfo>(m_actions);
	RegisterDefaultAction<commands::UnloadPatch>(m_actions);
	RegisterDefaultAction<commands::CallEntryPoint>(m_actions);
	RegisterDefaultAction<commands::LogOutput>(m_actions);
	RegisterDefaultAction<commands::ReadyForCompilation>(m_actions);
	RegisterDefaultAction<commands::CompilationFinished>(m_actions);
	RegisterDefaultAction<commands::DisconnectClient>(m_actions);
	RegisterDefaultAction<commands::TriggerRecompile>(m_actions);	
	RegisterDefaultAction<commands::BuildPatch>(m_actions);
	RegisterDefaultAction<commands::HandleException>(m_actions);
	RegisterDefaultAction<commands::HandleExceptionFinished>(m_actions);

	// BEGIN EPIC MOD - Adding ShowConsole command
	RegisterDefaultAction<commands::ShowConsole>(m_actions);
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetVisible command
	RegisterDefaultAction<commands::SetVisible>(m_actions);
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetActive command
	RegisterDefaultAction<commands::SetActive>(m_actions);
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetBuildArguments command
	RegisterDefaultAction<commands::SetBuildArguments>(m_actions);
	// END EPIC MOD

	RegisterDefaultAction<commands::ApplySettingBool>(m_actions);
	RegisterDefaultAction<commands::ApplySettingInt>(m_actions);
	RegisterDefaultAction<commands::ApplySettingString>(m_actions);
}


CommandMap::~CommandMap(void)
{
}


bool CommandMap::HandleCommands(const DuplexPipe* pipe, void* context)
{
	for (;;)
	{
		// fetch incoming command id
		uint32_t commandId = 0u;
		{
			const bool success = pipe->ReceiveCommandId(&commandId);
			if (!success)
			{
				return false;
			}
		}

		// call handler for this command
		const Action action = m_actions[commandId];

		const bool continueExecution = action(pipe, context);
		if (!continueExecution)
		{
			return true;
		}
	}
}
