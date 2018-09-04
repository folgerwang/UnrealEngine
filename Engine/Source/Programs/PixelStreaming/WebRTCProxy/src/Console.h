// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"
#include "Logging.h"

/**
 * Creates or manages the existing Windows console
 * If the application already has a console, it will use it, if not, it will create
 * one
 */
class FConsole : public ILogOutput
{
public:

	/**
	 * Colours allowed
	 */
	enum class EColour
	{
		Black = 0x0,
		Blue = 0x01,
		Green = 0x02,
		Red = 0x04,
		Cyan = Blue + Green,
		Pink = Blue + Red,
		Yellow = Green + Red,
		White = Blue + Green + Red,
		BrightBlue = Blue + 0x08,
		BrightGreen = Green + 0x08,
		BrightRed = Red + 0x08,
		BrightCyan = Cyan + 0x08,
		BrightPink = Pink + 0x08,
		BrightYellow = Yellow + 0x08,
		BrightWhite = White + 0x08
	};

	FConsole();
	~FConsole();

	/**
	 * Initialize the console
	 * @param Width width of the console window
	 * @param Height height of the console window
	 * @param BufferWidth
	 *	width of the buffer itself (it can be larger than the window to allow scrolling
	 *	horizontally
	 * @param BufferHeight
	 *	Number of lines of the console buffer. Allows keeping some history, so you
	 *	can scroll vertically and take a look at older entries
	 */
	void Init(short Width, short Height, short BufferWidth, short BufferHeight);

	/**
	 * Prints a string using the current foreground/background colours
	 */
	void Print(const char* Str);
	/**
	 * Prints a string using the current foreground/background colours
	 */
	void Printf(_Printf_format_string_ const char* Fmt, ...);

	//
	// ILogOutput interface
	//
	void Log(const char* File, int Line, const FLogCategoryBase* Category, ELogVerbosity Verbosity, const char* Msg) override;

	/**
	* Allows the console to print UTF8 content
	* \note This only works if the console font is anything other than "Raster Font"
	*/
	void EnableUTF8Support();

	/**
	 * Centers the console window on the screen
	 */
	void Center();

private:
	void SetTextColour(EColour Colour);
	HANDLE hConsoleHandle = INVALID_HANDLE_VALUE;

	// Each process can have only 1 console.
	// This tells if the console was created by this class, and if so, it will
	// be deleted when the object is destroyed
	bool bOwnsConsole = false;

	EColour CurrColour = EColour::White;
};


