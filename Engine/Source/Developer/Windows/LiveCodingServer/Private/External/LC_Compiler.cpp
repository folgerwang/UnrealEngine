// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Compiler.h"
#include "LC_StringUtil.h"
#include "LC_FileUtil.h"
#include "LC_Process.h"
#include "LC_Environment.h"
#include "LC_CriticalSection.h"
#include "LC_Logging.h"


namespace
{
	// simple key-value cache for storing environment blocks for certain compilers
	class CompilerEnvironmentCache
	{
	public:
		CompilerEnvironmentCache(void)
			: m_cache(16u)
		{
		}

		~CompilerEnvironmentCache(void)
		{
			for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
			{
				environment::Block* block = it->second;
				environment::DestroyBlock(block);
			}
		}

		void Insert(const wchar_t* key, environment::Block* value)
		{
			m_cache[key] = value;
		}

		const environment::Block* Fetch(const wchar_t* key)
		{
			const auto it = m_cache.find(key);
			if (it != m_cache.end())
			{
				return it->second;
			}

			return nullptr;
		}

	private:
		types::unordered_map<std::wstring, environment::Block*> m_cache;
	};

	static CompilerEnvironmentCache g_compilerEnvironmentCache;

	static CriticalSection g_compilerCacheCS;


	static std::vector<const wchar_t*> DetermineRelativePathToVcvarsFile(const wchar_t* absolutePathToCompilerExe)
	{
		// COMPILER SPECIFIC: Visual Studio. other compilers and linkers don't need vcvars*.bat to be invoked.
		std::vector<const wchar_t*> paths;
		paths.reserve(5u);

		// find out which vcvars*.bat file we have to call, based on the path to the compiler used.
		// make sure to carry out the comparison with lowercase strings only.
		wchar_t lowercaseAbsolutePathToCompilerExe[MAX_PATH] = {};
		wcscpy_s(lowercaseAbsolutePathToCompilerExe, absolutePathToCompilerExe);
		_wcslwr_s(lowercaseAbsolutePathToCompilerExe);

		// Visual Studio 2017 and above
		if (string::Contains(lowercaseAbsolutePathToCompilerExe, L"bin\\hostx86\\x86"))
		{
			paths.push_back(L"\\..\\..\\..\\..\\..\\..\\Auxiliary\\Build\\vcvars32.bat");
		}
		else if (string::Contains(lowercaseAbsolutePathToCompilerExe, L"bin\\hostx86\\x64"))
		{
			paths.push_back(L"\\..\\..\\..\\..\\..\\..\\Auxiliary\\Build\\vcvarsx86_amd64.bat");
		}
		else if (string::Contains(lowercaseAbsolutePathToCompilerExe, L"bin\\hostx64\\x64"))
		{
			paths.push_back(L"\\..\\..\\..\\..\\..\\..\\Auxiliary\\Build\\vcvars64.bat");
		}
		else if (string::Contains(lowercaseAbsolutePathToCompilerExe, L"bin\\hostx64\\x86"))
		{
			paths.push_back(L"\\..\\..\\..\\..\\..\\..\\Auxiliary\\Build\\vcvarsamd64_x86.bat");
		}

		// Visual Studio 2015 and below
		else if (string::Contains(lowercaseAbsolutePathToCompilerExe, L"vc\\bin\\amd64_x86"))
		{
			paths.push_back(L"\\vcvarsamd64_x86.bat");
		}
		else if (string::Contains(lowercaseAbsolutePathToCompilerExe, L"vc\\bin\\x86_amd64"))
		{
			paths.push_back(L"\\vcvarsx86_amd64.bat");
		}
		else if (string::Contains(lowercaseAbsolutePathToCompilerExe, L"vc\\bin\\amd64"))
		{
			paths.push_back(L"\\vcvars64.bat");
		}
		else if (string::Contains(lowercaseAbsolutePathToCompilerExe, L"vc\\bin"))
		{
			paths.push_back(L"\\vcvars32.bat");
		}

		// fallback for toolchains which are not installed at the default location.
		// in this case, we assume the vcvars*.bat file is in the same directory and try all different flavours later.
		else
		{
			paths.push_back(L"\\vcvars64.bat");
			paths.push_back(L"\\vcvarsamd64_x86.bat");
			paths.push_back(L"\\vcvarsx86_amd64.bat");
			paths.push_back(L"\\vcvars32.bat");
		}

		return paths;
	}
}


const environment::Block* compiler::CreateEnvironmentCacheEntry(const wchar_t* absolutePathToCompilerExe)
{
	LC_LOG_DEV("Creating environment cache entry for %S", absolutePathToCompilerExe);

	// COMPILER SPECIFIC: Visual Studio. other compilers and linkers don't need vcvars*.bat to be invoked.
	{
		// bail out early in case this is the LLVM/clang/lld toolchain
		const std::wstring& toolFilename = file::GetFilename(absolutePathToCompilerExe);
		if (string::Matches(toolFilename.c_str(), L"lld.exe"))
		{
			return nullptr;
		}
		else if (string::Matches(toolFilename.c_str(), L"lld-link.exe"))
		{
			return nullptr;
		}
		else if (string::Matches(toolFilename.c_str(), L"ld.lld.exe"))
		{
			return nullptr;
		}
		else if (string::Matches(toolFilename.c_str(), L"ld64.lld.exe"))
		{
			return nullptr;
		}
	}

	const std::wstring& path = file::GetDirectory(absolutePathToCompilerExe);

	// get all possible paths to vcvars*.bat files and check which one is available
	const std::vector<const wchar_t*>& relativePathsToVcvarsFile = DetermineRelativePathToVcvarsFile(absolutePathToCompilerExe);
	for (size_t i = 0u; i < relativePathsToVcvarsFile.size(); ++i)
	{
		std::wstring pathToVcvars(path);
		pathToVcvars += relativePathsToVcvarsFile[i];

		LC_LOG_DEV("Trying vcvars*.bat at %S", pathToVcvars.c_str());

		const file::Attributes& attributes = file::GetAttributes(pathToVcvars.c_str());
		if (file::DoesExist(attributes))
		{
			// this is the correct vcvars*.bat

			// quote path to batch file
			std::wstring vcvarsBat(L"\"");
			vcvarsBat += path;
			vcvarsBat += relativePathsToVcvarsFile[i];
			vcvarsBat += L"\"";

			// now that we have the path to the vcvars*.bat to call, construct a command that first invokes
			// the batch file and then outputs the environment variables to a file.
			const std::wstring& tempFile = file::CreateTempFile();
			const std::wstring& cmdPath = environment::GetVariable(L"COMSPEC");

			// tell cmd.exe to execute commands, and quote all filenames involved
			std::wstring commandLine(L"/c \"");
			commandLine += vcvarsBat;
			commandLine += L" && set > \"";
			commandLine += tempFile;
			commandLine += L"\"\"";

			process::Context* vcvarsProcess = process::Spawn(cmdPath.c_str(), nullptr, commandLine.c_str(), nullptr, process::SpawnFlags::NONE);
			const unsigned int exitCode = process::Wait(vcvarsProcess);
			process::Destroy(vcvarsProcess);

			if (exitCode == 0u)
			{
				// the temporary file now holds the full environment block after vcvars*.bat has executed.
				// load it and insert it into the cache.
				environment::Block* block = environment::CreateBlockFromFile(tempFile.c_str());
				{
					CriticalSection::ScopedLock lock(&g_compilerCacheCS);
					g_compilerEnvironmentCache.Insert(absolutePathToCompilerExe, block);

					if (block)
					{
						environment::DumpBlockData(vcvarsBat.c_str(), block);
					}
				}

				return block;
			}

			LC_WARNING_USER("vcvars*.bat could not be invoked at %S", vcvarsBat.c_str());
			return nullptr;
		}
		else
		{
			LC_LOG_DEV("%S does not exist", pathToVcvars.c_str());
		}
	}

	LC_WARNING_USER("Cannot determine vcvars*.bat environment for compiler/linker %S", absolutePathToCompilerExe);
	return nullptr;
}


const environment::Block* compiler::GetEnvironmentFromCache(const wchar_t* absolutePathToCompilerExe)
{
	CriticalSection::ScopedLock lock(&g_compilerCacheCS);
	return g_compilerEnvironmentCache.Fetch(absolutePathToCompilerExe);
}


const environment::Block* compiler::UpdateEnvironmentCache(const wchar_t* absolutePathToCompilerExe)
{
	const environment::Block* block = GetEnvironmentFromCache(absolutePathToCompilerExe);
	if (block)
	{
		return block;
	}

	return CreateEnvironmentCacheEntry(absolutePathToCompilerExe);
}
