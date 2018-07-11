// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Char.h"
#include "GenericPlatform/GenericPlatformStricmp.h"
#include "GenericPlatform/GenericPlatformString.h"

#if PLATFORM_APPLE || PLATFORM_LINUX || PLATFORM_HTML5 || PLATFORM_PS4 || PLATFORM_SWITCH || PLATFORM_ANDROID

/**
* Standard implementation
**/
struct FGenericWidePlatformString : public FGenericPlatformString
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

public:
	/**
	 * Compares two strings case-insensitive.
	 *
	 * @param String1 First string to compare.
	 * @param String2 Second string to compare.
	 *
	 * @returns Zero if both strings are equal. Greater than zero if first
	 *          string is greater than the second one. Less than zero
	 *          otherwise.
	 */
	template <typename CharType1, typename CharType2>
	static inline int32 Stricmp(const CharType1* String1, const CharType2* String2)
	{
		return FGenericPlatformStricmp::Stricmp(String1, String2);
	}

	template <typename CharType>
	static inline int32 Strnicmp( const CharType* String1, const CharType* String2, SIZE_T Count )
	{
		// walk the strings, comparing them case insensitively, up to a max size
		for (; (*String1 || *String2) && Count > 0; String1++, String2++, Count--)
		{
			if(*String1 != *String2)
			{
				CharType Char1 = TChar<CharType>::ToUpper(*String1), Char2 = TChar<CharType>::ToUpper(*String2);
				if (Char1 != Char2)
				{
					return Char1 - Char2;
				}
			}
		}
		return 0;
	}

	/**
	 * Unicode implementation
	 **/
	static WIDECHAR* Strcpy(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src);
	static WIDECHAR* Strncpy(WIDECHAR* Dest, const WIDECHAR* Src, SIZE_T MaxLen);
	static WIDECHAR* Strcat(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src);

	static int32 Strcmp( const WIDECHAR* String1, const WIDECHAR* String2 )
	{
		// walk the strings, comparing them case sensitively
		for (; *String1 || *String2; String1++, String2++)
		{
			WIDECHAR A = *String1, B = *String2;
			if (A != B)
			{
				return A - B;
			}
		}
		return 0;
	}

	static int32 Strncmp( const WIDECHAR* String1, const WIDECHAR* String2, SIZE_T Count )
	{
		// walk the strings, comparing them case sensitively, up to a max size
		for (; (*String1 || *String2) && Count; String1++, String2++, Count--)
		{
			TCHAR A = *String1, B = *String2;
			if (A != B)
			{
				return A - B;
			}
		}
		return 0;
	}

	static int32 Strlen( const WIDECHAR* String )
	{
		int32 Length = -1;
		
		do
		{
			Length++;
		}
		while (*String++);
		
		return Length;
	}

	static const WIDECHAR* Strstr( const WIDECHAR* String, const WIDECHAR* Find)
	{
		WIDECHAR Char1, Char2;
		if ((Char1 = *Find++) != 0)
		{
			size_t Length = Strlen(Find);
			
			do
			{
				do
				{
					if ((Char2 = *String++) == 0)
					{
						return nullptr;
					}
				}
				while (Char1 != Char2);
			}
			while (Strncmp(String, Find, Length) != 0);
			
			String--;
		}
		
		return String;
	}

	static const WIDECHAR* Strchr( const WIDECHAR* String, WIDECHAR C)
	{
		while (*String != C && *String != 0)
		{
			String++;
		}
		
		return (*String == C) ? (TCHAR *)String : nullptr;
	}

	static const WIDECHAR* Strrchr( const WIDECHAR* String, WIDECHAR C)
	{
		const WIDECHAR *Last = nullptr;
		
		while (true)
		{
			if (*String == C)
			{
				Last = String;
			}
			
			if (*String == 0)
			{
				break;
			}
			
			String++;
		}
		
		return Last;
	}

	static int32 Strtoi( const WIDECHAR* Start, WIDECHAR** End, int32 Base );
	static int64 Strtoi64( const WIDECHAR* Start, WIDECHAR** End, int32 Base );
	static uint64 Strtoui64( const WIDECHAR* Start, WIDECHAR** End, int32 Base );
	static float Atof(const WIDECHAR* String);
	static double Atod(const WIDECHAR* String);

	static FORCEINLINE int32 Atoi(const WIDECHAR* String)
	{
		return Strtoi( String, NULL, 10 );
	}
	
	static FORCEINLINE int64 Atoi64(const WIDECHAR* String)
	{
		return Strtoi64( String, NULL, 10 );
	}

	
	
	static WIDECHAR* Strtok( WIDECHAR* StrToken, const WIDECHAR* Delim, WIDECHAR** Context );
	static int32 GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr );

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
		return strtoll(Start, End, Base);
	}

	static FORCEINLINE uint64 Strtoui64( const ANSICHAR* Start, ANSICHAR** End, int32 Base )
	{
		return strtoull(Start, End, Base);
	}

	static FORCEINLINE ANSICHAR* Strtok(ANSICHAR* StrToken, const ANSICHAR* Delim, ANSICHAR** Context)
	{
		return strtok(StrToken, Delim);
	}

	static int32 GetVarArgs( ANSICHAR* Dest, SIZE_T DestSize, int32 Count, const ANSICHAR*& Fmt, va_list ArgPtr )
	{
		int32 Result = vsnprintf(Dest,Count,Fmt,ArgPtr);
		va_end( ArgPtr );
		return Result;
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
