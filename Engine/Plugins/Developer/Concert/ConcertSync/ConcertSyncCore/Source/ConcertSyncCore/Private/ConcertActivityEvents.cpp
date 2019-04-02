// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertActivityEvents.h"

#define LOCTEXT_NAMESPACE "ConcertActivity"

namespace ActivityEventsUtil
{
	const FText BoldTextFormat = INVTEXT("<ActivityText.Bold>{0}</>");

	FText ToRichTextBold(const FText& InArgs, bool bToRichText)
	{
		return bToRichText ? FText::Format(BoldTextFormat, InArgs) : InArgs;
	}

	FText ToRichTextBold(const FString& InArgs, bool bToRichText)
	{
		return ToRichTextBold(FText::FromString(InArgs), bToRichText);
	}

	FText ToRichTextBold(const FName& InArgs, bool bToRichText)
	{
		return ToRichTextBold(FText::FromName(InArgs), bToRichText);
	}
};

FConcertActivityEvent::~FConcertActivityEvent()
{
}

FText FConcertActivityEvent::ToDisplayText(bool bRichText) const
{
	return FText();
}

FText FConcertActivityEvent::ToLongDisplayText() const
{
	return ToDisplayText();
}

FText FConcertActivityEvent::GetClientDisplayName() const
{
	return ClientInfo.DisplayName.IsEmpty() ? LOCTEXT("UnknownUser", "Unknown") : FText::FromString(ClientInfo.DisplayName);
}

FConcertConnectionActivityEvent::~FConcertConnectionActivityEvent()
{
}

FText FConcertConnectionActivityEvent::ToDisplayText(bool bRichText) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	return FText::Format(LOCTEXT("ClientJoined", "{UserName} joined the session."), Arguments);
}

FConcertDisconnectionActivityEvent::~FConcertDisconnectionActivityEvent()
{
}

FText FConcertDisconnectionActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	return FText::Format(LOCTEXT("ClientLeaved", "{UserName} left the session."), Arguments);
}

FConcertTransactionActivityEvent::~FConcertTransactionActivityEvent()
{
}

FText FConcertTransactionActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	Arguments.Add(TEXT("Object"), ActivityEventsUtil::ToRichTextBold(ObjectName, bRichText));
	Arguments.Add(TEXT("PackageName"), ActivityEventsUtil::ToRichTextBold(PackageName, bRichText));
	return FText::Format(LOCTEXT("Transaction", "{UserName} modified {Object} in {PackageName}."), Arguments);
}

FText FConcertTransactionActivityEvent::ToLongDisplayText() const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("DisplayText"), ToDisplayText());
	Arguments.Add(TEXT("TransactionIndex"), TransactionIndex);
	Arguments.Add(TEXT("TransactionTitle"), TransactionTitle);
	return FText::Format(LOCTEXT("TransactionLongText", "{DisplayText} Transaction Index: {TransactionIndex}. Transaction Title: {TransactionTitle}."), Arguments);
}

FConcertTransactionRenameActivityEvent::~FConcertTransactionRenameActivityEvent()
{
}

FText FConcertTransactionRenameActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	Arguments.Add(TEXT("OldObjectName"), ActivityEventsUtil::ToRichTextBold(ObjectName, bRichText));
	Arguments.Add(TEXT("PackageName"), ActivityEventsUtil::ToRichTextBold(PackageName, bRichText));
	Arguments.Add(TEXT("NewObjectName"), ActivityEventsUtil::ToRichTextBold(NewObjectName, bRichText));
	return FText::Format(LOCTEXT("TransactionRename", "{UserName} renamed {OldObjectName} to {NewObjectName} in {PackageName}."), Arguments);
}

FConcertTransactionDeleteActivityEvent::~FConcertTransactionDeleteActivityEvent()
{
}

FText FConcertTransactionDeleteActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	Arguments.Add(TEXT("ObjectName"), ActivityEventsUtil::ToRichTextBold(ObjectName, bRichText));
	Arguments.Add(TEXT("PackageName"), ActivityEventsUtil::ToRichTextBold(PackageName, bRichText));
	return FText::Format(LOCTEXT("TransactionDelete", "{UserName} deleted {ObjectName} in {PackageName}."), Arguments);
}

FConcertTransactionCreateActivityEvent::~FConcertTransactionCreateActivityEvent()
{
}

FText FConcertTransactionCreateActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	Arguments.Add(TEXT("ObjectName"), ActivityEventsUtil::ToRichTextBold(ObjectName, bRichText));
	Arguments.Add(TEXT("PackageName"), ActivityEventsUtil::ToRichTextBold(PackageName, bRichText));
	return FText::Format(LOCTEXT("TransactionCreate", "{UserName} created {ObjectName} in {PackageName}."), Arguments);
}

FConcertPackageUpdatedActivityEvent::~FConcertPackageUpdatedActivityEvent()
{
}

FText FConcertPackageUpdatedActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	Arguments.Add(TEXT("PackageName"), ActivityEventsUtil::ToRichTextBold(PackageName, bRichText));
	return FText::Format(LOCTEXT("PackageUpdated", "{UserName} saved the package {PackageName}."), Arguments);
}

FText FConcertPackageUpdatedActivityEvent::ToLongDisplayText() const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("DisplayText"), ToDisplayText());
	Arguments.Add(TEXT("PackageRevision"), Revision); 
	return FText::Format(LOCTEXT("PackageLongText", "{DisplayText} Package Revision: {PackageRevision}."), Arguments);;
}

FConcertPackageAddedActivityEvent::~FConcertPackageAddedActivityEvent()
{
}

FText FConcertPackageAddedActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	Arguments.Add(TEXT("PackageName"), ActivityEventsUtil::ToRichTextBold(PackageName, bRichText));
	return FText::Format(LOCTEXT("PackageAdded", "{UserName} created the package {PackageName}."), Arguments);
}

FConcertPackageDeletedActivityEvent::~FConcertPackageDeletedActivityEvent()
{
}

FText FConcertPackageDeletedActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	Arguments.Add(TEXT("PackageName"), ActivityEventsUtil::ToRichTextBold(PackageName, bRichText));
	return FText::Format(LOCTEXT("PackageDeleted", "{UserName} deleted the package {PackageName}."), Arguments);
}

FConcertPackageRenamedActivityEvent::~FConcertPackageRenamedActivityEvent()
{
}

FText FConcertPackageRenamedActivityEvent::ToDisplayText(bool bRichText /*=false*/) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UserName"), ActivityEventsUtil::ToRichTextBold(GetClientDisplayName(), bRichText));
	Arguments.Add(TEXT("PackageOldName"), ActivityEventsUtil::ToRichTextBold(PackageName, bRichText));
	Arguments.Add(TEXT("PackageNewName"), ActivityEventsUtil::ToRichTextBold(NewPackageName, bRichText));
	return FText::Format(LOCTEXT("PackageRenamed", "{UserName} moved the package {PackageOldName} to {PackageNewName}."), Arguments);
}

#undef LOCTEXT_NAMESPACE
