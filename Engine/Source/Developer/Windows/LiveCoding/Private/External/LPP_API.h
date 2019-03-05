// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

/******************************************************************************/
/* HOOKS                                                                      */
/******************************************************************************/

// concatenates two preprocessor tokens, even when the tokens themselves are macros
#define LPP_CONCATENATE_HELPER_HELPER(_a, _b)		_a##_b
#define LPP_CONCATENATE_HELPER(_a, _b)				LPP_CONCATENATE_HELPER_HELPER(_a, _b)
#define LPP_CONCATENATE(_a, _b)						LPP_CONCATENATE_HELPER(_a, _b)

// generates a unique identifier inside a translation unit
#define LPP_IDENTIFIER(_identifier)					LPP_CONCATENATE(_identifier, __LINE__)

// custom section names for hooks
#define LPP_PREPATCH_SECTION			".lc_prepatch_hooks"
#define LPP_POSTPATCH_SECTION			".lc_postpatch_hooks"
#define LPP_COMPILE_START_SECTION		".lc_compile_start_hooks"
#define LPP_COMPILE_SUCCESS_SECTION		".lc_compile_success_hooks"
#define LPP_COMPILE_ERROR_SECTION		".lc_compile_error_hooks"

// register a pre-patch hook in a custom section
#define LPP_PREPATCH_HOOK(_function)																							\
	__pragma(section(LPP_PREPATCH_SECTION, read))																				\
	__declspec(allocate(LPP_PREPATCH_SECTION)) extern void (*LPP_IDENTIFIER(lpp_prepatch_hook_function))(void) = &_function

// register a post-patch hook in a custom section
#define LPP_POSTPATCH_HOOK(_function)																							\
	__pragma(section(LPP_POSTPATCH_SECTION, read))																				\
	__declspec(allocate(LPP_POSTPATCH_SECTION)) extern void (*LPP_IDENTIFIER(lpp_postpatch_hook_function))(void) = &_function

// register a compile start hook in a custom section
#define LPP_COMPILE_START_HOOK(_function)																								\
	__pragma(section(LPP_COMPILE_START_SECTION, read))																					\
	__declspec(allocate(LPP_COMPILE_START_SECTION)) extern void (*LPP_IDENTIFIER(lpp_compile_start_hook_function))(void) = &_function

// register a compile success hook in a custom section
#define LPP_COMPILE_SUCCESS_HOOK(_function)																									\
	__pragma(section(LPP_COMPILE_SUCCESS_SECTION, read))																					\
	__declspec(allocate(LPP_COMPILE_SUCCESS_SECTION)) extern void (*LPP_IDENTIFIER(lpp_compile_success_hook_function))(void) = &_function

// register a compile error hook in a custom section
#define LPP_COMPILE_ERROR_HOOK(_function)																								\
	__pragma(section(LPP_COMPILE_ERROR_SECTION, read))																					\
	__declspec(allocate(LPP_COMPILE_ERROR_SECTION)) extern void (*LPP_IDENTIFIER(lpp_compile_error_hook_function))(void) = &_function
