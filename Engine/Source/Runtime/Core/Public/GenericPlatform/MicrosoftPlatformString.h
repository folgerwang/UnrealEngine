// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Char.h"
#include "GenericPlatform/GenericPlatformString.h"
#include "GenericPlatform/GenericPlatformStricmp.h"

/**
* Microsoft specific implementation 
**/

#pragma warning(push)
#pragma warning(disable : 4996) // 'function' was declared deprecated  (needed for the secure string functions)
#pragma warning(disable : 4995) // 'function' was declared deprecated  (needed for the secure string functions)

struct FMicrosoftPlatformString : public FGenericPlatformString
{
	/** 
	 * Wide character implementation 
	 **/
	static FORCEINLINE WIDECHAR* Strcpy(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
	{
		return (WIDECHAR*)_tcscpy(Dest, Src);
	}

	static FORCEINLINE WIDECHAR* Strncpy(WIDECHAR* Dest, const WIDECHAR* Src, SIZE_T MaxLen)
	{
		_tcsncpy(Dest, Src, MaxLen-1);
		Dest[MaxLen-1] = 0;
		return Dest;
	}

	static FORCEINLINE WIDECHAR* Strcat(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
	{
		return (WIDECHAR*)_tcscat(Dest, Src);
	}

	static FORCEINLINE WIDECHAR* Strupr(WIDECHAR* Dest, SIZE_T DestCount)
	{
		return (WIDECHAR*)_tcsupr(Dest);
	}

	static FORCEINLINE int32 Strcmp( const WIDECHAR* String1, const WIDECHAR* String2 )
	{
		return _tcscmp(String1, String2);
	}

	static FORCEINLINE int32 Strncmp( const WIDECHAR* String1, const WIDECHAR* String2, SIZE_T Count )
	{
		return _tcsncmp( String1, String2, Count );
	}

	static FORCEINLINE int32 Strlen( const WIDECHAR* String )
	{
		return _tcslen( String );
	}

	static FORCEINLINE const WIDECHAR* Strstr( const WIDECHAR* String, const WIDECHAR* Find)
	{
		return _tcsstr( String, Find );
	}

	static FORCEINLINE const WIDECHAR* Strchr( const WIDECHAR* String, WIDECHAR C)
	{
		return _tcschr( String, C ); 
	}

	static FORCEINLINE const WIDECHAR* Strrchr( const WIDECHAR* String, WIDECHAR C)
	{
		return _tcsrchr( String, C ); 
	}

	static FORCEINLINE int32 Atoi(const WIDECHAR* String)
	{
		return _tstoi( String ); 
	}

	static FORCEINLINE int64 Atoi64(const WIDECHAR* String)
	{
		return _tstoi64( String ); 
	}

	static FORCEINLINE float Atof(const WIDECHAR* String)
	{
		return _tstof( String );
	}

	static FORCEINLINE double Atod(const WIDECHAR* String)
	{
		return _tcstod( String, NULL ); 
	}

	static FORCEINLINE int32 Strtoi( const WIDECHAR* Start, WIDECHAR** End, int32 Base ) 
	{
		return _tcstoul( Start, End, Base );
	}

	static FORCEINLINE int64 Strtoi64( const WIDECHAR* Start, WIDECHAR** End, int32 Base ) 
	{
		return _tcstoi64( Start, End, Base ); 
	}

	static FORCEINLINE uint64 Strtoui64( const WIDECHAR* Start, WIDECHAR** End, int32 Base ) 
	{
		return _tcstoui64( Start, End, Base ); 
	}

	static FORCEINLINE WIDECHAR* Strtok(WIDECHAR* StrToken, const WIDECHAR* Delim, WIDECHAR** Context)
	{
		return _tcstok_s(StrToken, Delim, Context);
	}

	UE_DEPRECATED(4.22, "GetVarArgs with DestSize and Count arguments has been deprecated - only DestSize should be passed")
	static FORCEINLINE int32 GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr )
	{
		return GetVarArgs(Dest, DestSize, Fmt, ArgPtr);
	}

	static FORCEINLINE int32 GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, const WIDECHAR*& Fmt, va_list ArgPtr )
	{
		int32 Result = vswprintf(Dest, DestSize, Fmt, ArgPtr);
		va_end( ArgPtr );
		return Result;
	}

	/** 
	 * Ansi implementation 
	 **/
	static FORCEINLINE ANSICHAR* Strcpy(ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src)
	{
		return (ANSICHAR*)strcpy(Dest, Src);
	}

	static FORCEINLINE void Strncpy(ANSICHAR* Dest, const ANSICHAR* Src, SIZE_T MaxLen)
	{
		strncpy(Dest, Src, MaxLen);
		Dest[MaxLen-1] = 0;
	}

	static FORCEINLINE ANSICHAR* Strcat(ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src)
	{
		return (ANSICHAR*)strcat( Dest, Src );
	}

	static FORCEINLINE ANSICHAR* Strupr(ANSICHAR* Dest, SIZE_T DestCount)
	{
		return (ANSICHAR*)_strupr(Dest);
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
		return _strtoi64( String, NULL, 10 ); 
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
		return _strtoi64( Start, End, Base ); 
	}

	static FORCEINLINE uint64 Strtoui64( const ANSICHAR* Start, ANSICHAR** End, int32 Base ) 
	{
		return _strtoui64( Start, End, Base ); 
	}

	static FORCEINLINE ANSICHAR* Strtok(ANSICHAR* StrToken, const ANSICHAR* Delim, ANSICHAR** Context)
	{
		return strtok_s(StrToken, Delim, Context);
	}

	UE_DEPRECATED(4.22, "GetVarArgs with DestSize and Count arguments has been deprecated - only DestSize should be passed")
	static FORCEINLINE int32 GetVarArgs( ANSICHAR* Dest, SIZE_T DestSize, int32 Count, const ANSICHAR*& Fmt, va_list ArgPtr )
	{
		return GetVarArgs(Dest, DestSize, Fmt, ArgPtr);
	}

	static FORCEINLINE int32 GetVarArgs( ANSICHAR* Dest, SIZE_T DestSize, const ANSICHAR*& Fmt, va_list ArgPtr )
	{
		int32 Result = vsnprintf( Dest, DestSize, Fmt, ArgPtr );
		va_end( ArgPtr );
		return (Result != -1 && Result < (int32)DestSize) ? Result : -1;
	}

	/**
	 * UCS2CHAR implementation - this is identical to WIDECHAR for Windows platforms
	 */

	static FORCEINLINE int32 Strlen( const UCS2CHAR* String )
	{
		return _tcslen( (const WIDECHAR*)String );
	}
};

#pragma warning(pop) // 'function' was was declared deprecated  (needed for the secure string functions)
