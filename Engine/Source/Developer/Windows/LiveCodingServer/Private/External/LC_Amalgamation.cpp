// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Amalgamation.h"
#include "LC_StringUtil.h"
#include "LC_FileUtil.h"
#include "LC_GrowingMemoryBlock.h"
#include "LC_Logging.h"


namespace
{
	static const char* const LPP_AMALGAMATION_PART= ".lpp_part.";
	static const wchar_t* const LPP_AMALGAMATION_PART_WIDE = L".lpp_part.";

	struct Database
	{
		static const uint32_t MAGIC_NUMBER = 0x4C505020;	// "LPP "
		static const uint32_t VERSION = 8u;

		struct Dependency
		{
			std::string filename;
			uint64_t timestamp;
		};
	};


	// helper function to generate the database path for an .obj file
	static std::wstring GenerateDatabasePath(const symbols::ObjPath& objPath)
	{
		std::wstring path = string::ToWideString(objPath.c_str());
		path = file::RemoveExtension(path);
		path += L".ldb";

		return path;
	}


	// helper function to generate a timestamp for a file
	static uint64_t GenerateTimestamp(const wchar_t* path)
	{
		const file::Attributes& attributes = file::GetAttributes(path);
		return file::GetLastModificationTime(attributes);
	}


	// helper function to generate a database dependency for a file
	static Database::Dependency GenerateDatabaseDependency(const ImmutableString& path)
	{
		return Database::Dependency { path.c_str(), GenerateTimestamp(string::ToWideString(path).c_str()) };
	}


	// helper function to generate a database dependency for a file, normalizing the path to the file
	static Database::Dependency GenerateNormalizedDatabaseDependency(const ImmutableString& path)
	{
		const std::wstring widePath = string::ToWideString(path);
		const std::wstring normalizedPath = file::NormalizePath(widePath.c_str());
		return Database::Dependency { string::ToUtf8String(normalizedPath).c_str(), GenerateTimestamp(normalizedPath.c_str()) };
	}
}


// serializes values and databases into an in-memory representation
namespace serializationToMemory
{
	bool Write(const void* buffer, size_t size, GrowingMemoryBlock* dbInMemory)
	{
		return dbInMemory->Insert(buffer, size);
	}

	template <typename T>
	bool Write(const T& value, GrowingMemoryBlock* dbInMemory)
	{
		return dbInMemory->Insert(&value, sizeof(T));
	}

	bool Write(const ImmutableString& str, GrowingMemoryBlock* dbInMemory)
	{
		// write length without null terminator and then the string
		const uint32_t lengthWithoutNull = str.GetLength();
		if (!Write(lengthWithoutNull, dbInMemory))
		{
			return false;
		}

		if (!Write(str.c_str(), lengthWithoutNull, dbInMemory))
		{
			return false;
		}

		return true;
	}

	bool Write(const std::string& str, GrowingMemoryBlock* dbInMemory)
	{
		// write length without null terminator and then the string
		const uint32_t lengthWithoutNull = static_cast<uint32_t>(str.length() * sizeof(char));
		if (!Write(lengthWithoutNull, dbInMemory))
		{
			return false;
		}

		if (!Write(str.c_str(), lengthWithoutNull, dbInMemory))
		{
			return false;
		}

		return true;
	}

	bool Write(const std::wstring& str, GrowingMemoryBlock* dbInMemory)
	{
		// write length without null terminator and then the string
		const uint32_t lengthWithoutNull = static_cast<uint32_t>(str.length() * sizeof(wchar_t));
		if (!Write(lengthWithoutNull, dbInMemory))
		{
			return false;
		}

		if (!Write(str.c_str(), lengthWithoutNull, dbInMemory))
		{
			return false;
		}

		return true;
	}

	bool Write(const Database::Dependency& dependency, GrowingMemoryBlock* dbInMemory)
	{
		if (!Write(dependency.filename, dbInMemory))
		{
			return false;
		}

		if (!Write(dependency.timestamp, dbInMemory))
		{
			return false;
		}

		return true;
	}
}


// serializes values and databases from disk
namespace serializationFromDisk
{
	struct ReadBuffer
	{
		const void* data;
		uint64_t leftToRead;
	};


	bool Read(void* memory, size_t size, ReadBuffer* buffer)
	{
		// is there enough data left to read?
		if (buffer->leftToRead < size)
		{
			return false;
		}

		memcpy(memory, buffer->data, size);
		buffer->data = static_cast<const char*>(buffer->data) + size;
		buffer->leftToRead -= size;

		return true;
	}

	template <typename T>
	bool Read(T& value, ReadBuffer* buffer)
	{
		return Read(&value, sizeof(T), buffer);
	}

	bool Read(std::string& str, ReadBuffer* buffer)
	{
		// read length first
		uint32_t length = 0u;
		if (!Read(length, buffer))
		{
			return false;
		}

		// read data
		str.resize(length + 1u, '\0');
		return Read(&str[0], length, buffer);
	}

	bool Read(Database::Dependency& dependency, ReadBuffer* buffer)
	{
		// read filename first
		if (!Read(dependency.filename, buffer))
		{
			return false;
		}

		// read timestamp
		return Read(dependency.timestamp, buffer);
	}


	bool Compare(const void* memory, size_t size, ReadBuffer* buffer)
	{
		// is there enough data left to read?
		if (buffer->leftToRead < size)
		{
			return false;
		}

		const bool identical = (memcmp(memory, buffer->data, size) == 0);
		if (!identical)
		{
			return false;
		}

		buffer->data = static_cast<const char*>(buffer->data) + size;
		buffer->leftToRead -= size;

		return true;
	}

	template <typename T>
	bool Compare(const T& value, ReadBuffer* buffer)
	{
		return Compare(&value, sizeof(T), buffer);
	}

	bool Compare(const ImmutableString& str, ReadBuffer* buffer)
	{
		// compare length first
		const uint32_t length = str.GetLength();
		if (!Compare(length, buffer))
		{
			return false;
		}

		// compare data
		return Compare(str.c_str(), length, buffer);
	}

	bool Compare(const std::string& str, ReadBuffer* buffer)
	{
		// compare length first
		const uint32_t length = static_cast<uint32_t>(str.length() * sizeof(char));
		if (!Compare(length, buffer))
		{
			return false;
		}

		// compare data
		return Compare(str.c_str(), length, buffer);
	}

	bool Compare(const std::wstring& str, ReadBuffer* buffer)
	{
		// compare length first
		const uint32_t length = static_cast<uint32_t>(str.length() * sizeof(wchar_t));
		if (!Compare(length, buffer))
		{
			return false;
		}

		// compare data
		return Compare(str.c_str(), length, buffer);
	}

	bool Compare(const Database::Dependency& dependency, ReadBuffer* buffer)
	{
		// compare filename first
		if (!Compare(dependency.filename, buffer))
		{
			return false;
		}

		// compare timestamp
		return Compare(dependency.timestamp, buffer);
	}
}


bool amalgamation::IsPartOfAmalgamation(const char* normalizedObjPath)
{
	return string::Contains(normalizedObjPath, LPP_AMALGAMATION_PART);
}


bool amalgamation::IsPartOfAmalgamation(const wchar_t* normalizedObjPath)
{
	return string::Contains(normalizedObjPath, LPP_AMALGAMATION_PART_WIDE);
}


std::wstring amalgamation::CreateObjPart(const std::wstring& normalizedFilename)
{
	std::wstring newObjPart(LPP_AMALGAMATION_PART_WIDE);
	newObjPart += file::RemoveExtension(file::GetFilename(normalizedFilename));
	newObjPart += L".obj";

	return newObjPart;
}


std::wstring amalgamation::CreateObjPath(const std::wstring& normalizedAmalgamatedObjPath, const std::wstring& objPart)
{
	std::wstring newObjPath(normalizedAmalgamatedObjPath);
	newObjPath = file::RemoveExtension(newObjPath);
	newObjPath += objPart;

	return newObjPath;
}


bool amalgamation::ReadAndCompareDatabase(const symbols::ObjPath& objPath, const std::wstring& compilerPath, const symbols::Compiland* compiland, const std::wstring& additionalCompilerOptions)
{
	// check if the .obj is there. if not, there is no need to check the database at all.
	{
		const file::Attributes& objAttributes = file::GetAttributes(string::ToWideString(objPath).c_str());
		if (!file::DoesExist(objAttributes))
		{
			return false;
		}
	}

	const std::wstring databasePath = GenerateDatabasePath(objPath);
	const file::Attributes& fileAttributes = file::GetAttributes(databasePath.c_str());
	if (!file::DoesExist(fileAttributes))
	{
		return false;
	}

	const uint64_t bytesLeftToRead = file::GetSize(fileAttributes);
	if (bytesLeftToRead == 0u)
	{
		LC_LOG_DEV("Failed to retrieve size of database file %S", databasePath.c_str());
		return false;
	}

	file::MemoryFile* memoryFile = file::Open(databasePath.c_str(), file::OpenMode::READ_ONLY);
	if (!memoryFile)
	{
		// database cannot be opened, treat as if a change was detected
		return false;
	}

	// start reading the database from disk, comparing against the compiland's database at the same time
	serializationFromDisk::ReadBuffer readBuffer { file::GetData(memoryFile), bytesLeftToRead };
	if (!serializationFromDisk::Compare(Database::MAGIC_NUMBER, &readBuffer))
	{
		LC_LOG_DEV("Wrong magic number in database file %S", databasePath.c_str());

		file::Close(memoryFile);
		return false;
	}

	if (!serializationFromDisk::Compare(Database::VERSION, &readBuffer))
	{
		LC_LOG_DEV("Version has changed in database file %S", databasePath.c_str());

		file::Close(memoryFile);
		return false;
	}

	if (!serializationFromDisk::Compare(compilerPath, &readBuffer))
	{
		LC_LOG_DEV("Compiler path has changed in database file %S", databasePath.c_str());

		file::Close(memoryFile);
		return false;
	}

	if (!serializationFromDisk::Compare(GenerateTimestamp(compilerPath.c_str()), &readBuffer))
	{
		LC_LOG_DEV("Compiler timestamp has changed in database file %S", databasePath.c_str());

		file::Close(memoryFile);
		return false;
	}

	if (!serializationFromDisk::Compare(compiland->commandLine, &readBuffer))
	{
		LC_LOG_DEV("Compiland compiler options have changed in database file %S", databasePath.c_str());

		file::Close(memoryFile);
		return false;
	}

	if (!serializationFromDisk::Compare(additionalCompilerOptions, &readBuffer))
	{
		LC_LOG_DEV("Additional compiler options have changed in database file %S", databasePath.c_str());

		file::Close(memoryFile);
		return false;
	}

	if (!serializationFromDisk::Compare(GenerateNormalizedDatabaseDependency(compiland->srcPath), &readBuffer))
	{
		LC_LOG_DEV("Source file has changed in database file %S", databasePath.c_str());

		file::Close(memoryFile);
		return false;
	}

	// dependencies need to be treated differently, because the current list of files might differ from the one
	// stored in the database. this is not a problem however, because the database is always kept up-to-date as
	// soon as a file was compiled.
	// we need to read all files from the database and check their timestamp against the timestamp of the file
	// on disk.
	{
		uint32_t count = 0u;
		if (!serializationFromDisk::Read(count, &readBuffer))
		{
			LC_LOG_DEV("Failed to read dependency count in database file %S", databasePath.c_str());

			file::Close(memoryFile);
			return false;
		}

		for (uint32_t i = 0u; i < count; ++i)
		{
			Database::Dependency dependency = {};
			if (!serializationFromDisk::Read(dependency, &readBuffer))
			{
				LC_LOG_DEV("Failed to read dependency in database file %S", databasePath.c_str());

				file::Close(memoryFile);
				return false;
			}

			// check dependency timestamp
			const file::Attributes& attributes = file::GetAttributes(string::ToWideString(dependency.filename).c_str());
			if (file::GetLastModificationTime(attributes) != dependency.timestamp)
			{
				LC_LOG_DEV("Dependency has changed in database file %S", databasePath.c_str());

				file::Close(memoryFile);
				return false;
			}
		}
	}

	// no change detected
	file::Close(memoryFile);
	return true;
}


void amalgamation::WriteDatabase(const symbols::ObjPath& objPath, const std::wstring& compilerPath, const symbols::Compiland* compiland, const std::wstring& additionalCompilerOptions)
{
	// first serialize the database to memory and then write it to disk in one go.
	// note that we write the database to a temporary file first, and then move it to its final destination.
	// because moving is atomic, this ensures that databases are either fully written or not at all.
	GrowingMemoryBlock dbInMemory(1u * 1024u * 1024u);
	if (!serializationToMemory::Write(Database::MAGIC_NUMBER, &dbInMemory))
	{
		LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
		return;
	}

	if (!serializationToMemory::Write(Database::VERSION, &dbInMemory))
	{
		LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
		return;
	}

	if (!serializationToMemory::Write(compilerPath, &dbInMemory))
	{
		LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
		return;
	}

	if (!serializationToMemory::Write(GenerateTimestamp(compilerPath.c_str()), &dbInMemory))
	{
		LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
		return;
	}

	if (!serializationToMemory::Write(compiland->commandLine, &dbInMemory))
	{
		LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
		return;
	}

	if (!serializationToMemory::Write(additionalCompilerOptions, &dbInMemory))
	{
		LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
		return;
	}

	// the source file itself is treated as a dependency
	if (!serializationToMemory::Write(GenerateNormalizedDatabaseDependency(compiland->srcPath), &dbInMemory))
	{
		LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
		return;
	}

	// write all file dependencies
	{
		const bool hasDependencies = (compiland->sourceFiles != nullptr);
		const uint32_t count = hasDependencies
			? static_cast<uint32_t>(compiland->sourceFiles->files.size())
			: 0u;

		if (!serializationToMemory::Write(count, &dbInMemory))
		{
			LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
			return;
		}

		if (hasDependencies)
		{
			const types::vector<ImmutableString>& sourceFiles = compiland->sourceFiles->files;
			for (uint32_t i = 0u; i < count; ++i)
			{
				const ImmutableString& sourcePath = sourceFiles[i];
				if (!serializationToMemory::Write(GenerateDatabaseDependency(sourcePath), &dbInMemory))
				{
					LC_LOG_DEV("Failed to serialize database for compiland %s", objPath.c_str());
					return;
				}
			}
		}
	}

	const std::wstring databasePath = GenerateDatabasePath(objPath);
	std::wstring tempDatabasePath = databasePath;
	tempDatabasePath += L".tmp";

	if (!file::CreateFileWithData(tempDatabasePath.c_str(), dbInMemory.GetData(), dbInMemory.GetSize()))
	{
		LC_LOG_DEV("Failed to write database for compiland %s", objPath.c_str());
		return;
	}

	file::Move(tempDatabasePath.c_str(), databasePath.c_str());
}


void amalgamation::DeleteDatabase(const symbols::ObjPath& objPath)
{
	const std::wstring& databasePath = GenerateDatabasePath(objPath);
	file::DeleteIfExists(databasePath.c_str());
}
