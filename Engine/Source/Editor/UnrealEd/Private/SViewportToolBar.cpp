// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "SViewportToolBar.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "ViewportToolBar"

namespace ToolBarConstants
{
	/** The opacity when we are hovered */
	const float HoveredOpacity = 1.0f;
	/** The opacity when we are not hovered */
	const float NonHoveredOpacity = .75f;
	/** The amount of time to wait before fading out the toolbar after the mouse leaves it (to reduce poping when mouse moves in and out frequently */
	const float TimeToFadeOut = 1.0f;
	/** The amount of time spent actually fading in or out */
	const float FadeTime = .15f;
}

void SViewportToolBar::Construct( const FArguments& InArgs )
{
	bIsHovered = false;

	FadeInSequence = FCurveSequence( 0.0f, ToolBarConstants::FadeTime );
	FadeOutSequence = FCurveSequence( ToolBarConstants::TimeToFadeOut, ToolBarConstants::FadeTime );
	FadeOutSequence.JumpToEnd();
}

TWeakPtr<SMenuAnchor> SViewportToolBar::GetOpenMenu() const
{
	return OpenedMenu;
}

void SViewportToolBar::SetOpenMenu( TSharedPtr< SMenuAnchor >& NewMenu )
{
	if( OpenedMenu.IsValid() && OpenedMenu.Pin() != NewMenu )
	{
		// Close any other open menus
		OpenedMenu.Pin()->SetIsOpen( false );
	}
	OpenedMenu = NewMenu;
}

FLinearColor SViewportToolBar::OnGetColorAndOpacity() const
{
	FLinearColor Color = FLinearColor::White;
	
	if( OpenedMenu.IsValid() && OpenedMenu.Pin()->IsOpen() )
	{
		// Never fade out the toolbar if a menu is open
		Color.A = ToolBarConstants::HoveredOpacity;
	}
	else if( FadeOutSequence.IsPlaying() || !bIsHovered )
	{
		Color.A = FMath::Lerp( ToolBarConstants::HoveredOpacity, ToolBarConstants::NonHoveredOpacity, FadeOutSequence.GetLerp() );
	}
	else
	{
		Color.A = FMath::Lerp( ToolBarConstants::NonHoveredOpacity, ToolBarConstants::HoveredOpacity, FadeInSequence.GetLerp() );
	}

	return Color;
}


void SViewportToolBar::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// The viewport could potentially be moved around inside the toolbar when the mouse is captured
	// If that is the case we do not play the fade transition
	if( !FSlateApplication::Get().IsUsingHighPrecisionMouseMovment() )
	{
		bIsHovered = true;
		if( FadeOutSequence.IsPlaying() )
		{
			// Fade out is already playing so just force the fade in curve to the end so we don't have a "pop" 
			// effect from quickly resetting the alpha
			FadeInSequence.JumpToEnd();
		}
		else
		{
			FadeInSequence.Play( this->AsShared() );
		}
	}

}

void SViewportToolBar::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	// The viewport could potentially be moved around inside the toolbar when the mouse is captured
	// If that is the case we do not play the fade transition
	if( !FSlateApplication::Get().IsUsingHighPrecisionMouseMovment() )
	{
		bIsHovered = false;
		FadeOutSequence.Play( this->AsShared() );
	}
}

FText SViewportToolBar::GetCameraMenuLabelFromViewportType(const ELevelViewportType ViewportType) const
{
	FText Label = LOCTEXT("CameraMenuTitle_Default", "Camera");
	switch (ViewportType)
	{
	case LVT_Perspective:
		Label = LOCTEXT("CameraMenuTitle_Perspective", "Perspective");
		break;

	case LVT_OrthoXY:
		Label = LOCTEXT("CameraMenuTitle_Top", "Top");
		break;

	case LVT_OrthoNegativeXZ:
		Label = LOCTEXT("CameraMenuTitle_Left", "Left");
		break;

	case LVT_OrthoNegativeYZ:
		Label = LOCTEXT("CameraMenuTitle_Front", "Front");
		break;

	case LVT_OrthoNegativeXY:
		Label = LOCTEXT("CameraMenuTitle_Bottom", "Bottom");
		break;

	case LVT_OrthoXZ:
		Label = LOCTEXT("CameraMenuTitle_Right", "Right");
		break;

	case LVT_OrthoYZ:
		Label = LOCTEXT("CameraMenuTitle_Back", "Back");
		break;
	case LVT_OrthoFreelook:
		break;
	}

	return Label;
}

const FSlateBrush* SViewportToolBar::GetCameraMenuLabelIconFromViewportType(const ELevelViewportType ViewportType) const
{
	static FName PerspectiveIcon("EditorViewport.Perspective");
	static FName TopIcon("EditorViewport.Top");
	static FName LeftIcon("EditorViewport.Left");
	static FName FrontIcon("EditorViewport.Front");
	static FName BottomIcon("EditorViewport.Bottom");
	static FName RightIcon("EditorViewport.Right");
	static FName BackIcon("EditorViewport.Back");

	FName Icon = NAME_None;

	switch (ViewportType)
	{
	case LVT_Perspective:
		Icon = PerspectiveIcon;
		break;

	case LVT_OrthoXY:
		Icon = TopIcon;
		break;

	case LVT_OrthoNegativeXZ:
		Icon = LeftIcon;
		break;

	case LVT_OrthoNegativeYZ:
		Icon = FrontIcon;
		break;

	case LVT_OrthoNegativeXY:
		Icon = BottomIcon;
		break;

	case LVT_OrthoXZ:
		Icon = RightIcon;
		break;

	case LVT_OrthoYZ:
		Icon = BackIcon;
		break;
	case LVT_OrthoFreelook:
		break;
	}

	return FEditorStyle::GetBrush(Icon);
}

bool SViewportToolBar::IsViewModeSupported(EViewModeIndex ViewModeIndex) const 
{
	switch (ViewModeIndex)
	{
	case VMI_PrimitiveDistanceAccuracy:
	case VMI_MaterialTextureScaleAccuracy:
	case VMI_RequiredTextureResolution:
		return false;
	default:
		return true;
	}
	return true; 
}

#undef LOCTEXT_NAMESPACE
