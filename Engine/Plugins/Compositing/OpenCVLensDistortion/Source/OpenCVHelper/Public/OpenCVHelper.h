// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

/*
Like many third party headers, OpenCV headers require some care when importing.

When including opencv headers, the includes should be wrapped like this:

	OPENCV_INCLUDES_START
	#undef check 

	// your opencv include directives go here...

	OPENCV_INCLUDES_END

Note that the #undef directive is required. The START/END macros will ensure that 
the previous value is saved and restored, but due to limits of the preprocessor 
cannot undefine the value.

*/

#if PLATFORM_WINDOWS
#define OPENCV_INCLUDES_START THIRD_PARTY_INCLUDES_START \
	__pragma(warning(disable: 4190))  /* 'identifier1' has C-linkage specified, but returns UDT 'identifier2' which is incompatible with C */ \
	__pragma(warning(disable: 6297))  /* Arithmetic overflow:  32-bit value is shifted, then cast to 64-bit value.  Results might not be an expected value. */ \
	__pragma(warning(disable: 6294))  /* Ill-defined for-loop:  initial condition does not satisfy test.  Loop body not executed. */ \
	__pragma(warning(disable: 6201))  /* Index '<x>' is out of valid index range '<a>' to '<b>' for possibly stack allocated buffer '<variable>'. */ \
	PUSH_MACRO(check)
#else
// TODO: when adding support for other platforms, this definition may require updating
#define OPENCV_INCLUDES_START THIRD_PARTY_INCLUDES_START PUSH_MACRO(check)
#endif

#define OPENCV_INCLUDES_END THIRD_PARTY_INCLUDES_END POP_MACRO(check)
	