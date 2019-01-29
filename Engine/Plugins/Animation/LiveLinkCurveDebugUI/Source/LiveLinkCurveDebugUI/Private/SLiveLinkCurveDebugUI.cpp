// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "SLiveLinkCurveDebugUI.h"
#include "LiveLinkCurveDebugPrivate.h"

#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "SLiveLinkCurveDebugUIListItem.h"
#include "Framework/Application/SlateApplication.h"

#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SScaleBox.h"


#define LOCTEXT_NAMESPACE "SLiveLinkCurveDebugUI"

void SLiveLinkCurveDebugUI::Construct(const FArguments& InArgs)
{
	this->UpdateRate = InArgs._UpdateRate;
	this->OnSubjectNameChanged = InArgs._OnSubjectNameChanged;

	CachedLiveLinkSubjectName = InArgs._InitialLiveLinkSubjectName;

	//Try and get the LiveLink Client now and cache it off
	CachedLiveLinkClient = nullptr;
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		CachedLiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		ensureAlwaysMsgf((nullptr != CachedLiveLinkClient), TEXT("No valid LiveLinkClient when trying to use a SLiveLinkCurveDebugUI! LiveLinkCurveDebugUI requires LiveLinkClient!"));
	}

	const EVisibility LiveLinkSubjectHeaderVis = InArgs._ShowLiveLinkSubjectNameHeader ? EVisibility::Visible : EVisibility::Collapsed;

	SUserWidget::Construct(SUserWidget::FArguments()
	[
		SNew(SDPIScaler)
		.DPIScale(InArgs._DPIScale)
		[
			SNew(SSafeZone)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				.AutoHeight()
				[
					SNew(STextBlock)
						.Text(this, &SLiveLinkCurveDebugUI::GetLiveLinkSubjectNameHeader)
						.ColorAndOpacity(FLinearColor(.8f, .8f, .8f, 1.0f))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						.Visibility(LiveLinkSubjectHeaderVis)
				]

				+SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					[
						SAssignNew(DebugListView, SListView<TSharedPtr<FLiveLinkDebugCurveNodeBase>>)
						.ListItemsSource(&CurveData)
						.SelectionMode(ESelectionMode::None)
						.OnGenerateRow(this, &SLiveLinkCurveDebugUI::GenerateListRow)
						.HeaderRow
						(
							SNew(SHeaderRow)

							+SHeaderRow::Column(SLiveLinkCurveDebugUIListItem::NAME_CurveName)
							.DefaultLabel(LOCTEXT("CurveName","Curve Name"))
							.FillWidth(.15f)

							+ SHeaderRow::Column(SLiveLinkCurveDebugUIListItem::NAME_CurveValue)
							.DefaultLabel(LOCTEXT("CurveValue", "Curve Value"))
							.FillWidth(.85f)
						)
					]
				]
			]
		]
	]);

	//Kick off initial CurveData generation
	UpdateCurveData();
	NextUpdateTime = static_cast<double>(UpdateRate) + FSlateApplication::Get().GetCurrentTime();
}

TSharedRef<ITableRow> SLiveLinkCurveDebugUI::GenerateListRow(TSharedPtr<FLiveLinkDebugCurveNodeBase> InItem, const TSharedRef<STableViewBase>& InOwningTable)
{
	return SNew(SLiveLinkCurveDebugUIListItem, InOwningTable)
		.CurveInfo(InItem);
}

void SLiveLinkCurveDebugUI::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (DebugListView.IsValid())
	{
		const double CurrentTime = FSlateApplication::Get().GetCurrentTime();
		if (CurrentTime > NextUpdateTime)
		{
			UpdateCurveData();
			NextUpdateTime = static_cast<double>(UpdateRate) + CurrentTime;

			DebugListView->RequestListRefresh();
		}
	}

	SUserWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SLiveLinkCurveDebugUI::UpdateCurveData()
{
	CurveData.Reset();

	//If we don't have a good live link subject name, lets try to get one
	if (!CachedLiveLinkSubjectName.IsValid() || CachedLiveLinkSubjectName.IsNone())
	{
		ChangeToNextValidLiveLinkSubjectName();
	}

	if (ensureMsgf((nullptr!= CachedLiveLinkClient), TEXT("No valid LiveLinkClient! Can not update curve data for LiveLinkCurveDebugUI")))
	{
		const FLiveLinkSubjectFrame* SubjectFrame = CachedLiveLinkClient->GetSubjectData(CachedLiveLinkSubjectName);
		if (SubjectFrame && (SubjectFrame->Curves.Num() > 0))
		{
			for (int CurveIndex = 0; CurveIndex < SubjectFrame->CurveKeyData.CurveNames.Num(); ++CurveIndex)
			{
				FName CurveName = SubjectFrame->CurveKeyData.CurveNames[CurveIndex];

				float CurveValueToSet = 0.0f;
				if (SubjectFrame->Curves.IsValidIndex(CurveIndex))
				{
					const FOptionalCurveElement& CurveElement = SubjectFrame->Curves[CurveIndex];
					if (CurveElement.bValid)
					{
						CurveValueToSet = CurveElement.Value;
					}
				}

				CurveData.Add(MakeShareable<FLiveLinkDebugCurveNodeBase>(new FLiveLinkDebugCurveNodeBase(CurveName, CurveValueToSet)));
			}
		}
		//Just show an error curve message until we have a frame for the client.
		else
		{
			CurveData.Reset();

			const FText NoCurvesText = LOCTEXT("NoCurvesForSubject", "No Curve Data");
			CurveData.Add(MakeShareable<FLiveLinkDebugCurveNodeBase>(new FLiveLinkDebugCurveNodeBase(*NoCurvesText.ToString(), 0.0f)));
		}
	}
}

void SLiveLinkCurveDebugUI::ChangeToNextValidLiveLinkSubjectName()
{
	if (nullptr != CachedLiveLinkClient)
	{
		TArray<FName> AllSubjectNames;
		CachedLiveLinkClient->GetSubjectNames(AllSubjectNames);

		if (AllSubjectNames.Num() > 0)
		{
			bool bFindOldNameFirst = (CachedLiveLinkSubjectName.IsValid() && !CachedLiveLinkSubjectName.IsNone());
			bool bFoundOldName = false;
			FName FirstValidResult;
			FName SubjectNameToActuallySetTo;

			for (FName SubjectName : AllSubjectNames)
			{
				const FLiveLinkSubjectFrame* SubjectFrame = CachedLiveLinkClient->GetSubjectData(SubjectName);
				if (SubjectFrame && (SubjectFrame->Curves.Num() > 0))
				{
					//If we aren't looking for our old name first, and we have found a valid name, just use it!
					if (!bFindOldNameFirst)
					{
						SubjectNameToActuallySetTo = SubjectName;
						break;
					}
					//If we have already found our OldName, and this is a new valid name, use it!
					else if (bFoundOldName)
					{
						SubjectNameToActuallySetTo = SubjectName;
						break;
					}
					//We have found our old name, so mark that so we know to use the next valid name
					else if (SubjectName == CachedLiveLinkSubjectName)
					{
						bFoundOldName = true;
					}
					//We have found a valid hit, but we are still looking for our OldNameFirst, save this off
					//in case we don't find any other valid results so we can use this one
					else if (FirstValidResult.IsNone())
					{
						FirstValidResult = SubjectName;
					}
				}
			}

			//If we didn't find a valid result after our OldName, but we did find a valid result, use that.
			//This is basically looping back to the first valid result in a list when no valid results were between
			//the old name and the end of the list
			if (SubjectNameToActuallySetTo.IsNone() && !FirstValidResult.IsNone())
			{
				SubjectNameToActuallySetTo = FirstValidResult;
			}

			//Only call set if we have found a valid subject name, otherwise we will just stay with our current cached name
			if (SubjectNameToActuallySetTo.IsValid() && !SubjectNameToActuallySetTo.IsNone())
			{
				SetLiveLinkSubjectName(SubjectNameToActuallySetTo);
			}
		}
	}
}

void SLiveLinkCurveDebugUI::GetAllSubjectNames(TArray<FName>& OutSubjectNames) const
{
	OutSubjectNames.Reset();

	if (ensureAlwaysMsgf(CachedLiveLinkClient, TEXT("No valid CachedLiveLinkClient when attempting to use SLiveLinkCurveDebugUI::GetAllSubjectNames! The SLiveLinkCurveDebugUI should always have a cached live link client!")))
	{
		CachedLiveLinkClient->GetSubjectNames(OutSubjectNames);
	}
}

void SLiveLinkCurveDebugUI::SetLiveLinkSubjectName(FName SubjectName)
{
	if (SubjectName != CachedLiveLinkSubjectName)
	{
		CachedLiveLinkSubjectName = SubjectName;
		UE_LOG(LogLiveLinkCurveDebugUI, Display, TEXT("Set LiveLinkSubjectName: %s"), *CachedLiveLinkSubjectName.ToString());

		//Update next tick
		NextUpdateTime = FSlateApplication::Get().GetCurrentTime();

		if (OnSubjectNameChanged.IsBound())
		{
			OnSubjectNameChanged.Execute(CachedLiveLinkSubjectName);
		}
	}
}

FText SLiveLinkCurveDebugUI::GetLiveLinkSubjectNameHeader() const
{
	return FText::Format(LOCTEXT("LiveLinkSubjectNameHeader", "Currently Viewing: {0}"), FText::FromName(CachedLiveLinkSubjectName));
}

TSharedRef<SLiveLinkCurveDebugUI> SLiveLinkCurveDebugUI::New()
{
	return MakeShareable(new SLiveLinkCurveDebugUI());
}

#undef LOCTEXT_NAMESPACE