// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/** Needed to define USING_CODE_ANALYSIS */
#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformCodeAnalysis.h"
#elif defined(__clang__)
#include "Clang/ClangPlatformCodeAnalysis.h"
#endif

/** Include SQLite, but not if we're building for analysis as the code emits warnings */
#if !USING_CODE_ANALYSIS
#include "sqlite/sqlite3.inl"
#endif
