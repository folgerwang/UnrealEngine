// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_LiveProcess.h"
#include "LC_HeartBeat.h"
#include "LC_CodeCave.h"


LiveProcess::LiveProcess(process::Handle processHandle, unsigned int processId, unsigned int commandThreadId, const DuplexPipe* pipe)
	: m_processHandle(processHandle)
	, m_processId(processId)
	, m_commandThreadId(commandThreadId)
	, m_pipe(pipe)
	, m_imagesTriedToLoad()
	, m_heartBeatDelta(0ull)
	, m_codeCave(nullptr)
{
	m_imagesTriedToLoad.reserve(256u);
}


void LiveProcess::ReadHeartBeatDelta(const wchar_t* const processGroupName)
{
	HeartBeat heartBeat(processGroupName, m_processId);
	m_heartBeatDelta = heartBeat.ReadBeatDelta();
}


bool LiveProcess::MadeProgress(void) const
{
	if (m_heartBeatDelta >= 100ull * 10000ull)
	{
		// the client process hasn't stored a new heart beat in more than 100ms.
		// as long as it is running, it stores a new heart beat every 10ms, so we conclude that it
		// didn't make progress, e.g. because it is being held in the debugger.
		return false;
	}

	return true;
}


void LiveProcess::InstallCodeCave(void)
{
	m_codeCave = new CodeCave(m_processHandle, m_processId, m_commandThreadId);
	m_codeCave->Install();
}


void LiveProcess::UninstallCodeCave(void)
{
	m_codeCave->Uninstall();
	delete m_codeCave;
	m_codeCave = nullptr;
}


void LiveProcess::AddLoadedImage(const executable::Header& imageHeader)
{
	m_imagesTriedToLoad.insert(imageHeader);
}


void LiveProcess::RemoveLoadedImage(const executable::Header& imageHeader)
{
	m_imagesTriedToLoad.erase(imageHeader);
}


bool LiveProcess::TriedToLoadImage(const executable::Header& imageHeader) const
{
	return (m_imagesTriedToLoad.find(imageHeader) != m_imagesTriedToLoad.end());
}

// BEGIN EPIC MOD - Allow lazy-loading modules
void LiveProcess::AddLazyLoadedModule(const std::wstring moduleName, Windows::HMODULE moduleBase)
{
	LazyLoadedModule module;
	module.m_moduleBase = moduleBase;
	module.m_loaded = false;
	m_lazyLoadedModules.insert(std::make_pair(moduleName, module));
}

void LiveProcess::SetLazyLoadedModuleAsLoaded(const std::wstring moduleName)
{
	std::unordered_map<std::wstring, LazyLoadedModule>::iterator it = m_lazyLoadedModules.find(moduleName);
	if (it != m_lazyLoadedModules.end())
	{
		it->second.m_loaded = true;
	}
}

bool LiveProcess::IsPendingLazyLoadedModule(const std::wstring& moduleName) const
{
	std::unordered_map<std::wstring, LazyLoadedModule>::const_iterator iter = m_lazyLoadedModules.find(moduleName);
	return iter != m_lazyLoadedModules.end() && !iter->second.m_loaded;
}

Windows::HMODULE LiveProcess::GetLazyLoadedModuleBase(const std::wstring& moduleName) const
{
	std::unordered_map<std::wstring, LazyLoadedModule>::const_iterator iter = m_lazyLoadedModules.find(moduleName);
	if (iter == m_lazyLoadedModules.end())
	{
		return nullptr;
	}
	else
	{
		return iter->second.m_moduleBase;
	}
}
// END EPIC MOD
