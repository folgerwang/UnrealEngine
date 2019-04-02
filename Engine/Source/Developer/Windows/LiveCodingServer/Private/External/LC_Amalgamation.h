// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Symbols.h"


namespace amalgamation
{
	bool IsPartOfAmalgamation(const char* normalizedObjPath);
	bool IsPartOfAmalgamation(const wchar_t* normalizedObjPath);

	// creates part of the .obj file for .cpp files that are part of an amalgamation, e.g.
	// turns C:\AbsoluteDir\SourceFile.cpp into .lpp_part.SourceFile.obj
	std::wstring CreateObjPart(const std::wstring& normalizedFilename);

	// creates a full .obj path for single .obj files that are part of an amalgamation, e.g.
	// turns C:\AbsoluteDir\Amalgamated.obj into C:\AbsoluteDir\Amalgamated.lpp_part.SourceFile.obj
	std::wstring CreateObjPath(const std::wstring& normalizedAmalgamatedObjPath, const std::wstring& objPart);


	// dependency database handling
	
	// reads a database from disk and compares it against the compiland's data.
	// returns true if the database was read correctly and no change was detected.
	bool ReadAndCompareDatabase(const symbols::ObjPath& objPath, const std::wstring& compilerPath, const symbols::Compiland* compiland, const std::wstring& additionalCompilerOptions);

	// writes a compiland's dependency database to disk
	void WriteDatabase(const symbols::ObjPath& objPath, const std::wstring& compilerPath, const symbols::Compiland* compiland, const std::wstring& additionalCompilerOptions);

	// deletes a compiland's dependency database from disk
	void DeleteDatabase(const symbols::ObjPath& objPath);
}
