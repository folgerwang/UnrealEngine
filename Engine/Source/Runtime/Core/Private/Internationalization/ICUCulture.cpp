// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/ICUCulture.h"
#include "Misc/ScopeLock.h"
#include "Containers/SortedMap.h"
#include "Internationalization/FastDecimalFormat.h"

#if UE_ENABLE_ICU
#include "Internationalization/ICUUtilities.h"

namespace
{
	TSharedRef<const icu::BreakIterator> CreateBreakIterator( const icu::Locale& ICULocale, const EBreakIteratorType Type)
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		icu::BreakIterator* (*FactoryFunction)(const icu::Locale&, UErrorCode&) = nullptr;
		switch (Type)
		{
		case EBreakIteratorType::Grapheme:
			FactoryFunction = icu::BreakIterator::createCharacterInstance;
			break;
		case EBreakIteratorType::Word:
			FactoryFunction = icu::BreakIterator::createWordInstance;
			break;
		case EBreakIteratorType::Line:
			FactoryFunction = icu::BreakIterator::createLineInstance;
			break;
		case EBreakIteratorType::Sentence:
			FactoryFunction = icu::BreakIterator::createSentenceInstance;
			break;
		case EBreakIteratorType::Title:
			FactoryFunction = icu::BreakIterator::createTitleInstance;
			break;
		default:
			checkf(false, TEXT("Unhandled break iterator type"));
		}
		TSharedPtr<const icu::BreakIterator> Ptr = MakeShareable( FactoryFunction(ICULocale, ICUStatus) );
		checkf(Ptr.IsValid(), TEXT("Creating a break iterator object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::Collator, ESPMode::ThreadSafe> CreateCollator( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<const icu::Collator, ESPMode::ThreadSafe> Ptr = MakeShareable( icu::Collator::createInstance( ICULocale, ICUStatus ) );
		checkf(Ptr.IsValid(), TEXT("Creating a collator object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DateFormat> CreateDateFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<icu::DateFormat> Ptr = MakeShareable( icu::DateFormat::createDateInstance( icu::DateFormat::EStyle::kDefault, ICULocale ) );
		checkf(Ptr.IsValid(), TEXT("Creating a date format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		Ptr->adoptTimeZone( icu::TimeZone::createDefault() );
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DateFormat> CreateTimeFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<icu::DateFormat> Ptr = MakeShareable( icu::DateFormat::createTimeInstance( icu::DateFormat::EStyle::kDefault, ICULocale ) );
		checkf(Ptr.IsValid(), TEXT("Creating a time format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		Ptr->adoptTimeZone( icu::TimeZone::createDefault() );
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DateFormat> CreateDateTimeFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<icu::DateFormat> Ptr = MakeShareable( icu::DateFormat::createDateTimeInstance( icu::DateFormat::EStyle::kDefault, icu::DateFormat::EStyle::kDefault, ICULocale ) );
		checkf(Ptr.IsValid(), TEXT("Creating a date-time format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		Ptr->adoptTimeZone( icu::TimeZone::createDefault() );
		return Ptr.ToSharedRef();
	}
}

ETextPluralForm ICUPluralFormToUE(const icu::UnicodeString& InICUTag)
{
	static const icu::UnicodeString ZeroStr("zero");
	static const icu::UnicodeString OneStr("one");
	static const icu::UnicodeString TwoStr("two");
	static const icu::UnicodeString FewStr("few");
	static const icu::UnicodeString ManyStr("many");
	static const icu::UnicodeString OtherStr("other");

	if (InICUTag == ZeroStr)
	{
		return ETextPluralForm::Zero;
	}
	if (InICUTag == OneStr)
	{
		return ETextPluralForm::One;
	}
	if (InICUTag == TwoStr)
	{
		return ETextPluralForm::Two;
	}
	if (InICUTag == FewStr)
	{
		return ETextPluralForm::Few;
	}
	if (InICUTag == ManyStr)
	{
		return ETextPluralForm::Many;
	}
	if (InICUTag == OtherStr)
	{
		return ETextPluralForm::Other;
	}

	ensureAlwaysMsgf(false, TEXT("Unknown ICU plural form tag! Returning 'other'."));
	return ETextPluralForm::Other;
}

FCulture::FICUCultureImplementation::FICUCultureImplementation(const FString& LocaleName)
	: ICULocale( TCHAR_TO_ANSI( *LocaleName ) )
{
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		ICUCardinalPluralRules = icu::PluralRules::forLocale(ICULocale, UPLURAL_TYPE_CARDINAL, ICUStatus);
		checkf(U_SUCCESS(ICUStatus) && ICUCardinalPluralRules, TEXT("Creating a cardinal plural rules object failed using locale %s. Perhaps this locale has no data."), *LocaleName);
	}
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		ICUOrdianalPluralRules = icu::PluralRules::forLocale(ICULocale, UPLURAL_TYPE_ORDINAL, ICUStatus);
		checkf(U_SUCCESS(ICUStatus) && ICUOrdianalPluralRules, TEXT("Creating an ordinal plural rules object failed using locale %s. Perhaps this locale has no data."), *LocaleName);
	}
}

FString FCulture::FICUCultureImplementation::GetDisplayName() const
{
	icu::UnicodeString ICUResult;
	ICULocale.getDisplayName(ICUResult);
	return ICUUtilities::ConvertString(ICUResult);
}

FString FCulture::FICUCultureImplementation::GetEnglishName() const
{
	icu::UnicodeString ICUResult;
	ICULocale.getDisplayName(icu::Locale("en"), ICUResult);
	return ICUUtilities::ConvertString(ICUResult);
}

int FCulture::FICUCultureImplementation::GetKeyboardLayoutId() const
{
	return 0;
}

int FCulture::FICUCultureImplementation::GetLCID() const
{
	return ICULocale.getLCID();
}

FString FCulture::FICUCultureImplementation::GetCanonicalName(const FString& Name)
{
#define USE_ICU_CANONIZATION (0)

#if USE_ICU_CANONIZATION

	const FString SanitizedName = ICUUtilities::SanitizeCultureCode(Name);

	static const int32 CanonicalNameBufferSize = 64;
	char CanonicalNameBuffer[CanonicalNameBufferSize];

	UErrorCode ICUStatus = U_ZERO_ERROR;
	uloc_canonicalize(TCHAR_TO_ANSI(*SanitizedName), CanonicalNameBuffer, CanonicalNameBufferSize-1, &ICUStatus);
	CanonicalNameBuffer[CanonicalNameBufferSize-1] = 0;

	FString CanonicalNameString = CanonicalNameBuffer;
	CanonicalNameString.ReplaceInline(TEXT("_"), TEXT("-"));
	return CanonicalNameString;

#else	// USE_ICU_CANONIZATION

	auto IsScriptCode = [](const FString& InCode)
	{
		// Script codes must be 4 letters
		return InCode.Len() == 4;
	};

	auto IsRegionCode = [](const FString& InCode)
	{
		// Region codes must be 2 or 3 letters
		return InCode.Len() == 2 || InCode.Len() == 3;
	};

	auto ConditionLanguageCode = [](FString& InOutCode)
	{
		// Language codes are lowercase
		InOutCode.ToLowerInline();
	};

	auto ConditionScriptCode = [](FString& InOutCode)
	{
		// Script codes are titlecase
		InOutCode.ToLowerInline();
		if (InOutCode.Len() > 0)
		{
			InOutCode[0] = FChar::ToUpper(InOutCode[0]);
		}
	};

	auto ConditionRegionCode = [](FString& InOutCode)
	{
		// Region codes are uppercase
		InOutCode.ToUpperInline();
	};

	auto ConditionVariant = [](FString& InOutVariant)
	{
		// Variants are uppercase
		InOutVariant.ToUpperInline();
	};

	auto ConditionKeywordArgKey = [](FString& InOutKey)
	{
		// Keyword argument keys are lowercase
		InOutKey.ToLowerInline();
	};

	enum class ENameTagType : uint8
	{
		Language,
		Script,
		Region,
		Variant,
	};

	struct FNameTag
	{
		FString Str;
		ENameTagType Type;
	};

	struct FCanonizedTagData
	{
		const TCHAR* CanonizedNameTag;
		const TCHAR* KeywordArgKey;
		const TCHAR* KeywordArgValue;
	};

	static const TSortedMap<FString, FCanonizedTagData> CanonizedTagMap = []()
	{
		TSortedMap<FString, FCanonizedTagData> TmpCanonizedTagMap;
		TmpCanonizedTagMap.Add(TEXT(""),				{ TEXT("en-US-POSIX"), nullptr, nullptr });
		TmpCanonizedTagMap.Add(TEXT("c"),				{ TEXT("en-US-POSIX"), nullptr, nullptr });
		TmpCanonizedTagMap.Add(TEXT("posix"),			{ TEXT("en-US-POSIX"), nullptr, nullptr });
		TmpCanonizedTagMap.Add(TEXT("ca-ES-PREEURO"),	{ TEXT("ca-ES"), TEXT("currency"), TEXT("ESP") });
		TmpCanonizedTagMap.Add(TEXT("de-AT-PREEURO"),	{ TEXT("de-AT"), TEXT("currency"), TEXT("ATS") });
		TmpCanonizedTagMap.Add(TEXT("de-DE-PREEURO"),	{ TEXT("de-DE"), TEXT("currency"), TEXT("DEM") });
		TmpCanonizedTagMap.Add(TEXT("de-LU-PREEURO"),	{ TEXT("de-LU"), TEXT("currency"), TEXT("LUF") });
		TmpCanonizedTagMap.Add(TEXT("el-GR-PREEURO"),	{ TEXT("el-GR"), TEXT("currency"), TEXT("GRD") });
		TmpCanonizedTagMap.Add(TEXT("en-BE-PREEURO"),	{ TEXT("en-BE"), TEXT("currency"), TEXT("BEF") });
		TmpCanonizedTagMap.Add(TEXT("en-IE-PREEURO"),	{ TEXT("en-IE"), TEXT("currency"), TEXT("IEP") });
		TmpCanonizedTagMap.Add(TEXT("es-ES-PREEURO"),	{ TEXT("es-ES"), TEXT("currency"), TEXT("ESP") });
		TmpCanonizedTagMap.Add(TEXT("eu-ES-PREEURO"),	{ TEXT("eu-ES"), TEXT("currency"), TEXT("ESP") });
		TmpCanonizedTagMap.Add(TEXT("fi-FI-PREEURO"),	{ TEXT("fi-FI"), TEXT("currency"), TEXT("FIM") });
		TmpCanonizedTagMap.Add(TEXT("fr-BE-PREEURO"),	{ TEXT("fr-BE"), TEXT("currency"), TEXT("BEF") });
		TmpCanonizedTagMap.Add(TEXT("fr-FR-PREEURO"),	{ TEXT("fr-FR"), TEXT("currency"), TEXT("FRF") });
		TmpCanonizedTagMap.Add(TEXT("fr-LU-PREEURO"),	{ TEXT("fr-LU"), TEXT("currency"), TEXT("LUF") });
		TmpCanonizedTagMap.Add(TEXT("ga-IE-PREEURO"),	{ TEXT("ga-IE"), TEXT("currency"), TEXT("IEP") });
		TmpCanonizedTagMap.Add(TEXT("gl-ES-PREEURO"),	{ TEXT("gl-ES"), TEXT("currency"), TEXT("ESP") });
		TmpCanonizedTagMap.Add(TEXT("it-IT-PREEURO"),	{ TEXT("it-IT"), TEXT("currency"), TEXT("ITL") });
		TmpCanonizedTagMap.Add(TEXT("nl-BE-PREEURO"),	{ TEXT("nl-BE"), TEXT("currency"), TEXT("BEF") });
		TmpCanonizedTagMap.Add(TEXT("nl-NL-PREEURO"),	{ TEXT("nl-NL"), TEXT("currency"), TEXT("NLG") });
		TmpCanonizedTagMap.Add(TEXT("pt-PT-PREEURO"),	{ TEXT("pt-PT"), TEXT("currency"), TEXT("PTE") });
		return TmpCanonizedTagMap;
	}();

	static const TSortedMap<FString, FCanonizedTagData> VariantMap = []()
	{
		TSortedMap<FString, FCanonizedTagData> TmpVariantMap;
		TmpVariantMap.Add(TEXT("EURO"), { nullptr, TEXT("currency"), TEXT("EUR") });
		return TmpVariantMap;
	}();

	// Sanitize any nastiness from the culture code
	const FString SanitizedName = ICUUtilities::SanitizeCultureCode(Name);

	// These will be populated as the string is processed and are used to re-build the canonized string
	TArray<FNameTag, TInlineAllocator<4>> ParsedNameTags;
	TSortedMap<FString, FString, TInlineAllocator<4>> ParsedKeywords;

	// Parse the string into its component parts
	{
		// 1) Split the string so that the keywords exist in a separate string (both halves need separate processing)
		FString NameTag;
		FString NameKeywords;
		{
			int32 NameKeywordsSplitIndex = INDEX_NONE;
			SanitizedName.FindChar(TEXT('@'), NameKeywordsSplitIndex);

			int32 EncodingSplitIndex = INDEX_NONE;
			SanitizedName.FindChar(TEXT('.'), EncodingSplitIndex);

			// The name tags part of the string ends at either the start of the keywords or encoding (whichever is smaller)
			const int32 NameTagEndIndex = FMath::Min(
				NameKeywordsSplitIndex == INDEX_NONE ? SanitizedName.Len() : NameKeywordsSplitIndex, 
				EncodingSplitIndex == INDEX_NONE ? SanitizedName.Len() : EncodingSplitIndex
				);

			NameTag = SanitizedName.Left(NameTagEndIndex);
			NameTag.ReplaceInline(TEXT("_"), TEXT("-"), ESearchCase::CaseSensitive);

			if (NameKeywordsSplitIndex != INDEX_NONE)
			{
				NameKeywords = SanitizedName.Mid(NameKeywordsSplitIndex + 1);
			}
		}

		// 2) Perform any wholesale substitution (which may also add keywords into ParsedKeywords)
		if (const FCanonizedTagData* CanonizedTagData = CanonizedTagMap.Find(NameTag))
		{
			NameTag = CanonizedTagData->CanonizedNameTag;
			if (CanonizedTagData->KeywordArgKey && CanonizedTagData->KeywordArgValue)
			{
				ParsedKeywords.Add(CanonizedTagData->KeywordArgKey, CanonizedTagData->KeywordArgValue);
			}
		}

		// 3) Split the name tag into its component parts (produces the initial set of ParsedNameTags)
		{
			int32 NameTagStartIndex = 0;
			int32 NameTagEndIndex = 0;
			do
			{
				// Walk to the next breaking point
				for (; NameTagEndIndex < NameTag.Len() && NameTag[NameTagEndIndex] != TEXT('-'); ++NameTagEndIndex) {}

				// Process the tag
				{
					FString NameTagStr = NameTag.Mid(NameTagStartIndex, NameTagEndIndex - NameTagStartIndex);
					const FCanonizedTagData* VariantTagData = nullptr;

					// What kind of tag is this?
					ENameTagType NameTagType = ENameTagType::Variant;
					if (ParsedNameTags.Num() == 0)
					{
						// todo: map 3 letter language codes into 2 letter language codes like ICU would?
						NameTagType = ENameTagType::Language;
						ConditionLanguageCode(NameTagStr);
					}
					else if (ParsedNameTags.Num() == 1 && ParsedNameTags.Last().Type == ENameTagType::Language && IsScriptCode(NameTagStr))
					{
						NameTagType = ENameTagType::Script;
						ConditionScriptCode(NameTagStr);
					}
					else if (ParsedNameTags.Num() <= 2 && (ParsedNameTags.Last().Type == ENameTagType::Language || ParsedNameTags.Last().Type == ENameTagType::Script) && IsRegionCode(NameTagStr))
					{
						// todo: map 3 letter region codes into 2 letter region codes like ICU would?
						NameTagType = ENameTagType::Region;
						ConditionRegionCode(NameTagStr);
					}
					else
					{
						ConditionVariant(NameTagStr);
						VariantTagData = VariantMap.Find(NameTagStr);
					}

					if (VariantTagData)
					{
						check(VariantTagData->KeywordArgKey && VariantTagData->KeywordArgValue);
						ParsedKeywords.Add(VariantTagData->KeywordArgKey, VariantTagData->KeywordArgValue);
					}
					else
					{
						ParsedNameTags.Add({ MoveTemp(NameTagStr), NameTagType });
					}
				}

				// Prepare for the next loop
				NameTagStartIndex = NameTagEndIndex + 1;
				NameTagEndIndex = NameTagStartIndex;
			}
			while (NameTagEndIndex < NameTag.Len());
		}

		// 4) Parse the keywords (this may produce both variants into ParsedNameTags, and keywords into ParsedKeywords)
		{
			TArray<FString> NameKeywordArgs;
			NameKeywords.ParseIntoArray(NameKeywordArgs, TEXT(";"));

			for (FString& NameKeywordArg : NameKeywordArgs)
			{
				int32 KeyValueSplitIndex = INDEX_NONE;
				NameKeywordArg.FindChar(TEXT('='), KeyValueSplitIndex);

				if (KeyValueSplitIndex == INDEX_NONE)
				{
					// Single values are treated as variants
					ConditionVariant(NameKeywordArg);
					ParsedNameTags.Add({ MoveTemp(NameKeywordArg), ENameTagType::Variant });
				}
				else
				{
					// Key->Value pairs are treated as keywords
					FString NameKeywordArgKey = NameKeywordArg.Left(KeyValueSplitIndex);
					ConditionKeywordArgKey(NameKeywordArgKey);
					FString NameKeywordArgValue = NameKeywordArg.Mid(KeyValueSplitIndex + 1);
					ParsedKeywords.Add(MoveTemp(NameKeywordArgKey), MoveTemp(NameKeywordArgValue));
				}
			}
		}
	}

	// Re-assemble the string into its canonized form
	FString CanonicalName;
	{
		// Assemble the name tags first
		for (int32 NameTagIndex = 0; NameTagIndex < ParsedNameTags.Num(); ++NameTagIndex)
		{
			const FNameTag& NameTag = ParsedNameTags[NameTagIndex];

			switch (NameTag.Type)
			{
			case ENameTagType::Language:
				CanonicalName = NameTag.Str;
				break;

			case ENameTagType::Script:
			case ENameTagType::Region:
				CanonicalName += TEXT('-');
				CanonicalName += NameTag.Str;
				break;

			case ENameTagType::Variant:
				// If the previous tag was a language, we need to add an extra hyphen for non-empty variants since ICU would produce a double hyphen in this case
				if (ParsedNameTags.IsValidIndex(NameTagIndex - 1) && ParsedNameTags[NameTagIndex - 1].Type == ENameTagType::Language && !NameTag.Str.IsEmpty())
				{
					CanonicalName += TEXT('-');
				}
				CanonicalName += TEXT('-');
				CanonicalName += NameTag.Str;
				break;

			default:
				break;
			}
		}

		// Now add the keywords
		if (ParsedKeywords.Num() > 0)
		{
			TCHAR NextToken = TEXT('@');
			for (const auto& ParsedKeywordPair : ParsedKeywords)
			{
				CanonicalName += NextToken;
				NextToken = TEXT(';');

				CanonicalName += ParsedKeywordPair.Key;
				CanonicalName += TEXT('=');
				CanonicalName += ParsedKeywordPair.Value;
			}
		}
	}
	return CanonicalName;

#endif	// USE_ICU_CANONIZATION

#undef USE_ICU_CANONIZATION
}

FString FCulture::FICUCultureImplementation::GetName() const
{
	FString Result = ICULocale.getName();
	Result.ReplaceInline(TEXT("_"), TEXT("-"), ESearchCase::IgnoreCase);
	return Result;
}

FString FCulture::FICUCultureImplementation::GetNativeName() const
{
	icu::UnicodeString ICUResult;
	ICULocale.getDisplayName(ICULocale, ICUResult);
	return ICUUtilities::ConvertString(ICUResult);
}

FString FCulture::FICUCultureImplementation::GetUnrealLegacyThreeLetterISOLanguageName() const
{
	FString Result( ICULocale.getISO3Language() );

	// Legacy Overrides (INT, JPN, KOR), also for new web localization (CHN)
	// and now for any other languages (FRA, DEU...) for correct redirection of documentation web links
	if (Result == TEXT("eng"))
	{
		Result = TEXT("INT");
	}
	else
	{
		Result = Result.ToUpper();
	}

	return Result;
}

FString FCulture::FICUCultureImplementation::GetThreeLetterISOLanguageName() const
{
	return ICULocale.getISO3Language();
}

FString FCulture::FICUCultureImplementation::GetTwoLetterISOLanguageName() const
{
	return ICULocale.getLanguage();
}

FString FCulture::FICUCultureImplementation::GetNativeLanguage() const
{
	icu::UnicodeString ICUNativeLanguage;
	ICULocale.getDisplayLanguage(ICULocale, ICUNativeLanguage);
	FString NativeLanguage;
	ICUUtilities::ConvertString(ICUNativeLanguage, NativeLanguage);

	icu::UnicodeString ICUNativeScript;
	ICULocale.getDisplayScript(ICULocale, ICUNativeScript);
	FString NativeScript;
	ICUUtilities::ConvertString(ICUNativeScript, NativeScript);

	if ( !NativeScript.IsEmpty() )
	{
		return NativeLanguage + TEXT(" (") + NativeScript + TEXT(")");
	}
	return NativeLanguage;
}

FString FCulture::FICUCultureImplementation::GetRegion() const
{
	return ICULocale.getCountry();
}

FString FCulture::FICUCultureImplementation::GetNativeRegion() const
{
	icu::UnicodeString ICUNativeCountry;
	ICULocale.getDisplayCountry(ICULocale, ICUNativeCountry);
	FString NativeCountry;
	ICUUtilities::ConvertString(ICUNativeCountry, NativeCountry);

	icu::UnicodeString ICUNativeVariant;
	ICULocale.getDisplayVariant(ICULocale, ICUNativeVariant);
	FString NativeVariant;
	ICUUtilities::ConvertString(ICUNativeVariant, NativeVariant);

	if ( !NativeVariant.IsEmpty() )
	{
		return NativeCountry + TEXT(", ") + NativeVariant;
	}
	return NativeCountry;
}

FString FCulture::FICUCultureImplementation::GetScript() const
{
	return ICULocale.getScript();
}

FString FCulture::FICUCultureImplementation::GetVariant() const
{
	return ICULocale.getVariant();
}

TSharedRef<const icu::BreakIterator> FCulture::FICUCultureImplementation::GetBreakIterator(const EBreakIteratorType Type)
{
	TSharedPtr<const icu::BreakIterator> Result;

	switch (Type)
	{
	case EBreakIteratorType::Grapheme:
		{
			Result = ICUGraphemeBreakIterator.IsValid() ? ICUGraphemeBreakIterator : ( ICUGraphemeBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	case EBreakIteratorType::Word:
		{
			Result = ICUWordBreakIterator.IsValid() ? ICUWordBreakIterator : ( ICUWordBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	case EBreakIteratorType::Line:
		{
			Result = ICULineBreakIterator.IsValid() ? ICULineBreakIterator : ( ICULineBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	case EBreakIteratorType::Sentence:
		{
			Result = ICUSentenceBreakIterator.IsValid() ? ICUSentenceBreakIterator : ( ICUSentenceBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	case EBreakIteratorType::Title:
		{
			Result = ICUTitleBreakIterator.IsValid() ? ICUTitleBreakIterator : ( ICUTitleBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	}

	return Result.ToSharedRef();
}

TSharedRef<const icu::Collator, ESPMode::ThreadSafe> FCulture::FICUCultureImplementation::GetCollator(const ETextComparisonLevel::Type ComparisonLevel)
{
	if (!ICUCollator.IsValid())
	{
		ICUCollator = CreateCollator( ICULocale );
	}

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const bool bIsDefault = (ComparisonLevel == ETextComparisonLevel::Default);
	const TSharedRef<const icu::Collator, ESPMode::ThreadSafe> DefaultCollator( ICUCollator.ToSharedRef() );
	if(bIsDefault)
	{
		return DefaultCollator;
	}
	else
	{
		const TSharedRef<icu::Collator, ESPMode::ThreadSafe> Collator( DefaultCollator->clone() );
		Collator->setAttribute(UColAttribute::UCOL_STRENGTH, UEToICU(ComparisonLevel), ICUStatus);
		return Collator;
	}
}

TSharedRef<const icu::DateFormat> FCulture::FICUCultureImplementation::GetDateFormatter(const EDateTimeStyle::Type DateStyle, const FString& TimeZone)
{
	if (!ICUDateFormat.IsValid())
	{
		ICUDateFormat = CreateDateFormat( ICULocale );
	}

	const FString SanitizedTimezoneCode = ICUUtilities::SanitizeTimezoneCode(TimeZone);

	icu::UnicodeString InputTimeZoneID;
	ICUUtilities::ConvertString(SanitizedTimezoneCode, InputTimeZoneID, false);

	const TSharedRef<const icu::DateFormat> DefaultFormatter( ICUDateFormat.ToSharedRef() );

	bool bIsDefaultTimeZone = SanitizedTimezoneCode.IsEmpty();
	if( !bIsDefaultTimeZone )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;

		icu::UnicodeString CanonicalInputTimeZoneID;
		icu::TimeZone::getCanonicalID(InputTimeZoneID, CanonicalInputTimeZoneID, ICUStatus);

		icu::UnicodeString DefaultTimeZoneID;
		DefaultFormatter->getTimeZone().getID(DefaultTimeZoneID);

		icu::UnicodeString CanonicalDefaultTimeZoneID;
		icu::TimeZone::getCanonicalID(DefaultTimeZoneID, CanonicalDefaultTimeZoneID, ICUStatus);

		bIsDefaultTimeZone = (CanonicalInputTimeZoneID == CanonicalDefaultTimeZoneID ? true : false);
	}

	const bool bIsDefault = 
		DateStyle == EDateTimeStyle::Default &&
		bIsDefaultTimeZone;

	if(bIsDefault)
	{
		return DefaultFormatter;
	}
	else
	{
		const TSharedRef<icu::DateFormat> Formatter( icu::DateFormat::createDateInstance( UEToICU(DateStyle), ICULocale ) );
		Formatter->adoptTimeZone( bIsDefaultTimeZone ? icu::TimeZone::createDefault() : icu::TimeZone::createTimeZone(InputTimeZoneID) );
		return Formatter;
	}
}

TSharedRef<const icu::DateFormat> FCulture::FICUCultureImplementation::GetTimeFormatter(const EDateTimeStyle::Type TimeStyle, const FString& TimeZone)
{
	if (!ICUTimeFormat.IsValid())
	{
		ICUTimeFormat = CreateTimeFormat( ICULocale );
	}

	const FString SanitizedTimezoneCode = ICUUtilities::SanitizeTimezoneCode(TimeZone);

	icu::UnicodeString InputTimeZoneID;
	ICUUtilities::ConvertString(SanitizedTimezoneCode, InputTimeZoneID, false);

	const TSharedRef<const icu::DateFormat> DefaultFormatter( ICUTimeFormat.ToSharedRef() );

	bool bIsDefaultTimeZone = SanitizedTimezoneCode.IsEmpty();
	if( !bIsDefaultTimeZone )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;

		icu::UnicodeString CanonicalInputTimeZoneID;
		icu::TimeZone::getCanonicalID(InputTimeZoneID, CanonicalInputTimeZoneID, ICUStatus);

		icu::UnicodeString DefaultTimeZoneID;
		DefaultFormatter->getTimeZone().getID(DefaultTimeZoneID);

		icu::UnicodeString CanonicalDefaultTimeZoneID;
		icu::TimeZone::getCanonicalID(DefaultTimeZoneID, CanonicalDefaultTimeZoneID, ICUStatus);

		bIsDefaultTimeZone = (CanonicalInputTimeZoneID == CanonicalDefaultTimeZoneID ? true : false);
	}

	const bool bIsDefault = 
		TimeStyle == EDateTimeStyle::Default &&
		bIsDefaultTimeZone;

	if(bIsDefault)
	{
		return DefaultFormatter;
	}
	else
	{
		const TSharedRef<icu::DateFormat> Formatter( icu::DateFormat::createTimeInstance( UEToICU(TimeStyle), ICULocale ) );
		Formatter->adoptTimeZone( bIsDefaultTimeZone ? icu::TimeZone::createDefault() : icu::TimeZone::createTimeZone(InputTimeZoneID) );
		return Formatter;
	}
}

TSharedRef<const icu::DateFormat> FCulture::FICUCultureImplementation::GetDateTimeFormatter(const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone)
{
	if (!ICUDateTimeFormat.IsValid())
	{
		ICUDateTimeFormat = CreateDateTimeFormat( ICULocale );
	}

	const FString SanitizedTimezoneCode = ICUUtilities::SanitizeTimezoneCode(TimeZone);

	icu::UnicodeString InputTimeZoneID;
	ICUUtilities::ConvertString(SanitizedTimezoneCode, InputTimeZoneID, false);

	const TSharedRef<const icu::DateFormat> DefaultFormatter( ICUDateTimeFormat.ToSharedRef() );

	bool bIsDefaultTimeZone = SanitizedTimezoneCode.IsEmpty();
	if( !bIsDefaultTimeZone )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;

		icu::UnicodeString CanonicalInputTimeZoneID;
		icu::TimeZone::getCanonicalID(InputTimeZoneID, CanonicalInputTimeZoneID, ICUStatus);

		icu::UnicodeString DefaultTimeZoneID;
		DefaultFormatter->getTimeZone().getID(DefaultTimeZoneID);

		icu::UnicodeString CanonicalDefaultTimeZoneID;
		icu::TimeZone::getCanonicalID(DefaultTimeZoneID, CanonicalDefaultTimeZoneID, ICUStatus);

		bIsDefaultTimeZone = (CanonicalInputTimeZoneID == CanonicalDefaultTimeZoneID ? true : false);
	}

	const bool bIsDefault = 
		DateStyle == EDateTimeStyle::Default &&
		TimeStyle == EDateTimeStyle::Default &&
		bIsDefaultTimeZone;

	if(bIsDefault)
	{
		return DefaultFormatter;
	}
	else
	{
		const TSharedRef<icu::DateFormat> Formatter( icu::DateFormat::createDateTimeInstance( UEToICU(DateStyle), UEToICU(TimeStyle), ICULocale ) );
		Formatter->adoptTimeZone( bIsDefaultTimeZone ? icu::TimeZone::createDefault() : icu::TimeZone::createTimeZone(InputTimeZoneID) );
		return Formatter;
	}
}

namespace
{

FDecimalNumberFormattingRules ExtractNumberFormattingRulesFromICUDecimalFormatter(icu::DecimalFormat& InICUDecimalFormat)
{
	FDecimalNumberFormattingRules NewUEDecimalNumberFormattingRules;

	// Extract the default formatting options before we mess around with the formatter object settings
	NewUEDecimalNumberFormattingRules.CultureDefaultFormattingOptions
		.SetUseGrouping(InICUDecimalFormat.isGroupingUsed() != 0)
		.SetRoundingMode(ICUToUE(InICUDecimalFormat.getRoundingMode()))
		.SetMinimumIntegralDigits(InICUDecimalFormat.getMinimumIntegerDigits())
		.SetMaximumIntegralDigits(InICUDecimalFormat.getMaximumIntegerDigits())
		.SetMinimumFractionalDigits(InICUDecimalFormat.getMinimumFractionDigits())
		.SetMaximumFractionalDigits(InICUDecimalFormat.getMaximumFractionDigits());

	// We force grouping to be on, even if a culture doesn't use it by default, so that we can extract meaningful grouping information
	// This allows us to use the correct groupings if we should ever force grouping for a number, rather than use the culture default
	InICUDecimalFormat.setGroupingUsed(true);

	auto ICUStringToTCHAR = [](const icu::UnicodeString& InICUString) -> TCHAR
	{
		check(InICUString.length() == 1);
		return static_cast<TCHAR>(InICUString.charAt(0));
	};

	auto ExtractFormattingSymbolAsCharacter = [&](icu::DecimalFormatSymbols::ENumberFormatSymbol InSymbolToExtract) -> TCHAR
	{
		const icu::UnicodeString& ICUSymbolString = InICUDecimalFormat.getDecimalFormatSymbols()->getConstSymbol(InSymbolToExtract);
		return ICUStringToTCHAR(ICUSymbolString); // For efficiency we assume that these symbols are always a single character
	};

	icu::UnicodeString ScratchICUString;

	// Extract the rules from the decimal formatter
	NewUEDecimalNumberFormattingRules.NaNString						= ICUUtilities::ConvertString(InICUDecimalFormat.getDecimalFormatSymbols()->getConstSymbol(icu::DecimalFormatSymbols::kNaNSymbol));
	NewUEDecimalNumberFormattingRules.NegativePrefixString			= ICUUtilities::ConvertString(InICUDecimalFormat.getNegativePrefix(ScratchICUString));
	NewUEDecimalNumberFormattingRules.NegativeSuffixString			= ICUUtilities::ConvertString(InICUDecimalFormat.getNegativeSuffix(ScratchICUString));
	NewUEDecimalNumberFormattingRules.PositivePrefixString			= ICUUtilities::ConvertString(InICUDecimalFormat.getPositivePrefix(ScratchICUString));
	NewUEDecimalNumberFormattingRules.PositiveSuffixString			= ICUUtilities::ConvertString(InICUDecimalFormat.getPositiveSuffix(ScratchICUString));
	NewUEDecimalNumberFormattingRules.PlusString					= ICUUtilities::ConvertString(InICUDecimalFormat.getDecimalFormatSymbols()->getConstSymbol(icu::DecimalFormatSymbols::kPlusSignSymbol));
	NewUEDecimalNumberFormattingRules.MinusString					= ICUUtilities::ConvertString(InICUDecimalFormat.getDecimalFormatSymbols()->getConstSymbol(icu::DecimalFormatSymbols::kMinusSignSymbol));
	NewUEDecimalNumberFormattingRules.GroupingSeparatorCharacter	= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kGroupingSeparatorSymbol);
	NewUEDecimalNumberFormattingRules.DecimalSeparatorCharacter		= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kDecimalSeparatorSymbol);
	NewUEDecimalNumberFormattingRules.PrimaryGroupingSize			= static_cast<uint8>(InICUDecimalFormat.getGroupingSize());
	NewUEDecimalNumberFormattingRules.SecondaryGroupingSize			= (InICUDecimalFormat.getSecondaryGroupingSize() < 1) 
																		? NewUEDecimalNumberFormattingRules.PrimaryGroupingSize 
																		: static_cast<uint8>(InICUDecimalFormat.getSecondaryGroupingSize());

	NewUEDecimalNumberFormattingRules.DigitCharacters[0]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kZeroDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[1]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kOneDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[2]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kTwoDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[3]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kThreeDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[4]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kFourDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[5]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kFiveDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[6]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kSixDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[7]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kSevenDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[8]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kEightDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[9]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kNineDigitSymbol);

	return NewUEDecimalNumberFormattingRules;
}

} // anonymous namespace

const FDecimalNumberFormattingRules& FCulture::FICUCultureImplementation::GetDecimalNumberFormattingRules()
{
	if (UEDecimalNumberFormattingRules.IsValid())
	{
		return *UEDecimalNumberFormattingRules;
	}

	// Create a culture decimal formatter
	TSharedPtr<icu::DecimalFormat> DecimalFormatterForCulture;
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		DecimalFormatterForCulture = MakeShareable(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createInstance(ICULocale, ICUStatus)));
		checkf(DecimalFormatterForCulture.IsValid(), TEXT("Creating a decimal format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
	}

	const FDecimalNumberFormattingRules NewUEDecimalNumberFormattingRules = ExtractNumberFormattingRulesFromICUDecimalFormatter(*DecimalFormatterForCulture);

	// Check the pointer again in case another thread beat us to it
	{
		FScopeLock PtrLock(&UEDecimalNumberFormattingRulesCS);

		if (!UEDecimalNumberFormattingRules.IsValid())
		{
			UEDecimalNumberFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUEDecimalNumberFormattingRules));
		}
	}

	return *UEDecimalNumberFormattingRules;
}

const FDecimalNumberFormattingRules& FCulture::FICUCultureImplementation::GetPercentFormattingRules()
{
	if (UEPercentFormattingRules.IsValid())
	{
		return *UEPercentFormattingRules;
	}

	// Create a culture percent formatter (doesn't call CreatePercentFormat as we need a mutable instance)
	TSharedPtr<icu::DecimalFormat> PercentFormatterForCulture;
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		PercentFormatterForCulture = MakeShareable(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createPercentInstance(ICULocale, ICUStatus)));
		checkf(PercentFormatterForCulture.IsValid(), TEXT("Creating a percent format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
	}

	const FDecimalNumberFormattingRules NewUEPercentFormattingRules = ExtractNumberFormattingRulesFromICUDecimalFormatter(*PercentFormatterForCulture);

	// Check the pointer again in case another thread beat us to it
	{
		FScopeLock PtrLock(&UEPercentFormattingRulesCS);

		if (!UEPercentFormattingRules.IsValid())
		{
			UEPercentFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUEPercentFormattingRules));
		}
	}

	return *UEPercentFormattingRules;
}

const FDecimalNumberFormattingRules& FCulture::FICUCultureImplementation::GetCurrencyFormattingRules(const FString& InCurrencyCode)
{
	const FString SanitizedCurrencyCode = ICUUtilities::SanitizeCurrencyCode(InCurrencyCode);
	const bool bUseDefaultFormattingRules = SanitizedCurrencyCode.IsEmpty();

	if (bUseDefaultFormattingRules)
	{
		if (UECurrencyFormattingRules.IsValid())
		{
			return *UECurrencyFormattingRules;
		}
	}
	else
	{
		FScopeLock MapLock(&UEAlternateCurrencyFormattingRulesCS);

		auto FoundUEAlternateCurrencyFormattingRules = UEAlternateCurrencyFormattingRules.FindRef(SanitizedCurrencyCode);
		if (FoundUEAlternateCurrencyFormattingRules.IsValid())
		{
			return *FoundUEAlternateCurrencyFormattingRules;
		}
	}

	// Create a currency specific formatter (doesn't call CreateCurrencyFormat as we need a mutable instance)
	TSharedPtr<icu::DecimalFormat> CurrencyFormatterForCulture;
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		CurrencyFormatterForCulture = MakeShareable(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createCurrencyInstance(ICULocale, ICUStatus)));
		checkf(CurrencyFormatterForCulture.IsValid(), TEXT("Creating a currency format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
	}

	if (!bUseDefaultFormattingRules)
	{
		// Set the custom currency before we extract the data from the formatter
		icu::UnicodeString ICUCurrencyCode = ICUUtilities::ConvertString(SanitizedCurrencyCode);
		CurrencyFormatterForCulture->setCurrency(ICUCurrencyCode.getBuffer());
	}

	const FDecimalNumberFormattingRules NewUECurrencyFormattingRules = ExtractNumberFormattingRulesFromICUDecimalFormatter(*CurrencyFormatterForCulture);

	if (bUseDefaultFormattingRules)
	{
		// Check the pointer again in case another thread beat us to it
		{
			FScopeLock PtrLock(&UECurrencyFormattingRulesCS);

			if (!UECurrencyFormattingRules.IsValid())
			{
				UECurrencyFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUECurrencyFormattingRules));
			}
		}

		return *UECurrencyFormattingRules;
	}
	else
	{
		FScopeLock MapLock(&UEAlternateCurrencyFormattingRulesCS);

		// Find again in case another thread beat us to it
		auto FoundUEAlternateCurrencyFormattingRules = UEAlternateCurrencyFormattingRules.FindRef(SanitizedCurrencyCode);
		if (FoundUEAlternateCurrencyFormattingRules.IsValid())
		{
			return *FoundUEAlternateCurrencyFormattingRules;
		}

		FoundUEAlternateCurrencyFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUECurrencyFormattingRules));
		UEAlternateCurrencyFormattingRules.Add(SanitizedCurrencyCode, FoundUEAlternateCurrencyFormattingRules);
		return *FoundUEAlternateCurrencyFormattingRules;
	}
}

ETextPluralForm FCulture::FICUCultureImplementation::GetPluralForm(int32 Val, const ETextPluralType PluralType)
{
	checkf(Val >= 0, TEXT("GetPluralFormImpl requires a positive value"));

	const icu::PluralRules* ICUPluralRules = (PluralType == ETextPluralType::Cardinal) ? ICUCardinalPluralRules : ICUOrdianalPluralRules;
	const icu::UnicodeString ICUPluralFormTag = ICUPluralRules->select(Val);

	return ICUPluralFormToUE(ICUPluralFormTag);
}

ETextPluralForm FCulture::FICUCultureImplementation::GetPluralForm(double Val, const ETextPluralType PluralType)
{
	checkf(!FMath::IsNegativeDouble(Val), TEXT("GetPluralFormImpl requires a positive value"));

	const icu::PluralRules* ICUPluralRules = (PluralType == ETextPluralType::Cardinal) ? ICUCardinalPluralRules : ICUOrdianalPluralRules;
	const icu::UnicodeString ICUPluralFormTag = ICUPluralRules->select(Val);

	return ICUPluralFormToUE(ICUPluralFormTag);
}

#endif
