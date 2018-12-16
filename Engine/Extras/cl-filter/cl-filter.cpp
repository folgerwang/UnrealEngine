// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/**
 * This program is designed to execute the Visual C++ compiler (cl.exe) and filter off any output lines from the /showIncludes directive
 * into a separate file for dependency checking. GCC/Clang have a specific option for this, whereas MSVC does not.
 */

#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>

int wmain(int ArgC, const wchar_t* ArgV[])
{
	// Get the full command line, and find the '--' marker
	wchar_t* CmdLine = ::GetCommandLineW();

	wchar_t *ChildCmdLine = wcsstr(CmdLine, L" -- ");
	if (ChildCmdLine == nullptr)
	{
		wprintf(L"ERROR: Unable to find child command line (%s)", CmdLine);
		return -1;
	}

	*ChildCmdLine = 0;
	ChildCmdLine += 4;

	int NumArgs = 0;
	wchar_t** Args = CommandLineToArgvW(CmdLine, &NumArgs);

	if (NumArgs != 2)
	{
		wprintf(L"ERROR: Syntax: cl-filter <dependencies-file> -- <child command line>\n");
		return -1;
	}

	const wchar_t* OutputFileName = Args[1];

	// Create the child process
	PROCESS_INFORMATION ProcessInfo;
	ZeroMemory(&ProcessInfo, sizeof(ProcessInfo));

	SECURITY_ATTRIBUTES SecurityAttributes;
	ZeroMemory(&SecurityAttributes, sizeof(SecurityAttributes));
	SecurityAttributes.bInheritHandle = TRUE;

	HANDLE StdOutReadHandle;
	HANDLE StdOutWriteHandle;
	if (CreatePipe(&StdOutReadHandle, &StdOutWriteHandle, &SecurityAttributes, 0) == 0)
	{
		wprintf(L"ERROR: Unable to create output pipe for child process\n");
		return -1;
	}

	HANDLE StdErrWriteHandle;
	if (DuplicateHandle(GetCurrentProcess(), StdOutWriteHandle, GetCurrentProcess(), &StdErrWriteHandle, 0, true, DUPLICATE_SAME_ACCESS) == 0)
	{
		wprintf(L"ERROR: Unable to create stderr pipe handle for child process\n");
		return -1;
	}

	// Create the new process as suspended, so we can modify it before it starts executing (and potentially preempting us)
	STARTUPINFO StartupInfo;
	ZeroMemory(&StartupInfo, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.hStdInput = NULL;
	StartupInfo.hStdOutput = StdOutWriteHandle;
	StartupInfo.hStdError = StdErrWriteHandle;
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;

	DWORD ProcessCreationFlags = GetPriorityClass(GetCurrentProcess());
	if (CreateProcessW(NULL, ChildCmdLine, NULL, NULL, TRUE, ProcessCreationFlags, NULL, NULL, &StartupInfo, &ProcessInfo) == 0)
	{
		wprintf(L"ERROR: Unable to create child process\n");
		return -1;
	}

	// Close the write ends of the handle. We don't want any other process to be able to inherit these.
	CloseHandle(StdOutWriteHandle);
	CloseHandle(StdErrWriteHandle);

	// Create a file to contain the dependency list
	FILE* OutputFile;
	if (_wfopen_s(&OutputFile, OutputFileName, L"wt") != 0)
	{
		wprintf(L"ERROR: Unable to open %s for output", OutputFileName);
		return -1;
	}

	// Get the default console codepage
	UINT CodePage = GetConsoleCP();
	
	// Pipe the output to stdout
	wchar_t Buffer[4096];
	size_t BufferSize = 0;

	for (;;)
	{
		char InputData[4096];

		// Read the next chunk of data from the output stream
		DWORD BytesRead;
		if (!ReadFile(StdOutReadHandle, InputData, sizeof(InputData), &BytesRead, NULL))
		{
			if (GetLastError() != ERROR_BROKEN_PIPE)
			{
				wprintf(L"ERROR: Unable to read data from child process (%08x)", GetLastError());
			}
			BytesRead = 0;
		}

		// Convert it to a string
		if (BytesRead > 0)
		{
			int NumCharacters = MultiByteToWideChar(CodePage, 0, InputData, (int)BytesRead, Buffer + BufferSize, (int)((sizeof(Buffer) / sizeof(Buffer[0])) - BufferSize));
			if (NumCharacters > 0)
			{
				BufferSize += (size_t)NumCharacters;
			}
		}

		// Parse individual lines from the output
		size_t LineStart = 0;
		while(LineStart < BufferSize)
		{
			// Find the end of this line
			size_t LineEnd = LineStart;
			while (LineEnd < BufferSize && Buffer[LineEnd] != '\r' && Buffer[LineEnd] != '\n')
			{
				LineEnd++;
			}

			// If we didn't reach a line terminator, and we can still read more data, clear up some space and try again
			if (LineEnd == BufferSize && LineStart != 0)
			{
				break;
			}

			// Skip past the EOL markers
			if (LineEnd < BufferSize && Buffer[LineEnd] == '\r')
			{
				LineEnd++;
			}
			if (LineEnd < BufferSize && Buffer[LineEnd] == '\n')
			{
				LineEnd++;
			}

			// Filter out any lines that are 
			const wchar_t Prefix[] = L"Note: including file: ";
			if (wcsncmp(Buffer + LineStart, Prefix, sizeof(Prefix) / sizeof(Prefix[0]) - 1) == 0)
			{
				size_t FileNameStart = LineStart + sizeof(Prefix) / sizeof(Prefix[0]) - 1;
				while (FileNameStart < LineEnd && iswspace(Buffer[FileNameStart]))
				{
					FileNameStart++;
				}

				size_t FileNameEnd = LineEnd;
				while (FileNameEnd > FileNameStart && iswspace(Buffer[FileNameEnd - 1]))
				{
					FileNameEnd--;
				}

				Buffer[FileNameEnd] = 0;
				fwprintf(OutputFile, L"%s\n", Buffer + FileNameStart);
			}
			else
			{
				char RawOutput[4096];
				int RawOutputSize = WideCharToMultiByte(CodePage, 0, Buffer + LineStart, (int)(LineEnd - LineStart), RawOutput, sizeof(RawOutput), nullptr, nullptr);

				DWORD BytesWritten;
				WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), RawOutput, RawOutputSize, &BytesWritten, NULL);
			}

			// Move to the next line
			LineStart = LineEnd;
		}

		// Check if we've finished
		if (BufferSize == 0 && BytesRead == 0)
		{
			break;
		}

		// Shuffle everything down
		memmove(Buffer, Buffer + LineStart, (BufferSize - LineStart) * sizeof(wchar_t));
		BufferSize -= LineStart;
	}

	WaitForSingleObject(ProcessInfo.hProcess, INFINITE);

	DWORD ExitCode;
	if (!GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode))
	{
		ExitCode = (DWORD)-1;
	}

	fclose(OutputFile);

	return ExitCode;
}
