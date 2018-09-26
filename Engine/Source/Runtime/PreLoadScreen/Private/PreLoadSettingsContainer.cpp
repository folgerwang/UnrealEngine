#include "PreLoadSettingsContainer.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/UnicodeBlockRange.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"

#include "Internationalization/Culture.h"

FPreLoadSettingsContainerBase* FPreLoadSettingsContainerBase::Instance;

FPreLoadSettingsContainerBase::~FPreLoadSettingsContainerBase()
{
    for (auto KVPair : BrushResources)
    {
        FSlateApplication::Get().GetRenderer()->ReleaseDynamicResource(*KVPair.Value);
		delete KVPair.Value;
    }
    BrushResources.Empty();
    LocalizedTextResources.Empty();
    FontResources.Empty();
    ScreenGroupings.Empty();

    Instance = nullptr;
}

bool FPreLoadSettingsContainerBase::IsValidBrushConfig(TArray<FString>& SplitConfigEntry)
{
    return (SplitConfigEntry.Num() == 4);
}

void FPreLoadSettingsContainerBase::ParseBrushConfigEntry(const FString& ConfigEntry)
{
    TArray<FString> BrushComponents;
    ConfigEntry.ParseIntoArray(BrushComponents, TEXT(","), true);
    if (ensureAlwaysMsgf(IsValidBrushConfig(BrushComponents), TEXT("Invalid Custom Brush in config. Exptected Format: +CustomImageBrushes=(Identifier,Filename,Width,Height). Config Entry: %s"), *ConfigEntry))
    {
        //Clean up the identifier to remove extra spaces and the first (
        FString Identifier = BrushComponents[0];
        Identifier.TrimStartAndEndInline();
        Identifier.RemoveFromStart("(");

        FString FilePath = ConvertIfPluginRelativeContentPath(BrushComponents[1]);

        float Width = FCString::Atof(*BrushComponents[2]);
        float Height = FCString::Atof(*BrushComponents[3]);

        CreateCustomSlateImageBrush(Identifier, FilePath, FVector2D(Width, Height));
    }
}

bool FPreLoadSettingsContainerBase::IsValidFontConfigString(TArray<FString>& SplitConfigEntry)
{
    return (SplitConfigEntry.Num() == 3);
}

void FPreLoadSettingsContainerBase::ParseFontConfigEntry(const FString& SplitConfigEntry)
{
    TArray<FString> FontComponents;
    SplitConfigEntry.ParseIntoArray(FontComponents, TEXT(","), true);
    if (ensureAlwaysMsgf(IsValidFontConfigString(FontComponents), TEXT("Invalid Font Entry in config: Expected Format: +CustomFont=(FontIdentifier, Language, FileName) Config Entry: %s"), *SplitConfigEntry))
    {
        FString Identifier = FontComponents[0];
        Identifier.TrimStartAndEndInline();
        Identifier.RemoveFromStart("(");

        FString Language = FontComponents[1];
        Language.TrimStartAndEndInline();

        FString FilePath = FontComponents[2];
        FilePath.TrimStartAndEndInline();
        FilePath.RemoveFromEnd(TEXT(")"));
        FilePath = ConvertIfPluginRelativeContentPath(FilePath);

        BuildCustomFont(Identifier, Language, FilePath);
    }
}

void FPreLoadSettingsContainerBase::BuildCustomFont(const FString& FontIdentifier, const FString& Language, const FString& FilePath)
{
    //Try and find existing font, if we can't, make a new one
    TSharedPtr<FCompositeFont>& FontToBuild = FontResources.FindOrAdd(*FontIdentifier);
    if (!FontToBuild.IsValid())
    {
        FontToBuild = MakeShared<FCompositeFont>();
    }

    if (ensureAlwaysMsgf(FontToBuild.IsValid(), TEXT("Error creating custom font!")))
    {
        //If En, then setup as default font
        if (Language.Equals(TEXT("en")))
        {
            FontToBuild->DefaultTypeface.AppendFont(*FontIdentifier, FilePath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
        }
        // if not en, we need to setup some block ranges and subfonts to handle special characters
        else
        {
            TArray<EUnicodeBlockRange> BlockRangesForLanguage;
            //Arabic
            if (Language.Equals(TEXT("ar"), ESearchCase::IgnoreCase))
            {
                BlockRangesForLanguage.Add(EUnicodeBlockRange::Arabic);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::ArabicExtendedA);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::ArabicMathematicalAlphabeticSymbols);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::ArabicPresentationFormsA);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::ArabicPresentationFormsB);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::ArabicSupplement);
            }
            //Japanese
            else if (Language.Equals(TEXT("ja"), ESearchCase::IgnoreCase))
            {
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibility);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityForms);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityIdeographs);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityIdeographsSupplement);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKRadicalsSupplement);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKStrokes);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKSymbolsAndPunctuation);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographs);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionA);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionB);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionC);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionD);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionE);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::EnclosedCJKLettersAndMonths);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::Hiragana);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::Katakana);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::KatakanaPhoneticExtensions);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::Kanbun);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::HalfwidthAndFullwidthForms);
            }
            //Korean
            else if (Language.Equals(TEXT("ko"), ESearchCase::IgnoreCase))
            {
                BlockRangesForLanguage.Add(EUnicodeBlockRange::HangulJamo);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::HangulJamoExtendedA);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::HangulJamoExtendedB);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::HangulCompatibilityJamo);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::HangulSyllables);
            }
            //Simplified Chinese
            else if (Language.Equals(TEXT("zh-hans"), ESearchCase::IgnoreCase))
            {
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibility);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityForms);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityIdeographs);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityIdeographsSupplement);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKRadicalsSupplement);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKStrokes);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKSymbolsAndPunctuation);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographs);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionA);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionB);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionC);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionD);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionE);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::EnclosedCJKLettersAndMonths);
            }
            //Traditional Chinese
            else if (Language.Equals(TEXT("zh-hant"), ESearchCase::IgnoreCase))
            {
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibility);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityForms);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityIdeographs);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKCompatibilityIdeographsSupplement);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKRadicalsSupplement);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKStrokes);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKSymbolsAndPunctuation);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographs);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionA);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionB);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionC);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionD);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::CJKUnifiedIdeographsExtensionE);
                BlockRangesForLanguage.Add(EUnicodeBlockRange::EnclosedCJKLettersAndMonths);
            }

            //Build out actual sub font ranges
            FCompositeSubFont& SubFont = FontToBuild->SubTypefaces[FontToBuild->SubTypefaces.AddDefaulted()];
            SubFont.Cultures.Append(Language);
            for (EUnicodeBlockRange& BlockRange : BlockRangesForLanguage)
            {
                SubFont.CharacterRanges.Add(FUnicodeBlockRange::GetUnicodeBlockRange(BlockRange).Range);
            }

            //Finally append actual font
            SubFont.Typeface.AppendFont(*FontIdentifier, FilePath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
        }
    }
}

bool FPreLoadSettingsContainerBase::IsValidLocalizedTextConfigString(TArray<FString>& SplitConfigEntry)
{
    return (SplitConfigEntry.Num() == 4);
}

void FPreLoadSettingsContainerBase::ParseLocalizedTextConfigString(const FString& ConfigEntry)
{
    TArray<FString> LocalizedTextComponents;
    ConfigEntry.ParseIntoArray(LocalizedTextComponents, TEXT(","), true);
    if (ensureAlwaysMsgf(IsValidLocalizedTextConfigString(LocalizedTextComponents), TEXT("Invalid Localized Text Entry in config: Expected Format: +LocalizedText=(TextIdentifier, NS Localized Text) Config Entry: %s"), *ConfigEntry))
    {
        //Clean up the identifier to remove extra spaces and the first (
        FString Identifier = LocalizedTextComponents[0];
        Identifier.TrimStartAndEndInline();
        Identifier.RemoveFromStart("(");

        //LocalizedTextComponents[1] is the NameSpace for the loctext
        FString LocNameSpace = LocalizedTextComponents[1];
        LocNameSpace.TrimStartAndEndInline();
        LocNameSpace.RemoveFromStart("NSLOCTEXT(\"");
        LocNameSpace.RemoveFromEnd("\"");

        //LocalizedTextComponents[2] is the identifier for the FText
        FString LocIdentifier = LocalizedTextComponents[2];
        LocIdentifier.TrimStartAndEndInline();
        LocIdentifier.RemoveFromStart("\"");
        LocIdentifier.RemoveFromEnd("\"");

        //LocalizedTextComponents[3] is the default text for the FText
        FString LocInitialValue = LocalizedTextComponents[3];
        LocInitialValue.TrimStartAndEndInline();
        LocInitialValue.RemoveFromStart("\"");
        LocInitialValue.RemoveFromEnd(")"); //remove these separately so that if the file is missing 1 ) or the " is out of order it still works
        LocInitialValue.RemoveFromEnd(")");
        LocInitialValue.RemoveFromEnd("\"");

        //Actually try to add the FText to our list by finding it in the FText collection (should already be in there due to Localization system)
        FText FoundText = FText::GetEmpty();
        if (FText::FindText(LocNameSpace, LocIdentifier, FoundText))
        {
            AddLocalizedText(Identifier, FoundText);
        }
        //We couldn't find it already, so go ahead and add a version to FText with an initial value. This one won't be localized, but that may be intended
        else
        {
            AddLocalizedText(Identifier, FText::FromString(LocInitialValue));
        }
    }
}

bool FPreLoadSettingsContainerBase::IsValidScreenGrooupingConfigString(TArray<FString>& SplitConfigEntry)
{
    return (SplitConfigEntry.Num() == 4);
}

void FPreLoadSettingsContainerBase::ParseScreenGroupingConfigString(const FString& ConfigEntry)
{
    TArray<FString> ScreenGroupingComponents;
    ConfigEntry.ParseIntoArray(ScreenGroupingComponents, TEXT(","), true);

    if (ensureAlwaysMsgf(IsValidScreenGrooupingConfigString(ScreenGroupingComponents), TEXT("Invalid ScreenGrouping Entry in config: Expected Format: +ScreenGrouping(ScreenIdentifier, Brush Identifier, Text Identifier, Font Size) Config Entry: %s"), *ConfigEntry))
    {
        //Clean up the identifier to remove extra spaces and the first (
        FString GroupIdentifier = ScreenGroupingComponents[0];
        GroupIdentifier.TrimStartAndEndInline();
        GroupIdentifier.RemoveFromStart("(");

        FString BrushIdentifier = ScreenGroupingComponents[1];
        BrushIdentifier.TrimStartAndEndInline();

        FString TextIdentifier = ScreenGroupingComponents[2];
        TextIdentifier.TrimStartAndEndInline();
        
        float FontSize = FCString::Atof(*ScreenGroupingComponents[3]);

        FScreenGroupingBase NewGrouping(BrushIdentifier, TextIdentifier, FontSize);
        AddScreenGrouping(GroupIdentifier, NewGrouping);
    }
}

FString FPreLoadSettingsContainerBase::ConvertIfPluginRelativeContentPath(const FString& FilePath)
{
    FString ReturnPath = FilePath.TrimStartAndEnd();
    if (!FPaths::FileExists(ReturnPath))
    {
        ReturnPath = PluginContentDir / ReturnPath;
    }

    ensureAlwaysMsgf(FPaths::FileExists(ReturnPath), TEXT("Can not find specified file %s"), *ReturnPath);
    return ReturnPath;
}

void FPreLoadSettingsContainerBase::CreateCustomSlateImageBrush(const FString& Identifier, const FString& TexturePath, const FVector2D& ImageDimensions)
{
    BrushResources.Add(*Identifier, new FSlateDynamicImageBrush(*TexturePath, ImageDimensions));

    //Make sure this dynamic image resource is registered with the SlateApplication
    FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(*TexturePath);
}

void FPreLoadSettingsContainerBase::AddLocalizedText(const FString& Identifier, FText LocalizedText)
{
    LocalizedTextResources.Add(*Identifier, LocalizedText);
}

void FPreLoadSettingsContainerBase::AddScreenGrouping(const FString& Identifier, FScreenGroupingBase& ScreenGrouping)
{
    ScreenGroupings.Add(*Identifier, ScreenGrouping);
}

const FSlateDynamicImageBrush* FPreLoadSettingsContainerBase::GetBrush(const FString& Identifier)
{
    const FSlateDynamicImageBrush*const* FoundBrush = BrushResources.Find(*Identifier);
    return FoundBrush ? *FoundBrush : nullptr;
}

FText FPreLoadSettingsContainerBase::GetLocalizedText(const FString& Identifier)
{
    const FText* FoundText = LocalizedTextResources.Find(*Identifier);
    return FoundText ? *FoundText : FText::GetEmpty();
}

TSharedPtr<FCompositeFont> FPreLoadSettingsContainerBase::GetFont(const FString& Identifier)
{
    TSharedPtr<FCompositeFont>* FoundFontPointer = FontResources.Find(*Identifier);
    return FoundFontPointer ? *FoundFontPointer : TSharedPtr<FCompositeFont>();
}

FPreLoadSettingsContainerBase::FScreenGroupingBase* FPreLoadSettingsContainerBase::GetScreenGrouping(const FString& Identifier)
{
    return ScreenGroupings.Find(*Identifier);
}