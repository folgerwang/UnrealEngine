#include "OculusToolWidget.h"
#include "OculusEditorSettings.h"
#include "OculusHMD.h"
#include "DetailLayoutBuilder.h"
#include "Engine/RendererSettings.h"
#include "Engine/Blueprint.h"
#include "GeneralProjectSettings.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "UObject/EnumProperty.h"
#include "EdGraph/EdGraph.h"
#include "UnrealEdMisc.h"

#define CALL_MEMBER_FUNCTION(object, memberFn) ((object).*(memberFn))

#define LOCTEXT_NAMESPACE "OculusToolWidget"

// Misc notes and known issues:
// * I save after every change because UE4 wasn't prompting to save on exit, but this makes it tough for users to undo, and doesn't prompt shader rebuild. Alternatives?

TSharedRef<SHorizontalBox> SOculusToolWidget::CreateSimpleSetting(SimpleSetting* setting)
{
	auto box = SNew(SHorizontalBox).Visibility(this, &SOculusToolWidget::IsVisible, setting->tag)
		+ SHorizontalBox::Slot().FillWidth(10).VAlign(VAlign_Center)
		[
			SNew(SRichTextBlock)
			.Visibility(this, &SOculusToolWidget::IsVisible, setting->tag)
		.DecoratorStyleSet(&FEditorStyle::Get())
		.Text(setting->description).AutoWrapText(true)
		+ SRichTextBlock::HyperlinkDecorator(TEXT("HyperlinkDecorator"), this, &SOculusToolWidget::OnBrowserLinkClicked)
		];
	if (setting->ClickFunc != NULL)
	{
		box.Get().AddSlot()
			.AutoWidth().VAlign(VAlign_Top)
			[
				SNew(SButton)
				.Text(setting->buttonText)
			.OnClicked(this, setting->ClickFunc, true)
			.Visibility(this, &SOculusToolWidget::IsVisible, setting->tag)
			];
	}
	box.Get().AddSlot().AutoWidth().VAlign(VAlign_Top)
		[
			SNew(SButton)
			.Text(LOCTEXT("IgnorePerfRec", "Ignore"))
		.OnClicked(this, &SOculusToolWidget::IgnoreRecommendation, setting->tag)
		.Visibility(this, &SOculusToolWidget::IsVisible, setting->tag)
		];
	return box;
}

EVisibility SOculusToolWidget::IsVisible(FName tag) const
{
	const SimpleSetting* setting = SimpleSettings.Find(tag);
	checkf(setting != NULL, TEXT("Failed to find tag %s."), *tag.ToString());
	if(SettingIgnored(setting->tag)) return EVisibility::Collapsed;
	UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
	EOculusPlatform targetPlatform = EditorSettings->PerfToolTargetPlatform;
	 
	if(targetPlatform == EOculusPlatform::Mobile && !((int)setting->supportMask & (int)SupportFlags::SupportMobile)) return EVisibility::Collapsed;
	if(targetPlatform == EOculusPlatform::PC && !((int)setting->supportMask & (int)SupportFlags::SupportPC)) return EVisibility::Collapsed;

	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	const bool bForwardShading = Settings->bForwardShading != 0;
	if (bForwardShading && ((int)setting->supportMask & (int)SupportFlags::ExcludeForward)) return EVisibility::Collapsed;
	if (!bForwardShading && ((int)setting->supportMask & (int)SupportFlags::ExcludeDeferred)) return EVisibility::Collapsed;

	return CALL_MEMBER_FUNCTION(*this, setting->VisFunc)(setting->tag);
}

void SOculusToolWidget::AddSimpleSetting(TSharedRef<SVerticalBox> box, SimpleSetting* setting)
{
		box.Get().AddSlot().AutoHeight()
		.Padding(5, 5)
		[
			CreateSimpleSetting(setting)
		];
}

bool SOculusToolWidget::SettingIgnored(FName settingKey) const
{
	UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
	bool* ignoreSetting = EditorSettings->PerfToolIgnoreList.Find(settingKey);
	return (ignoreSetting != NULL && *ignoreSetting == true);
}

TSharedRef<SVerticalBox> SOculusToolWidget::NewCategory(TSharedRef<SScrollBox> scroller, FText heading)
{
	scroller.Get().AddSlot()
	.Padding(0, 0)
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder") )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().Padding(5,5).FillWidth(1)
			[
				SNew(SRichTextBlock)
				.TextStyle(FEditorStyle::Get(), "ToolBar.Heading")
				.DecoratorStyleSet(&FEditorStyle::Get()).AutoWrapText(true)
				.Text(heading)
				+ SRichTextBlock::HyperlinkDecorator(TEXT("HyperlinkDecorator"), this, &SOculusToolWidget::OnBrowserLinkClicked)
			]
		]
	];

	TSharedPtr<SVerticalBox> box;
	scroller.Get().AddSlot()
	.Padding(0, 0, 0, 2)
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SAssignNew(box, SVerticalBox)
		]
	];
	return box.ToSharedRef();
}

void SOculusToolWidget::RebuildLayout()
{
	if (!ScrollingContainer.IsValid()) return;
	TSharedRef<SScrollBox> scroller = ScrollingContainer.ToSharedRef();

	UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
	uint8 initiallySelected = 0;
	for (uint8 i = 0; i < (uint8)EOculusPlatform::Length; ++i)
	{
		if ((uint8)EditorSettings->PerfToolTargetPlatform == i)
		{
			initiallySelected = i;
		}
	}

	scroller.Get().ClearChildren();

	scroller.Get().AddSlot()
	.Padding(2, 2)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			//.BorderImage( FEditorStyle::GetBrush("ToolPanel.LightGroupBorder") ).Visibility(this, &SOculusToolWidget::RestartVisible)
			.BorderImage( FEditorStyle::GetBrush("SceneOutliner.ChangedItemHighlight") ).Visibility(this, &SOculusToolWidget::RestartVisible)
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder") )
				.Padding(2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(10).VAlign(VAlign_Center)
					[
						SNew(SRichTextBlock)
						.Text(LOCTEXT("RestartRequired", "<RichTextBlock.TextHighlight>Restart required:You have made changes that require an editor restart to take effect.</>")).DecoratorStyleSet(&FEditorStyle::Get())
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
					[
						SNew(SButton)
						.Text(LOCTEXT("RestartNow", "Restart Editor"))
						.OnClicked(this, &SOculusToolWidget::OnRestartClicked)
					]
				]
			]
		]
	];
	
	TSharedRef<SVerticalBox> box = NewCategory(scroller, LOCTEXT("GeneralSettings", "<RichTextBlock.Bold>General Settings</>"));

	box.Get().AddSlot()
	.Padding(5, 5)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(10).VAlign(VAlign_Top)
		[
			SNew(SRichTextBlock)
			.Text(LOCTEXT("TargetPlatform", "Target Platform: "))
		]
		+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Top)
		[
			SNew(STextComboBox)
			.OptionsSource( &Platforms )
			.InitiallySelectedItem(Platforms[initiallySelected])
			.OnSelectionChanged( this, &SOculusToolWidget::OnChangePlatform )
		]
	];

	AddSimpleSetting(box, SimpleSettings.Find(FName("StartInVR")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("SupportDash")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("ForwardShading")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("AllowStaticLighting")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("InstancedStereo")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileMultiView")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileHDR")));

	box = NewCategory(scroller, LOCTEXT("PostProcessHeader", "<RichTextBlock.Bold>Post-Processing Settings:</>\nThe below settings all refer to your project's post-processing settings. Post-processing can be very expensive in VR, so we recommend disabling many expensive post-processing effects. You can fine-tune your post-processing settings with a Post Process Volume. <a href=\"https://docs.unrealengine.com/en-us/Platforms/VR/VRPerformance\" id=\"HyperlinkDecorator\">Read more.</>."));
	AddSimpleSetting(box, SimpleSettings.Find(FName("LensFlare")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("AntiAliasing")));

	DynamicLights.Empty();

	for (TObjectIterator<ULightComponent> LightItr; LightItr; ++LightItr)
	{
		AActor* owner = LightItr->GetOwner();
		if (owner != NULL && (owner->IsRootComponentStationary() || owner->IsRootComponentMovable()) && !owner->IsHiddenEd() && owner->IsEditable() && owner->IsSelectable() && LightItr->GetWorld() == GEditor->GetEditorWorldContext().World())
		{
			FString lightIgnoreKey = "IgnoreLight_" + LightItr->GetName();
			if (!SettingIgnored(FName(lightIgnoreKey.GetCharArray().GetData())))
			{
				DynamicLights.Add(LightItr->GetName(), TWeakObjectPtr<ULightComponent>(*LightItr));
			}
		}
	}

	if (DynamicLights.Num() > 0)
	{
		box = NewCategory(scroller, LOCTEXT("DynamicLightsHeader", "<RichTextBlock.Bold>Dynamic Lights:</>\nThe following lights are not static. They will use dynamic lighting instead of lightmaps, and will be much more expensive on the GPU. (Most of the cost will show up in the GPU profiler as ShadowDepths and ShadowProjectonOnOpaque.) In some cases they will also give superior results. This is a fidelity-performance tradeoff. <a href=\"https://docs.unrealengine.com/en-us/Engine/Rendering/LightingAndShadows/LightMobility\" id=\"HyperlinkDecorator\">Read more.</>\nFixes: select the light and change its mobility to stationary to pre-compute its lighting. You will need to rebuild lightmaps. Alternatively, you can disable Cast Shadows."));

		for (auto it = DynamicLights.CreateIterator(); it; ++it)
		{
			box.Get().AddSlot()
			.Padding(5, 5)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(5).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(it->Key))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SelectLight", "Select Light"))
					.OnClicked(this, &SOculusToolWidget::SelectLight, it->Key)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("IgnoreLight", "Ignore Light"))
					.OnClicked(this, &SOculusToolWidget::IgnoreLight, it->Key)
				]
			];
		}
	}

	box = NewCategory(scroller, LOCTEXT("MiscHeader", ""));
	box.Get().AddSlot()
	.Padding(10, 5)
	[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(10)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UnhidePerfIgnores", "Unhide all ignored recommendations.")).AutoWrapText(true)
					.Visibility(this, &SOculusToolWidget::CanUnhideIgnoredRecommendations)
				]
			+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("UnhidePerfIgnoresButton", "Unhide"))
					.OnClicked(this, &SOculusToolWidget::UnhideIgnoredRecommendations)
					.Visibility(this, &SOculusToolWidget::CanUnhideIgnoredRecommendations)
				]
	];
	box.Get().AddSlot()
	.Padding(10, 5).AutoHeight()
	[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(10)
			+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshButton", "Refresh"))
					.OnClicked(this, &SOculusToolWidget::UnhideIgnoredRecommendations)
				]
	];
}

void SOculusToolWidget::Construct(const FArguments& InArgs)
{
	pendingRestart = false;
	PlatformEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EOculusPlatform"), true);
	Platforms.Reset(2);

	UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
	for (uint8 i = 0; i < (uint8)EOculusPlatform::Length; ++i)
	{
		Platforms.Add(MakeShareable(new FString(PlatformEnum->GetDisplayNameTextByIndex((int64)i).ToString())));
	}

	PostProcessVolume = NULL;
	for (TActorIterator<APostProcessVolume> ActorItr(GEditor->GetEditorWorldContext().World()); ActorItr; ++ActorItr)
	{
		PostProcessVolume = *ActorItr;
	}

	SimpleSettings.Add(FName("StartInVR"), {
		FName("StartInVR"),
		LOCTEXT("StartInVRDescription", "Enable the \"Start in VR\" setting to ensure your app starts in VR. (You can also ignore this and pass -vr at the command line.)"),
		LOCTEXT("StartInVRButtonText", "Enable Start in VR"),
		&SOculusToolWidget::StartInVRVisibility,
		&SOculusToolWidget::StartInVREnable,
		(int)SupportFlags::SupportMobile | (int)SupportFlags::SupportPC
	});

	SimpleSettings.Add(FName("SupportDash"), {
		FName("SupportDash"),
		LOCTEXT("SupportDashDescription", "Dash support is not enabled. Click to enable it, but make sure to handle the appropriate focus events. <a href=\"https://developer.oculus.com/documentation/unreal/latest/concepts/unreal-dash/\" id=\"HyperlinkDecorator\">Read more.</>"),
		LOCTEXT("SupportDashButtonText", "Enable Dash Support"),
		&SOculusToolWidget::SupportDashVisibility,
		&SOculusToolWidget::SupportDashEnable,
		(int)SupportFlags::SupportMobile | (int)SupportFlags::SupportPC
	});

	SimpleSettings.Add(FName("ForwardShading"), {
		FName("ForwardShading"),
		LOCTEXT("ForwardShadingDescription", "Forward shading is not enabled for this project. Forward shading is often better suited for VR rendering. <a href=\"https://docs.unrealengine.com/en-us/Engine/Performance/ForwardRenderer\" id=\"HyperlinkDecorator\">Read more.</>"),
		LOCTEXT("ForwardShadingButtonText", "Enable Forward Shading"),
		&SOculusToolWidget::ForwardShadingVisibility,
		&SOculusToolWidget::ForwardShadingEnable,
		(int)SupportFlags::SupportMobile | (int)SupportFlags::SupportPC
	});

	SimpleSettings.Add(FName("InstancedStereo"), {
		FName("InstancedStereo"),
		LOCTEXT("InstancedStereoDescription", "Instanced stereo is not enabled for this project. Instanced stereo substantially reduces draw calls, and improves rendering performance."),
		LOCTEXT("InstancedStereoButtonText", "Enable Instanced Stereo"),
		&SOculusToolWidget::InstancedStereoVisibility,
		&SOculusToolWidget::InstancedStereoEnable,
		(int)SupportFlags::SupportPC
	});

	SimpleSettings.Add(FName("MobileMultiView"), {
		FName("MobileMultiView"),
		LOCTEXT("MobileMultiViewDescription", "Enable mobile multi-view and direct mobile multi-view to significantly reduce CPU overhead."),
		LOCTEXT("MobileMultiViewButton", "Enable Multi-View"),
		&SOculusToolWidget::MobileMultiViewVisibility,
		&SOculusToolWidget::MobileMultiViewEnable,
		(int)SupportFlags::SupportMobile
	});

	SimpleSettings.Add(FName("MobileHDR"), {
		FName("MobileHDR"),
		LOCTEXT("MobileHDRDescription", "Mobile HDR has performance and stability issues in VR. We strongly recommend disabling it."),
		LOCTEXT("MobileHDRButton", "Disable Mobile HDR"),
		&SOculusToolWidget::MobileHDRVisibility,
		&SOculusToolWidget::MobileHDRDisable,
		(int)SupportFlags::SupportMobile
	});

	// Post-Processing Settings
	SimpleSettings.Add(FName("LensFlare"), {
		FName("LensFlare"),
		LOCTEXT("LensFlareDescription", "Lens flare is enabled. It can be expensive, and exhibit visible artifacts in VR."),
		LOCTEXT("LensFlareButton", "Disable Lens Flare"),
		&SOculusToolWidget::LensFlareVisibility,
		&SOculusToolWidget::LensFlareDisable,
		(int)SupportFlags::SupportMobile | (int)SupportFlags::SupportPC
	});

	// Only used for PC right now. Mobile MSAA is a separate setting.
	SimpleSettings.Add(FName("AntiAliasing"), {
		FName("AntiAliasing"),
		LOCTEXT("AntiAliasingDescription", "The forward render supports MSAA and Temporal anti-aliasing. Enable one of these for the best VR visual-performance tradeoff. (This button will enable temporal anti-aliasing. You can enable MSAA instead in Edit -> Project Settings -> Rendering.)"),
		LOCTEXT("AntiAliasingButton", "Enable Temporal AA"),
		&SOculusToolWidget::AntiAliasingVisibility,
		&SOculusToolWidget::AntiAliasingEnable,
		(int)SupportFlags::SupportPC | (int)SupportFlags::ExcludeDeferred
	});

	SimpleSettings.Add(FName("AllowStaticLighting"), {
		FName("AllowStaticLighting"),
		LOCTEXT("AllowStaticLightingDescription", "Your project does not allow static lighting. You should only disallow static lighting if you intend for your project to be 100% dynamically lit."),
		LOCTEXT("AllowStaticLightingButton", "Allow Static Lighting"),
		&SOculusToolWidget::AllowStaticLightingVisibility,
		&SOculusToolWidget::AllowStaticLightingEnable,
		(int)SupportFlags::SupportMobile | (int)SupportFlags::SupportPC
	});

	auto scroller = SNew(SScrollBox);
	ScrollingContainer = scroller;
	RebuildLayout();

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.LightGroupBorder") )
			.Padding(2)
			[
				scroller
			]
		];
}

void SOculusToolWidget::OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
	const FString* url = Metadata.Find(TEXT("href"));

	if ( url != NULL )
	{
		FPlatformProcess::LaunchURL(**url, NULL, NULL);
	}
}

FReply SOculusToolWidget::OnRestartClicked()
{
	FUnrealEdMisc::Get().RestartEditor(true);
	return FReply::Handled();
}

EVisibility SOculusToolWidget::RestartVisible() const
{
	return pendingRestart ? EVisibility::Visible : EVisibility::Collapsed;
}

void SOculusToolWidget::OnChangePlatform(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	if (!ItemSelected.IsValid())
	{
		return;
	}

	int32 idx = PlatformEnum->GetIndexByNameString(*ItemSelected);
	if (idx != INDEX_NONE)
	{
		UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
		EditorSettings->PerfToolTargetPlatform = (EOculusPlatform)idx;
		EditorSettings->SaveConfig();
	}
	RebuildLayout();
}

FReply SOculusToolWidget::IgnoreRecommendation(FName tag)
{
	UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
	EditorSettings->PerfToolIgnoreList.Add(tag, true);
	EditorSettings->SaveConfig();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::CanUnhideIgnoredRecommendations() const
{
	UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
	return EditorSettings->PerfToolIgnoreList.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::UnhideIgnoredRecommendations()
{
	UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
	EditorSettings->PerfToolIgnoreList.Empty();
	EditorSettings->SaveConfig();
	RebuildLayout();
	return FReply::Handled();
}

FReply SOculusToolWidget::Refresh()
{
	RebuildLayout();
	return FReply::Handled();
}

void SOculusToolWidget::SuggestRestart()
{
	pendingRestart = true;
}

FReply SOculusToolWidget::ForwardShadingEnable(bool text)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(ANSI_TO_TCHAR("r.ForwardShading"));
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bForwardShading = 1;
	Settings->SaveConfig();
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::ForwardShadingVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	const bool bForwardShading = Settings->bForwardShading != 0;

	return bForwardShading ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::InstancedStereoEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bInstancedStereo = 1;
	Settings->SaveConfig();
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::InstancedStereoVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	const bool bInstancedStereo = Settings->bInstancedStereo != 0;

	return bInstancedStereo ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::MobileMultiViewEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bMobileMultiView = 1;
	Settings->bMobileMultiViewDirect = 1;
	Settings->SaveConfig();
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobileMultiViewVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	const bool bMMV = Settings->bMobileMultiView != 0;
	const bool bMMVD = Settings->bMobileMultiViewDirect != 0;

	return (bMMV && bMMVD) ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::MobileHDRDisable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bMobileHDR = 0;
	Settings->SaveConfig();
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobileHDRVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	return Settings->bMobileHDR == 0 ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::AntiAliasingEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->DefaultFeatureAntiAliasing = EAntiAliasingMethod::AAM_TemporalAA;
	Settings->SaveConfig();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::AntiAliasingVisibility(FName tag) const
{
	// TODO: can we get MSAA level? 2 is fast, 4 is reasonable, anything higher is insane.
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();

	static IConsoleVariable* CVarMSAACount = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MSAACount"));
	CVarMSAACount->Set(4);

	const bool bAADisabled = Settings->bForwardShading && Settings->DefaultFeatureAntiAliasing != EAntiAliasingMethod::AAM_TemporalAA && Settings->DefaultFeatureAntiAliasing != EAntiAliasingMethod::AAM_MSAA;

	return bAADisabled ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::AllowStaticLightingEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bAllowStaticLighting = true;
	Settings->SaveConfig();
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::AllowStaticLightingVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	return Settings->bAllowStaticLighting ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::LensFlareDisable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bDefaultFeatureLensFlare = false;

	if (PostProcessVolume != NULL)
	{
		PostProcessVolume->Settings.bOverride_LensFlareIntensity = 0;
	}
	Settings->SaveConfig();

	return FReply::Handled();
}

EVisibility SOculusToolWidget::LensFlareVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	bool bLensFlare = Settings->bDefaultFeatureLensFlare != 0;

	if (PostProcessVolume != NULL)
	{
		if (PostProcessVolume->Settings.bOverride_LensFlareIntensity != 0)
		{
			bLensFlare = PostProcessVolume->Settings.LensFlareIntensity > 0.0f;
		}
	}

	return bLensFlare ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::SelectLight(FString lightName)
{
	for (TObjectIterator<ULightComponent> LightItr; LightItr; ++LightItr)
	{
		if (LightItr->GetName().Compare(lightName) == 0 && LightItr->GetOwner() != NULL && LightItr->GetWorld() == GEditor->GetEditorWorldContext().World())
		{
			GEditor->SelectNone(true, true);
			GEditor->SelectActor(LightItr->GetAttachmentRootActor(), true, true);
			GEditor->SelectActor(LightItr->GetOwner(), true, true);
			GEditor->SelectComponent(*LightItr, true, true, true);
			break;
		}
	}
	return FReply::Handled();
}

FReply SOculusToolWidget::IgnoreLight(FString lightName)
{
	UOculusEditorSettings* EditorSettings = GetMutableDefault<UOculusEditorSettings>();
	FString lightIgnoreKey = "IgnoreLight_" + lightName;
	EditorSettings->PerfToolIgnoreList.Add(FName(lightIgnoreKey.GetCharArray().GetData()), true);
	EditorSettings->SaveConfig();
	return FReply::Handled();
}

FReply SOculusToolWidget::StartInVREnable(bool text)
{
	UGeneralProjectSettings* Settings = GetMutableDefault<UGeneralProjectSettings>();
	Settings->bStartInVR = 1;
	Settings->SaveConfig();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::StartInVRVisibility(FName tag) const
{
	const UGeneralProjectSettings* Settings = GetDefault<UGeneralProjectSettings>();
	const bool bStartInVR = Settings->bStartInVR != 0;
	return bStartInVR ? EVisibility::Collapsed : EVisibility::Visible;
	return EVisibility::Collapsed;
}

FReply SOculusToolWidget::SupportDashEnable(bool text)
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	GConfig->SetBool(OculusSettings, TEXT("bSupportsDash"), true, GEngineIni);
	return FReply::Handled();
}

EVisibility SOculusToolWidget::SupportDashVisibility(FName tag) const
{
	bool v = false;
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	return (GConfig->GetBool(OculusSettings, TEXT("bSupportsDash"), v, GEngineIni) && v) ? EVisibility::Collapsed : EVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE
