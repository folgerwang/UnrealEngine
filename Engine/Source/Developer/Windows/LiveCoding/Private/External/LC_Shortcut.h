// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <string>

namespace shortcut
{
	int ConvertKeysToShortcut(bool control, bool alt, bool shift, unsigned int virtualKey);
	bool ContainsControl(int shortcutValue);
	bool ContainsAlt(int shortcutValue);
	bool ContainsShift(int shortcutValue);
	int GetVirtualKeyCode(int shortcutValue);
	std::wstring ConvertShortcutToText(int shortcutValue);
}
