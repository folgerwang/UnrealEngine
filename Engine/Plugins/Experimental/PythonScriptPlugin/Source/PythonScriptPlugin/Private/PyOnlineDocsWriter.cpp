// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PyOnlineDocsWriter.h"
#include "PyUtil.h"
#include "HAL/FileManager.h"
#include "Logging/MessageLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_PYTHON

void FPyOnlineDocsModule::AccumulateFunction(const TCHAR* InFunctionName)
{
	FunctionNames.Add(InFunctionName);
}


void FPyOnlineDocsSection::AccumulateClass(const TCHAR* InTypeName)
{
	TypeNames.Add(InTypeName);
}


TSharedRef<FPyOnlineDocsModule> FPyOnlineDocsWriter::CreateModule(const FString& InModuleName)
{
	return Modules.Add_GetRef(MakeShared<FPyOnlineDocsModule>(InModuleName));
}

TSharedRef<FPyOnlineDocsSection> FPyOnlineDocsWriter::CreateSection(const FString& InSectionName)
{
	return Sections.Add_GetRef(MakeShared<FPyOnlineDocsSection>(InSectionName));
}

FString FPyOnlineDocsWriter::GetSphinxDocsPath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir()) / TEXT("Experimental/PythonScriptPlugin/SphinxDocs");
}

FString FPyOnlineDocsWriter::GetSourcePath() const
{
	return GetSphinxDocsPath() / TEXT("source");
}

FString FPyOnlineDocsWriter::GetTemplatePath() const
{
	return GetSourcePath() / TEXT("_templates");
}

void FPyOnlineDocsWriter::GenerateIndexFile()
{
	// Load up index.rst template
	const FString IndexTemplatePath = GetTemplatePath() / TEXT("index.rst");
	FString IndexTemplate;
	if (!FFileHelper::LoadFileToString(IndexTemplate, *IndexTemplatePath))
	{
		UE_LOG(LogPython, Warning, TEXT("Documentation generation template file '%s' was not found!"), *IndexTemplatePath);
		return;
	}

	// Sort the items in each module and keep the modules in the order they were created
	for (const TSharedRef<FPyOnlineDocsModule>& Module : Modules)
	{
		Module->FunctionNames.StableSort();
	}

	// Sort the items in each section and keep the sections in the order they were created
	for (const TSharedRef<FPyOnlineDocsSection>& Section : Sections)
	{
		Section->TypeNames.StableSort();
	}

	FString TableOfContents;

	// Accumulate all the modules into the table of contents
	if (Modules.Num() > 0)
	{
		TableOfContents +=
			LINE_TERMINATOR
			TEXT(".. toctree::") LINE_TERMINATOR
			TEXT("    :maxdepth: 1") LINE_TERMINATOR
			TEXT("    :caption: Modules") LINE_TERMINATOR LINE_TERMINATOR;

		for (const TSharedRef<FPyOnlineDocsModule>& Module : Modules)
		{
			TableOfContents += FString::Printf(TEXT("    module/%s") LINE_TERMINATOR, *Module->Name);
		}
	}

	// Accumulate all the classes for each section into the table of contents
	for (const TSharedRef<FPyOnlineDocsSection>& Section : Sections)
	{
		TableOfContents +=
			LINE_TERMINATOR
			TEXT(".. toctree::") LINE_TERMINATOR
			TEXT("    :maxdepth: 1") LINE_TERMINATOR
			TEXT("    :caption: ");
		TableOfContents += Section->Name;
		TableOfContents += LINE_TERMINATOR LINE_TERMINATOR;

		for (const FString& TypeName : Section->TypeNames)
		{
			TableOfContents += FString::Printf(TEXT("    class/%s") LINE_TERMINATOR, *TypeName);
		}
	}

	// Replace {{TableOfContents}} with actual list
	FString IndexText = IndexTemplate;
	IndexText.ReplaceInline(TEXT("{{TableOfContents}}"), *TableOfContents, ESearchCase::CaseSensitive);

	// Save out index file
	const FString IndexPath = GetSourcePath() / TEXT("index.rst");
	FFileHelper::SaveStringToFile(IndexText, *IndexPath, FFileHelper::EEncodingOptions::ForceUTF8);
}

void FPyOnlineDocsWriter::GenerateModuleFiles()
{
	// Erase any previous files
	const FString ModuleSourcePath = GetSourcePath() / TEXT("module");
	IFileManager::Get().DeleteDirectory(*ModuleSourcePath, false, true);

	// Load up Module.rst template
	const FString ModuleTemplatePath = GetTemplatePath() / TEXT("Module.rst");
	FString ModuleTemplate;
	if (!FFileHelper::LoadFileToString(ModuleTemplate, *ModuleTemplatePath))
	{
		UE_LOG(LogPython, Warning, TEXT("Documentation generation template file '%s' was not found!"), *ModuleTemplatePath);
		return;
	}

	// Create page for each module
	for (const TSharedRef<FPyOnlineDocsModule>& Module : Modules)
	{
		FString ModuleFunctions;
		for (const FString& FunctionName : Module->FunctionNames)
		{
			ModuleFunctions += FString::Printf(TEXT(".. autofunction:: unreal.%s") LINE_TERMINATOR, *FunctionName);
		}

		// Replace {{Module}} with actual module name
		// Replace {{ModuleFunctions}} with actual module functions list
		FString ModuleText = ModuleTemplate;
		ModuleText.ReplaceInline(TEXT("{{Module}}"), *Module->Name, ESearchCase::CaseSensitive);
		ModuleText.ReplaceInline(TEXT("{{ModuleFunctions}}"), *ModuleFunctions, ESearchCase::CaseSensitive);

		// Write out module file
		const FString ModulePath = ModuleSourcePath / Module->Name + TEXT(".rst");
		FFileHelper::SaveStringToFile(ModuleText, *ModulePath, FFileHelper::EEncodingOptions::ForceUTF8);
	}
}

void FPyOnlineDocsWriter::GenerateClassFiles()
{
	// Erase any previous files
	const FString ClassSourcePath = GetSourcePath() / TEXT("class");
	IFileManager::Get().DeleteDirectory(*ClassSourcePath, false, true);

	// Load up Class.rst template
	const FString ClassTemplatePath = GetTemplatePath() / TEXT("Class.rst");
	FString ClassTemplate;
	if (!FFileHelper::LoadFileToString(ClassTemplate, *ClassTemplatePath))
	{
		UE_LOG(LogPython, Warning, TEXT("Documentation generation template file '%s' was not found!"), *ClassTemplatePath);
		return;
	}

	// Create page for each class in each section
	for (const TSharedRef<FPyOnlineDocsSection>& Section : Sections)
	{
		for (const FString& TypeName : Section->TypeNames)
		{
			// Replace {{Class}} with actual class name
			FString ClassText = ClassTemplate;
			ClassText.ReplaceInline(TEXT("{{Class}}"), *TypeName, ESearchCase::CaseSensitive);

			// Write out class file
			const FString ClassPath = ClassSourcePath / TypeName + TEXT(".rst");
			FFileHelper::SaveStringToFile(ClassText, *ClassPath, FFileHelper::EEncodingOptions::ForceUTF8);
		}
	}
}

void FPyOnlineDocsWriter::GenerateFiles(const FString& InPythonStubPath)
{
	UE_LOG(LogPython, Log, TEXT("Generating Python API online docs used by Sphinx to generate static HTML..."));

	// Copy generated unreal module stub file to PythonScriptPlugin/SphinxDocs/modules
	const FString MoldulesPath = GetSphinxDocsPath() / TEXT("modules") / FPaths::GetCleanFilename(InPythonStubPath);
	IFileManager::Get().Copy(*MoldulesPath, *InPythonStubPath);

	GenerateIndexFile();
	GenerateModuleFiles();
	GenerateClassFiles();

	UE_LOG(LogPython, Log, TEXT(
		"... Finished generating Python API online docs.\n"
		"In the OS command prompt in PythonPlugin/SphinxDocs call `sphinx-build -b html source/ build/` to generate the HTML."
	));
}

#endif	// WITH_PYTHON
