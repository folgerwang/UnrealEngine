// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TranslucencyPass.h: Translucency pass enum.
=============================================================================*/

#pragma once

// enum instead of bool to get better visibility when we pass around multiple bools, also allows for easier extensions
namespace ETranslucencyPass
{
	enum Type
	{
		TPT_StandardTranslucency,
		TPT_TranslucencyAfterDOF,

		/** Drawing all translucency, regardless of separate or standard.  Used when drawing translucency outside of the main renderer, eg FRendererModule::DrawTile. */
		TPT_AllTranslucency,
		TPT_MAX
	};
};