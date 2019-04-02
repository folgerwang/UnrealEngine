// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextHistory.h"
#include "UObject/ObjectVersion.h"
#include "Internationalization/ITextData.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "UObject/PropertyPortFlags.h"

#include "Internationalization/FastDecimalFormat.h"
#include "Internationalization/TextFormatter.h"
#include "Internationalization/TextChronoFormatter.h"
#include "Internationalization/TextTransformer.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTextHistory, Log, All);
DEFINE_LOG_CATEGORY(LogTextHistory);

/** Utilities for stringifying text */
namespace TextStringificationUtil
{

bool PeekMarker(const TCHAR* Buffer, const TCHAR* InMarker, const int32 InMarkerLen)
{
	return FCString::Strncmp(Buffer, InMarker, InMarkerLen) == 0;
}

bool PeekInsensitiveMarker(const TCHAR* Buffer, const TCHAR* InMarker, const int32 InMarkerLen)
{
	return FCString::Strnicmp(Buffer, InMarker, InMarkerLen) == 0;
}

const TCHAR* SkipMarker(const TCHAR* Buffer, const TCHAR* InMarker, const int32 InMarkerLen)
{
	if (!PeekMarker(Buffer, InMarker, InMarkerLen))
	{
		return nullptr;
	}

	Buffer += InMarkerLen;
	return Buffer;
}

const TCHAR* SkipInsensitiveMarker(const TCHAR* Buffer, const TCHAR* InMarker, const int32 InMarkerLen)
{
	if (!PeekInsensitiveMarker(Buffer, InMarker, InMarkerLen))
	{
		return nullptr;
	}

	Buffer += InMarkerLen;
	return Buffer;
}

const TCHAR* SkipWhitespace(const TCHAR* Buffer)
{
	while (*Buffer && (*Buffer == TCHAR(' ') || *Buffer == TCHAR('\t')) && *Buffer != TCHAR('\n') && *Buffer != TCHAR('\r'))
	{
		++Buffer;
	}

	return Buffer;
}

const TCHAR* SkipWhitespaceToCharacter(const TCHAR* Buffer, const TCHAR InChar)
{
	Buffer = SkipWhitespace(Buffer);

	if (Buffer && *Buffer != InChar)
	{
		return nullptr;
	}

	return Buffer;
}

const TCHAR* SkipWhitespaceAndCharacter(const TCHAR* Buffer, const TCHAR InChar)
{
	Buffer = SkipWhitespaceToCharacter(Buffer, InChar);

	if (Buffer)
	{
		++Buffer;
	}

	return Buffer;
}

const TCHAR* ReadNumberFromBuffer(const TCHAR* Buffer, FFormatArgumentValue& OutValue)
{
	static const TCHAR ValidNumericChars[] = TEXT("+-0123456789.ful");
	static const TCHAR SuffixNumericChars[] = TEXT("ful");

	FString NumericString;
	while (*Buffer && FCString::Strchr(ValidNumericChars, *Buffer))
	{
		NumericString += *Buffer++;
	}

	FString SuffixString;
	while (NumericString.Len() > 0 && FCString::Strchr(SuffixNumericChars, NumericString[NumericString.Len() - 1]))
	{
		SuffixString += NumericString[NumericString.Len() - 1];
		NumericString.RemoveAt(NumericString.Len() - 1, 1, /*bAllowShrinking*/false);
	}

	if (!NumericString.IsNumeric())
	{
		return nullptr;
	}

	if (FCString::Strchr(*SuffixString, TEXT('f')))
	{
		// Probably a float
		float LocalFloat = 0.0f;
		LexFromString(LocalFloat, *NumericString);
		OutValue = FFormatArgumentValue(LocalFloat);
	}
	else if (FCString::Strchr(*SuffixString, TEXT('u')))
	{
		// Probably unsigned
		uint64 LocalUInt = 0;
		LexFromString(LocalUInt, *NumericString);
		OutValue = FFormatArgumentValue(LocalUInt);
	}
	else if (FCString::Strchr(*NumericString, TEXT('.')))
	{
		// Probably a double (or unmarked float)
		double LocalDouble = 0.0;
		LexFromString(LocalDouble, *NumericString);
		OutValue = FFormatArgumentValue(LocalDouble);
	}
	else
	{
		// Probably an int (or unmarked unsigned)
		int64 LocalInt = 0;
		LexFromString(LocalInt, *NumericString);
		OutValue = FFormatArgumentValue(LocalInt);
	}

	return Buffer;
}

const TCHAR* ReadAlnumFromBuffer(const TCHAR* Buffer, FString& OutValue)
{
	OutValue.Reset();
	while (*Buffer && (FChar::IsAlnum(*Buffer) || *Buffer == TEXT('_')))
	{
		OutValue += *Buffer++;
	}

	if (OutValue.IsEmpty())
	{
		return nullptr;
	}

	return Buffer;
}

const TCHAR* ReadQuotedStringFromBuffer(const TCHAR* Buffer, FString& OutStr)
{
	// Might be wrapped in TEXT(...) if the string came from C++
	const bool bIsMacroWrapped = TEXT_STRINGIFICATION_PEEK_MARKER(TextMarker);
	if (bIsMacroWrapped)
	{
		// Skip the TEXT marker
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextMarker);

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');
	}

	// Read the quoted string
	{
		int32 CharsRead = 0;
		if (!FParse::QuotedString(Buffer, OutStr, &CharsRead))
		{
			return nullptr;
		}
		Buffer += CharsRead;
	}

	// Skip the end of the macro
	if (bIsMacroWrapped)
	{
		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');
	}

	return Buffer;
}

template <typename T>
void WriteNumberFormattingOptionToBuffer(FString& Buffer, const TCHAR* OptionFunctionName, const T& OptionValue, const T& DefaultOptionValue, TFunctionRef<void(FString&, const T&)> WriteOptionValue)
{
	if (OptionValue != DefaultOptionValue)
	{
		if (!Buffer.IsEmpty())
		{
			Buffer += TEXT('.');
		}
		Buffer += OptionFunctionName;
		Buffer += TEXT('(');
		WriteOptionValue(Buffer, OptionValue);
		Buffer += TEXT(')');
	}
}

void WriteNumberFormattingOptionsToBuffer(FString& Buffer, const FNumberFormattingOptions& Options)
{
	auto WriteBoolOption = [](FString& OutValueBuffer, const bool& InValue)
	{
		OutValueBuffer += LexToString(InValue);
	};

	auto WriteIntOption = [](FString& OutValueBuffer, const int32& InValue)
	{
		OutValueBuffer += LexToString(InValue);
	};

	auto WriteRoundingModeOption = [](FString& OutValueBuffer, const ERoundingMode& InValue)
	{
		WriteScopedEnumToBuffer(OutValueBuffer, TEXT("ERoundingMode::"), InValue);
	};

	static const FNumberFormattingOptions DefaultOptions;

	#define WRITE_CUSTOM_OPTION(Option, WriteOptionValue) WriteNumberFormattingOptionToBuffer<decltype(Options.Option)>(Buffer, TEXT("Set"#Option), Options.Option, DefaultOptions.Option, WriteOptionValue)
	WRITE_CUSTOM_OPTION(AlwaysSign, WriteBoolOption);
	WRITE_CUSTOM_OPTION(UseGrouping, WriteBoolOption);
	WRITE_CUSTOM_OPTION(RoundingMode, WriteRoundingModeOption);
	WRITE_CUSTOM_OPTION(MinimumIntegralDigits, WriteIntOption);
	WRITE_CUSTOM_OPTION(MaximumIntegralDigits, WriteIntOption);
	WRITE_CUSTOM_OPTION(MinimumFractionalDigits, WriteIntOption);
	WRITE_CUSTOM_OPTION(MaximumFractionalDigits, WriteIntOption);
	#undef WRITE_CUSTOM_OPTION
}

template <typename T>
const TCHAR* ReadNumberFormattingOptionFromBuffer(const TCHAR* Buffer, const FString& OptionFunctionName, T& OutOptionValue, TFunctionRef<const TCHAR*(const TCHAR*, T&)> ReadOptionValue)
{
	if (PeekMarker(Buffer, *OptionFunctionName, OptionFunctionName.Len()))
	{
		// Walk over the function name
		Buffer += OptionFunctionName.Len();

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read the option value
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(ReadOptionValue, OutOptionValue);

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');
	}

	return Buffer;
}

const TCHAR* ReadNumberFormattingOptionsFromBuffer(const TCHAR* Buffer, FNumberFormattingOptions& OutOptions)
{
	auto ReadBoolOption = [](const TCHAR* InValueBuffer, bool& OutValue) -> const TCHAR*
	{
		#define READ_BOOL_OPTION(Value)															\
			{																					\
				static const FString ValueString = TEXT(#Value);								\
				if (FCString::Strnicmp(InValueBuffer, *ValueString, ValueString.Len()) == 0)	\
				{																				\
					OutValue = Value;															\
					InValueBuffer += ValueString.Len();											\
					return InValueBuffer;														\
				}																				\
			}
		READ_BOOL_OPTION(true);
		READ_BOOL_OPTION(false);
		#undef READ_BOOL_OPTION 

		return nullptr;
	};

	auto ReadNumericOption = [](const TCHAR* InValueBuffer, int32& OutValue) -> const TCHAR*
	{
		FFormatArgumentValue ReadValue;
		InValueBuffer = ReadNumberFromBuffer(InValueBuffer, ReadValue);
		if (!InValueBuffer)
		{
			return nullptr;
		}

		switch (ReadValue.GetType())
		{
		case EFormatArgumentType::Int:
			OutValue = (int32)ReadValue.GetIntValue();
			break;
		case EFormatArgumentType::UInt:
			OutValue = (int32)ReadValue.GetUIntValue();
			break;
		case EFormatArgumentType::Float:
			OutValue = (int32)ReadValue.GetFloatValue();
			break;
		case EFormatArgumentType::Double:
			OutValue = (int32)ReadValue.GetDoubleValue();
			break;
		default:
			return nullptr;
		}

		return InValueBuffer;
	};

	auto ReadRoundingModeOption = [](const TCHAR* InValueBuffer, ERoundingMode& OutValue) -> const TCHAR*
	{
		static const FString RoundingModeMarker = TEXT("ERoundingMode::");
		return ReadScopedEnumFromBuffer(InValueBuffer, RoundingModeMarker, OutValue);
	};

	bool bDidReadOption = true;
	while (bDidReadOption)
	{
		bDidReadOption = false;
		#define READ_CUSTOM_OPTION(Option, ReadOptionValue)							\
		{																			\
			static const FString OptionMarker = TEXT("Set"#Option);					\
			if (*Buffer == TEXT('.')) { ++Buffer; }									\
			const TCHAR* const ValueStart = Buffer;									\
			TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(ReadNumberFormattingOptionFromBuffer<decltype(OutOptions.Option)>, OptionMarker, OutOptions.Option, ReadOptionValue); \
			if (Buffer != ValueStart) { bDidReadOption = true; }					\
		}
		READ_CUSTOM_OPTION(AlwaysSign, ReadBoolOption);
		READ_CUSTOM_OPTION(UseGrouping, ReadBoolOption);
		READ_CUSTOM_OPTION(RoundingMode, ReadRoundingModeOption);
		READ_CUSTOM_OPTION(MinimumIntegralDigits, ReadNumericOption);
		READ_CUSTOM_OPTION(MaximumIntegralDigits, ReadNumericOption);
		READ_CUSTOM_OPTION(MinimumFractionalDigits, ReadNumericOption);
		READ_CUSTOM_OPTION(MaximumFractionalDigits, ReadNumericOption);
		#undef READ_CUSTOM_OPTION
	}

	return Buffer;
}

void WriteNumberOrPercentToBuffer(FString& Buffer, const TCHAR* TokenMarker, const FFormatArgumentValue& SourceValue, const TOptional<FNumberFormattingOptions>& FormatOptions, FCulturePtr TargetCulture)
{
	FString Suffix;
	FString CustomOptions;
	if (FormatOptions.IsSet())
	{
		if (FormatOptions->IsIdentical(FNumberFormattingOptions::DefaultWithGrouping()))
		{
			Suffix = GroupedSuffix;
		}
		else if (FormatOptions->IsIdentical(FNumberFormattingOptions::DefaultNoGrouping()))
		{
			Suffix = UngroupedSuffix;
		}
		else
		{
			WriteNumberFormattingOptionsToBuffer(CustomOptions, FormatOptions.GetValue());
			if (!CustomOptions.IsEmpty())
			{
				Suffix = CustomSuffix;
			}
		}
	}

	// Produces LOCGEN_NUMBER/_GROUPED/_UNGROUPED(..., "...") or LOCGEN_NUMBER_CUSTOM(..., ..., "...")
	// Produces LOCGEN_PERCENT/_GROUPED/_UNGROUPED(..., "...") or LOCGEN_PERCENT_CUSTOM(..., ..., "...")
	Buffer += TokenMarker;
	Buffer += Suffix;
	Buffer += TEXT("(");
	SourceValue.ToExportedString(Buffer);
	if (Suffix == CustomSuffix)
	{
		Buffer += TEXT(", ");
		Buffer += CustomOptions;
	}
	Buffer += TEXT(", \"");
	if (TargetCulture)
	{
		Buffer += TargetCulture->GetName().ReplaceCharWithEscapedChar();
	}
	Buffer += TEXT("\")");
}

const TCHAR* ReadNumberOrPercentFromBuffer(const TCHAR* Buffer, const FString& TokenMarker, FFormatArgumentValue& OutSourceValue, TOptional<FNumberFormattingOptions>& OutFormatOptions, FCulturePtr& OutTargetCulture)
{
	if (PeekMarker(Buffer, *TokenMarker, TokenMarker.Len()))
	{
		// Parsing something of the form: LOCGEN_NUMBER/_GROUPED/_UNGROUPED(..., "...") or LOCGEN_NUMBER_CUSTOM(..., ..., "...")
		// Parsing something of the form: LOCGEN_PERCENT/_GROUPED/_UNGROUPED(..., "...") or LOCGEN_PERCENT_CUSTOM(..., ..., "...")
		Buffer += TokenMarker.Len();

		const bool bIsCustom = TEXT_STRINGIFICATION_PEEK_MARKER(CustomSuffix);
		if (bIsCustom)
		{
			TEXT_STRINGIFICATION_SKIP_MARKER_LEN(CustomSuffix);
		}
		else if (TEXT_STRINGIFICATION_PEEK_MARKER(GroupedSuffix))
		{
			TEXT_STRINGIFICATION_SKIP_MARKER_LEN(GroupedSuffix);
			OutFormatOptions = FNumberFormattingOptions::DefaultWithGrouping();
		}
		else if (TEXT_STRINGIFICATION_PEEK_MARKER(UngroupedSuffix))
		{
			TEXT_STRINGIFICATION_SKIP_MARKER_LEN(UngroupedSuffix);
			OutFormatOptions = FNumberFormattingOptions::DefaultNoGrouping();
		}
		else
		{
			OutFormatOptions.Reset();
		}

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read out the number
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_NUMBER(OutSourceValue);

		if (bIsCustom)
		{
			// Skip whitespace before the comma, and then step over it
			TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

			// Skip any whitespace before the value, and then read the custom format options
			FNumberFormattingOptions LocalFormatOptions;
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(ReadNumberFormattingOptionsFromBuffer, LocalFormatOptions);
			OutFormatOptions = MoveTemp(LocalFormatOptions);
		}

		// Skip whitespace before the comma, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

		// Skip whitespace before the value, and then read out the quoted culture name
		FString CultureNameString;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(CultureNameString);
		OutTargetCulture = (CultureNameString.IsEmpty()) ? nullptr : FInternationalization::Get().GetCulture(CultureNameString);

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

		return Buffer;
	}

	return nullptr;
}

void WriteDateTimeToBuffer(FString& Buffer, const TCHAR* TokenMarker, const FDateTime& DateTime, const EDateTimeStyle::Type* DateStylePtr, const EDateTimeStyle::Type* TimeStylePtr, const FString& TimeZone, FCulturePtr TargetCulture)
{
	auto WriteDateTimeStyle = [](FString& OutValueBuffer, const EDateTimeStyle::Type& InValue)
	{
		WriteScopedEnumToBuffer(OutValueBuffer, TEXT("EDateTimeStyle::"), InValue);
	};

	FString Suffix;
	if (TimeZone == FText::GetInvariantTimeZone())
	{
		Suffix = LocalSuffix;
	}
	else
	{
		Suffix = UtcSuffix;
	}

	// Produces LOCGEN_DATE_UTC(..., ..., "...", "...") or LOCGEN_DATE_LOCAL(..., ..., "...")
	// Produces LOCGEN_TIME_UTC(..., ..., "...", "...") or LOCGEN_TIME_LOCAL(..., ..., "...")
	// Produces LOCGEN_DATETIME_UTC(..., ..., ..., "...", "...") or LOCGEN_DATETIME_LOCAL(..., ..., ..., "...")
	Buffer += TokenMarker;
	Buffer += Suffix;
	Buffer += TEXT("(");
	FFormatArgumentValue(DateTime.ToUnixTimestamp()).ToExportedString(Buffer);
	if (DateStylePtr)
	{
		Buffer += TEXT(", ");
		WriteDateTimeStyle(Buffer, *DateStylePtr);
	}
	if (TimeStylePtr)
	{
		Buffer += TEXT(", ");
		WriteDateTimeStyle(Buffer, *TimeStylePtr);
	}
	if (Suffix == UtcSuffix)
	{
		Buffer += TEXT(", \"");
		Buffer += TimeZone.ReplaceCharWithEscapedChar();
		Buffer += TEXT("\"");
	}
	Buffer += TEXT(", \"");
	if (TargetCulture)
	{
		Buffer += TargetCulture->GetName().ReplaceCharWithEscapedChar();
	}
	Buffer += TEXT("\")");
}

const TCHAR* ReadDateTimeFromBuffer(const TCHAR* Buffer, const FString& TokenMarker, FDateTime& OutDateTime, EDateTimeStyle::Type* OutDateStylePtr, EDateTimeStyle::Type* OutTimeStylePtr, FString& OutTimeZone, FCulturePtr& OutTargetCulture)
{
	auto ReadDateTimeStyle = [](const TCHAR* InValueBuffer, EDateTimeStyle::Type& OutValue) -> const TCHAR*
	{
		static const FString DateTimeStyleMarker = TEXT("EDateTimeStyle::");
		return ReadScopedEnumFromBuffer(InValueBuffer, DateTimeStyleMarker, OutValue);
	};

	if (PeekMarker(Buffer, *TokenMarker, TokenMarker.Len()))
	{
		// Parsing something of the form: LOCGEN_DATE_UTC(..., ..., "...", "...") or LOCGEN_DATE_LOCAL(..., ..., "...")
		// Parsing something of the form: LOCGEN_TIME_UTC(..., ..., "...", "...") or LOCGEN_TIME_LOCAL(..., ..., "...")
		// Parsing something of the form: LOCGEN_DATETIME_UTC(..., ..., ..., "...", "...") or LOCGEN_DATETIME_LOCAL(..., ..., ..., "...")
		Buffer += TokenMarker.Len();

		if (TEXT_STRINGIFICATION_PEEK_MARKER(LocalSuffix))
		{
			TEXT_STRINGIFICATION_SKIP_MARKER_LEN(LocalSuffix);
			OutTimeZone = FText::GetInvariantTimeZone();
		}
		else if (TEXT_STRINGIFICATION_PEEK_MARKER(UtcSuffix))
		{
			TEXT_STRINGIFICATION_SKIP_MARKER_LEN(UtcSuffix);
			OutTimeZone.Reset();
		}
		else
		{
			return nullptr;
		}

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read out the number
		FFormatArgumentValue UnixTimestampValue;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_NUMBER(UnixTimestampValue);

		switch (UnixTimestampValue.GetType())
		{
		case EFormatArgumentType::Int:
			OutDateTime = FDateTime::FromUnixTimestamp(UnixTimestampValue.GetIntValue());
			break;
		case EFormatArgumentType::UInt:
			OutDateTime = FDateTime::FromUnixTimestamp(UnixTimestampValue.GetUIntValue());
			break;
		case EFormatArgumentType::Float:
			OutDateTime = FDateTime::FromUnixTimestamp(UnixTimestampValue.GetFloatValue());
			break;
		case EFormatArgumentType::Double:
			OutDateTime = FDateTime::FromUnixTimestamp(UnixTimestampValue.GetDoubleValue());
			break;
		default:
			return nullptr;
		}

		if (OutDateStylePtr)
		{
			// Skip whitespace before the comma, then step over it
			TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

			// Skip any whitespace before the value, and then read the date style
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(ReadDateTimeStyle, *OutDateStylePtr);
		}

		if (OutTimeStylePtr)
		{
			// Skip whitespace before the comma, then step over it
			TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

			// Skip any whitespace before the value, and then read the time style
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(ReadDateTimeStyle, *OutTimeStylePtr);
		}

		if (OutTimeZone.IsEmpty())
		{
			// Skip whitespace before the comma, and then step over it
			TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

			// Skip whitespace before the value, and then read out the quoted timezone name
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			TEXT_STRINGIFICATION_READ_QUOTED_STRING(OutTimeZone);
		}

		// Skip whitespace before the comma, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

		// Skip whitespace before the value, and then read out the quoted culture name
		FString CultureNameString;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(CultureNameString);
		OutTargetCulture = (CultureNameString.IsEmpty()) ? nullptr : FInternationalization::Get().GetCulture(CultureNameString);

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

		return Buffer;
	}

	return nullptr;
}

typedef TFunctionRef<void(const FString*, const FFormatArgumentValue&)> FTextFormatArgumentEnumeratorCallback;
void WriteTextFormatToBuffer(FString& Buffer, const FString& TokenMarker, const FTextFormat& SourceFmt, TFunctionRef<void(FTextFormatArgumentEnumeratorCallback)> ArgumentEnumerator)
{
	// Produces LOCGEN_FORMAT_NAMED(..., [...]) or LOCGEN_FORMAT_ORDERED(..., [...])
	Buffer += TokenMarker;
	Buffer += TEXT("(");
	FTextStringHelper::WriteToBuffer(Buffer, SourceFmt.GetSourceText(), true);
	ArgumentEnumerator([&Buffer](const FString* InKey, const FFormatArgumentValue& InValue)
	{
		if (InKey)
		{
			Buffer += TEXT(", \"");
			Buffer += *InKey;
			Buffer += TEXT("\"");
		}

		Buffer += TEXT(", ");
		InValue.ToExportedString(Buffer);
	});
	Buffer += TEXT(")");
}

}	// namespace TextStringificationUtil

///////////////////////////////////////
// FTextHistory

/** Base class for all FText history types */

FTextHistory::FTextHistory()
	: Revision(FTextLocalizationManager::Get().GetTextRevision())
{
}

FTextHistory::FTextHistory(FTextHistory&& Other)
	: Revision(MoveTemp(Other.Revision))
{
}

FTextHistory& FTextHistory::operator=(FTextHistory&& Other)
{
	if (this != &Other)
	{
		Revision = Other.Revision;
	}
	return *this;
}

bool FTextHistory::IsOutOfDate() const
{
	return Revision != FTextLocalizationManager::Get().GetTextRevision();
}

const FString* FTextHistory::GetSourceString() const
{
	return NULL;
}

void FTextHistory::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
}

bool FTextHistory::GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const
{
	return false;
}

void FTextHistory::SerializeForDisplayString(FStructuredArchive::FRecord Record, FTextDisplayStringPtr& InOutDisplayString)
{
	if(Record.GetUnderlyingArchive().IsLoading())
	{
		PrepareDisplayStringForRebuild(InOutDisplayString);
	}
}

void FTextHistory::PrepareDisplayStringForRebuild(FTextDisplayStringPtr& OutDisplayString)
{
	// We will definitely need to do a rebuild later
	Revision = 0;

	//When duplicating, the CDO is used as the template, then values for the instance are assigned.
	//If we don't duplicate the string, the CDO and the instance are both pointing at the same thing.
	//This would result in all subsequently duplicated objects stamping over formerly duplicated ones.
	OutDisplayString = MakeShared<FString, ESPMode::ThreadSafe>();
}

bool FTextHistory::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return false;
}

const TCHAR* FTextHistory::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	return nullptr;
}

bool FTextHistory::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	return false;
}

void FTextHistory::Rebuild(TSharedRef< FString, ESPMode::ThreadSafe > InDisplayString)
{
	const bool bIsOutOfDate = IsOutOfDate();
	if(bIsOutOfDate)
	{
		// FTextHistory_Base will never report being able to rebuild its text, but we need to keep the history 
		// revision in sync with the head culture so that FTextSnapshot::IdenticalTo still works correctly
		Revision = FTextLocalizationManager::Get().GetTextRevision();

		const bool bCanRebuildLocalizedDisplayString = CanRebuildLocalizedDisplayString();
		if(bCanRebuildLocalizedDisplayString)
		{
			InDisplayString.Get() = BuildLocalizedDisplayString();
		}
	}
}

///////////////////////////////////////
// FTextHistory_Base

FTextHistory_Base::FTextHistory_Base(FString&& InSourceString)
	: SourceString(MoveTemp(InSourceString))
{
}

FTextHistory_Base::FTextHistory_Base(FTextHistory_Base&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceString(MoveTemp(Other.SourceString))
{
}

FTextHistory_Base& FTextHistory_Base::operator=(FTextHistory_Base&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceString = MoveTemp(Other.SourceString);
	}
	return *this;
}

FString FTextHistory_Base::BuildLocalizedDisplayString() const
{
	// This should never be called for base text (CanRebuildLocalizedDisplayString is false)
	check(0);
	return FString();
}

FString FTextHistory_Base::BuildInvariantDisplayString() const
{
	return SourceString;
}

const FString* FTextHistory_Base::GetSourceString() const
{
	return &SourceString;
}

void FTextHistory_Base::Serialize(FStructuredArchive::FRecord Record)
{
	// If I serialize out the Namespace and Key HERE, then we can load it up.
	if(Record.GetUnderlyingArchive().IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::Base;
		Record << NAMED_FIELD(HistoryType);
	}
}

void FTextHistory_Base::SerializeForDisplayString(FStructuredArchive::FRecord Record, FTextDisplayStringPtr& InOutDisplayString)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = 0;

		FString Namespace;
		FString Key;

		Record << NAMED_FIELD(Namespace);
		Record << NAMED_FIELD(Key);
		Record << NAMED_FIELD(SourceString);

#if USE_STABLE_LOCALIZATION_KEYS
		// Make sure the package namespace for this text property is up-to-date
		// We do this on load (as well as save) to handle cases where data is being duplicated, as it will be written by one package and loaded into another
		if (GIsEditor && !Record.GetUnderlyingArchive().HasAnyPortFlags(PPF_DuplicateVerbatim | PPF_DuplicateForPIE))
		{
			const FString PackageNamespace = TextNamespaceUtil::GetPackageNamespace(BaseArchive);
			if (!PackageNamespace.IsEmpty())
			{
				const FString FullNamespace = TextNamespaceUtil::BuildFullNamespace(Namespace, PackageNamespace);
				if (!Namespace.Equals(FullNamespace, ESearchCase::CaseSensitive))
				{
					// We may assign a new key when loading if we don't have the correct package namespace in order to avoid identity conflicts when instancing (which duplicates without any special flags)
					// This can happen if an asset was duplicated (and keeps the same keys) but later both assets are instanced into the same world (causing them to both take the worlds package id, and conflict with each other)
					Namespace = FullNamespace;
					Key = FGuid::NewGuid().ToString();
				}
			}
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
#if WITH_EDITOR
		if (!GIsEditor)
		{
			// Strip the package localization ID to match how text works at runtime (properties do this when saving during cook)
			TextNamespaceUtil::StripPackageNamespaceInline(Namespace);
		}
#endif // WITH_EDITOR

		// Using the deserialized namespace and key, find the DisplayString.
		InOutDisplayString = FTextLocalizationManager::Get().GetDisplayString(Namespace, Key, &SourceString);
	}
	else if(BaseArchive.IsSaving())
	{
		check(InOutDisplayString.IsValid());

		FString Namespace;
		FString Key;
		const bool bFoundNamespaceAndKey = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(InOutDisplayString.ToSharedRef(), Namespace, Key);

		if (BaseArchive.IsCooking())
		{
			// We strip the package localization off the serialized text for a cooked game, as they're not used at runtime
			TextNamespaceUtil::StripPackageNamespaceInline(Namespace);
		}
		else
#if USE_STABLE_LOCALIZATION_KEYS
		// Make sure the package namespace for this text property is up-to-date
		if (GIsEditor && !BaseArchive.HasAnyPortFlags(PPF_DuplicateVerbatim | PPF_DuplicateForPIE))
		{
			const FString PackageNamespace = TextNamespaceUtil::GetPackageNamespace(BaseArchive);
			if (!PackageNamespace.IsEmpty())
			{
				const FString FullNamespace = TextNamespaceUtil::BuildFullNamespace(Namespace, PackageNamespace);
				if (!Namespace.Equals(FullNamespace, ESearchCase::CaseSensitive))
				{
					// We may assign a new key when saving if we don't have the correct package namespace in order to avoid identity conflicts when instancing (which duplicates without any special flags)
					// This can happen if an asset was duplicated (and keeps the same keys) but later both assets are instanced into the same world (causing them to both take the worlds package id, and conflict with each other)
					Namespace = FullNamespace;
					Key = FGuid::NewGuid().ToString();
				}
			}
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		// If this has no key, give it a GUID for a key
		if (!bFoundNamespaceAndKey && GIsEditor && (BaseArchive.IsPersistent() && !BaseArchive.HasAnyPortFlags(PPF_Duplicate)))
		{
			Key = FGuid::NewGuid().ToString();
			if (!FTextLocalizationManager::Get().AddDisplayString(InOutDisplayString.ToSharedRef(), Namespace, Key))
			{
				// Could not add display string, reset namespace and key.
				Namespace.Empty();
				Key.Empty();
			}
		}

		// Serialize the Namespace
		Record << NAMED_FIELD(Namespace);

		// Serialize the Key
		Record << NAMED_FIELD(Key);

		// Serialize the SourceString
		Record << NAMED_FIELD(SourceString);
	}
}

bool FTextHistory_Base::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::NsLocTextMarker)
		|| TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocTextMarker);
}

const TCHAR* FTextHistory_Base::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
#define LOC_DEFINE_REGION
	if (TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::NsLocTextMarker))
	{
		// Parsing something of the form: NSLOCTEXT("...", "...", "...")
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextStringificationUtil::NsLocTextMarker);

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read out the quoted namespace
		FString NamespaceString;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(NamespaceString);

		// Skip whitespace before the comma, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

		// Skip whitespace before the value, and then read out the quoted key
		FString KeyString;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(KeyString);

		// Skip whitespace before the comma, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

		// Skip whitespace before the value, and then read out the quoted source string
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(SourceString);

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

		if (KeyString.IsEmpty())
		{
			KeyString = FGuid::NewGuid().ToString();
		}

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor && PackageNamespace && *PackageNamespace)
		{
			const FString FullNamespace = TextNamespaceUtil::BuildFullNamespace(NamespaceString, PackageNamespace);
			if (!NamespaceString.Equals(FullNamespace, ESearchCase::CaseSensitive))
			{
				// We may assign a new key when importing if we don't have the correct package namespace in order to avoid identity conflicts when instancing (which duplicates without any special flags)
				// This can happen if an asset was duplicated (and keeps the same keys) but later both assets are instanced into the same world (causing them to both take the worlds package id, and conflict with each other)
				NamespaceString = FullNamespace;
				KeyString = FGuid::NewGuid().ToString();
			}
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
		if (!GIsEditor)
		{
			// Strip the package localization ID to match how text works at runtime (properties do this when saving during cook)
			TextNamespaceUtil::StripPackageNamespaceInline(NamespaceString);
		}
		OutDisplayString = FTextLocalizationManager::Get().GetDisplayString(NamespaceString, KeyString, &SourceString);

		// We will definitely need to do a rebuild later
		Revision = 0;

		return Buffer;
	}
	
	if (TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocTextMarker))
	{
		// Parsing something of the form: LOCTEXT("...", "...")
		// This only exists as people sometimes do this in config files. We assume an empty namespace should be used
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextStringificationUtil::LocTextMarker);

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read out the quoted key
		FString KeyString;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(KeyString);

		// Skip whitespace before the comma, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

		// Skip whitespace before the value, and then read out the quoted source string
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(SourceString);

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

		if (KeyString.IsEmpty())
		{
			KeyString = FGuid::NewGuid().ToString();
		}

		FString NamespaceString = (TextNamespace) ? TextNamespace : FString();
#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor && PackageNamespace && *PackageNamespace)
		{
			const FString FullNamespace = TextNamespaceUtil::BuildFullNamespace(NamespaceString, PackageNamespace);
			if (!NamespaceString.Equals(FullNamespace, ESearchCase::CaseSensitive))
			{
				// We may assign a new key when importing if we don't have the correct package namespace in order to avoid identity conflicts when instancing (which duplicates without any special flags)
				// This can happen if an asset was duplicated (and keeps the same keys) but later both assets are instanced into the same world (causing them to both take the worlds package id, and conflict with each other)
				NamespaceString = FullNamespace;
				KeyString = FGuid::NewGuid().ToString();
			}
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
		if (!GIsEditor)
		{
			// Strip the package localization ID to match how text works at runtime (properties do this when saving during cook)
			TextNamespaceUtil::StripPackageNamespaceInline(NamespaceString);
		}
		OutDisplayString = FTextLocalizationManager::Get().GetDisplayString(NamespaceString, KeyString, &SourceString);

		// We will definitely need to do a rebuild later
		Revision = 0;

		return Buffer;
	}
#undef LOC_DEFINE_REGION

	return nullptr;
}

bool FTextHistory_Base::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	FString Namespace;
	FString Key;
	const bool bFoundNamespaceAndKey = DisplayString.IsValid() && FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(DisplayString.ToSharedRef(), Namespace, Key);

	if (bFoundNamespaceAndKey)
	{
#define LOC_DEFINE_REGION
		// Produces NSLOCTEXT("...", "...", "...")
		Buffer += TEXT("NSLOCTEXT(\"");
		Buffer += Namespace.ReplaceCharWithEscapedChar();
		Buffer += TEXT("\", \"");
		Buffer += Key.ReplaceCharWithEscapedChar();
		Buffer += TEXT("\", \"");
		Buffer += SourceString.ReplaceCharWithEscapedChar();
		Buffer += TEXT("\")");
#undef LOC_DEFINE_REGION

		return true;
	}

	return false;
}

///////////////////////////////////////
// FTextHistory_NamedFormat

FTextHistory_NamedFormat::FTextHistory_NamedFormat(FTextFormat&& InSourceFmt, FFormatNamedArguments&& InArguments)
	: SourceFmt(MoveTemp(InSourceFmt))
	, Arguments(MoveTemp(InArguments))
{
}

FTextHistory_NamedFormat::FTextHistory_NamedFormat(FTextHistory_NamedFormat&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceFmt(MoveTemp(Other.SourceFmt))
	, Arguments(MoveTemp(Other.Arguments))
{
}

FTextHistory_NamedFormat& FTextHistory_NamedFormat::operator=(FTextHistory_NamedFormat&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceFmt = MoveTemp(Other.SourceFmt);
		Arguments = MoveTemp(Other.Arguments);
	}
	return *this;
}

FString FTextHistory_NamedFormat::BuildLocalizedDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, false);
}

FString FTextHistory_NamedFormat::BuildInvariantDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, true);
}

void FTextHistory_NamedFormat::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::NamedFormat;
		Record << NAMED_FIELD(HistoryType);
	}

	if (BaseArchive.IsSaving())
	{
		FText FormatText = SourceFmt.GetSourceText();
		Record.EnterField(FIELD_NAME_TEXT("FormatText")) << FormatText;
	}
	else if (BaseArchive.IsLoading())
	{
		FText FormatText;
		Record.EnterField(FIELD_NAME_TEXT("FormatText")) << FormatText;
		SourceFmt = FTextFormat(FormatText);
	}

	Record << NAMED_FIELD(Arguments);
}

bool FTextHistory_NamedFormat::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenFormatNamedMarker);
}

const TCHAR* FTextHistory_NamedFormat::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	if (TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenFormatNamedMarker))
	{
		// Parsing something of the form: LOCGEN_FORMAT_NAMED(..., [...])
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextStringificationUtil::LocGenFormatNamedMarker);

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read out the text
		FText FormatText;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(FTextStringHelper::ReadFromBuffer, FormatText, nullptr, nullptr, true);
		SourceFmt = FTextFormat(FormatText);

		// Read out arguments until we run out
		Arguments.Reset();
		for (;;)
		{
			// Skip whitespace and see if we've found a comma (for another argument)
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			if (*Buffer != TEXT(','))
			{
				// Finished parsing
				break;
			}

			// Step over the comma
			++Buffer;

			// Skip whitespace before the value, and then read out the quoted argument name
			FString ArgumentName;
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			TEXT_STRINGIFICATION_READ_QUOTED_STRING(ArgumentName);

			// Skip whitespace before the comma, then step over it
			TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

			// Skip whitespace before the value, and then read the new argument
			FFormatArgumentValue& ArgumentValue = Arguments.Add(MoveTemp(ArgumentName));
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(ArgumentValue.FromExportedString);
		}

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

		PrepareDisplayStringForRebuild(OutDisplayString);
		return Buffer;
	}

	return nullptr;
}

bool FTextHistory_NamedFormat::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	TextStringificationUtil::WriteTextFormatToBuffer(Buffer, TextStringificationUtil::LocGenFormatNamedMarker, SourceFmt, [this](TextStringificationUtil::FTextFormatArgumentEnumeratorCallback Callback)
	{
		for (const auto& ArgumentPair : Arguments)
		{
			Callback(&ArgumentPair.Key, ArgumentPair.Value);
		}
	});
	return true;
}

void FTextHistory_NamedFormat::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
	// Process the formatting text in-case it's a recursive format
	FTextInspector::GetHistoricFormatData(SourceFmt.GetSourceText(), OutHistoricFormatData);

	for (auto It = Arguments.CreateConstIterator(); It; ++It)
	{
		const FFormatArgumentValue& ArgumentValue = It.Value();
		if (ArgumentValue.GetType() == EFormatArgumentType::Text)
		{
			// Process the text argument in-case it's a recursive format
			FTextInspector::GetHistoricFormatData(ArgumentValue.GetTextValue(), OutHistoricFormatData);
		}
	}

	// Add ourself now that we've processed any format dependencies
	OutHistoricFormatData.Emplace(InText, FTextFormat(SourceFmt), FFormatNamedArguments(Arguments));
}

///////////////////////////////////////
// FTextHistory_OrderedFormat

FTextHistory_OrderedFormat::FTextHistory_OrderedFormat(FTextFormat&& InSourceFmt, FFormatOrderedArguments&& InArguments)
	: SourceFmt(MoveTemp(InSourceFmt))
	, Arguments(MoveTemp(InArguments))
{
}

FTextHistory_OrderedFormat::FTextHistory_OrderedFormat(FTextHistory_OrderedFormat&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceFmt(MoveTemp(Other.SourceFmt))
	, Arguments(MoveTemp(Other.Arguments))
{
}

FTextHistory_OrderedFormat& FTextHistory_OrderedFormat::operator=(FTextHistory_OrderedFormat&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceFmt = MoveTemp(Other.SourceFmt);
		Arguments = MoveTemp(Other.Arguments);
	}
	return *this;
}

FString FTextHistory_OrderedFormat::BuildLocalizedDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, false);
}

FString FTextHistory_OrderedFormat::BuildInvariantDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, true);
}

void FTextHistory_OrderedFormat::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::OrderedFormat;
		Record << NAMED_FIELD(HistoryType);
	}

	if (BaseArchive.IsSaving())
	{
		FText FormatText = SourceFmt.GetSourceText();
		Record.EnterField(FIELD_NAME_TEXT("FormatText")) << FormatText;
	}
	else if (BaseArchive.IsLoading())
	{
		FText FormatText;
		Record.EnterField(FIELD_NAME_TEXT("FormatText")) << FormatText;
		SourceFmt = FTextFormat(FormatText);
	}

	Record << NAMED_FIELD(Arguments);
}

bool FTextHistory_OrderedFormat::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenFormatOrderedMarker);
}

const TCHAR* FTextHistory_OrderedFormat::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	if (TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenFormatOrderedMarker))
	{
		// Parsing something of the form: LOCGEN_FORMAT_ORDERED(..., [...])
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextStringificationUtil::LocGenFormatOrderedMarker);

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read out the text
		FText FormatText;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(FTextStringHelper::ReadFromBuffer, FormatText, nullptr, nullptr, true);
		SourceFmt = FTextFormat(FormatText);

		// Read out arguments until we run out
		Arguments.Reset();
		for (;;)
		{
			// Skip whitespace and see if we've found a comma (for another argument)
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			if (*Buffer != TEXT(','))
			{
				// Finished parsing
				break;
			}

			// Step over the comma
			++Buffer;

			// Skip whitespace before the value, and then read the new argument
			FFormatArgumentValue& ArgumentValue = Arguments.AddDefaulted_GetRef();
			TEXT_STRINGIFICATION_SKIP_WHITESPACE();
			TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(ArgumentValue.FromExportedString);
		}

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

		PrepareDisplayStringForRebuild(OutDisplayString);
		return Buffer;
	}

	return nullptr;
}

bool FTextHistory_OrderedFormat::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	TextStringificationUtil::WriteTextFormatToBuffer(Buffer, TextStringificationUtil::LocGenFormatOrderedMarker, SourceFmt, [this](TextStringificationUtil::FTextFormatArgumentEnumeratorCallback Callback)
	{
		for (const FFormatArgumentValue& ArgumentValue : Arguments)
		{
			Callback(nullptr, ArgumentValue);
		}
	});
	return true;
}

void FTextHistory_OrderedFormat::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
	// Process the formatting text in-case it's a recursive format
	FTextInspector::GetHistoricFormatData(SourceFmt.GetSourceText(), OutHistoricFormatData);

	for (auto It = Arguments.CreateConstIterator(); It; ++It)
	{
		const FFormatArgumentValue& ArgumentValue = *It;
		if (ArgumentValue.GetType() == EFormatArgumentType::Text)
		{
			// Process the text argument in-case it's a recursive format
			FTextInspector::GetHistoricFormatData(ArgumentValue.GetTextValue(), OutHistoricFormatData);
		}
	}

	// Add ourself now that we've processed any format dependencies
	FFormatNamedArguments NamedArgs;
	NamedArgs.Reserve(Arguments.Num());
	for (int32 ArgIndex = 0; ArgIndex < Arguments.Num(); ++ArgIndex)
	{
		const FFormatArgumentValue& ArgumentValue = Arguments[ArgIndex];
		NamedArgs.Emplace(FString::FromInt(ArgIndex), ArgumentValue);
	}
	OutHistoricFormatData.Emplace(InText, FTextFormat(SourceFmt), MoveTemp(NamedArgs));
}

///////////////////////////////////////
// FTextHistory_ArgumentDataFormat

FTextHistory_ArgumentDataFormat::FTextHistory_ArgumentDataFormat(FTextFormat&& InSourceFmt, TArray<FFormatArgumentData>&& InArguments)
	: SourceFmt(MoveTemp(InSourceFmt))
	, Arguments(MoveTemp(InArguments))
{
}

FTextHistory_ArgumentDataFormat::FTextHistory_ArgumentDataFormat(FTextHistory_ArgumentDataFormat&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceFmt(MoveTemp(Other.SourceFmt))
	, Arguments(MoveTemp(Other.Arguments))
{
}

FTextHistory_ArgumentDataFormat& FTextHistory_ArgumentDataFormat::operator=(FTextHistory_ArgumentDataFormat&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceFmt = MoveTemp(Other.SourceFmt);
		Arguments = MoveTemp(Other.Arguments);
	}
	return *this;
}

FString FTextHistory_ArgumentDataFormat::BuildLocalizedDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, false);
}

FString FTextHistory_ArgumentDataFormat::BuildInvariantDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, true);
}

void FTextHistory_ArgumentDataFormat::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::ArgumentFormat;
		Record << NAMED_FIELD(HistoryType);
	}

	if (BaseArchive.IsSaving())
	{
		FText FormatText = SourceFmt.GetSourceText();
		Record.EnterField(FIELD_NAME_TEXT("FormatText")) << FormatText;
	}
	else if (BaseArchive.IsLoading())
	{
		FText FormatText;
		Record.EnterField(FIELD_NAME_TEXT("FormatText")) << FormatText;
		SourceFmt = FTextFormat(FormatText);
	}

	Record << NAMED_FIELD(Arguments);
}

bool FTextHistory_ArgumentDataFormat::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return false;
}

const TCHAR* FTextHistory_ArgumentDataFormat::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	return nullptr;
}

bool FTextHistory_ArgumentDataFormat::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	TextStringificationUtil::WriteTextFormatToBuffer(Buffer, TextStringificationUtil::LocGenFormatNamedMarker, SourceFmt, [this](TextStringificationUtil::FTextFormatArgumentEnumeratorCallback Callback)
	{
		for (const FFormatArgumentData& Argument : Arguments)
		{
			Callback(&Argument.ArgumentName, Argument.ToArgumentValue());
		}
	});
	return true;
}

void FTextHistory_ArgumentDataFormat::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
	// Process the formatting text in-case it's a recursive format
	FTextInspector::GetHistoricFormatData(SourceFmt.GetSourceText(), OutHistoricFormatData);

	for (const FFormatArgumentData& ArgumentData : Arguments)
	{
		if (ArgumentData.ArgumentValueType == EFormatArgumentType::Text)
		{
			// Process the text argument in-case it's a recursive format
			FTextInspector::GetHistoricFormatData(ArgumentData.ArgumentValue, OutHistoricFormatData);
		}
	}

	// Add ourself now that we've processed any format dependencies
	FFormatNamedArguments NamedArgs;
	NamedArgs.Reserve(Arguments.Num());
	for (const FFormatArgumentData& ArgumentData : Arguments)
	{
		FFormatArgumentValue ArgumentValue;
		switch (ArgumentData.ArgumentValueType)
		{
		case EFormatArgumentType::Int:
			ArgumentValue = FFormatArgumentValue(ArgumentData.ArgumentValueInt);
			break;
		case EFormatArgumentType::Float:
			ArgumentValue = FFormatArgumentValue(ArgumentData.ArgumentValueFloat);
			break;
		case EFormatArgumentType::Gender:
			ArgumentValue = FFormatArgumentValue(ArgumentData.ArgumentValueGender);
			break;
		default:
			ArgumentValue = FFormatArgumentValue(ArgumentData.ArgumentValue);
			break;
		}

		NamedArgs.Emplace(ArgumentData.ArgumentName, MoveTemp(ArgumentValue));
	}
	OutHistoricFormatData.Emplace(InText, FTextFormat(SourceFmt), MoveTemp(NamedArgs));
}

///////////////////////////////////////
// FTextHistory_FormatNumber

FTextHistory_FormatNumber::FTextHistory_FormatNumber(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture)
	: SourceValue(MoveTemp(InSourceValue))
	, FormatOptions()
	, TargetCulture(MoveTemp(InTargetCulture))
{
	if(InFormatOptions)
	{
		FormatOptions = *InFormatOptions;
	}
}

FTextHistory_FormatNumber::FTextHistory_FormatNumber(FTextHistory_FormatNumber&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceValue(MoveTemp(Other.SourceValue))
	, FormatOptions(MoveTemp(Other.FormatOptions))
	, TargetCulture(MoveTemp(Other.TargetCulture))
{
}

FTextHistory_FormatNumber& FTextHistory_FormatNumber::operator=(FTextHistory_FormatNumber&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceValue = MoveTemp(Other.SourceValue);
		FormatOptions = MoveTemp(Other.FormatOptions);
		TargetCulture = MoveTemp(Other.TargetCulture);
	}
	return *this;
}

void FTextHistory_FormatNumber::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	Record << NAMED_FIELD(SourceValue);

	bool bHasFormatOptions = FormatOptions.IsSet();
	Record << NAMED_FIELD(bHasFormatOptions);

	if(BaseArchive.IsLoading())
	{
		if(bHasFormatOptions)
		{
			FormatOptions = FNumberFormattingOptions();
		}
		else
		{
			FormatOptions.Reset();
		}
	}
	if(bHasFormatOptions)
	{
		check(FormatOptions.IsSet());
		FNumberFormattingOptions& Options = FormatOptions.GetValue();
		Record << NAMED_FIELD(Options);
	}

	if(BaseArchive.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Record << NAMED_FIELD(CultureName);
	}
	else if(BaseArchive.IsLoading())
	{
		FString CultureName;
		Record << NAMED_FIELD(CultureName);

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

FString FTextHistory_FormatNumber::BuildNumericDisplayString(const FDecimalNumberFormattingRules& InFormattingRules, const int32 InValueMultiplier) const
{
	check(InValueMultiplier > 0);

	const FNumberFormattingOptions& FormattingOptions = (FormatOptions.IsSet()) ? FormatOptions.GetValue() : InFormattingRules.CultureDefaultFormattingOptions;
	switch (SourceValue.GetType())
	{
	case EFormatArgumentType::Int:
		return FastDecimalFormat::NumberToString(SourceValue.GetIntValue() * static_cast<int64>(InValueMultiplier), InFormattingRules, FormattingOptions);
	case EFormatArgumentType::UInt:
		return FastDecimalFormat::NumberToString(SourceValue.GetUIntValue() * static_cast<uint64>(InValueMultiplier), InFormattingRules, FormattingOptions);
	case EFormatArgumentType::Float:
		return FastDecimalFormat::NumberToString(SourceValue.GetFloatValue() * static_cast<float>(InValueMultiplier), InFormattingRules, FormattingOptions);
	case EFormatArgumentType::Double:
		return FastDecimalFormat::NumberToString(SourceValue.GetDoubleValue() * static_cast<double>(InValueMultiplier), InFormattingRules, FormattingOptions);
	default:
		break;
	}
	return FString();
}

///////////////////////////////////////
// FTextHistory_AsNumber

FTextHistory_AsNumber::FTextHistory_AsNumber(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture)
	: FTextHistory_FormatNumber(MoveTemp(InSourceValue), InFormatOptions, MoveTemp(InTargetCulture))
{
}

FTextHistory_AsNumber::FTextHistory_AsNumber(FTextHistory_AsNumber&& Other)
	: FTextHistory_FormatNumber(MoveTemp(Other))
{
}

FTextHistory_AsNumber& FTextHistory_AsNumber::operator=(FTextHistory_AsNumber&& Other)
{
	FTextHistory_FormatNumber::operator=(MoveTemp(Other));
	return *this;
}

FString FTextHistory_AsNumber::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetDecimalNumberFormattingRules();
	return BuildNumericDisplayString(FormattingRules);
}

FString FTextHistory_AsNumber::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetDecimalNumberFormattingRules();
	return BuildNumericDisplayString(FormattingRules);
}

void FTextHistory_AsNumber::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsNumber;
		Record << NAMED_FIELD(HistoryType);
	}

	FTextHistory_FormatNumber::Serialize(Record);
}

bool FTextHistory_AsNumber::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenNumberMarker);
}

const TCHAR* FTextHistory_AsNumber::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	static const FString TokenMarker = TextStringificationUtil::LocGenNumberMarker;
	TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadNumberOrPercentFromBuffer, TokenMarker, SourceValue, FormatOptions, TargetCulture);
	PrepareDisplayStringForRebuild(OutDisplayString);
	return Buffer;
}

bool FTextHistory_AsNumber::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	TextStringificationUtil::WriteNumberOrPercentToBuffer(Buffer, TextStringificationUtil::LocGenNumberMarker, SourceValue, FormatOptions, TargetCulture);
	return true;
}

bool FTextHistory_AsNumber::GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const
{
	OutHistoricNumericData = FHistoricTextNumericData(FHistoricTextNumericData::EType::AsNumber, SourceValue, FormatOptions);
	return true;
}

///////////////////////////////////////
// FTextHistory_AsPercent

FTextHistory_AsPercent::FTextHistory_AsPercent(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture)
	: FTextHistory_FormatNumber(MoveTemp(InSourceValue), InFormatOptions, MoveTemp(InTargetCulture))
{
}

FTextHistory_AsPercent::FTextHistory_AsPercent(FTextHistory_AsPercent&& Other)
	: FTextHistory_FormatNumber(MoveTemp(Other))
{
}

FTextHistory_AsPercent& FTextHistory_AsPercent::operator=(FTextHistory_AsPercent&& Other)
{
	FTextHistory_FormatNumber::operator=(MoveTemp(Other));
	return *this;
}

FString FTextHistory_AsPercent::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetPercentFormattingRules();
	return BuildNumericDisplayString(FormattingRules, 100);
}

FString FTextHistory_AsPercent::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetPercentFormattingRules();
	return BuildNumericDisplayString(FormattingRules, 100);
}

void FTextHistory_AsPercent::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsPercent;
		Record << NAMED_FIELD(HistoryType);
	}

	FTextHistory_FormatNumber::Serialize(Record);
}

bool FTextHistory_AsPercent::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenPercentMarker);
}

const TCHAR* FTextHistory_AsPercent::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	static const FString TokenMarker = TextStringificationUtil::LocGenPercentMarker;
	TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadNumberOrPercentFromBuffer, TokenMarker, SourceValue, FormatOptions, TargetCulture);
	PrepareDisplayStringForRebuild(OutDisplayString);
	return Buffer;
}

bool FTextHistory_AsPercent::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	TextStringificationUtil::WriteNumberOrPercentToBuffer(Buffer, TextStringificationUtil::LocGenPercentMarker, SourceValue, FormatOptions, TargetCulture);
	return true;
}

bool FTextHistory_AsPercent::GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const
{
	OutHistoricNumericData = FHistoricTextNumericData(FHistoricTextNumericData::EType::AsPercent, SourceValue, FormatOptions);
	return true;
}

///////////////////////////////////////
// FTextHistory_AsCurrency

FTextHistory_AsCurrency::FTextHistory_AsCurrency(FFormatArgumentValue InSourceValue, FString InCurrencyCode, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture)
	: FTextHistory_FormatNumber(MoveTemp(InSourceValue), InFormatOptions, MoveTemp(InTargetCulture))
	, CurrencyCode(MoveTemp(InCurrencyCode))
{
}

FTextHistory_AsCurrency::FTextHistory_AsCurrency(FTextHistory_AsCurrency&& Other)
	: FTextHistory_FormatNumber(MoveTemp(Other))
	, CurrencyCode(MoveTemp(Other.CurrencyCode))
{
}

FTextHistory_AsCurrency& FTextHistory_AsCurrency::operator=(FTextHistory_AsCurrency&& Other)
{
	FTextHistory_FormatNumber::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		CurrencyCode = MoveTemp(Other.CurrencyCode);
	}
	return *this;
}

FString FTextHistory_AsCurrency::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	// when we remove AsCurrency should be easy to switch these to AsCurrencyBase and change SourceValue to be BaseVal in AsCurrencyBase (currently is the pre-divided value)
	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(CurrencyCode);
	return BuildNumericDisplayString(FormattingRules);
}

FString FTextHistory_AsCurrency::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	// when we remove AsCurrency should be easy to switch these to AsCurrencyBase and change SourceValue to be BaseVal in AsCurrencyBase (currently is the pre-divided value)
	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(CurrencyCode);
	return BuildNumericDisplayString(FormattingRules);
}

void FTextHistory_AsCurrency::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsCurrency;
		Record << NAMED_FIELD(HistoryType);
	}

	if (BaseArchive.UE4Ver() >= VER_UE4_ADDED_CURRENCY_CODE_TO_FTEXT)
	{
		Record << NAMED_FIELD(CurrencyCode);
	}

	FTextHistory_FormatNumber::Serialize(Record);
}

bool FTextHistory_AsCurrency::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenCurrencyMarker);
}

const TCHAR* FTextHistory_AsCurrency::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	if (TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenCurrencyMarker))
	{
		// Parsing something of the form: LOCGEN_CURRENCY(..., "...", "...")
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextStringificationUtil::LocGenCurrencyMarker);

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read out the number
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_NUMBER(SourceValue);

		// Skip whitespace before the comma, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

		// Skip whitespace before the value, and then read out the quoted currency name
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(CurrencyCode);

		// Skip whitespace before the comma, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

		// Skip whitespace before the value, and then read out the quoted culture name
		FString CultureNameString;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(CultureNameString);
		TargetCulture = (CultureNameString.IsEmpty()) ? nullptr : FInternationalization::Get().GetCulture(CultureNameString);

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

		// Get the "base" value as a double
		double BaseValue = 0.0;
		switch (SourceValue.GetType())
		{
		case EFormatArgumentType::Int:
			BaseValue = SourceValue.GetIntValue();
			break;
		case EFormatArgumentType::UInt:
			BaseValue = SourceValue.GetUIntValue();
			break;
		case EFormatArgumentType::Float:
			BaseValue = SourceValue.GetFloatValue();
			break;
		case EFormatArgumentType::Double:
			BaseValue = SourceValue.GetDoubleValue();
			break;
		default:
			return nullptr;
		}

		// We need to convert the "base" value back to its pre-divided version
		const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(CurrencyCode);
		const FNumberFormattingOptions& FormattingOptions = FormattingRules.CultureDefaultFormattingOptions;
		SourceValue = BaseValue / FMath::Pow(10.0f, FormattingOptions.MaximumFractionalDigits);

		PrepareDisplayStringForRebuild(OutDisplayString);
		return Buffer;
	}

	return nullptr;
}

bool FTextHistory_AsCurrency::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	// Get the pre-divided value as a double
	double DividedValue = 0.0;
	switch (SourceValue.GetType())
	{
	case EFormatArgumentType::Int:
		DividedValue = SourceValue.GetIntValue();
		break;
	case EFormatArgumentType::UInt:
		DividedValue = SourceValue.GetUIntValue();
		break;
	case EFormatArgumentType::Float:
		DividedValue = SourceValue.GetFloatValue();
		break;
	case EFormatArgumentType::Double:
		DividedValue = SourceValue.GetDoubleValue();
		break;
	default:
		break;
	}

	// We need to convert the value back to its "base" version
	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(CurrencyCode);
	const FNumberFormattingOptions& FormattingOptions = FormattingRules.CultureDefaultFormattingOptions;
	const int64 BaseVal = static_cast<int64>(DividedValue * FMath::Pow(10.0f, FormattingOptions.MaximumFractionalDigits));

	// Produces LOCGEN_CURRENCY(..., "...", "...")
	Buffer += TEXT("LOCGEN_CURRENCY(");
	FFormatArgumentValue(BaseVal).ToExportedString(Buffer);
	Buffer += TEXT(", \"");
	Buffer += CurrencyCode.ReplaceCharWithEscapedChar();
	Buffer += TEXT("\", \"");
	if (TargetCulture)
	{
		Buffer += TargetCulture->GetName().ReplaceCharWithEscapedChar();
	}
	Buffer += TEXT("\")");

	return true;
}

///////////////////////////////////////
// FTextHistory_AsDate

FTextHistory_AsDate::FTextHistory_AsDate(FDateTime InSourceDateTime, const EDateTimeStyle::Type InDateStyle, FString InTimeZone, FCulturePtr InTargetCulture)
	: SourceDateTime(MoveTemp(InSourceDateTime))
	, DateStyle(InDateStyle)
	, TimeZone(MoveTemp(InTimeZone))
	, TargetCulture(MoveTemp(InTargetCulture))
{
}

FTextHistory_AsDate::FTextHistory_AsDate(FTextHistory_AsDate&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceDateTime(MoveTemp(Other.SourceDateTime))
	, DateStyle(Other.DateStyle)
	, TimeZone(MoveTemp(Other.TimeZone))
	, TargetCulture(MoveTemp(Other.TargetCulture))
{
}

FTextHistory_AsDate& FTextHistory_AsDate::operator=(FTextHistory_AsDate&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceDateTime = MoveTemp(Other.SourceDateTime);
		DateStyle = Other.DateStyle;
		TimeZone = MoveTemp(Other.TimeZone);
		TargetCulture = MoveTemp(Other.TargetCulture);
	}
	return *this;
}

void FTextHistory_AsDate::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsDate;
		Record << NAMED_FIELD(HistoryType);
	}

	Record << NAMED_FIELD(SourceDateTime);

	int8 DateStyleInt8 = (int8)DateStyle;
	Record << NAMED_FIELD(DateStyleInt8);
	DateStyle = (EDateTimeStyle::Type)DateStyleInt8;

	if( BaseArchive.UE4Ver() >= VER_UE4_FTEXT_HISTORY_DATE_TIMEZONE )
	{
		Record << NAMED_FIELD(TimeZone);
	}

	if(BaseArchive.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Record << NAMED_FIELD(CultureName);
	}
	else if(BaseArchive.IsLoading())
	{
		FString CultureName;
		Record << NAMED_FIELD(CultureName);

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

bool FTextHistory_AsDate::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenDateMarker);
}

const TCHAR* FTextHistory_AsDate::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	static const FString TokenMarker = TextStringificationUtil::LocGenDateMarker;
	TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadDateTimeFromBuffer, TokenMarker, SourceDateTime, &DateStyle, nullptr, TimeZone, TargetCulture);
	PrepareDisplayStringForRebuild(OutDisplayString);
	return Buffer;
}

bool FTextHistory_AsDate::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	TextStringificationUtil::WriteDateTimeToBuffer(Buffer, TextStringificationUtil::LocGenDateMarker, SourceDateTime, &DateStyle, nullptr, TimeZone, TargetCulture);
	return true;
}

FString FTextHistory_AsDate::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	return FTextChronoFormatter::AsDate(SourceDateTime, DateStyle, TimeZone, Culture);
}

FString FTextHistory_AsDate::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	return FTextChronoFormatter::AsDate(SourceDateTime, DateStyle, TimeZone, Culture);
}

///////////////////////////////////////
// FTextHistory_AsTime

FTextHistory_AsTime::FTextHistory_AsTime(FDateTime InSourceDateTime, const EDateTimeStyle::Type InTimeStyle, FString InTimeZone, FCulturePtr InTargetCulture)
	: SourceDateTime(MoveTemp(InSourceDateTime))
	, TimeStyle(InTimeStyle)
	, TimeZone(MoveTemp(InTimeZone))
	, TargetCulture(MoveTemp(InTargetCulture))
{
}

FTextHistory_AsTime::FTextHistory_AsTime(FTextHistory_AsTime&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceDateTime(MoveTemp(Other.SourceDateTime))
	, TimeStyle(Other.TimeStyle)
	, TimeZone(MoveTemp(Other.TimeZone))
	, TargetCulture(MoveTemp(Other.TargetCulture))
{
}

FTextHistory_AsTime& FTextHistory_AsTime::operator=(FTextHistory_AsTime&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceDateTime = MoveTemp(Other.SourceDateTime);
		TimeStyle = Other.TimeStyle;
		TimeZone = MoveTemp(Other.TimeZone);
		TargetCulture = MoveTemp(Other.TargetCulture);
	}
	return *this;
}

void FTextHistory_AsTime::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsTime;
		Record << NAMED_FIELD(HistoryType);
	}

	Record << NAMED_FIELD(SourceDateTime);

	int8 TimeStyleInt8 = (int8)TimeStyle;
	Record << NAMED_ITEM("TimeStyle", TimeStyleInt8);
	TimeStyle = (EDateTimeStyle::Type)TimeStyleInt8;

	Record << NAMED_FIELD(TimeZone);

	if(BaseArchive.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Record << NAMED_FIELD(CultureName);
	}
	else if(BaseArchive.IsLoading())
	{
		FString CultureName;
		Record << NAMED_FIELD(CultureName);

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

bool FTextHistory_AsTime::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenTimeMarker);
}

const TCHAR* FTextHistory_AsTime::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	static const FString TokenMarker = TextStringificationUtil::LocGenTimeMarker;
	TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadDateTimeFromBuffer, TokenMarker, SourceDateTime, nullptr, &TimeStyle, TimeZone, TargetCulture);
	PrepareDisplayStringForRebuild(OutDisplayString);
	return Buffer;
}

bool FTextHistory_AsTime::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	TextStringificationUtil::WriteDateTimeToBuffer(Buffer, TextStringificationUtil::LocGenTimeMarker, SourceDateTime, nullptr, &TimeStyle, TimeZone, TargetCulture);
	return true;
}

FString FTextHistory_AsTime::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	return FTextChronoFormatter::AsTime(SourceDateTime, TimeStyle, TimeZone, Culture);
}

FString FTextHistory_AsTime::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	return FTextChronoFormatter::AsTime(SourceDateTime, TimeStyle, TimeZone, Culture);
}

///////////////////////////////////////
// FTextHistory_AsDateTime

FTextHistory_AsDateTime::FTextHistory_AsDateTime(FDateTime InSourceDateTime, const EDateTimeStyle::Type InDateStyle, const EDateTimeStyle::Type InTimeStyle, FString InTimeZone, FCulturePtr InTargetCulture)
	: SourceDateTime(MoveTemp(InSourceDateTime))
	, DateStyle(InDateStyle)
	, TimeStyle(InTimeStyle)
	, TimeZone(MoveTemp(InTimeZone))
	, TargetCulture(MoveTemp(InTargetCulture))
{
}

FTextHistory_AsDateTime::FTextHistory_AsDateTime(FTextHistory_AsDateTime&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceDateTime(MoveTemp(Other.SourceDateTime))
	, DateStyle(Other.DateStyle)
	, TimeStyle(Other.TimeStyle)
	, TimeZone(MoveTemp(Other.TimeZone))
	, TargetCulture(MoveTemp(Other.TargetCulture))
{
}

FTextHistory_AsDateTime& FTextHistory_AsDateTime::operator=(FTextHistory_AsDateTime&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceDateTime = MoveTemp(Other.SourceDateTime);
		DateStyle = Other.DateStyle;
		TimeStyle = Other.TimeStyle;
		TimeZone = MoveTemp(Other.TimeZone);
		TargetCulture = MoveTemp(Other.TargetCulture);
	}
	return *this;
}

void FTextHistory_AsDateTime::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if(BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsDateTime;
		Record << NAMED_FIELD(HistoryType);
	}

	Record << NAMED_FIELD(SourceDateTime);

	int8 DateStyleInt8 = (int8)DateStyle;
	Record << NAMED_ITEM("DateStyle", DateStyleInt8);
	DateStyle = (EDateTimeStyle::Type)DateStyleInt8;

	int8 TimeStyleInt8 = (int8)TimeStyle;
	Record << NAMED_ITEM("TimeStyle", TimeStyleInt8);
	TimeStyle = (EDateTimeStyle::Type)TimeStyleInt8;

	Record << NAMED_FIELD(TimeZone);

	if(BaseArchive.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Record << NAMED_FIELD(CultureName);
	}
	else if(BaseArchive.IsLoading())
	{
		FString CultureName;
		Record << NAMED_FIELD(CultureName);

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

bool FTextHistory_AsDateTime::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenDateTimeMarker);
}

const TCHAR* FTextHistory_AsDateTime::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	static const FString TokenMarker = TextStringificationUtil::LocGenDateTimeMarker;
	TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadDateTimeFromBuffer, TokenMarker, SourceDateTime, &DateStyle, &TimeStyle, TimeZone, TargetCulture);
	PrepareDisplayStringForRebuild(OutDisplayString);
	return Buffer;
}

bool FTextHistory_AsDateTime::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	TextStringificationUtil::WriteDateTimeToBuffer(Buffer, TextStringificationUtil::LocGenDateTimeMarker, SourceDateTime, &DateStyle, &TimeStyle, TimeZone, TargetCulture);
	return true;
}

FString FTextHistory_AsDateTime::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	return FTextChronoFormatter::AsDateTime(SourceDateTime, DateStyle, TimeStyle, TimeZone, Culture);
}

FString FTextHistory_AsDateTime::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	return FTextChronoFormatter::AsDateTime(SourceDateTime, DateStyle, TimeStyle, TimeZone, Culture);
}

///////////////////////////////////////
// FTextHistory_Transform

FTextHistory_Transform::FTextHistory_Transform(FText InSourceText, const ETransformType InTransformType)
	: SourceText(MoveTemp(InSourceText))
	, TransformType(InTransformType)
{
}

FTextHistory_Transform::FTextHistory_Transform(FTextHistory_Transform&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceText(MoveTemp(Other.SourceText))
	, TransformType(Other.TransformType)
{
}

FTextHistory_Transform& FTextHistory_Transform::operator=(FTextHistory_Transform&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceText = MoveTemp(Other.SourceText);
		TransformType = Other.TransformType;
	}
	return *this;
}

void FTextHistory_Transform::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if (BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::Transform;
		Record << NAMED_FIELD(HistoryType);
	}

	Record << NAMED_FIELD(SourceText);

	uint8& TransformTypeRef = (uint8&)TransformType;
	Record << NAMED_ITEM("TransformType", TransformTypeRef);
}

bool FTextHistory_Transform::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenToLowerMarker)
		|| TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenToUpperMarker);
}

const TCHAR* FTextHistory_Transform::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
	// Parsing something of the form: LOCGEN_TOLOWER(...) or LOCGEN_TOUPPER
	if (TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenToLowerMarker))
	{
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextStringificationUtil::LocGenToLowerMarker);
		TransformType = ETransformType::ToLower;
	}
	else if (TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocGenToUpperMarker))
	{
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextStringificationUtil::LocGenToUpperMarker);
		TransformType = ETransformType::ToUpper;
	}
	else
	{
		return nullptr;
	}

	// Skip whitespace before the opening bracket, and then step over it
	TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

	// Skip whitespace before the value, and then read out the text
	TEXT_STRINGIFICATION_SKIP_WHITESPACE();
	TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(FTextStringHelper::ReadFromBuffer, SourceText, nullptr, nullptr, true);

	// Skip whitespace before the closing bracket, and then step over it
	TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

	PrepareDisplayStringForRebuild(OutDisplayString);
	return Buffer;
}

bool FTextHistory_Transform::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	// Produces LOCGEN_TOLOWER(...) or LOCGEN_TOUPPER
	switch (TransformType)
	{
	case ETransformType::ToLower:
		Buffer += TEXT("LOCGEN_TOLOWER(");
		break;
	case ETransformType::ToUpper:
		Buffer += TEXT("LOCGEN_TOUPPER(");
		break;
	default:
		break;
	}
	FTextStringHelper::WriteToBuffer(Buffer, SourceText, true);
	Buffer += TEXT(")");

	return true;
}

FString FTextHistory_Transform::BuildLocalizedDisplayString() const
{
	SourceText.Rebuild();

	switch (TransformType)
	{
	case ETransformType::ToLower:
		return FTextTransformer::ToLower(SourceText.ToString());
	case ETransformType::ToUpper:
		return FTextTransformer::ToUpper(SourceText.ToString());
	default:
		break;
	}
	return FString();
}

FString FTextHistory_Transform::BuildInvariantDisplayString() const
{
	SourceText.Rebuild();

	switch (TransformType)
	{
	case ETransformType::ToLower:
		return FTextTransformer::ToLower(SourceText.BuildSourceString());
	case ETransformType::ToUpper:
		return FTextTransformer::ToUpper(SourceText.BuildSourceString());
	default:
		break;
	}
	return FString();
}

void FTextHistory_Transform::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
	FTextInspector::GetHistoricFormatData(SourceText, OutHistoricFormatData);
}

bool FTextHistory_Transform::GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const
{
	return FTextInspector::GetHistoricNumericData(SourceText, OutHistoricNumericData);
}

///////////////////////////////////////
// FTextHistory_StringTableEntry

FTextHistory_StringTableEntry::FTextHistory_StringTableEntry(FName InTableId, FString&& InKey, const EStringTableLoadingPolicy InLoadingPolicy)
	: StringTableReferenceData(MakeShared<FStringTableReferenceData, ESPMode::ThreadSafe>())
{
	StringTableReferenceData->Initialize(&Revision, InTableId, MoveTemp(InKey), InLoadingPolicy);
}

FTextHistory_StringTableEntry::FTextHistory_StringTableEntry(FTextHistory_StringTableEntry&& Other)
	: FTextHistory(MoveTemp(Other))
	, StringTableReferenceData(MoveTemp(Other.StringTableReferenceData))
{
	Other.StringTableReferenceData.Reset();
}

FTextHistory_StringTableEntry& FTextHistory_StringTableEntry::operator=(FTextHistory_StringTableEntry&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		StringTableReferenceData = MoveTemp(Other.StringTableReferenceData);
		Other.StringTableReferenceData.Reset();
	}
	return *this;
}

FString FTextHistory_StringTableEntry::BuildLocalizedDisplayString() const
{
	// This should never be called for string table entries (CanRebuildLocalizedDisplayString is false)
	check(0);
	return FString();
}

FString FTextHistory_StringTableEntry::BuildInvariantDisplayString() const
{
	return *GetSourceString();
}

const FString* FTextHistory_StringTableEntry::GetSourceString() const
{
	FStringTableEntryConstPtr StringTableEntryPin = StringTableReferenceData ? StringTableReferenceData->ResolveStringTableEntry() : nullptr;
	if (StringTableEntryPin.IsValid())
	{
		return &StringTableEntryPin->GetSourceString();
	}
	return &FStringTableEntry::GetPlaceholderSourceString();
}

FTextDisplayStringRef FTextHistory_StringTableEntry::GetDisplayString() const
{
	FStringTableEntryConstPtr StringTableEntryPin = StringTableReferenceData ? StringTableReferenceData->ResolveStringTableEntry() : nullptr;
	if (StringTableEntryPin.IsValid())
	{
		FTextDisplayStringPtr DisplayString = StringTableEntryPin->GetDisplayString();
		if (DisplayString.IsValid())
		{
			return DisplayString.ToSharedRef();
		}
	}
	return FStringTableEntry::GetPlaceholderDisplayString();
}

void FTextHistory_StringTableEntry::GetTableIdAndKey(FName& OutTableId, FString& OutKey) const
{
	if (StringTableReferenceData)
	{
		StringTableReferenceData->GetTableIdAndKey(OutTableId, OutKey);
	}
}

void FTextHistory_StringTableEntry::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();

	if (BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::StringTableEntry;
		Record << NAMED_FIELD(HistoryType);
	}

	if (BaseArchive.IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = 0;

		FName TableId;
		FString Key;
		Record << NAMED_FIELD(TableId);
		Record << NAMED_FIELD(Key);

		// String Table assets should already have been created via dependency loading when using the EDL (although they may not be fully loaded yet)
		StringTableReferenceData = MakeShared<FStringTableReferenceData, ESPMode::ThreadSafe>();
		StringTableReferenceData->Initialize(&Revision, TableId, MoveTemp(Key), GEventDrivenLoaderEnabled ? EStringTableLoadingPolicy::Find : EStringTableLoadingPolicy::FindOrLoad);
	}
	else if (BaseArchive.IsSaving())
	{
		FName TableId;
		FString Key;
		if (StringTableReferenceData)
		{
			StringTableReferenceData->GetTableIdAndKey(TableId, Key);
		}

		Record << NAMED_FIELD(TableId);
		Record << NAMED_FIELD(Key);
	}

	// Collect string table asset references
	if (StringTableReferenceData)
	{
		StringTableReferenceData->CollectStringTableAssetReferences(Record);
	}
}

void FTextHistory_StringTableEntry::SerializeForDisplayString(FStructuredArchive::FRecord Record, FTextDisplayStringPtr& InOutDisplayString)
{
	if (Record.GetUnderlyingArchive().IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = 0;
	}
}

bool FTextHistory_StringTableEntry::StaticShouldReadFromBuffer(const TCHAR* Buffer)
{
	return TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocTableMarker);
}

const TCHAR* FTextHistory_StringTableEntry::ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString)
{
#define LOC_DEFINE_REGION
	if (TEXT_STRINGIFICATION_PEEK_MARKER(TextStringificationUtil::LocTableMarker))
	{
		// Parsing something of the form: LOCTABLE("...", "...")
		TEXT_STRINGIFICATION_SKIP_MARKER_LEN(TextStringificationUtil::LocTableMarker);

		// Skip whitespace before the opening bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR('(');

		// Skip whitespace before the value, and then read out the quoted table ID
		FString TableIdString;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(TableIdString);
		FName TableId = *TableIdString;

		// Skip whitespace before the comma, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(',');

		// Skip whitespace before the value, and then read out the quoted key
		FString Key;
		TEXT_STRINGIFICATION_SKIP_WHITESPACE();
		TEXT_STRINGIFICATION_READ_QUOTED_STRING(Key);

		// Skip whitespace before the closing bracket, and then step over it
		TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(')');

		// Prepare the string table reference
		StringTableReferenceData = MakeShared<FStringTableReferenceData, ESPMode::ThreadSafe>();
		StringTableReferenceData->Initialize(&Revision, TableId, MoveTemp(Key), EStringTableLoadingPolicy::FindOrLoad);

		// We will definitely need to do a rebuild later
		Revision = 0;

		return Buffer;
	}
#undef LOC_DEFINE_REGION

	return nullptr;
}

bool FTextHistory_StringTableEntry::WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const
{
	if (StringTableReferenceData)
	{
		FName TableId;
		FString Key;
		StringTableReferenceData->GetTableIdAndKey(TableId, Key);

#define LOC_DEFINE_REGION
		// Produces LOCTABLE("...", "...")
		Buffer += TEXT("LOCTABLE(\"");
		Buffer += TableId.ToString().ReplaceCharWithEscapedChar();
		Buffer += TEXT("\", \"");
		Buffer += Key.ReplaceCharWithEscapedChar();
		Buffer += TEXT("\")");
#undef LOC_DEFINE_REGION

		return true;
	}

	return false;
}

void FTextHistory_StringTableEntry::FStringTableReferenceData::Initialize(uint16* InRevisionPtr, FName InTableId, FString&& InKey, const EStringTableLoadingPolicy InLoadingPolicy)
{
	RevisionPtr = InRevisionPtr;
	TableId = InTableId;
	Key = MoveTemp(InKey);
	FStringTableRedirects::RedirectTableIdAndKey(TableId, Key);

	if (InLoadingPolicy == EStringTableLoadingPolicy::Find)
	{
		// No loading attempt
		LoadingPhase = EStringTableLoadingPhase::Loaded;
		ResolveStringTableEntry();
	}
	else if (InLoadingPolicy == EStringTableLoadingPolicy::FindOrFullyLoad && IsInGameThread())
	{
		// Forced synchronous load
		LoadingPhase = EStringTableLoadingPhase::Loaded;
		IStringTableEngineBridge::FullyLoadStringTableAsset(TableId);
		ResolveStringTableEntry();
	}
	else
	{
		// Potential asynchronous load
		LoadingPhase = EStringTableLoadingPhase::PendingLoad;
		ConditionalBeginAssetLoad();
	}
}

FName FTextHistory_StringTableEntry::FStringTableReferenceData::GetTableId() const
{
	FScopeLock ScopeLock(&DataCS);
	return TableId;
}

FString FTextHistory_StringTableEntry::FStringTableReferenceData::GetKey() const
{
	FScopeLock ScopeLock(&DataCS);
	return Key;
}

void FTextHistory_StringTableEntry::FStringTableReferenceData::GetTableIdAndKey(FName& OutTableId, FString& OutKey) const
{
	FScopeLock ScopeLock(&DataCS);
	OutTableId = TableId;
	OutKey = Key;
}

void FTextHistory_StringTableEntry::FStringTableReferenceData::CollectStringTableAssetReferences(FStructuredArchive::FRecord Record) const
{
	if (Record.GetUnderlyingArchive().IsObjectReferenceCollector())
	{
		FScopeLock ScopeLock(&DataCS);
		IStringTableEngineBridge::CollectStringTableAssetReferences(TableId, Record.EnterField(FIELD_NAME_TEXT("AssetReferences")));
	}
}

FStringTableEntryConstPtr FTextHistory_StringTableEntry::FStringTableReferenceData::ResolveStringTableEntry()
{
	FStringTableEntryConstPtr StringTableEntryPin = StringTableEntry.Pin();

	if (!StringTableEntryPin.IsValid())
	{
		ConditionalBeginAssetLoad();
	}

	if (!StringTableEntryPin.IsValid() || !StringTableEntryPin->IsOwned())
	{
		FScopeLock ScopeLock(&DataCS);

		// Reset for the case it was disowned rather than became null
		StringTableEntry.Reset();
		StringTableEntryPin.Reset();

		if (LoadingPhase != EStringTableLoadingPhase::Loaded)
		{
			// Table still loading - cannot be resolved yet
			return nullptr;
		}

		FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
		if (StringTable.IsValid())
		{
			if (!StringTable->IsLoaded())
			{
				// Table still loading - cannot be resolved yet
				return nullptr;
			}
			StringTableEntryPin = StringTable->FindEntry(Key);
		}
		
		StringTableEntry = StringTableEntryPin;
	}

	if (!StringTableEntryPin.IsValid())
	{
		FStringTableRegistry::Get().LogMissingStringTableEntry(TableId, Key);
	}

	return StringTableEntryPin;
}

void FTextHistory_StringTableEntry::FStringTableReferenceData::ConditionalBeginAssetLoad()
{
	if (!IsInGameThread())
	{
		return;
	}

	FName TableIdToLoad;
	{
		FScopeLock ScopeLock(&DataCS);

		if (LoadingPhase != EStringTableLoadingPhase::PendingLoad)
		{
			return;
		}

		TableIdToLoad = TableId;
		LoadingPhase = EStringTableLoadingPhase::Loading;
	}

	IStringTableEngineBridge::LoadStringTableAsset(TableIdToLoad, [WeakThis = FStringTableReferenceDataWeakPtr(AsShared())](FName InRequestedTableId, FName InLoadedTableId)
	{
		// Was this request still valid?
		FStringTableReferenceDataPtr This = WeakThis.Pin();
		if (!This)
		{
			return;
		}

		FScopeLock ScopeLock(&This->DataCS);
		check(This->TableId == InRequestedTableId);

		// If this string table loaded, then update the table ID using the potentially redirected value
		if (!InLoadedTableId.IsNone())
		{
			This->TableId = InLoadedTableId;
		}
		This->LoadingPhase = EStringTableLoadingPhase::Loaded;

		// We will definitely need to do a rebuild later
		if (This->RevisionPtr)
		{
			*This->RevisionPtr = 0;
		}

		This->ResolveStringTableEntry();
	});
}


///////////////////////////////////////
// FTextHistory_TextGenerator

FTextHistory_TextGenerator::FTextHistory_TextGenerator(const TSharedRef<ITextGenerator>& InTextGenerator)
	: TextGenerator(InTextGenerator)
{
}

//~ Begin FTextHistory Interface
FString FTextHistory_TextGenerator::BuildLocalizedDisplayString() const
{
	return TextGenerator
		? TextGenerator->BuildLocalizedDisplayString()
		: FString();
}

FString FTextHistory_TextGenerator::BuildInvariantDisplayString() const
{
	return TextGenerator
		? TextGenerator->BuildInvariantDisplayString()
		: FString();
}

void FTextHistory_TextGenerator::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& BaseArchive = Record.GetUnderlyingArchive();
	if (BaseArchive.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::TextGenerator;
		Record << NAMED_FIELD(HistoryType);
	}

	FName GeneratorTypeID = (BaseArchive.IsSaving() && TextGenerator)
		? TextGenerator->GetTypeID()
		: FName();
	Record << NAMED_FIELD(GeneratorTypeID);

	TArray<uint8> GeneratorContents;

	if (BaseArchive.IsLoading())
	{
		TextGenerator.Reset();

		// Look up and construct or skip
		if (GeneratorTypeID != NAME_None)
		{
			FText::FCreateTextGeneratorDelegate FactoryFunction = FText::FindRegisteredTextGenerator(GeneratorTypeID);
			Record << NAMED_FIELD(GeneratorContents);

			if (ensureMsgf(FactoryFunction.IsBound(), TEXT("FTextHistory_TextGenerator::Serialize(): Unable to find registered text generator for \"%s\". Use FText::RegisterTextGenerator() to register a handler."),
				*GeneratorTypeID.ToString()))
			{
				FMemoryReader ArReader(GeneratorContents);
				FStructuredArchiveFromArchive ArStructuredReader(ArReader);

				{
					FStructuredArchive::FRecord ContentRecord = ArStructuredReader.GetSlot().EnterRecord();
					TextGenerator = FactoryFunction.Execute(ContentRecord);
					TextGenerator->Serialize(ContentRecord);
				}

				if (ArReader.IsError())
				{
					BaseArchive.SetError();
				}
			}
		}
	}
	else if (BaseArchive.IsSaving())
	{
		if (ensureMsgf(GeneratorTypeID != NAME_None, TEXT("FTextHistory_TextGenerator::Serialize(): Attempting to serialize a generator type that is not serializable")))
		{
			ensureMsgf(
				FText::FindRegisteredTextGenerator(GeneratorTypeID).IsBound(),
				TEXT("FTextHistory_TextGenerator::Serialize(): No generator factory function is registered for type \"%s\". Deserialization will fail. Use FText::RegisterTextGenerator() to register a handler."),
				*GeneratorTypeID.ToString()
			);

			FMemoryWriter ArWriter(GeneratorContents);
			FStructuredArchiveFromArchive ArStructuredWriter(ArWriter);

			TextGenerator->Serialize(ArStructuredWriter.GetSlot().EnterRecord());
			Record << NAMED_FIELD(GeneratorContents);

			if (ArWriter.IsError())
			{
				BaseArchive.SetError();
			}
		}
	}
}
