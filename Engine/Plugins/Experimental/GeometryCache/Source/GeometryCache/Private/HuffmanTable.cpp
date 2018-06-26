// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "HuffmanTable.h"
#include "HuffmanBitStream.h"

/**
To read the algorithmic docs you need to understand the following terms

- Symbol: An input value we want to compress, 0,1,2,3,...
- Codeword: The Huffman code. A variable length bit pattern.
- Some symbols will get a codeword assigned others which never occurred will not get a codeword assigned
- Symbols which occur more often preferably get a shorter code word

We often reference the Huffman algorithm as used by JPEG standard in the comments. This code uses an almost identical
algorithm. The main differences with the JPEG standard are:
- Arbitrary code lengths and symbols (JPEG is limited to 16 bits length, with 256 symbols)
- No special codes (all 1 bits) which JPEG uses as bit stream sync points.

*/

#define NO_LOOKAHEAD 0

/**
	Uninitialized Huffman table, call Initialize() first
*/
FHuffmanTable::FHuffmanTable()
{	
}

/**
	Initialize a Huffman table for encoding values in the range [0..NumSymbols[
	The table will initially be in prepass mode.
*/
void FHuffmanTable::Initialize(int32 InNumSymbols)
{
	NumSymbols = InNumSymbols;
	SymbolFrequencies.Init(0, NumSymbols + 1);
	CodewordSymbols.Init(0, NumSymbols + 1);
	// There may be less codewords than symbols but never more so this is a safe bet for the size of this array.
	CodeWords.Init(FCodeWord(), NumSymbols);
	ResetFrequencies();
	bIsPrepass = true;
}

/**
	Initialize a Huffman table based on the serialized table.
	The table will initially be in encode/decode mode.
*/
void FHuffmanTable::Initialize(FHuffmanBitStreamReader &Stream)
{
	NumSymbols = Stream.Read(32);

	// We only serialize enough bits depending on the number of symbols available.
	int32 LastSymbol = NumSymbols - 1;
	uint64 HighestOne = FPlatformMath::FloorLog2(LastSymbol); // FloorLog2 is basically returning the highest set bit (zero based index)
	int32 SymbolBitsToSerialize = HighestOne + 1;

	// Initialize tables
	SymbolFrequencies.Empty();

	CodewordSymbols.Empty(NumSymbols + 1);
	CodewordSymbols.SetNumZeroed(NumSymbols + 1);

	CodeWords.Empty(NumSymbols);
	CodeWords.AddDefaulted(NumSymbols);

	// Load NumCodeWordsOfLength table
	NumCodeWordsOfLength[0] = 0;
	for (int32 Length = 1; Length < MaxCodeLength + 1; Length++)
	{
		NumCodeWordsOfLength[Length] = Stream.Read(32);
	}

	// Calculate this instead of storing it in the stream
	CountTotalCodewords();

	// LoadCodewordSymbols table
	for (int32 CodeWordId = 0; CodeWordId < TotalCodeWords; CodeWordId++)
	{
		CodewordSymbols[CodeWordId] = 0;
		CodewordSymbols[CodeWordId] = Stream.Read(SymbolBitsToSerialize);
	}

	// Generate derived tables
	GenerateCodewords();
	bIsPrepass = false;
}

/**
	Serialize the Huffman table to the bitstream, deserialize by calling Initialise(Reader)
*/
void FHuffmanTable::Serialize(FHuffmanBitStreamWriter& Stream)
{
	Stream.Write(NumSymbols, 32);

	// We only serialize enough bits depending on the number of symbols available.
	int32 LastSymbol = NumSymbols - 1;
	uint64 HighestOne = FPlatformMath::FloorLog2(LastSymbol); // FloorLog2 is basically returning the highest set bit (zero based index)
	int32 SymbolBitsToSerialize = HighestOne + 1;

	// Save NumCodeWordsOfLength table
	for (int32 Length = 1; Length < MaxCodeLength + 1; Length++)
	{
		Stream.Write(NumCodeWordsOfLength[Length], 32);
	}

	// Save CodewordSymbols table
	for (int32 CodeWordId = 0; CodeWordId < TotalCodeWords; CodeWordId++)
	{
		Stream.Write(CodewordSymbols[CodeWordId], SymbolBitsToSerialize);
	}

}

/**
Toggle between prepass mode and encoding/decoding mode.
Note: Toggling will update internal data structures and may have take more time than setting a simple variable.
*/
void FHuffmanTable::SetPrepass(bool bInIsPrepass)
{
	// If we enabled prepass reset the histogram
	if (bIsPrepass == false && bInIsPrepass == true)
	{
		SymbolFrequencies.Init(0, NumSymbols + 1);
		ResetFrequencies();
	}
	// If we disabled prepass calculate the tables
	if (bIsPrepass == true && bInIsPrepass == false)
	{
		GenerateOptimalTable();
		// We don't need the frequency table anymore once we generated the tables
		SymbolFrequencies.Empty(0);
	}
	bIsPrepass = bInIsPrepass;
}

/**
	Reset the frequency of all symbols to zero.
*/
void FHuffmanTable::ResetFrequencies()
{
	for (int32 i = 0; i < SymbolFrequencies.Num(); i++)
	{
		SymbolFrequencies[i] = 0;
	}
}

/**
	Bump the frequency of all symbols by one.
*/
void FHuffmanTable::MarkAllSymbols()
{
	check(bIsPrepass); // This only makes sense when we are doing a prepass

	// We simply make all frequencies at least one here. But we also bump the count of the
	// actually occurring frequencies to give them a small advantage over the ones we never
	// actually saw occurring in the sample stream.
	for (int32 i = 0; i < SymbolFrequencies.Num(); i++)
	{
		SymbolFrequencies[i] += 1;
	}
}

/**
	Generate the NumCodeWordsOfLength and CodewordSymbols tables based on the SymbolFrequencies table.
*/
void FHuffmanTable::GenerateOptimalTable()
{
	// We first assign temporary code lengths to symbols. This value is the maximum length
	// of these temporary symbols. If we generate codes longer than this length we cannot continue the generation.
	// These temporary code lengths will then be shortened to the MaxCodeLength set by the user. At a cost
	// of optimality in the Huffman table.
	const int32 MaxTemporaryCodeLength = 32;
	int32 CodesWithLength[MaxTemporaryCodeLength + 1]; // Number of codes with length i
	
	TArray<int32> CodeLengths;
	CodeLengths.Init(0, NumSymbols + 1);
	TArray<int32> Others;
	Others.Init(0, NumSymbols + 1);

	/* This algorithm is explained in section K.2 of the JPEG standard */

	//
	// Initialize tables
	//
	memset(CodesWithLength, 0, sizeof(int) * (MaxTemporaryCodeLength +1));
	for (int32 Symbol = 0; Symbol < (NumSymbols + 1); Symbol++)
	{
		Others[Symbol] = -1;
	}

	SymbolFrequencies[NumSymbols] = 1;		// make sure the last, sentinel, symbol has a nonzero count

	//
	// Here we fill the others and codesize arrays
	// This looks a bit weird it doesn't actually explicitly build the trees it just generates some 
	// bookkeeping info on the trees like frequencies and number of bits.
	//
	for (;;)
	{
		// Find the largest symbol with the lowest frequency greater than zero.
		// i.e. if two codewords have equal frequency select the largest of the two.
		int32 Largest1 = -1;
		uint64 lowestFreq = 0xffffffffffffffff/*ui64*/;
		for (int32 Symbol=0; Symbol <= NumSymbols; Symbol++)
		{
			if (SymbolFrequencies[Symbol] && SymbolFrequencies[Symbol] <= lowestFreq)
			{
				lowestFreq = SymbolFrequencies[Symbol];
				Largest1 = Symbol;
			}
		}

		// Find the second largest symbol with the lowest frequency greater than zero.
		// again, if two codewords have equal frequency select the largest of the two.
		int32 Largest2 = -1;
		lowestFreq = 0xffffffffffffffff/*ui64*/;
		for (int32 Symbol = 0; Symbol <= NumSymbols; Symbol++)
		{
			if (SymbolFrequencies[Symbol] && SymbolFrequencies[Symbol] <= lowestFreq && Symbol != Largest1)
			{
				lowestFreq = SymbolFrequencies[Symbol];
				Largest2 = Symbol;
			}
		}

		// Done if we've merged everything down into one root node
		if (Largest2 < 0)
			break;

		// Largest1 and Largest2 are now the two codewords we have to put together as childs of a new node...
		// we store this "new" node in index Largest1
		SymbolFrequencies[Largest1] += SymbolFrequencies[Largest2]; // Combined frequencies of the two nodes
		SymbolFrequencies[Largest2] = 0; // Other node gets removed

		// We need an additional bit to make the difference between Largest1 sub tree and Largest2 sub tree
		// so update the books accordingly for every node in those subtrees

		// Increment the code size with one bit of everything in Largest1's tree branch
		CodeLengths[Largest1]++;
		while (Others[Largest1] >= 0)
		{
			Largest1 = Others[Largest1];
			CodeLengths[Largest1]++;
		}

		// Attach Largest2 in the newly merged tree
		Others[Largest1] = Largest2;

		// Increment the code size of everything in Largest2's tree branch
		CodeLengths[Largest2]++;
		while (Others[Largest2] >= 0)
		{
			Largest2 = Others[Largest2];
			CodeLengths[Largest2]++;
		}
	}

	// Now count the number of symbols of each code length and store in the bits array
	// we include the sentinel element here
	for (int32 Symbol = 0; Symbol < NumSymbols+1; Symbol++)
	{
		if (CodeLengths[Symbol])
		{
			// We have so many symbols that the code length is already longer than our internal maximum
			check(CodeLengths[Symbol] < MaxTemporaryCodeLength + 1);
			CodesWithLength[CodeLengths[Symbol]]++;
		}
	}

	/* We don't allow symbols with code lengths over MaxCodeLength bits, so if the pure
	Huffman procedure assigned any such lengths, we must adjust the coding.
	So we go against the optimal tree specified by frequencies in order to make
	it more balanced assigning shorter codes to two subtrees.
	
	Note this code only again only does the bookkeeping of how many symbols assigned
	to what lengths at first it seems magic but once you realize it's just doing
	bookkeeping it's all rather straightforward.
	
	This rather magic code is explained as follows in the spec (sic.):
	Since symbols are paired for the longest Huffman code, the symbols are
	removed from this length category two at a time.  The prefix for the pair
	(which is one bit shorter) is allocated to one of the pair; then,
	skipping the BITS entry for that prefix length, a code word from the next
	shortest nonzero BITS entry is converted into a prefix for two code words
	one bit longer.
	*/

	int32 CodeLength;
	for (CodeLength = MaxTemporaryCodeLength; CodeLength > MaxCodeLength; CodeLength--)
	{
		while (CodesWithLength[CodeLength] > 0)
		{
			// find length of new prefix to be used
			// we take one of the codewords and assign it a different length below
			// so it can't be 0 which is why we need the loop here
			int32 PrefixLen = CodeLength - 2;
			while (CodesWithLength[PrefixLen] == 0)
				PrefixLen--;
			
			// So now we take one of the symbols assigned a codeword of 'prefixLen' bits
			// and assign that to a codeword of codeLength-1 bits. We then
			// use the freed up prefixLen-bit codeword as a prefix for two symbols of prefixLen+1 bits.

			CodesWithLength[CodeLength] -= 2;		// remove two symbols from this length
			CodesWithLength[CodeLength-1] += 1;		// the removal above freed one new symbol with length length codeLength-1
			CodesWithLength[PrefixLen+1] += 2;		// two new symbols with length prefixLen+1
			CodesWithLength[PrefixLen] -= 1;		// One of the codewords of this length is now a prefix (for the two j+1 ones in the previous line) and no longer assigned to any symbol
		}
	}

	// Remove the count for the pseudo-symbol 'NumSymbols' from the largest code length still in use
	while (CodesWithLength[CodeLength] == 0)
		CodeLength--;
	CodesWithLength[CodeLength]--;

	// Save final symbol counts
	memcpy(NumCodeWordsOfLength, CodesWithLength, sizeof(NumCodeWordsOfLength));

	// The list of symbols is now sorted according to code size.
	// JPEG spec seems to think this works without considering the new lengths we assigned above ?!?
	// Probably because these new lengths didn't change the relative code word order only the code word lengths....

	// Create a reverse lookup table converting a code word to a symbol
	// Just loop over it from shortest to longest length
	for (int32 Length=1, CodeWordId=0; Length<=MaxTemporaryCodeLength; Length++)
	{
		// Just do a brute force search for codewords with this length
		for (int32 Symbol = 0; Symbol<NumSymbols; Symbol++)
		{
			if (CodeLengths[Symbol] == Length)
			{
				CodewordSymbols[CodeWordId] = Symbol;
				CodeWordId++;
			}
		}
	}

	// Update our derived values
	CountTotalCodewords();
	GenerateCodewords();
}

void FHuffmanTable::CountTotalCodewords()
{
	// As some symbols may have frequency 0 they are not assigned a codeword
	// and thus totalCodeWords may be smaller than the number of symbols 
	TotalCodeWords = 0;
	for (int32 Length = 1; Length<MaxCodeLength; Length++)
	{
		TotalCodeWords += NumCodeWordsOfLength[Length];
	}
}

/**
	Generate the codeWords, DecodeTable, and Lookahead tables based on the
	NumCodeWordsOfLength and CodewordSymbols tables.
*/
void FHuffmanTable::GenerateCodewords()
{
	// There are always less or equal codewords compared to code values so we can safely preallocate these
	// to NumValues, we add a sentinel element as well.
	TArray<int8> CodeWordLengths; //size of the i'th code word
	CodeWordLengths.Init(0, NumSymbols + 1);
	TArray<uint32> CodeWordBits; // Bit pattern of the i'th code word
	CodeWordBits.Init(0, NumSymbols + 1);

	// Figure C.1 in JPEG: create a table of code length for each code word. Note that this is in increasing code-length order.
	int32 CodeWordId = 0;
	for (int32 Length = 1; Length <= MaxCodeLength; Length++)
	{
		for (int32 Counter = 1; Counter <= (int)NumCodeWordsOfLength[Length]; Counter++)
		{
			CodeWordLengths[CodeWordId++] = (char)Length;
		}
	}
	CodeWordLengths[CodeWordId] = 0;
	int32 LastCodeWordId = CodeWordId;

	// Figure C.2 in JPEG: generate the codes themselves in increasing order
	int32 Code = 0;
	int32 CurrentLength = CodeWordLengths[0];
	CodeWordId = 0;
	while (CodeWordLengths[CodeWordId])
	{
		// Loop over all codewords of length 'CurrentLength' and assign codes
		while (((int)CodeWordLengths[CodeWordId]) == CurrentLength)
		{
			CodeWordBits[CodeWordId++] = Code;
			Code++;
		}
		// Increase the code length
		Code <<= 1;
		CurrentLength++;
	}

	// Figure C.3 in JPEG: generate encoding tables
	// These are code and size indexed by symbol value

	// As not all symbols may have a codeword assigned we set the length initially to 0 for those
	for (int32 SymbolIndex = 0; SymbolIndex < NumSymbols; SymbolIndex++)
	{
		CodeWords[SymbolIndex].Bits = 0;
		CodeWords[SymbolIndex].Length = 0;
	}

	// Any assign the codeword for any symbols which got one assigned
	for (CodeWordId = 0; CodeWordId < LastCodeWordId; CodeWordId++)
	{
		CodeWords[CodewordSymbols[CodeWordId]].Bits = CodeWordBits[CodeWordId];
		CodeWords[CodewordSymbols[CodeWordId]].Length = CodeWordLengths[CodeWordId];
	}

	//
	// Generate the decoding tables
	//

	// Figure F.15 in JPEG: generate decoding tables for bit-sequential decoding

	CodeWordId = 0;
	DecodeTable[0].MaxCode = DecodeTable[0].MinCode = DecodeTable[0].ValPtr = 0;

	for (int32 Length = 1; Length <= MaxCodeLength; Length++)
	{
		if (NumCodeWordsOfLength[Length])
		{
			DecodeTable[Length].ValPtr = CodeWordId; // index of 1st symbol of code length l
			DecodeTable[Length].MinCode = CodeWordBits[CodeWordId]; // minimum code of length l
			CodeWordId += NumCodeWordsOfLength[Length];
			DecodeTable[Length].MaxCode = CodeWordBits[CodeWordId - 1]; // maximum code of length l
		}
		else
		{		
			DecodeTable[Length].MaxCode = -1;	// -1 if no codes of this length
			// Just zero the rest these values shouldn't ever matter
			DecodeTable[Length].MinCode = 0;
			DecodeTable[Length].ValPtr = 0;
		}
	}

	DecodeTable[MaxCodeLength+1].MaxCode = 0xFFFFFFL; // ensures the decoder always terminates

	// This is an optimization not forced in the jpeg standard but commonly used in decoders.
	// We make a quick lookup table which allows us to look at the next byte (8 bits) coming up in the
	// bit stream and quickly determine what the corresponding symbol is (obviously only for codewords shorter than 8 bits)

	// Initialize to 0 which means we can't recognize a symbol in the lookahead so it's coding a prefix
	// for a symbol longer than LookaheadBits
	for (int32 i = 0; i < LookaheadEntries; i++)
	{
		Lookahead[i].Length = 0;
		Lookahead[i].Symbol = 0;
	}

	CodeWordId = 0;
	for (int32 Length = 1; Length <= LookaheadBits; Length++)
	{
		for (int32 Counter = 1; Counter <= (int)NumCodeWordsOfLength[Length]; Counter++, CodeWordId++)
		{
			// Left justified codeword bit sequence. Rightmost bits are zero
			int32 Bits = CodeWordBits[CodeWordId] << (LookaheadBits - Length);

			// Initialize table entries at all possible bit sequences that may follow the current codeword
			// They all refer to the same codeword and length
			for (int32 Entry = 1 << (LookaheadBits - Length); Entry > 0; Entry--)
			{
				Lookahead[Bits].Length = Length;
				Lookahead[Bits].Symbol = CodewordSymbols[CodeWordId];
				Bits++;
			}
		}
	}
}

void FHuffmanTable::Encode(FHuffmanBitStreamWriter& Stream, int32 Symbol)
{
	// Check if symbol is in the valid range
	check(Symbol >= 0);
	check(Symbol < NumSymbols);

	if (bIsPrepass)
	{
		SymbolFrequencies[Symbol]++;
	}
	else
	{
		FCodeWord& CodeWord = CodeWords[Symbol];
		check(CodeWord.Length > 0); // We try to encode a symbol with code length zero, this means a codeword we never saw during our prepass
								// if you only do a partial prepass you should probably use the MarkAllSymbols function to ensure
								// all symbols get assigned a code word
		// and was thus not assigned a codeword
		Stream.Write(CodeWord.Bits, CodeWord.Length);
	}
}

int32 FHuffmanTable::LongDecode(FHuffmanBitStreamReader& Stream, int32 MinLength)
{
	int32 Code = 0;

	//We need to at least decode MinLength bits possibly more
	check(MinLength >= 1);
	Code = Stream.Read(MinLength);

	// Read the rest one bit at a time (Figure F.16 in the JPEG spec)
	int32 Length = MinLength;
	while (Length <= MaxCodeLength && Code > DecodeTable[Length].MaxCode)
	{
		Code <<= 1;
		Code |= Stream.Read(1);
		Length++;
	}

	// Guard against bit stream errors
	if (Length > MaxCodeLength)
	{
		return 0;
	}

	const int32 OffsetInCodesOfLength = Code - DecodeTable[Length].MinCode;
	return CodewordSymbols[DecodeTable[Length].ValPtr + OffsetInCodesOfLength];
}

int32 FHuffmanTable::Decode(FHuffmanBitStreamReader &Stream)
{
	check(bIsPrepass == false);

#if NO_LOOKAHEAD
	return LongDecode(Stream, 1);
#else	
	//	Peek the first valid byte	
	const int32 Bits = Stream.PeekWithZeros(LookaheadBits); // Should return zeros if past end

	// Found in lookahead?
	const int32 Length = Lookahead[Bits].Length;
	if (Length)
	{
		Stream.Read(Length);
		return Lookahead[Bits].Symbol;
	}
	else
	{
		return LongDecode(Stream, LookaheadBits + 1);
	}
#endif
}
