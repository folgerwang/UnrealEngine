// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"

#define IOS_MAX_PATH 1024
#define CMD_LINE_MAX 16384

extern FString GSavedCommandLine;

class FIOSCommandLineHelper
{
	public:
		/**
		 * Merge the given commandline with GSavedCommandLinePortion, which may start with ?
		 * options that need to come after first token
		 */

		static void MergeCommandlineWithSaved(TCHAR CommandLine[16384])
		{
			// the saved commandline may be in the format ?opt?opt -opt -opt, so we need to insert it 
			// after the first token on the commandline unless the first token starts with a -, in which 
			// case use it at the start of the command line
			if (CommandLine[0] == '-' || CommandLine[0] == 0)
			{
				// handle the easy - case, just use the saved command line part as the start, in case it
				// started with a ?
				FString CombinedCommandLine = GSavedCommandLine + CommandLine;
				FCString::Strcpy(CommandLine, CMD_LINE_MAX, *CombinedCommandLine);
			}
			else
			{
				// otherwise, we need to get the first token from the command line and insert after
				TCHAR* Space = FCString::Strchr(CommandLine, ' ');
				if (Space == NULL)
				{
					// if there is only one token (no spaces), just append us after it
					FCString::Strcat(CommandLine, CMD_LINE_MAX, *GSavedCommandLine);
				}
				else
				{
					// save off what's after the space (include the space for pasting later)
					FString AfterSpace(Space);
					// copy the save part where the space was
					FCString::Strcpy(Space, CMD_LINE_MAX - (Space - CommandLine), *GSavedCommandLine);
					// now put back the 2nd and so on token
					FCString::Strcat(CommandLine, CMD_LINE_MAX, *AfterSpace);
				}
			}
		}

		static bool TryReadCommandLineFile(const FString& CommandLineFilePath)
		{
			FILE* CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
			if (CommandLineFile)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Checking for command line in %s... FOUND!") LINE_TERMINATOR, *CommandLineFilePath);
				char CommandLine[CMD_LINE_MAX] = { 0 };
				char* DataExists = fgets(CommandLine, ARRAY_COUNT(CommandLine) - 1, CommandLineFile);
				if (DataExists)
				{
					// chop off trailing spaces
					while (*CommandLine && isspace(CommandLine[strlen(CommandLine) - 1]))
					{
						CommandLine[strlen(CommandLine) - 1] = 0;
					}

					FCommandLine::Append(UTF8_TO_TCHAR(CommandLine));
				}
				fclose(CommandLineFile);
				return true;
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Checking for command line in %s... NOT FOUND!") LINE_TERMINATOR, *CommandLineFilePath);
				return false;
			}
		}

		static void InitCommandArgs(FString AdditionalCommandArgs)
		{
			// initialize the commandline
			FCommandLine::Set(TEXT(""));

			// command line text file included in the bundle
			FString BundleCommandLineFilePath = FString([[NSBundle mainBundle] bundlePath]) + TEXT("/ue4commandline.txt");

#if !UE_BUILD_SHIPPING
			// command line text file pushed to the documents folder
			FString DocumentsCommandLineFilePath = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/ue4commandline.txt");
			if (!TryReadCommandLineFile(DocumentsCommandLineFilePath))
#endif
			{
				TryReadCommandLineFile(BundleCommandLineFilePath);
			}

			if (!AdditionalCommandArgs.IsEmpty() && !FChar::IsWhitespace(AdditionalCommandArgs[0]))
			{
				FCommandLine::Append(TEXT(" "));
			}
			FCommandLine::Append(*AdditionalCommandArgs);

			// now merge the GSavedCommandLine with the rest
			FCommandLine::Append(*GSavedCommandLine);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Combined iOS Commandline: %s") LINE_TERMINATOR, FCommandLine::Get());
		}
};

