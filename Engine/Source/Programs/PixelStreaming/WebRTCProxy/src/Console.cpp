// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Console.h"
#include "StringUtils.h"

FConsole::FConsole()
{
}

FConsole::~FConsole()
{
	if (bOwnsConsole && hConsoleHandle != INVALID_HANDLE_VALUE)
	{
		FreeConsole();
	}
}

void FConsole::Init(short Width, short Height, short BufferWidth, short BufferHeight)
{
	check(hConsoleHandle == INVALID_HANDLE_VALUE);
	if (hConsoleHandle != INVALID_HANDLE_VALUE)
	{
		return;
	}

	CONSOLE_SCREEN_BUFFER_INFO ConInfo;

	// allocate a console for this app.
	// NOTE: It fails if there is a console already
	bOwnsConsole = AllocConsole() == Windows::TRUE ? true : false;

	hConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	// set the screen buffer to be big enough to let us scroll text
	GetConsoleScreenBufferInfo(hConsoleHandle, &ConInfo);
	// Set the screen buffer size
	ConInfo.dwSize.Y = BufferHeight;
	ConInfo.dwSize.X = BufferWidth;
	SetConsoleScreenBufferSize(hConsoleHandle, ConInfo.dwSize);
	// Set the real window size (need to be smaller than the buffer
	ConInfo.srWindow.Bottom = Height - 1;
	ConInfo.srWindow.Right = Width - 1;
	SetConsoleWindowInfo(hConsoleHandle, Windows::TRUE, &ConInfo.srWindow);
	Center();
	EnableUTF8Support();
	SetTextColour(EColour::White);
}

void FConsole::Center()
{
	// Reposition windows
	RECT ScreenRect;
	GetWindowRect(GetDesktopWindow(), &ScreenRect);
	int ScreenWidth = ScreenRect.right - ScreenRect.left + 1;
	int ScreenHeight = ScreenRect.bottom - ScreenRect.top + 1;

	HWND hConsoleWnd = GetConsoleWindow();

	RECT ConsoleRect;
	GetWindowRect(hConsoleWnd, &ConsoleRect);
	int ConsoleWidth = ConsoleRect.right - ConsoleRect.left + 1;
	int ConsoleHeight = ConsoleRect.bottom - ConsoleRect.top + 1;

	SetWindowPos(
	    hConsoleWnd,
	    0,
	    (ScreenWidth - ConsoleWidth) / 2,
	    (ScreenHeight - ConsoleHeight) / 2,
	    0,
	    0,
	    SWP_NOSIZE | SWP_NOZORDER);

	SwitchToThisWindow(hConsoleWnd, Windows::TRUE);
}

void FConsole::EnableUTF8Support()
{
	BOOL ret = SetConsoleOutputCP(
	    65001);  // utf codepage, as in http://msdn.microsoft.com/en-us/library/dd317756(v=vs.85).aspx
	ret = SetConsoleCP(65001);
}

void FConsole::Print(const char* Str)
{
	DWORD Written;
	// WriteConsoleA, to force using Ansi/UTF8
	WriteConsoleA(hConsoleHandle, Str, static_cast<DWORD>(strlen(Str)), &Written, NULL);
}

void FConsole::Printf(_Printf_format_string_ const char* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	Print(FormatStringVA(Fmt, Args));
	va_end(Args);
}

void FConsole::Log(
    const char* File, int Line, const FLogCategoryBase* Category, ELogVerbosity Verbosity, const char* Msg)
{
	EColour Colour = CurrColour;
	if (Verbosity == ELogVerbosity::Log)
	{
		SetTextColour(EColour::White);
	}
	else if (Verbosity == ELogVerbosity::Warning)
	{
		SetTextColour(EColour::Yellow);
	}
	else
	{
		SetTextColour(EColour::Red);
	}
	Printf(Msg);
	SetTextColour(Colour);
}

void FConsole::SetTextColour(EColour Colour)
{
	SetConsoleTextAttribute(hConsoleHandle, (WORD)Colour);
	CurrColour = Colour;
}
