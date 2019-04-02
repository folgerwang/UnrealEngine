// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Process.h"
#include "LC_Executable.h"
#include "LC_Types.h"


class DuplexPipe;
class CodeCave;

class LiveProcess
{
public:
	LiveProcess(process::Handle processHandle, unsigned int processId, unsigned int commandThreadId, const DuplexPipe* pipe);


	void ReadHeartBeatDelta(const wchar_t* const processGroupName);

	// returns whether this process made some progress, based on the heart beat received from the client
	bool MadeProgress(void) const;


	void InstallCodeCave(void);
	void UninstallCodeCave(void);


	void AddLoadedImage(const executable::Header& imageHeader);
	void RemoveLoadedImage(const executable::Header& imageHeader);
	bool TriedToLoadImage(const executable::Header& imageHeader) const;


	inline process::Handle GetProcessHandle(void) const
	{
		return m_processHandle;
	}

	inline unsigned int GetProcessId(void) const
	{
		return m_processId;
	}

	inline unsigned int GetCommandThreadId(void) const
	{
		return m_commandThreadId;
	}

	inline const DuplexPipe* GetPipe(void) const
	{
		return m_pipe;
	}

	// BEGIN EPIC MOD - Add build arguments
	inline void SetBuildArguments(const wchar_t* buildArguments)
	{
		m_buildArguments = buildArguments;
	}

	inline const wchar_t* GetBuildArguments()
	{
		return m_buildArguments.c_str();
	}
	// END EPIC MOD

	// BEGIN EPIC MOD - Allow lazy-loading modules
	void AddLazyLoadedModule(const std::wstring moduleName, Windows::HMODULE moduleBase);
	void SetLazyLoadedModuleAsLoaded(const std::wstring moduleName);
	bool IsPendingLazyLoadedModule(const std::wstring& moduleName) const;
	Windows::HMODULE GetLazyLoadedModuleBase(const std::wstring& moduleName) const;
	// END EPIC MOD

private:
	process::Handle m_processHandle;
	unsigned int m_processId;
	unsigned int m_commandThreadId;
	const DuplexPipe* m_pipe;

	// BEGIN EPIC MOD - Add build arguments
	std::wstring m_buildArguments;
	// END EPIC MOD

	// BEGIN EPIC MOD - Allow lazy-loading modules
	struct LazyLoadedModule
	{
		Windows::HMODULE m_moduleBase;
		bool m_loaded;
	};

	types::unordered_map<std::wstring, LazyLoadedModule> m_lazyLoadedModules;
	// END EPIC MOD

	// loaded modules are not identified by their full path, but by their executable image header.
	// we do this to ensure that the same executable loaded from a different path is not treated as
	// a different executable.
	types::unordered_set<executable::Header> m_imagesTriedToLoad;

	uint64_t m_heartBeatDelta;
	CodeCave* m_codeCave;
};
