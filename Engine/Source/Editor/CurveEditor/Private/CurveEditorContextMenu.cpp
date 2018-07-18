// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CurveEditorContextMenu.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/GenericCommands.h"
#include "CurveEditor.h"
#include "CurveEditorCommands.h"

#define LOCTEXT_NAMESPACE "CurveEditorContextMenu"

void FCurveEditorContextMenu::BuildMenu(FMenuBuilder& MenuBuilder, TWeakPtr<FCurveEditor> WeakCurveEditor, TOptional<FCurvePointHandle> ClickedPoint, TOptional<FCurveModelID> HoveredCurveID)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor.IsValid())
	{
		return;
	}

	int32 NumSelectedKeys = CurveEditor->Selection.Count();
	if (NumSelectedKeys > 0)
	{
		MenuBuilder.BeginSection("CurveEditorKeySection", FText::Format(LOCTEXT("CurveEditorKeySection", "{0} Selected {0}|plural(one=Key,other=Keys)"), NumSelectedKeys));
		{
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);

			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().FlattenTangents);
			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().StraightenTangents);
			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().ReduceCurve);
			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().BakeCurve);

			MenuBuilder.AddMenuSeparator();

			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().InterpolationCubicAuto);
			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().InterpolationCubicUser);
			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().InterpolationCubicBreak);
			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().InterpolationLinear);
			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().InterpolationConstant);

			MenuBuilder.AddMenuSeparator();

			MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().InterpolationToggleWeighted);
		}
		MenuBuilder.EndSection();
	}
	else
	{
		const FCurveModel* HoveredCurve = HoveredCurveID.IsSet() ? CurveEditor->FindCurve(HoveredCurveID.GetValue()) : nullptr;
		if (HoveredCurve)
		{
			MenuBuilder.BeginSection("CurveEditorCurveSection", FText::Format(LOCTEXT("CurveNameFormat", "Curve '{0}'"), HoveredCurve->GetDisplayName()));
			{
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().AddKeyHovered);

				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().ReduceCurve);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().BakeCurve);

				MenuBuilder.AddSubMenu(LOCTEXT("PreExtrapText", "Pre-Extrap"), FText(), FNewMenuDelegate::CreateLambda(
					[](FMenuBuilder& SubMenu)
					{
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapCycle);
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapCycleWithOffset);
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapOscillate);
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapLinear);
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapConstant);
					})
				);

				MenuBuilder.AddSubMenu(LOCTEXT("PostExtrapText", "Post-Extrap"), FText(), FNewMenuDelegate::CreateLambda(
					[](FMenuBuilder& SubMenu)
					{
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapCycle);
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapCycleWithOffset);
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapOscillate);
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapLinear);
						SubMenu.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapConstant);
					})
				);
			}
			MenuBuilder.EndSection();
		}
		else
		{
			MenuBuilder.BeginSection("CurveEditorAllCurveSections", LOCTEXT("CurveEditorAllCurveSections", "All Curves"));
			{
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().AddKeyToAllCurves);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().AddKeyToAllCurvesHere);
			}
			MenuBuilder.EndSection();
		}
	}
}

#undef LOCTEXT_NAMESPACE