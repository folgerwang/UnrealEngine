// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "HuffmanBitStream.h"

/**
	An encapsulated Huffman coding/decoding class. To use this class roughly follow the following sequence of calls

	//Create a table
	HuffmanTable Tab(MaxValue);

	// First pass, gather statistics about the data to code
	Tab.SetPrepass(true)
	for each value to code
		Tab.Encode(value)
	// Second pass output actual bits
	Tab.SetPrepass(false)
	for each value to code
		Tab.Encode(value)

	The generated bits are then saved to disc alongside the generated Huffman table.
	To decode the following is then done

	HuffmanTable Tab(SerializedTable)
	for each value to decode
		value = Tab.Decode()

	To read the docs you need to understand two terms

	- Symbol: An input value we want to compress, 0,1,2,3,...
	- Codeword: The Huffman code. A variable length bit pattern.
	- Some symbols will get a codeword assigned others which never occurred will not get a codeword assigned
	- Symbols which occur more often preferably get a shorter code word

*/
class FHuffmanTable
{
	// This has a sentinel element at index NumValues!! And thus contains NumValues+1 elements
	TArray<uint32> SymbolFrequencies;
	TArray<int32> CodewordSymbols;
	static const int32 MaxCodeLength = 24;

	struct FCodeWord
	{
		int32 Bits;
		int32 Length;
	};

	struct FDecodeTableEntry
	{
		int32 MinCode;	// contains the smallest code value for a given length I
		int32 MaxCode;	// contains the largest code value for a given length I
		int32 ValPtr;	//contains the index to the start of the list of values in HUFFVAL which are decoded by code words of length I.
	};

	struct FLookaheadEntry
	{
		int32 Length; //Length of the codeword we recognized in the lookahead
		int32 Symbol; //Symbol it corresponds to
	};

	TArray<FCodeWord> CodeWords; // For every symbol the corresponding huffcode bits
	int32 NumCodeWordsOfLength[MaxCodeLength+1]; //Number of codewords with a given length, element 0 is unused
	int32 NumSymbols;

	void GenerateOptimalTable();
	void CountTotalCodewords();
	void GenerateCodewords();
	
	FDecodeTableEntry DecodeTable[MaxCodeLength+2];

	static const int32 LookaheadBits = 8;
	static const int32 LookaheadEntries = 1 << LookaheadBits;

	FLookaheadEntry Lookahead[LookaheadEntries];

	int32 TotalCodeWords;

	int32 LongDecode(FHuffmanBitStreamReader& Stream, int32 MinLength);
	void ResetFrequencies();
	bool bIsPrepass;

public:

	/**
		Uninitialized Huffman table, call Initialize() first
	*/
	FHuffmanTable();

	/**
		Initialize a Huffman table for encoding values in the range [0..NumSymbols[
		The table will initially be in prepass mode.
	*/
	void Initialize(int32 NumSymbols);

	/**
		Initialize a Huffman table based on the serialized table.
		The table will initially be in encode/decode mode.
	*/
	void Initialize(FHuffmanBitStreamReader& Stream);

	/**
		Serialize the Huffman table to the bitstream, deserialize by calling Initialise(Reader)
	*/
	void Serialize(FHuffmanBitStreamWriter& Stream);	

	/**
		Toggle between prepass mode and encoding/decoding mode.
		Note: Toggling will update internal data structures and may have take more time than setting a simple variable.
	*/
	void SetPrepass(bool bInIsPrepass);

	/**
		Check if the table is currently operating in prepass mode.
	*/
	bool IsPrepass()
	{
		return bIsPrepass;
	}

	/**
		Encode a symbol to the bitstream. If the table is in prepass mode no actual bits will be emitted to the stream.
	*/
	void Encode(FHuffmanBitStreamWriter& Stream, int32 Symbol);

	/**
		Decode the next symbol from the bitstream.
	*/
	int32 Decode(FHuffmanBitStreamReader& Stream);

	/**
		Mark all symbols as being used at least once. This is useful if your prepass is not using the whole dataset but just a small fraction of it.
		Symbols never occurred in the sample dataset can still be coded (but will obviously have a high coding cost).
	*/
	void MarkAllSymbols();
};
