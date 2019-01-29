// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"
#include "Internationalization/Text.h"
#include "Internationalization/ITextGenerator.h"
#include "Internationalization/StringTableCoreFwd.h"
#include "Misc/DateTime.h"

struct FDecimalNumberFormattingRules;

enum class ETextHistoryType : int8
{
	None = -1,
	Base = 0,
	NamedFormat,
	OrderedFormat,
	ArgumentFormat,
	AsNumber,
	AsPercent,
	AsCurrency,
	AsDate,
	AsTime,
	AsDateTime,
	Transform,
	StringTableEntry,
	TextGenerator,

	// Add new enum types at the end only! They are serialized by index.
};

#define OVERRIDE_TEXT_HISTORY_STRINGIFICATION																																\
	static  bool StaticShouldReadFromBuffer(const TCHAR* Buffer);																											\
	virtual bool ShouldReadFromBuffer(const TCHAR* Buffer) const override { return StaticShouldReadFromBuffer(Buffer); }													\
	virtual const TCHAR* ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString) override;	\
	virtual bool WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const override;

/** Utilities for stringifying text */
namespace TextStringificationUtil
{

#define LOC_DEFINE_REGION
static const TCHAR TextMarker[] = TEXT("TEXT");
static const TCHAR InvTextMarker[] = TEXT("INVTEXT");
static const TCHAR NsLocTextMarker[] = TEXT("NSLOCTEXT");
static const TCHAR LocTextMarker[] = TEXT("LOCTEXT");
static const TCHAR LocTableMarker[] = TEXT("LOCTABLE");
static const TCHAR LocGenNumberMarker[] = TEXT("LOCGEN_NUMBER");
static const TCHAR LocGenPercentMarker[] = TEXT("LOCGEN_PERCENT");
static const TCHAR LocGenCurrencyMarker[] = TEXT("LOCGEN_CURRENCY");
static const TCHAR LocGenDateMarker[] = TEXT("LOCGEN_DATE");
static const TCHAR LocGenTimeMarker[] = TEXT("LOCGEN_TIME");
static const TCHAR LocGenDateTimeMarker[] = TEXT("LOCGEN_DATETIME");
static const TCHAR LocGenToLowerMarker[] = TEXT("LOCGEN_TOLOWER");
static const TCHAR LocGenToUpperMarker[] = TEXT("LOCGEN_TOUPPER");
static const TCHAR LocGenFormatOrderedMarker[] = TEXT("LOCGEN_FORMAT_ORDERED");
static const TCHAR LocGenFormatNamedMarker[] = TEXT("LOCGEN_FORMAT_NAMED");
static const TCHAR GroupedSuffix[] = TEXT("_GROUPED");
static const TCHAR UngroupedSuffix[] = TEXT("_UNGROUPED");
static const TCHAR CustomSuffix[] = TEXT("_CUSTOM");
static const TCHAR UtcSuffix[] = TEXT("_UTC");
static const TCHAR LocalSuffix[] = TEXT("_LOCAL");
#undef LOC_DEFINE_REGION

#define TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(Func, ...)		\
	Buffer = Func(Buffer, ##__VA_ARGS__);									\
	if (!Buffer) { return nullptr; }

#define TEXT_STRINGIFICATION_PEEK_MARKER(T)					TextStringificationUtil::PeekMarker(Buffer, T, ARRAY_COUNT(T) - 1)
#define TEXT_STRINGIFICATION_PEEK_INSENSITIVE_MARKER(T)		TextStringificationUtil::PeekInsensitiveMarker(Buffer, T, ARRAY_COUNT(T) - 1)
bool PeekMarker(const TCHAR* Buffer, const TCHAR* InMarker, const int32 InMarkerLen);
bool PeekInsensitiveMarker(const TCHAR* Buffer, const TCHAR* InMarker, const int32 InMarkerLen);

#define TEXT_STRINGIFICATION_SKIP_MARKER(T)					TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::SkipMarker, T, ARRAY_COUNT(T) - 1)
#define TEXT_STRINGIFICATION_SKIP_INSENSITIVE_MARKER(T)		TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::SkipInsensitiveMarker, T, ARRAY_COUNT(T) - 1)
#define TEXT_STRINGIFICATION_SKIP_MARKER_LEN(T)				Buffer += (ARRAY_COUNT(T) - 1)
const TCHAR* SkipMarker(const TCHAR* Buffer, const TCHAR* InMarker, const int32 InMarkerLen);
const TCHAR* SkipInsensitiveMarker(const TCHAR* Buffer, const TCHAR* InMarker, const int32 InMarkerLen);

#define TEXT_STRINGIFICATION_SKIP_WHITESPACE()				TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::SkipWhitespace)
const TCHAR* SkipWhitespace(const TCHAR* Buffer);

#define TEXT_STRINGIFICATION_SKIP_WHITESPACE_TO_CHAR(C)		TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::SkipWhitespaceToCharacter, TEXT(C))
const TCHAR* SkipWhitespaceToCharacter(const TCHAR* Buffer, const TCHAR InChar);

#define TEXT_STRINGIFICATION_SKIP_WHITESPACE_AND_CHAR(C)	TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::SkipWhitespaceAndCharacter, TEXT(C))
const TCHAR* SkipWhitespaceAndCharacter(const TCHAR* Buffer, const TCHAR InChar);

#define TEXT_STRINGIFICATION_READ_NUMBER(V)					TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadNumberFromBuffer, V)
const TCHAR* ReadNumberFromBuffer(const TCHAR* Buffer, FFormatArgumentValue& OutValue);

#define TEXT_STRINGIFICATION_READ_ALNUM(V)					TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadAlnumFromBuffer, V)
const TCHAR* ReadAlnumFromBuffer(const TCHAR* Buffer, FString& OutValue);

#define TEXT_STRINGIFICATION_READ_QUOTED_STRING(V)			TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadQuotedStringFromBuffer, V)
const TCHAR* ReadQuotedStringFromBuffer(const TCHAR* Buffer, FString& OutStr);

#define TEXT_STRINGIFICATION_READ_SCOPED_ENUM(S, V)			TEXT_STRINGIFICATION_FUNC_MODIFY_BUFFER_AND_VALIDATE(TextStringificationUtil::ReadScopedEnumFromBuffer, S, V)
template <typename T>
const TCHAR* ReadScopedEnumFromBuffer(const TCHAR* Buffer, const FString& Scope, T& OutValue)
{
	if (PeekInsensitiveMarker(Buffer, *Scope, Scope.Len()))
	{
		// Parsing something of the form: EEnumName::...
		Buffer += Scope.Len();

		FString EnumValueString;
		Buffer = ReadAlnumFromBuffer(Buffer, EnumValueString);

		if (Buffer && LexTryParseString(OutValue, *EnumValueString))
		{
			return Buffer;
		}
	}

	return nullptr;
}

template <typename T>
void WriteScopedEnumToBuffer(FString& Buffer, const TCHAR* Scope, const T Value)
{
	Buffer += Scope;
	Buffer += LexToString(Value);
}

}	// namespace TextStringificationUtil

/** Base interface class for all FText history types */
class CORE_API FTextHistory
{
public:
	FTextHistory();

	virtual ~FTextHistory() {}

	/** Allow moving */
	FTextHistory(FTextHistory&& Other);
	FTextHistory& operator=(FTextHistory&& Other);

	/** Get the type of this history */
	virtual ETextHistoryType GetType() const = 0;

	/** Build the display string for the current culture */
	virtual FString BuildLocalizedDisplayString() const = 0;

	/** Build the display string for the invariant culture */
	virtual FString BuildInvariantDisplayString() const = 0;
	
	/** Serializes the history to/from a structured archive slot */
	virtual void Serialize(FStructuredArchive::FRecord Record) = 0;

	/** Serializes data needed to get the FText's DisplayString */
	virtual void SerializeForDisplayString(FStructuredArchive::FRecord Record, FTextDisplayStringPtr& InOutDisplayString);

	/**
	 * Check the given stream of text to see if it looks like something this class could process in via ReadFromBuffer.
	 * @note This doesn't guarantee that ReadFromBuffer will be able to process the stream, only that it could attempt to.
	 */
	static  bool StaticShouldReadFromBuffer(const TCHAR* Buffer);
	virtual bool ShouldReadFromBuffer(const TCHAR* Buffer) const { return StaticShouldReadFromBuffer(Buffer); }

	/**
	 * Attempt to parse this text history from the given stream of text.
	 *
	 * @param Buffer			The buffer of text to read from.
	 * @param TextNamespace		An optional namespace to use when parsing texts that use LOCTEXT (default is an empty namespace).
	 * @param PackageNamespace	The package namespace of the containing object (if loading for a property - see TextNamespaceUtil::GetPackageNamespace).
	 * @param OutDisplayString	The display string pointer to potentially fill in (depending on the history type).
	 *
	 * @return The updated buffer after we parsed this text history, or nullptr on failure
	 */
	virtual const TCHAR* ReadFromBuffer(const TCHAR* Buffer, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, FTextDisplayStringPtr& OutDisplayString);

	/**
	 * Write this text history to a stream of text
	 *
	 * @param Buffer			The buffer of text to write to.
	 * @param DisplayString		The display string associated with the text being written
	 *
	 * @return True if we wrote valid data into Buffer, false otherwise
	 */
	virtual bool WriteToBuffer(FString& Buffer, FTextDisplayStringPtr DisplayString) const;

	/** Returns TRUE if the Revision is out of date */
	virtual bool IsOutOfDate() const;

	/** Returns the source string managed by the history (if any). */
	virtual const FString* GetSourceString() const;

	/** Get any historic text format data from this history */
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const;

	/** Get any historic numeric format data from this history */
	virtual bool GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const;

	/** Will rebuild the display string if out of date. */
	void Rebuild(TSharedRef< FString, ESPMode::ThreadSafe > InDisplayString);

	/** Get the raw revision history. Note: Usually you can to call IsOutOfDate rather than test this! */
	uint16 GetRevision() const { return Revision; }

protected:
	/** Returns true if this kind of text history is able to rebuild its localized display string */
	virtual bool CanRebuildLocalizedDisplayString() { return true; }

	/** Common logic for setting the display string correctly on load so that it will perform a rebuild */
	void PrepareDisplayStringForRebuild(FTextDisplayStringPtr& OutDisplayString);

	/** Revision index of this history, rebuilds when it is out of sync with the FTextLocalizationManager */
	uint16 Revision;

private:
	/** Disallow copying */
	FTextHistory(const FTextHistory&);
	FTextHistory& operator=(FTextHistory&);
};

/** No complexity to it, just holds the source string. */
class CORE_API FTextHistory_Base : public FTextHistory
{
public:
	FTextHistory_Base() {}
	explicit FTextHistory_Base(FString&& InSourceString);

	/** Allow moving */
	FTextHistory_Base(FTextHistory_Base&& Other);
	FTextHistory_Base& operator=(FTextHistory_Base&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::Base; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual void SerializeForDisplayString(FStructuredArchive::FRecord Record, FTextDisplayStringPtr& InOutDisplayString) override;
	virtual const FString* GetSourceString() const override;
	//~ End FTextHistory Interface

protected:
	//~ Begin FTextHistory Interface
	virtual bool CanRebuildLocalizedDisplayString() { return false; }
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_Base(const FTextHistory_Base&);
	FTextHistory_Base& operator=(FTextHistory_Base&);

	/** The source string for an FText */
	FString SourceString;
};

/** Handles history for FText::Format when passing named arguments */
class CORE_API FTextHistory_NamedFormat : public FTextHistory
{
public:
	FTextHistory_NamedFormat() {}
	FTextHistory_NamedFormat(FTextFormat&& InSourceFmt, FFormatNamedArguments&& InArguments);

	/** Allow moving */
	FTextHistory_NamedFormat(FTextHistory_NamedFormat&& Other);
	FTextHistory_NamedFormat& operator=(FTextHistory_NamedFormat&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::NamedFormat; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_NamedFormat(const FTextHistory_NamedFormat&);
	FTextHistory_NamedFormat& operator=(FTextHistory_NamedFormat&);

	/** The pattern used to format the text */
	FTextFormat SourceFmt;
	/** Arguments to replace in the pattern string */
	FFormatNamedArguments Arguments;
};

/** Handles history for FText::Format when passing ordered arguments */
class CORE_API FTextHistory_OrderedFormat : public FTextHistory
{
public:
	FTextHistory_OrderedFormat() {}
	FTextHistory_OrderedFormat(FTextFormat&& InSourceFmt, FFormatOrderedArguments&& InArguments);

	/** Allow moving */
	FTextHistory_OrderedFormat(FTextHistory_OrderedFormat&& Other);
	FTextHistory_OrderedFormat& operator=(FTextHistory_OrderedFormat&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::OrderedFormat; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_OrderedFormat(const FTextHistory_OrderedFormat&);
	FTextHistory_OrderedFormat& operator=(FTextHistory_OrderedFormat&);

	/** The pattern used to format the text */
	FTextFormat SourceFmt;
	/** Arguments to replace in the pattern string */
	FFormatOrderedArguments Arguments;
};

/** Handles history for FText::Format when passing raw argument data */
class CORE_API FTextHistory_ArgumentDataFormat : public FTextHistory
{
public:
	FTextHistory_ArgumentDataFormat() {}
	FTextHistory_ArgumentDataFormat(FTextFormat&& InSourceFmt, TArray<FFormatArgumentData>&& InArguments);

	/** Allow moving */
	FTextHistory_ArgumentDataFormat(FTextHistory_ArgumentDataFormat&& Other);
	FTextHistory_ArgumentDataFormat& operator=(FTextHistory_ArgumentDataFormat&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::ArgumentFormat; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_ArgumentDataFormat(const FTextHistory_ArgumentDataFormat&);
	FTextHistory_ArgumentDataFormat& operator=(FTextHistory_ArgumentDataFormat&);

	/** The pattern used to format the text */
	FTextFormat SourceFmt;
	/** Arguments to replace in the pattern string */
	TArray<FFormatArgumentData> Arguments;
};

/** Base class for managing formatting FText's from: AsNumber, AsPercent, and AsCurrency. Manages data serialization of these history events */
class CORE_API FTextHistory_FormatNumber : public FTextHistory
{
public:
	FTextHistory_FormatNumber() {}
	FTextHistory_FormatNumber(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_FormatNumber(FTextHistory_FormatNumber&& Other);
	FTextHistory_FormatNumber& operator=(FTextHistory_FormatNumber&& Other);

	//~ Begin FTextHistory Interface
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End FTextHistory interface

protected:
	/** Build the numeric display string using the given formatting rules */
	FString BuildNumericDisplayString(const FDecimalNumberFormattingRules& InFormattingRules, const int32 InValueMultiplier = 1) const;

	/** The source value to format from */
	FFormatArgumentValue SourceValue;
	/** All the formatting options available to format using. This can be empty. */
	TOptional<FNumberFormattingOptions> FormatOptions;
	/** The culture to format using */
	FCulturePtr TargetCulture;

private:
	/** Disallow copying */
	FTextHistory_FormatNumber(const FTextHistory_FormatNumber&);
	FTextHistory_FormatNumber& operator=(FTextHistory_FormatNumber&);
};

/**  Handles history for formatting using AsNumber */
class CORE_API FTextHistory_AsNumber : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsNumber() {}
	FTextHistory_AsNumber(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsNumber(FTextHistory_AsNumber&& Other);
	FTextHistory_AsNumber& operator=(FTextHistory_AsNumber&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsNumber; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual bool GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const override;
	//~ End FTextHistory interface

private:
	/** Disallow copying */
	FTextHistory_AsNumber(const FTextHistory_AsNumber&);
	FTextHistory_AsNumber& operator=(FTextHistory_AsNumber&);
};

/**  Handles history for formatting using AsPercent */
class CORE_API FTextHistory_AsPercent : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsPercent() {}
	FTextHistory_AsPercent(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsPercent(FTextHistory_AsPercent&& Other);
	FTextHistory_AsPercent& operator=(FTextHistory_AsPercent&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsPercent; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual bool GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const override;
	//~ End FTextHistory interface

private:
	/** Disallow copying */
	FTextHistory_AsPercent(const FTextHistory_AsPercent&);
	FTextHistory_AsPercent& operator=(FTextHistory_AsPercent&);
};

/**  Handles history for formatting using AsCurrency */
class CORE_API FTextHistory_AsCurrency : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsCurrency() {}
	FTextHistory_AsCurrency(FFormatArgumentValue InSourceValue, FString InCurrencyCode, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsCurrency(FTextHistory_AsCurrency&& Other);
	FTextHistory_AsCurrency& operator=(FTextHistory_AsCurrency&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsCurrency; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_AsCurrency(const FTextHistory_AsCurrency&);
	FTextHistory_AsCurrency& operator=(FTextHistory_AsCurrency&);

	/** The currency used to format the number. */
	FString CurrencyCode;
};

/**  Handles history for formatting using AsDate */
class CORE_API FTextHistory_AsDate : public FTextHistory
{
public:
	FTextHistory_AsDate() {}
	FTextHistory_AsDate(FDateTime InSourceDateTime, const EDateTimeStyle::Type InDateStyle, FString InTimeZone, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsDate(FTextHistory_AsDate&& Other);
	FTextHistory_AsDate& operator=(FTextHistory_AsDate&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsDate; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_AsDate(const FTextHistory_AsDate&);
	FTextHistory_AsDate& operator=(FTextHistory_AsDate&);

	/** The source date structure to format */
	FDateTime SourceDateTime;
	/** Style to format the date using */
	EDateTimeStyle::Type DateStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the date in */
	FCulturePtr TargetCulture;
};

/**  Handles history for formatting using AsTime */
class CORE_API FTextHistory_AsTime : public FTextHistory
{
public:
	FTextHistory_AsTime() {}
	FTextHistory_AsTime(FDateTime InSourceDateTime, const EDateTimeStyle::Type InTimeStyle, FString InTimeZone, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsTime(FTextHistory_AsTime&& Other);
	FTextHistory_AsTime& operator=(FTextHistory_AsTime&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsTime; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_AsTime(const FTextHistory_AsTime&);
	FTextHistory_AsTime& operator=(FTextHistory_AsTime&);

	/** The source time structure to format */
	FDateTime SourceDateTime;
	/** Style to format the time using */
	EDateTimeStyle::Type TimeStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the time in */
	FCulturePtr TargetCulture;
};

/**  Handles history for formatting using AsDateTime */
class CORE_API FTextHistory_AsDateTime : public FTextHistory
{
public:
	FTextHistory_AsDateTime() {}
	FTextHistory_AsDateTime(FDateTime InSourceDateTime, const EDateTimeStyle::Type InDateStyle, const EDateTimeStyle::Type InTimeStyle, FString InTimeZone, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsDateTime(FTextHistory_AsDateTime&& Other);
	FTextHistory_AsDateTime& operator=(FTextHistory_AsDateTime&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsDateTime; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End FTextHistory Interfaces

private:
	/** Disallow copying */
	FTextHistory_AsDateTime(const FTextHistory_AsDateTime&);
	FTextHistory_AsDateTime& operator=(FTextHistory_AsDateTime&);

	/** The source date and time structure to format */
	FDateTime SourceDateTime;
	/** Style to format the date using */
	EDateTimeStyle::Type DateStyle;
	/** Style to format the time using */
	EDateTimeStyle::Type TimeStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the time in */
	FCulturePtr TargetCulture;
};

/**  Handles history for transforming text (eg, ToLower/ToUpper) */
class CORE_API FTextHistory_Transform : public FTextHistory
{
public:
	enum class ETransformType : uint8
	{
		ToLower = 0,
		ToUpper,

		// Add new enum types at the end only! They are serialized by index.
	};

	FTextHistory_Transform() {}
	FTextHistory_Transform(FText InSourceText, const ETransformType InTransformType);

	/** Allow moving */
	FTextHistory_Transform(FTextHistory_Transform&& Other);
	FTextHistory_Transform& operator=(FTextHistory_Transform&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::Transform; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const override;
	virtual bool GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const override;
	//~ End FTextHistory Interfaces

private:
	/** Disallow copying */
	FTextHistory_Transform(const FTextHistory_Transform&);
	FTextHistory_Transform& operator=(FTextHistory_Transform&);

	/** The source text instance that was transformed */
	FText SourceText;
	/** How the source text was transformed */
	ETransformType TransformType;
};

/** Holds a pointer to a referenced display string from a string table. */
class CORE_API FTextHistory_StringTableEntry : public FTextHistory
{
public:
	FTextHistory_StringTableEntry() {}
	FTextHistory_StringTableEntry(FName InTableId, FString&& InKey, const EStringTableLoadingPolicy InLoadingPolicy);

	/** Allow moving */
	FTextHistory_StringTableEntry(FTextHistory_StringTableEntry&& Other);
	FTextHistory_StringTableEntry& operator=(FTextHistory_StringTableEntry&& Other);

	//~ Begin FTextHistory Interface
	OVERRIDE_TEXT_HISTORY_STRINGIFICATION;
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::StringTableEntry; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual void SerializeForDisplayString(FStructuredArchive::FRecord Record, FTextDisplayStringPtr& InOutDisplayString) override;
	virtual const FString* GetSourceString() const override;
	//~ End FTextHistory Interface

	FTextDisplayStringRef GetDisplayString() const;

	void GetTableIdAndKey(FName& OutTableId, FString& OutKey) const;

protected:
	//~ Begin FTextHistory Interface
	virtual bool CanRebuildLocalizedDisplayString() { return false; }
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_StringTableEntry(const FTextHistory_StringTableEntry&);
	FTextHistory_StringTableEntry& operator=(FTextHistory_StringTableEntry&);

	enum class EStringTableLoadingPhase : uint8
	{
		/** This string table is pending load, and load should be attempted when possible */
		PendingLoad,
		/** This string table is currently being loaded, potentially asynchronously */
		Loading,
		/** This string was loaded, though that load may have failed */
		Loaded,
	};

	/** Hosts the reference data for this text history */
	class FStringTableReferenceData : public TSharedFromThis<FStringTableReferenceData, ESPMode::ThreadSafe>
	{
	public:
		/** Initialize this data, immediately starting an asset load if required and possible */
		void Initialize(uint16* InRevisionPtr, FName InTableId, FString&& InKey, const EStringTableLoadingPolicy InLoadingPolicy);

		/** Get the string table ID being referenced */
		FName GetTableId() const;

		/** Get the key within the string table being referenced */
		FString GetKey() const;

		/** Get the table ID and key within it that are being referenced */
		void GetTableIdAndKey(FName& OutTableId, FString& OutKey) const;

		/** Collect any string table asset references */
		void CollectStringTableAssetReferences(FStructuredArchive::FRecord Record) const;

		/** Resolve the string table pointer, potentially re-caching it if it's missing or stale */
		FStringTableEntryConstPtr ResolveStringTableEntry();

	private:
		/** Begin an asset load if required and possible */
		void ConditionalBeginAssetLoad();

		/** Pointer to the owner text history revision that we need to reset when the cached string table entry pointer changes */
		uint16* RevisionPtr = nullptr;

		/** The string table ID being referenced */
		FName TableId;

		/** The key within the string table being referenced */
		FString Key;

		/** The loading phase of any referenced string table asset */
		EStringTableLoadingPhase LoadingPhase = EStringTableLoadingPhase::PendingLoad;

		/** Cached string table entry pointer */
		FStringTableEntryConstWeakPtr StringTableEntry;

		/** Critical section preventing concurrent access to the resolved data */
		mutable FCriticalSection DataCS;
	};
	typedef TSharedPtr<FStringTableReferenceData, ESPMode::ThreadSafe> FStringTableReferenceDataPtr;
	typedef TWeakPtr<FStringTableReferenceData, ESPMode::ThreadSafe> FStringTableReferenceDataWeakPtr;

	/** The reference data for this text history */
	FStringTableReferenceDataPtr StringTableReferenceData;
};

/** Handles history for FText::FromTextGenerator */
class CORE_API FTextHistory_TextGenerator : public FTextHistory
{
public:
	FTextHistory_TextGenerator() {}
	FTextHistory_TextGenerator(const TSharedRef<ITextGenerator>& InTextGenerator);

	/** Disallow copying */
	FTextHistory_TextGenerator(const FTextHistory_TextGenerator&) = delete;
	FTextHistory_TextGenerator& operator=(const FTextHistory_TextGenerator&) = delete;

	/** Allow moving */
	FTextHistory_TextGenerator(FTextHistory_TextGenerator&& Other) = default;
	FTextHistory_TextGenerator& operator=(FTextHistory_TextGenerator&& Other) = default;

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::TextGenerator; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End FTextHistory Interface

private:
	/** The object implementing the custom generation code */
	TSharedPtr<ITextGenerator> TextGenerator;
};

#undef OVERRIDE_TEXT_HISTORY_STRINGIFICATION
