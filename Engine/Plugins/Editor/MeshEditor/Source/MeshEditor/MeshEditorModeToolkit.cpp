// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshEditorModeToolkit.h"
#include "IMeshEditorModeUIContract.h"
#include "MeshEditorCommands.h"
#include "MeshEditorStyle.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "MeshEditorModeToolkit"


class SMeshEditorModeControlWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMeshEditorModeControlWidget ) {}
	SLATE_END_ARGS()

public:

	/** SCompoundWidget functions */
	void Construct( const FArguments& InArgs, const FText& GroupName, const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& Actions, const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& SelectionModifiers )
	{
		TSharedRef< SHorizontalBox > SelectionModifiersButtons = SNew( SHorizontalBox );

		if ( SelectionModifiers.Num() > 1 ) // Only display the list of selection modifiers if there's more than 1.
		{
			SelectionModifiersButtons->AddSlot()
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.Padding( 3.0f, 1.0f, 3.0f, 1.0f )
			[
				SNew( STextBlock )
				.TextStyle( FMeshEditorStyle::Get(), "EditingMode.Entry.Text" )
				.Text( LOCTEXT( "Selection", "Selection" ) )
			];

			for ( const TTuple<TSharedPtr<FUICommandInfo>, FUIAction>& Action : SelectionModifiers )
			{
				const FUICommandInfo& CommandInfo = *Action.Get<0>();
				const FUIAction& UIAction = Action.Get<1>();

				SelectionModifiersButtons->AddSlot()
				.AutoWidth()
				.Padding( 3.0f, 1.0f, 3.0f, 1.0f )
				[
					SNew( SCheckBox )
					.Style( FMeshEditorStyle::Get(), "EditingMode.Entry" )
					.ToolTip( SNew( SToolTip ).Text( CommandInfo.GetDescription() ) )
					.IsChecked_Lambda( [UIAction] { return UIAction.GetCheckState(); } )
					.OnCheckStateChanged_Lambda( [UIAction]( ECheckBoxState State ) { if( State == ECheckBoxState::Checked ) { UIAction.Execute(); } } )
					[
						SNew( SOverlay )
						+SOverlay::Slot()
						.VAlign( VAlign_Center )
						[
							SNew( SSpacer )
							.Size( FVector2D( 1, 30 ) )
						]
						+SOverlay::Slot()
						.Padding( FMargin( 8, 0, 8, 0 ) )
						.HAlign( HAlign_Center )
						.VAlign( VAlign_Center )
						[
							SNew( STextBlock )
							.TextStyle( FMeshEditorStyle::Get(), "EditingMode.Entry.Text" )
							.Text( CommandInfo.GetDefaultChord(EMultipleKeyBindingIndex::Primary).IsValidChord() ?
							FText::Format( LOCTEXT( "RadioButtonLabelAndShortcutFormat", "{0}  ({1})" ), CommandInfo.GetLabel(), CommandInfo.GetDefaultChord(EMultipleKeyBindingIndex::Primary).GetInputText() ) :
							CommandInfo.GetLabel() )
						]
					]
				];
			}
		}

		TSharedRef<SVerticalBox> Buttons = SNew( SVerticalBox );
		TSharedRef<SVerticalBox> RadioButtons = SNew( SVerticalBox );

		for( const TTuple<TSharedPtr<FUICommandInfo>, FUIAction>& Action : Actions )
		{
			const FUICommandInfo& CommandInfo = *Action.Get<0>();
			const FUIAction& UIAction = Action.Get<1>();

			if( CommandInfo.GetUserInterfaceType() == EUserInterfaceActionType::Button )
			{
				const bool bIsFirstItem = Buttons->NumSlots() == 0;
				Buttons->AddSlot()
				.AutoHeight()
				.Padding( 3.0f, bIsFirstItem ? 9.0f : 3.0f, 3.0f, 3.0f )
				[
					SNew( SButton )
					.HAlign( HAlign_Center )
					.VAlign( VAlign_Center )
					.ContentPadding( FMargin( 8.0f, 4.0f ) )
					.Text( CommandInfo.GetDefaultChord(EMultipleKeyBindingIndex::Primary).IsValidChord() ?
						FText::Format( LOCTEXT( "ButtonLabelAndShortcutFormat", "{0}  ({1})" ), CommandInfo.GetLabel(), CommandInfo.GetDefaultChord(EMultipleKeyBindingIndex::Primary).GetInputText() ) :
						CommandInfo.GetLabel() )
					.ToolTip( SNew( SToolTip ).Text( CommandInfo.GetDescription() ) )
					.OnClicked_Lambda( [UIAction] { UIAction.Execute(); return FReply::Handled(); } )
					.IsEnabled_Lambda( [UIAction] { return UIAction.CanExecute(); } )
				];
			}
			else if( CommandInfo.GetUserInterfaceType() == EUserInterfaceActionType::RadioButton )
			{
				const bool bIsFirstItem = RadioButtons->NumSlots() == 0;
				RadioButtons->AddSlot()
				.AutoHeight()
				.Padding( 3.0f, bIsFirstItem ? 7.0f : 1.0f, 3.0f, 1.0f )
				[
					SNew( SCheckBox )
					.Style( FMeshEditorStyle::Get(), "EditingMode.Entry" )
					.ToolTip( SNew( SToolTip ).Text( CommandInfo.GetDescription() ) )
					.IsChecked_Lambda( [UIAction] { return UIAction.GetCheckState(); } )
					.OnCheckStateChanged_Lambda( [UIAction]( ECheckBoxState State ) { if( State == ECheckBoxState::Checked ) { UIAction.Execute(); } } )
					[
						SNew( SOverlay )
						+SOverlay::Slot()
						.VAlign( VAlign_Center )
						[
							SNew( SSpacer )
							.Size( FVector2D( 1, 30 ) )
						]
						+SOverlay::Slot()
						.Padding( FMargin( 8, 0, 8, 0 ) )
						.HAlign( HAlign_Center )
						.VAlign( VAlign_Center )
						[
							SNew( STextBlock )
							.TextStyle( FMeshEditorStyle::Get(), "EditingMode.Entry.Text" )
							.Text( CommandInfo.GetDefaultChord(EMultipleKeyBindingIndex::Primary).IsValidChord() ?
								FText::Format( LOCTEXT( "RadioButtonLabelAndShortcutFormat", "{0}  ({1})" ), CommandInfo.GetLabel(), CommandInfo.GetDefaultChord(EMultipleKeyBindingIndex::Primary).GetInputText() ) :
								CommandInfo.GetLabel() )
						]
					]
				];
			}
		}

		// This is the basic layout for each selected element type:
		// First, a list of buttons, then a separator, and then a list of radio buttons.
		// @todo mesheditor: if we want to make these UI elements bigger (e.g. for ease of use with VR), we can easily change these
		// to icons with text to the side. The icon name is already registered with the FUICommandInfo (e.g. "MeshEditorVertex.MoveAction").
		ChildSlot
		[
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 6.0f, 6.0f, 6.0f, 2.0f )
			.HAlign( HAlign_Center )
			[
				SNew( STextBlock )
				.TextStyle( FMeshEditorStyle::Get(), "EditingMode.GroupName.Text" )
				.Text( GroupName )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign( HAlign_Center )
			[
				SelectionModifiersButtons
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign( HAlign_Center )
			[
				RadioButtons
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign( HAlign_Center )
			[
				Buttons
			]
		];
	}
};


class SMeshEditorSelectionModeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMeshEditorModeControlWidget ) {}
	SLATE_END_ARGS()

public:
	void Construct( const FArguments& InArgs, IMeshEditorModeUIContract& MeshEditorMode, EEditableMeshElementType ElementType, const FText& Label )
	{
		ChildSlot
		[
			SNew( SBox )
			.HAlign( HAlign_Fill )
			.VAlign( VAlign_Center )
			[
				SNew( SCheckBox )
				.Style( FMeshEditorStyle::Get(), "SelectionMode.Entry" )
				.HAlign( HAlign_Fill )
				.IsChecked_Lambda( [ &MeshEditorMode, ElementType ] { return ( MeshEditorMode.GetMeshElementSelectionMode() == ElementType ? ECheckBoxState::Checked : ECheckBoxState::Unchecked ); } )
				.OnCheckStateChanged_Lambda( [ &MeshEditorMode, ElementType ]( ECheckBoxState State ) { if( State == ECheckBoxState::Checked ) { MeshEditorMode.SetMeshElementSelectionMode( ElementType ); } } )
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.FillWidth( 1 )
					.HAlign( HAlign_Center )
					[
						SNew( STextBlock )
						.TextStyle( FMeshEditorStyle::Get(), "SelectionMode.Entry.Text" )
						.Text( Label )
					]
				]
			]
		];
	}
};


void SMeshEditorModeControls::Construct( const FArguments& InArgs, IMeshEditorModeUIContract& MeshEditorMode )
{
	// Uses the widget switcher widget so only the widget in the slot which corresponds to the selected mesh element type will be shown
	TSharedRef<SWidgetSwitcher> WidgetSwitcher = SNew( SWidgetSwitcher )
		.WidgetIndex_Lambda( [&MeshEditorMode]() -> int32
			{
				if( MeshEditorMode.GetMeshElementSelectionMode() != EEditableMeshElementType::Any )
				{
					return static_cast<int32>( MeshEditorMode.GetMeshElementSelectionMode() );
				}
				else
				{
					return static_cast<int32>( MeshEditorMode.GetSelectedMeshElementType() );
				}
			} 
		);

	WidgetSwitcher->AddSlot( static_cast<int32>( EEditableMeshElementType::Vertex ) )
	[
		SNew( SMeshEditorModeControlWidget, LOCTEXT( "VertexGroupName", "Vertex" ), MeshEditorMode.GetVertexActions(), MeshEditorMode.GetVertexSelectionModifiers() )
	];

	WidgetSwitcher->AddSlot( static_cast<int32>( EEditableMeshElementType::Edge ) )
	[
		SNew( SMeshEditorModeControlWidget, LOCTEXT( "EdgeGroupName", "Edge" ), MeshEditorMode.GetEdgeActions(), MeshEditorMode.GetEdgeSelectionModifiers() )
	];

	WidgetSwitcher->AddSlot( static_cast<int32>( EEditableMeshElementType::Polygon ) )
	[
		SNew( SMeshEditorModeControlWidget, LOCTEXT( "PolygonGroupName", "Polygon" ), MeshEditorMode.GetPolygonActions(), MeshEditorMode.GetPolygonSelectionModifiers() )
	];

	WidgetSwitcher->AddSlot( static_cast<int32>( EEditableMeshElementType::Invalid ) )
	[
		SNew( SBox )
		.Padding( 20.0f )
		.HAlign( HAlign_Center )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "NothingSelected", "Please select a mesh to edit." ) )
		]
	];

	ChildSlot
	[
		SNew( SScrollBox )
		+SScrollBox::Slot()
		.Padding( 6.0f )
		[
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.FillWidth( 1 )
				.Padding( 2 )
				[
					SNew( SMeshEditorSelectionModeWidget, MeshEditorMode, EEditableMeshElementType::Any, LOCTEXT( "AnyElementType", "Mesh" ) )
				]
				+SHorizontalBox::Slot()
				.FillWidth( 1 )
				.Padding( 2 )
				[
					SNew( SMeshEditorSelectionModeWidget, MeshEditorMode, EEditableMeshElementType::Polygon, LOCTEXT( "Polygon", "Polygon" ) )
				]
				+SHorizontalBox::Slot()
				.FillWidth( 1 )
				.Padding( 2 )
				[
					SNew( SMeshEditorSelectionModeWidget, MeshEditorMode, EEditableMeshElementType::Edge, LOCTEXT( "Edge", "Edge" ) )
				]
				+SHorizontalBox::Slot()
				.FillWidth( 1 )
				.Padding( 2 )
				[
					SNew( SMeshEditorSelectionModeWidget, MeshEditorMode, EEditableMeshElementType::Vertex, LOCTEXT( "Vertex", "Vertex" ) )
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0, 4, 0, 0 )
			.HAlign( HAlign_Right )
			[
				SNew( SHorizontalBox )
				.Visibility( EVisibility::Collapsed )	// @todo mesheditor instancing: UI for instancing features is disabled until this feature is working properly
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 2 )
				[
					SNew( SButton )
					.HAlign( HAlign_Center )
					.VAlign( VAlign_Center )
					.Text( LOCTEXT( "Propagate", "Propagate" ) )
					.ToolTip( SNew( SToolTip ).Text( LOCTEXT( "PropagateTooltip", "Propagates per-instance changes to the static mesh asset itself." ) ) )
					.IsEnabled_Lambda( [ &MeshEditorMode ] { return false; /*MeshEditorMode.CanPropagateInstanceChanges(); */ } )
					.OnClicked_Lambda( [ &MeshEditorMode ] { MeshEditorMode.PropagateInstanceChanges(); return FReply::Handled(); } )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 2 )
				[
					SNew( SBox )
					.HAlign( HAlign_Fill )
					.VAlign( VAlign_Center )
					[
						SNew( SCheckBox )
						.Style( FMeshEditorStyle::Get(), "SelectionMode.Entry" )
						.HAlign( HAlign_Fill )
						.ToolTip( SNew( SToolTip ).Text( LOCTEXT( "PerInstanceTooltip", "Toggles editing mode between editing instances and editing the original static mesh asset." ) ) )
						.IsChecked_Lambda( [ &MeshEditorMode ] { return ( MeshEditorMode.IsEditingPerInstance() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked ); } )
						.OnCheckStateChanged_Lambda( [ &MeshEditorMode ]( ECheckBoxState State ) { MeshEditorMode.SetEditingPerInstance( State == ECheckBoxState::Checked ); } )
						[
							SNew( SHorizontalBox )
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign( HAlign_Center )
							[
								SNew( STextBlock )
								.TextStyle( FMeshEditorStyle::Get(), "SelectionMode.Entry.Text" )
								.Text( LOCTEXT( "PerInstance", "Per Instance" ) )
							]
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SBorder )
				.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
				[
					SNew( SVerticalBox )
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew( SBox )
						.Padding( 0.0f )
						.Visibility_Lambda( [&MeshEditorMode, WidgetSwitcher]() 
							// Only show the widget switcher if either nothing is selected, or we have at least one mesh element selected
							{ 
								return ( MeshEditorMode.GetSelectedEditableMeshes().Num() == 0 || 
										 WidgetSwitcher->GetActiveWidgetIndex() > static_cast<int32>( EEditableMeshElementType::Invalid ) ) ? 
											EVisibility::Visible : EVisibility::Collapsed; 
							} )
						[
							WidgetSwitcher
						]
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding( 6.0f )
					[
						SNew( SSeparator )
						.Visibility_Lambda( [&MeshEditorMode, WidgetSwitcher]() 
							// Only show the separator if we have a polygon, vertex or edge selected
							{ 
								return ( MeshEditorMode.GetSelectedEditableMeshes().Num() > 0 && 
										 WidgetSwitcher->GetActiveWidgetIndex() > static_cast<int32>( EEditableMeshElementType::Invalid ) ) ? 
											EVisibility::Visible : EVisibility::Collapsed; 
							} )
						.Orientation( Orient_Horizontal )
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew( SBox )
						.Visibility_Lambda( [&MeshEditorMode]() { return MeshEditorMode.GetSelectedEditableMeshes().Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; } )
						[
							SNew( SMeshEditorModeControlWidget, LOCTEXT( "MeshGroupName", "Mesh" ), MeshEditorMode.GetCommonActions(), TArray< TTuple< TSharedPtr<FUICommandInfo>, FUIAction > >() )
						]
					]
				]
			]
		]
	];
}


void FMeshEditorModeToolkit::RegisterTabSpawners( const TSharedRef<FTabManager>& TabManager )
{
}


void FMeshEditorModeToolkit::UnregisterTabSpawners( const TSharedRef<FTabManager>& TabManager )
{
}


void FMeshEditorModeToolkit::Init( const TSharedPtr<IToolkitHost>& InitToolkitHost )
{
	ToolkitWidget = SNew( SMeshEditorModeControls, MeshEditorMode );

	FModeToolkit::Init( InitToolkitHost );
}


FName FMeshEditorModeToolkit::GetToolkitFName() const
{
	return FName( "MeshEditorMode" );
}


FText FMeshEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT( "ToolkitName", "Mesh Editor Mode" );
}


class FEdMode* FMeshEditorModeToolkit::GetEditorMode() const
{
	return GLevelEditorModeTools().GetActiveMode( "MeshEditor" );
}


TSharedPtr<SWidget> FMeshEditorModeToolkit::GetInlineContent() const
{
	return ToolkitWidget;
}

#undef LOCTEXT_NAMESPACE
