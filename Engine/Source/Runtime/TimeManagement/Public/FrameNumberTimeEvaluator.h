// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Optional.h"
#include "Templates/ValueOrError.h"
#include "Misc/ExpressionParserTypes.h"

struct FFrameTime;
struct FFrameRate;

#define DEFINE_EXPRESSION_OPERATOR_NODE2(TYPE, ...) \
namespace ExpressionParser {\
	struct TIMEMANAGEMENT_API TYPE { static const TCHAR* const Moniker; }; \
}\
DEFINE_EXPRESSION_NODE_TYPE(ExpressionParser::TYPE, __VA_ARGS__)

// @todo sequencer-timecode This is a duplicate from the BasicMathEvaluator, but that macro exports with CORE_API so we can't use it here.
// We should probably just explicitly define these nodes in the namespace instead of duplicating the macro.
DEFINE_EXPRESSION_OPERATOR_NODE2(FHour, 0x702443E8, 0xEF9A48A0, 0x8DC56394, 0x48F2632A);
DEFINE_EXPRESSION_OPERATOR_NODE2(FMinute, 0x4FED0D41, 0x298D481C, 0xAC022899, 0xF196E817);
DEFINE_EXPRESSION_OPERATOR_NODE2(FSecond, 0x43365BB1, 0x9A6B457E, 0xA6C71BB3, 0xCE6D5ACD);
DEFINE_EXPRESSION_OPERATOR_NODE2(FMillisecond, 0xBFA9E4FB, 0x45799275, 0x0EFA76E3, 0x76E3533F);
DEFINE_EXPRESSION_OPERATOR_NODE2(FFrames, 0xA0D341C4, 0xA51D40D3, 0xA9DBDD45, 0x83E1A21A);
DEFINE_EXPRESSION_OPERATOR_NODE2(FTimecodeDelimiter, 0x6E947008, 0x523D4A17, 0xB5F7BCDA, 0xA43B62FC);
DEFINE_EXPRESSION_OPERATOR_NODE2(FDropcodeDelimiter, 0x80AF2C2C, 0x544A451F, 0x822E5B24, 0xAFC78EEF);
DEFINE_EXPRESSION_OPERATOR_NODE2(FBracketStart, 0xA7358BD1, 0xB4EF466D, 0xA336CD84, 0x14C93D2E);
DEFINE_EXPRESSION_OPERATOR_NODE2(FBracketEnd, 0xE49500E9, 0x03E64440, 0x87802630, 0xF1C1DDDF);

struct TIMEMANAGEMENT_API FFrameNumberTimeEvaluator
{
	/** Constructor that sets up the parser's lexer and compiler */
	FFrameNumberTimeEvaluator();

	/** Evaluate the given expression, resulting in either a double value, or an error */
	TValueOrError<FFrameTime, FExpressionError> EvaluateTimecode(const TCHAR* InExpression, const FFrameRate& InDisplayFrameRate, const FFrameRate& InTickResolution, bool& OutDirectlyParsed) const;
	TValueOrError<FFrameTime, FExpressionError> EvaluateFrame(const TCHAR* InExpression, const FFrameRate& InDisplayFrameRate, const FFrameRate& InTickResolution, bool& OutDirectlyParsed) const;
	TValueOrError<FFrameTime, FExpressionError> EvaluateTime(const TCHAR* InExpression, FFrameRate InFrameRate, bool& OutDirectlyParsed) const;

	FTokenDefinitions TimecodeTokenDefinitions;
	FTokenDefinitions FrameTokenDefinitions;
	FTokenDefinitions TimeTokenDefinitions;

	FExpressionGrammar FrameGrammar;
	FExpressionGrammar TimecodeGrammar;
	FExpressionGrammar TimeGrammar;

	FOperatorJumpTable TimeJumpTable;
};