// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once


// joins two tokens, even when the tokens are macros themselves
#define LC_PP_JOIN_HELPER_HELPER(_0, _1)		_0##_1
#define LC_PP_JOIN_HELPER(_0, _1)				LC_PP_JOIN_HELPER_HELPER(_0, _1)
#define LC_PP_JOIN(_0, _1)						LC_PP_JOIN_HELPER(_0, _1)

// generates a unique name
#define LC_PP_UNIQUE_NAME(_name)				LC_PP_JOIN(_name, __COUNTER__)
