// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidString.h: Android platform string classes
==============================================================================================*/

#pragma once

#include "Misc/Build.h"
#include "HAL/Platform.h"

/**
 * Android string implementation
 **/

#if PLATFORM_TCHAR_IS_CHAR16

// By default we now use 2-byte strings on Android.

#include "GenericPlatform/GenericWidePlatformString.h"

typedef FGenericWidePlatformString FAndroidPlatformString;

#else

// NOTE: This legacy 4-byte implementation will probably be removed in the future.

// @todo android: probably rewrite/simplify most of this.  currently it converts all wide chars
// to ANSI.  The Android NDK appears to have wchar_t support, but many of the functions appear
// to just be stubs to the non-wide versions.  This doesn't work for obvious reasons.

#include "Misc/Char.h"
#include "GenericPlatform/GenericPlatformMemory.h"
#include "GenericPlatform/GenericPlatformString.h"

struct FAndroidPlatformString : public FGenericPlatformString
{
	template <typename CharType>
	static inline CharType* Strupr(CharType* Dest, SIZE_T DestCount)
	{
		for (CharType* Char = Dest; *Char && DestCount > 0; Char++, DestCount--)
		{
			*Char = TChar<CharType>::ToUpper(*Char);
		}
		return Dest;
	}

	/** 
	 * Widechar implementation 
	 **/
	static FORCEINLINE WIDECHAR* Strcpy(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
	{
		if (!Dest || !Src)
			return NULL;

		int Pos = 0;
		while (Src[Pos])
		{
			Dest[Pos] = Src[Pos];
			++Pos;
		}

		Dest[Pos] = 0;

		return Dest;
	}

	static FORCEINLINE WIDECHAR* Strncpy(WIDECHAR* Dest, const WIDECHAR* Src, SIZE_T MaxLen)
	{
		if (!Dest || !Src)
			return NULL;

		int Pos = 0;
		while ((Pos < MaxLen) && Src[Pos])
		{
			Dest[Pos] = Src[Pos];
			++Pos;
		}

		while (Pos < MaxLen)
		{
			Dest[Pos] = 0;
			++Pos;
		}

		Dest[MaxLen-1]=0;
		return Dest;
	}

	static FORCEINLINE WIDECHAR* Strcat(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
	{
		if (!Dest || !Src)
			return NULL;

		int DestPos = Strlen(Dest);
		int SrcPos = 0;

		while (Src[SrcPos])
		{
			Dest[DestPos] = Src[SrcPos];
			++SrcPos;
			++DestPos;
		}

		Dest[DestPos] = 0;

		return Dest;
	}

	static FORCEINLINE int32 Strcmp( const WIDECHAR* String1, const WIDECHAR* String2 )
	{
		for (; *String1 || *String2; String1++, String2++)
		{
			if (*String1 != *String2)
			{
				return *String1 - *String2;
			}
		}
		return 0;
	}

	static FORCEINLINE int32 Strncmp( const WIDECHAR* String1, const WIDECHAR* String2, SIZE_T Count )
	{
		for (; (*String1 || *String2) && Count > 0; String1++, String2++, Count--)
		{
			if (*String1 != *String2)
			{
				return *String1 - *String2;
			}
		}
		return 0;
	}

	static FORCEINLINE int32 Strlen( const WIDECHAR* String )
	{
		if (!String)
			return 0;

		int Len = 0;
		while (String[Len])
		{
			++Len;
		}

		return Len;
	}

	static FORCEINLINE void CopyWideToAnsi(ANSICHAR* Dest, const WIDECHAR* Src)
	{
		if (!Src || !Dest)
			return;

		int Pos = 0;
		while (Src[Pos])
		{
			if (Src[Pos] <= 255)
			{
				Dest[Pos] = Src[Pos];
			}
			else
			{
				Dest[Pos] = '?';
			}

			++Pos;
		}

		Dest[Pos] = 0;
	}

	static FORCEINLINE void CopyAnsiToWide(WIDECHAR* Dest, const ANSICHAR* Src)
	{
		if (!Src || !Dest)
			return;

		int Pos = 0;
		while (Src[Pos])
		{
			Dest[Pos] = Src[Pos];
			++Pos;
		}

		Dest[Pos] = 0;
	}

	static FORCEINLINE const WIDECHAR* Strstr( const WIDECHAR* String, const WIDECHAR* Find)
	{
		WIDECHAR FindChar = *Find;

		// Always find an empty string
		if (!FindChar)
			return String;

		++Find;
		size_t MemCmpLen = wcslen(Find);
		WIDECHAR* FoundChar;
		for (;;)
		{
			FoundChar = wcschr(String, FindChar);
			if (!FoundChar)
			{
				//	No more instances of FindChar in String, Find does not exist in String
				break;
			}

			// Found FindChar, now set String to one character after FoundChar...
			// We can now compare characters after FoundChar to Find, which is pointing at original Find[1], to see if the rest of the characters match
			String = FoundChar + 1;
			if (!wmemcmp(String, Find, MemCmpLen))
			{
				// Strings match, return pointer to beginning of Find instance in String
				return FoundChar;
			}

			// No match, String is already ready to loop again to find the next instance of FindChar
		}

		return nullptr;
	}

	static FORCEINLINE const WIDECHAR* Strchr( const WIDECHAR* String, WIDECHAR C)
	{
		if (!String)
			return NULL;

		int Pos = 0;
		while (String[Pos])
		{
			if(String[Pos] == C)
			{
				return &(String[Pos]);
			}

			++Pos;
		}

		if (C == 0)
		{
			return &(String[Pos]);
		}

		return NULL;
	}

	static FORCEINLINE const WIDECHAR* Strrchr( const WIDECHAR* String, WIDECHAR C)
	{
		if (!String)
			return NULL;

		const WIDECHAR* Last = NULL;

		int Pos = 0;
		while (String[Pos])
		{
			if(String[Pos] == C)
			{
				Last = &(String[Pos]);
			}

			++Pos;
		}

		if (C == 0)
		{
			Last = &(String[Pos]);
		}

		return Last;
	}

	static FORCEINLINE int32 Atoi(const WIDECHAR* String)
	{
		int StringLen = Strlen(String);
		ANSICHAR* AnsiString = (ANSICHAR*)FMemory_Alloca(StringLen+1);
		CopyWideToAnsi(AnsiString, String);

		return atoi(AnsiString);
	}

	static FORCEINLINE int64 Atoi64(const WIDECHAR* String)
	{
		int StringLen = Strlen(String);
		ANSICHAR* AnsiString = (ANSICHAR*)FMemory_Alloca(StringLen+1);
		CopyWideToAnsi(AnsiString, String);

		return strtoll(AnsiString, NULL, 10);
	}

	static FORCEINLINE float Atof(const WIDECHAR* String)
	{
		int StringLen = Strlen(String);
		ANSICHAR* AnsiString = (ANSICHAR*)FMemory_Alloca(StringLen+1);
		CopyWideToAnsi(AnsiString, String);

		return (float)atof(AnsiString);
	}

	static FORCEINLINE double Atod(const WIDECHAR* String)
	{
		int StringLen = Strlen(String);
		ANSICHAR* AnsiString = (ANSICHAR*)FMemory_Alloca(StringLen+1);
		CopyWideToAnsi(AnsiString, String);

		return atof(AnsiString);
	}

	static FORCEINLINE int32 Strtoi( const WIDECHAR* Start, WIDECHAR** End, int32 Base ) 
	{
		int StartLen = Strlen(Start);
		ANSICHAR* AnsiStart = (ANSICHAR*)FMemory_Alloca(StartLen+1);
		CopyWideToAnsi(AnsiStart, Start);

		ANSICHAR* AnsiEnd = NULL;

		int32 Res = strtol(AnsiStart, &AnsiEnd, Base);

		if (End)
		{
			if (AnsiEnd == NULL)
			{
				*End = NULL;
			}
			else
			{
				*End = (WIDECHAR*)(Start + (AnsiEnd - AnsiStart));
			}
		}

		return Res;
	}

	static FORCEINLINE int64 Strtoi64( const WIDECHAR* Start, WIDECHAR** End, int32 Base ) 
	{
		int StartLen = Strlen(Start);
		ANSICHAR* AnsiStart = (ANSICHAR*)FMemory_Alloca(StartLen+1);
		CopyWideToAnsi(AnsiStart, Start);

		ANSICHAR* AnsiEnd = NULL;

		uint64 Res = strtoll(AnsiStart, &AnsiEnd, Base);

		if (End)
		{
			if (AnsiEnd == NULL)
			{
				*End = NULL;
			}
			else
			{
				*End = (WIDECHAR*)(Start + (AnsiEnd - AnsiStart));
			}
		}

		return Res;
	}

	static FORCEINLINE uint64 Strtoui64( const WIDECHAR* Start, WIDECHAR** End, int32 Base ) 
	{
		int StartLen = Strlen(Start);
		ANSICHAR* AnsiStart = (ANSICHAR*)FMemory_Alloca(StartLen+1);
		CopyWideToAnsi(AnsiStart, Start);

		ANSICHAR* AnsiEnd = NULL;

		uint64 Res = strtoull(AnsiStart, &AnsiEnd, Base);

		if (End)
		{
			if (AnsiEnd == NULL)
			{
				*End = NULL;
			}
			else
			{
				*End = (WIDECHAR*)(Start + (AnsiEnd - AnsiStart));
			}
		}

		return Res;
	}

	static FORCEINLINE WIDECHAR* Strtok(WIDECHAR* StrToken, const WIDECHAR* Delim, WIDECHAR** Context)
	{
		int StrTokenLen = Strlen(StrToken);
		int DelimLen = Strlen(Delim);
		ANSICHAR* AnsiStrToken = (ANSICHAR*)FMemory_Alloca(StrTokenLen+1);
		ANSICHAR* AnsiDelim = (ANSICHAR*)FMemory_Alloca(DelimLen+1);
		CopyWideToAnsi(AnsiStrToken, StrToken);
		CopyWideToAnsi(AnsiDelim, Delim);

		ANSICHAR* Pos = strtok(AnsiStrToken, AnsiDelim);

		if (!Pos)
		{
			return NULL;
		}

		return StrToken + (Pos - AnsiStrToken);
	}

	UE_DEPRECATED(4.22, "GetVarArgs with DestSize and Count arguments has been deprecated - only DestSize should be passed")
	static FORCEINLINE int32 GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr )
	{
		return GetVarArgs(Dest, DestSize, Fmt, ArgPtr);
	}

	static FORCEINLINE int32 GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, const WIDECHAR*& Fmt, va_list ArgPtr )
	{
#if PLATFORM_USE_LS_SPEC_FOR_WIDECHAR
		// fix up the Fmt string, as fast as possible, without using an FString
		const WIDECHAR* OldFormat = Fmt;
		WIDECHAR* NewFormat = (WIDECHAR*)FMemory_Alloca((Strlen(Fmt) * 2 + 1) * sizeof(WIDECHAR));
		
		int32 NewIndex = 0;

		for (; *OldFormat != 0; NewIndex++, OldFormat++)
		{
			// fix up %s -> %ls
			if (OldFormat[0] == LITERAL(WIDECHAR, '%'))
			{
				NewFormat[NewIndex++] = *OldFormat++;

				if (*OldFormat == LITERAL(WIDECHAR, '%'))
				{
					NewFormat[NewIndex] = *OldFormat;
				}
				else
				{
					const WIDECHAR* NextChar = OldFormat;
					
					while(*NextChar != 0 && !FChar::IsAlpha(*NextChar)) 
					{ 
						NewFormat[NewIndex++] = *NextChar;
						++NextChar; 
					};

					if (*NextChar == LITERAL(WIDECHAR, 's'))
					{
						NewFormat[NewIndex++] = LITERAL(WIDECHAR, 'l');
						NewFormat[NewIndex] = *NextChar;
					}
					else
					{
						NewFormat[NewIndex] = *NextChar;
					}

					OldFormat = NextChar;
				}
			}
			else
			{
				NewFormat[NewIndex] = *OldFormat;
			}
		}
		NewFormat[NewIndex] = 0;
#endif // PLATFORM_USE_LS_SPEC_FOR_WIDECHAR
		int32 Result = vswprintf( Dest, DestSize, NewFormat, ArgPtr);
		va_end( ArgPtr );
		return Result;
	}

	/** 
	 * Ansi implementation 
	 **/
	static FORCEINLINE ANSICHAR* Strcpy(ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src)
	{
		return strcpy( Dest, Src );
	}

	static FORCEINLINE ANSICHAR* Strncpy(ANSICHAR* Dest, const ANSICHAR* Src, int32 MaxLen)
	{
		::strncpy(Dest, Src, MaxLen);
		Dest[MaxLen-1]=0;
		return Dest;
	}

	static FORCEINLINE ANSICHAR* Strcat(ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src)
	{
		return strcat( Dest, Src );
	}

	static FORCEINLINE int32 Strcmp( const ANSICHAR* String1, const ANSICHAR* String2 )
	{
		return strcmp(String1, String2);
	}

	static FORCEINLINE int32 Strncmp( const ANSICHAR* String1, const ANSICHAR* String2, SIZE_T Count )
	{
		return strncmp( String1, String2, Count );
	}

	static FORCEINLINE int32 Strlen( const ANSICHAR* String )
	{
		return strlen( String ); 
	}

	static FORCEINLINE const ANSICHAR* Strstr( const ANSICHAR* String, const ANSICHAR* Find)
	{
		return strstr(String, Find);
	}

	static FORCEINLINE const ANSICHAR* Strchr( const ANSICHAR* String, ANSICHAR C)
	{
		return strchr(String, C);
	}

	static FORCEINLINE const ANSICHAR* Strrchr( const ANSICHAR* String, ANSICHAR C)
	{
		return strrchr(String, C);
	}

	static FORCEINLINE int32 Atoi(const ANSICHAR* String)
	{
		return atoi( String ); 
	}

	static FORCEINLINE int64 Atoi64(const ANSICHAR* String)
	{
		return strtoll( String, NULL, 10 );
	}

	static FORCEINLINE float Atof(const ANSICHAR* String)
	{
		return (float)atof( String ); 
	}

	static FORCEINLINE double Atod(const ANSICHAR* String)
	{
		return atof( String ); 
	}

	static FORCEINLINE int32 Strtoi( const ANSICHAR* Start, ANSICHAR** End, int32 Base ) 
	{
		return strtol( Start, End, Base ); 
	}

	static FORCEINLINE int64 Strtoi64( const ANSICHAR* Start, ANSICHAR** End, int32 Base ) 
	{
		return strtoll(Start, End, Base);;
	}

	static FORCEINLINE uint64 Strtoui64( const ANSICHAR* Start, ANSICHAR** End, int32 Base ) 
	{
		return strtoull(Start, End, Base);;
	}

	static FORCEINLINE ANSICHAR* Strtok(ANSICHAR* StrToken, const ANSICHAR* Delim, ANSICHAR** Context)
	{
		return strtok(StrToken, Delim);
	}

	UE_DEPRECATED(4.22, "GetVarArgs with DestSize and Count arguments has been deprecated - only DestSize should be passed")
	static FORCEINLINE int32 GetVarArgs(ANSICHAR* Dest, SIZE_T DestSize, int32 Count, const ANSICHAR*& Fmt, va_list ArgPtr)
	{
		return GetVarArgs(Dest, DestSize, Count, Fmt, ArgPtr);
	}

	static FORCEINLINE int32 GetVarArgs( ANSICHAR* Dest, SIZE_T DestSize, const ANSICHAR*& Fmt, va_list ArgPtr )
	{
		int32 Result = vsnprintf(Dest, DestSize, Fmt, ArgPtr);
		va_end( ArgPtr );
		return (Result != -1 && Result < (int32)DestSize) ? Result : -1;
	}

	/** 
	 * UCS2 implementation 
	 **/

	static FORCEINLINE int32 Strlen( const UCS2CHAR* String )
	{
		int32 Result = 0;
		while (*String++)
		{
			++Result;
		}

		return Result;
	}
};
#endif

typedef FAndroidPlatformString FPlatformString;

