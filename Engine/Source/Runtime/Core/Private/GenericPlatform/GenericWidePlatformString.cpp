// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericWidePlatformString.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"

#if PLATFORM_TCHAR_IS_CHAR16

DEFINE_LOG_CATEGORY_STATIC(LogStandardPlatformString, Log, All);

WIDECHAR* FGenericWidePlatformString::Strcpy(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
{
	TCHAR *BufPtr = Dest;

	while (*Src && --DestCount)
	{
		*BufPtr++ = *Src++;
	}

	*BufPtr = 0;

	return Dest;
}

WIDECHAR* FGenericWidePlatformString::Strncpy(WIDECHAR* Dest, const WIDECHAR* Src, SIZE_T MaxLen)
{
	TCHAR *BufPtr = Dest;

	// the spec says that strncpy should fill the buffer with zeroes
	// we break the spec by enforcing a trailing zero, so we do --MaxLen instead of MaxLen--
	bool bFillWithZero = false;
	while (--MaxLen)
	{
		if (bFillWithZero)
		{
			*BufPtr++ = 0;
		}
		else
		{
			if (*Src == 0)
			{
				bFillWithZero = true;
			}
			*BufPtr++ = *Src++;
		}
	}

	// always have trailing zero
	*BufPtr = 0;

	return Dest;
}

WIDECHAR* FGenericWidePlatformString::Strcat(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
{
	TCHAR *String = Dest;

	while (*String != 0 && DestCount > 1)
	{
		String++;
		// remove how much we can copy in the lower loop
		DestCount--;
	}

	while (*Src != 0 && DestCount > 1)
	{
		*String++ = *Src++;
		DestCount--;
	}

	//	// rewind 1 for trailing zero
	//	String--;
	*String = 0;

	return Dest;
}

int32 FGenericWidePlatformString::Strtoi(const WIDECHAR* Start, WIDECHAR** End, int32 Base)
{
	if (End == nullptr)
	{
		return Strtoi(TCHAR_TO_UTF8(Start), nullptr, Base);
	}

#if UE_BUILD_DEBUG
	// make sure we aren't using high byte characters
	// @todo this!
#endif

	// convert to ANSI, and remember the end to get an offset
	auto Ansi = StringCast<ANSICHAR>(Start);
	ANSICHAR* AnsiEnd;
	int32 Result = Strtoi(Ansi.Get(), &AnsiEnd, Base);

	// the end is the offset from
	*End = const_cast<WIDECHAR*>(Start) + (AnsiEnd - Ansi.Get());

	return Result;
}

int64 FGenericWidePlatformString::Strtoi64(const WIDECHAR* Start, WIDECHAR** End, int32 Base)
{
	if (End == nullptr)
	{
		return Strtoi64(TCHAR_TO_UTF8(Start), nullptr, Base);
	}

#if UE_BUILD_DEBUG
	// make sure we aren't using high byte characters
	// @todo this!
#endif

	// convert to ANSI, and remember the end to get an offset
	auto Ansi = StringCast<ANSICHAR>(Start);
	ANSICHAR* AnsiEnd;
	int64 Result = Strtoi64(Ansi.Get(), &AnsiEnd, Base);

	// the end is the offset from
	*End = const_cast<WIDECHAR*>(Start) + (AnsiEnd - Ansi.Get());

	return Result;
}

uint64 FGenericWidePlatformString::Strtoui64(const WIDECHAR* Start, WIDECHAR** End, int32 Base)
{
	if (End == nullptr)
	{
		return Strtoui64(TCHAR_TO_UTF8(Start), nullptr, Base);
	}

#if UE_BUILD_DEBUG
	// make sure we aren't using high byte characters
	// @todo this!
#endif

	// convert to ANSI, and remember the end to get an offset
	auto Ansi = StringCast<ANSICHAR>(Start);
	ANSICHAR* AnsiEnd;
	uint64 Result = Strtoui64(Ansi.Get(), &AnsiEnd, Base);

	// the end is the offset from
	*End = const_cast<WIDECHAR*>(Start) + (AnsiEnd - Ansi.Get());

	return Result;
}

WIDECHAR* FGenericWidePlatformString::Strtok(WIDECHAR* StrToken, const WIDECHAR* Delim, WIDECHAR** Context)
{
	check(Context);
	check(Delim);

	WIDECHAR* SearchString = StrToken;
	if (!SearchString)
	{
		check(*Context);
		SearchString = *Context;
	}

	WIDECHAR* TokenStart = SearchString;
	while (*TokenStart && Strchr(Delim, *TokenStart))
	{
		++TokenStart;
	}

	if (*TokenStart == 0)
	{
		return nullptr;
	}

	WIDECHAR* TokenEnd = TokenStart;
	while (*TokenEnd && !Strchr(Delim, *TokenEnd))
	{
		++TokenEnd;
	}

	*TokenEnd = 0;
	*Context = TokenEnd + 1;

	return TokenStart;
}

float FGenericWidePlatformString::Atof(const WIDECHAR* String)
{
	return Atof(TCHAR_TO_UTF8(String));
}

double FGenericWidePlatformString::Atod(const WIDECHAR* String)
{
	return Atod(TCHAR_TO_UTF8(String));
}


#if PLATFORM_ANDROID
// This is a full copy of iswspace function from Android sources
// For some reason function from libc does not work correctly for some korean characters like: 0xBE0C
int iswspace(wint_t wc)
{
	static const wchar_t spaces[] = {
		' ', '\t', '\n', '\r', 11, 12,  0x0085,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005,
		0x2006, 0x2008, 0x2009, 0x200a,
		0x2028, 0x2029, 0x205f, 0x3000, 0
	};
	return wc && wcschr(spaces, wc);
}
#endif


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static const int OUTPUT_SIZE = 256;
int32 TestGetVarArgs(WIDECHAR* OutputString, const WIDECHAR* Format, ...)
{
	va_list ArgPtr;
	va_start(ArgPtr, Format);

	return FGenericWidePlatformString::GetVarArgs(OutputString, OUTPUT_SIZE, OUTPUT_SIZE - 1, Format, ArgPtr);
}

void RunGetVarArgsTests()
{
	WIDECHAR OutputString[OUTPUT_SIZE];

	TestGetVarArgs(OutputString, TEXT("Test A|%-20s|%20s|%10.2f|%-10.2f|"), TEXT("LEFT"), TEXT("RIGHT"), 33.333333, 66.666666);
	check(FString(OutputString) == FString(TEXT("Test A|LEFT                |               RIGHT|     33.33|66.67     |")));

	TestGetVarArgs(OutputString, TEXT("Test B|Percents:%%%%%%%d|"), 3);
	check(FString(OutputString) == FString(TEXT("Test B|Percents:%%%3|")));

	TestGetVarArgs(OutputString, TEXT("Test C|%d|%i|%X|%x|%u|"), 12345, 54321, 0x123AbC, 15, 99);
	check(FString(OutputString) == FString(TEXT("Test C|12345|54321|123ABC|f|99|")));

	TestGetVarArgs(OutputString, TEXT("Test D|%p|"), 0x12345);
	check(FString(OutputString) == FString(TEXT("Test D|0x12345|")));

	TestGetVarArgs(OutputString, TEXT("Test E|%lld|"), 12345678912345);
	check(FString(OutputString) == FString(TEXT("Test E|12345678912345|")));

	TestGetVarArgs(OutputString, TEXT("Test F|%f|%e|%g|"), 123.456, 123.456, 123.456);
	check(FString(OutputString) == FString(TEXT("Test F|123.456000|1.234560e+02|123.456|")));
}
#endif

int32 FGenericWidePlatformString::GetVarArgs(WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static bool bTested = false;
	if (!bTested)
	{
		bTested = true;
		RunGetVarArgsTests();
	}
#endif

	if (Fmt == nullptr)
	{
		if ((DestSize > 0) && (Dest != nullptr))
		{
			*Dest = 0;
		}
		return 0;
	}

	int FmtLen = FPlatformMath::Min(Count, Strlen(Fmt));
	const TCHAR *Src = Fmt;

	TCHAR *Dst = Dest;
	TCHAR *EndDst = Dst + (DestSize - 1);

	while ((*Src) && (Dst < EndDst))
	{
		if (*Src != '%')
		{
			*Dst = *Src;
			Dst++;
			Src++;
			continue;
		}

		const TCHAR *Percent = Src;
		int FieldLen = 0;
		int PrecisionLen = -1;

		Src++; // skip the '%' char...

		while (*Src == ' ')
		{
			*Dst = ' ';
			Dst++;
			Src++;
		}

		// Skip modifier flags that don't need additional processing;
		// they still get passed to snprintf() below based on the conversion.
		if (*Src == '+')
		{
			Src++;
		}

		// check for field width requests...
		if ((*Src == '-') || ((*Src >= '0') && (*Src <= '9')))
		{
			const TCHAR *Cur = Src + 1;
			while ((*Cur >= '0') && (*Cur <= '9'))
			{
				Cur++;
			}

			FieldLen = Atoi(Src);
			Src = Cur;
		}

		// check for dynamic field requests
		if (*Src == '*')
		{
			FieldLen = va_arg(ArgPtr, int32);
			Src++;
		}

		if (*Src == '.')
		{
			const TCHAR *Cur = Src + 1;
			while ((*Cur >= '0') && (*Cur <= '9'))
			{
				Cur++;
			}

			PrecisionLen = Atoi(Src + 1);
			Src = Cur;
		}

		// Check for 'ls' field, change to 's'
		if ((Src[0] == 'l' && Src[1] == 's'))
		{
			Src++;
		}

		switch (*Src)
		{
		case '%':
		{
			Src++;
			*Dst = '%';
			Dst++;
			break;
		}

		case 'c':
		{
			TCHAR Val = (TCHAR) va_arg(ArgPtr, int);
			Src++;
			*Dst = Val;
			Dst++;
			break;
		}

		case 'd':
		case 'i':
		case 'X':
		case 'x':
		case 'u':
		{
			Src++;
			int Val = va_arg(ArgPtr, int);
			ANSICHAR AnsiNum[30];
			ANSICHAR FmtBuf[30];

			// Yes, this is lame.
			int CpyIdx = 0;
			while (Percent < Src && CpyIdx < ARRAY_COUNT(FmtBuf))
			{
				FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
				Percent++;
				CpyIdx++;
			}
			FmtBuf[CpyIdx] = 0;

			int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
			if ((Dst + RetCnt) > EndDst)
			{
				return -1;	// Fail - the app needs to create a larger buffer and try again
			}
			for (int i = 0; i < RetCnt; i++)
			{
				*Dst = (TCHAR) AnsiNum[i];
				Dst++;
			}
			break;
		}

		case 'z':
		case 'Z':
		{
			Src += 2;

			size_t Val = va_arg(ArgPtr, size_t);

			ANSICHAR AnsiNum[30];
			ANSICHAR FmtBuf[30];

			// Yes, this is lame.
			int CpyIdx = 0;
			while (Percent < Src && CpyIdx < ARRAY_COUNT(FmtBuf))
			{
				FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
				Percent++;
				CpyIdx++;
			}
			FmtBuf[CpyIdx] = 0;

			int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
			if ((Dst + RetCnt) > EndDst)
			{
				return -1;	// Fail - the app needs to create a larger buffer and try again
			}
			for (int i = 0; i < RetCnt; i++)
			{
				*Dst = (TCHAR) AnsiNum[i];
				Dst++;
			}
			break;
		}

		case 'p':
		{
			Src++;
			void* Val = va_arg(ArgPtr, void*);
			ANSICHAR AnsiNum[30];
			ANSICHAR FmtBuf[30];

			// Yes, this is lame.
			int CpyIdx = 0;
			while (Percent < Src && CpyIdx < ARRAY_COUNT(FmtBuf))
			{
				FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
				Percent++;
				CpyIdx++;
			}
			FmtBuf[CpyIdx] = 0;

			int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
			if ((Dst + RetCnt) > EndDst)
			{
				return -1;	// Fail - the app needs to create a larger buffer and try again
			}
			for (int i = 0; i < RetCnt; i++)
			{
				*Dst = (TCHAR) AnsiNum[i];
				Dst++;
			}
			break;
		}

		case 'l':
		case 'I':
		case 'h':
		{
			int RemainingSize = Strlen(Src);

			// treat %ld as %d. Also shorts for %h will be promoted to ints
			if (RemainingSize >= 2 && ((Src[0] == 'l' && Src[1] == 'd') || Src[0] == 'h'))
			{
				Src += 2;
				int Val = va_arg(ArgPtr, int);
				ANSICHAR AnsiNum[30];
				ANSICHAR FmtBuf[30];

				// Yes, this is lame.
				int CpyIdx = 0;
				while (Percent < Src && CpyIdx < ARRAY_COUNT(FmtBuf))
				{
					FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
					Percent++;
					CpyIdx++;
				}
				FmtBuf[CpyIdx] = 0;

				int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
				if ((Dst + RetCnt) > EndDst)
				{
					return -1;	// Fail - the app needs to create a larger buffer and try again
				}
				for (int i = 0; i < RetCnt; i++)
				{
					*Dst = (TCHAR) AnsiNum[i];
					Dst++;
				}
				break;
			}
			// Treat %lf as a %f
			else if (RemainingSize >= 2 && Src[0] == 'l' && Src[1] == 'f')
			{
				Src += 2;
				double Val = va_arg(ArgPtr, double);
				ANSICHAR AnsiNum[30];
				ANSICHAR FmtBuf[30];

				// Yes, this is lame.
				int CpyIdx = 0;
				while (Percent < Src && CpyIdx < ARRAY_COUNT(FmtBuf))
				{
					FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
					Percent++;
					CpyIdx++;
				}
				FmtBuf[CpyIdx] = 0;

				int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
				if ((Dst + RetCnt) > EndDst)
				{
					return -1;	// Fail - the app needs to create a larger buffer and try again
				}
				for (int i = 0; i < RetCnt; i++)
				{
					*Dst = (TCHAR) AnsiNum[i];
					Dst++;
				}
				break;
			}

			if (RemainingSize >= 2 && (Src[0] == 'l' && Src[1] != 'l' && Src[1] != 'u' && Src[1] != 'x'))
			{
				printf("Unknown percent [%lc%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], Src[1], TCHAR_TO_ANSI(Fmt));
				Src++;  // skip it, I guess.
				break;
			}
			else if (RemainingSize >= 3 && Src[0] == 'I' && (Src[1] != '6' || Src[2] != '4'))
			{
				printf("Unknown percent [%lc%lc%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], Src[1], Src[2], TCHAR_TO_ANSI(Fmt));
				Src++;  // skip it, I guess.
				break;
			}

			// Yes, this is lame.
			int CpyIdx = 0;
			unsigned long long Val = va_arg(ArgPtr, unsigned long long);
			ANSICHAR AnsiNum[60];
			ANSICHAR FmtBuf[30];
			if (Src[0] == 'l')
			{
				Src += 3;
			}
			else
			{
				Src += 4;
				strcpy(FmtBuf, "%L");
				Percent += 4;
				CpyIdx = 2;
			}

			while (Percent < Src && CpyIdx < ARRAY_COUNT(FmtBuf))
			{
				FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
				Percent++;
				CpyIdx++;
			}
			FmtBuf[CpyIdx] = 0;

			int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
			if ((Dst + RetCnt) > EndDst)
			{
				return -1;	// Fail - the app needs to create a larger buffer and try again
			}
			for (int i = 0; i < RetCnt; i++)
			{
				*Dst = (TCHAR) AnsiNum[i];
				Dst++;
			}
			break;
		}

		case 'f':
		case 'e':
		case 'g':
		{
			Src++;
			double Val = va_arg(ArgPtr, double);
			ANSICHAR AnsiNum[30];
			ANSICHAR FmtBuf[30];

			// Yes, this is lame.
			int CpyIdx = 0;
			while (Percent < Src && CpyIdx < ARRAY_COUNT(FmtBuf))
			{
				FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
				Percent++;
				CpyIdx++;
			}
			FmtBuf[CpyIdx] = 0;

			int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
			if ((Dst + RetCnt) > EndDst)
			{
				return -1;	// Fail - the app needs to create a larger buffer and try again
			}
			for (int i = 0; i < RetCnt; i++)
			{
				*Dst = (TCHAR) AnsiNum[i];
				Dst++;
			}
			break;
		}

		case 's':
		{
			Src++;
			static const TCHAR* Null = TEXT("(null)");
			const TCHAR *Val = va_arg(ArgPtr, TCHAR *);
			if (Val == nullptr)
			{
				Val = Null;
			}

			int RetCnt = Strlen(Val);
			int Spaces = FPlatformMath::Max(FPlatformMath::Abs(FieldLen) - RetCnt, 0);
			if ((Dst + RetCnt + Spaces) > EndDst)
			{
				return -1;	// Fail - the app needs to create a larger buffer and try again
			}
			if (Spaces > 0 && FieldLen > 0)
			{
				for (int i = 0; i < Spaces; i++)
				{
					*Dst = TEXT(' ');
					Dst++;
				}
			}
			for (int i = 0; i < RetCnt; i++)
			{
				*Dst = *Val;
				Dst++;
				Val++;
			}
			if (Spaces > 0 && FieldLen < 0)
			{
				for (int i = 0; i < Spaces; i++)
				{
					*Dst = TEXT(' ');
					Dst++;
				}
			}
			break;
		}

		default:
			printf("Unknown percent [%%%c] in FGenericWidePlatformString::GetVarArgs().\n", *Src);
			Src++;  // skip char, I guess.
			break;
		}
	}

	// Check if we were able to finish the entire format string
	// If not, the app needs to create a larger buffer and try again
	if (*Src)
	{
		return -1;
	}

	*Dst = 0;  // null terminate the new string.
	return Dst - Dest;
}

#endif

