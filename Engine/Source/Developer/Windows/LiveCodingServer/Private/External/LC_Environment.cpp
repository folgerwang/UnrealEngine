// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Environment.h"
#include "LC_FileUtil.h"
#include "LC_MemoryFile.h"
#include "LC_StringUtil.h"
#include "LC_AppSettings.h"
#include "LC_Logging.h"


namespace environment
{
	struct Block
	{
		size_t size;
		char* data;
	};


	Block* CreateBlockFromFile(const wchar_t* path)
	{
		const file::Attributes& attributes = file::GetAttributes(path);
		const uint64_t fileSize = file::GetSize(attributes);

		file::MemoryFile* file = file::Open(path, file::OpenMode::READ_ONLY);
		if (!file)
		{
			return nullptr;
		}

		types::vector<char> blockData;
		blockData.reserve(static_cast<size_t>(fileSize));

		// walk through the file and gather individual environment variables line by line
		const char* memory = static_cast<const char*>(file::GetData(file));
		const char* memoryEnd = memory + fileSize;
		for (/*nothing*/; memory < memoryEnd; /*nothing*/)
		{
			const char* start = memory;

			// search for carriage return
			while (memory[0] != '\r')
			{
				++memory;
			}
			const char* end = memory;

			std::string line(start, end);

			// values can contain '=', '\r' and '\n', so parsing key=value pairs cannot be done
			// unambiguously given a block of text. to avoid passing an invalid environment block
			// to CreateProcess() when invoking the compiler/linker, we simply remove all lines
			// that do not contain an '=' - these are the ones that would lead to error 87
			// (invalid parameter) when calling CreateProcess().
			if (string::Contains(line.c_str(), "="))
			{
				// valid key=value pair.
				// insert line into block data, and add null terminator to signal the end of this variable.
				blockData.insert(blockData.end(), line.begin(), line.end());
				blockData.push_back('\0');
			}

			// skip carriage return and new line
			while ((memory[0] == '\r') || (memory[0] == '\n'))
			{
				++memory;
			}
		}

		// add null terminator that signals the end of the whole block
		blockData.push_back('\0');

		file::Close(file);

		Block* block = new Block;
		block->size = blockData.size();
		block->data = new char[block->size];
		memcpy(block->data, blockData.data(), block->size);

		return block;
	}


	void DestroyBlock(Block*& block)
	{
		if (block)
		{
			delete[] block->data;
		}

		delete block;
		block = nullptr;
	}


	void DumpBlockData(const wchar_t* name, const Block* block)
	{
		// don't do any parsing when dev output is turned off
		if (!appSettings::g_enableDevLog->GetValue())
		{
			return;
		}

		LC_LOG_DEV("Environment block %S:", name);
		LC_LOG_INDENT_DEV;

		const char* start = block->data;
		const char* end = start;
		while (end < block->data + block->size)
		{
			// search for null terminator
			while (*end != '\0')
			{
				++end;
			}

			LC_LOG_DEV("%s", start);

			++end;
			start = end;
		}
	}


	const void* GetBlockData(const Block* block)
	{
		return block->data;
	}


	const size_t GetBlockSize(const Block* block)
	{
		return block->size;
	}


	std::wstring GetVariable(const wchar_t* variable)
	{
		std::wstring value;
		value.resize(MAX_PATH);

		size_t bytes = 0u;
		_wgetenv_s(&bytes, &value[0], value.size(), variable);

		return value;
	}
}
