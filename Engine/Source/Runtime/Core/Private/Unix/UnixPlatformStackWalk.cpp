// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnixPlatformStackWalk.cpp: Unix implementations of stack walk functions
=============================================================================*/

#include "Unix/UnixPlatformStackWalk.h"
#include "CoreGlobals.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Unix/UnixPlatformCrashContext.h"
#include <cxxabi.h>
#include "HAL/ExceptionHandling.h"
#include "HAL/PlatformProcess.h"
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <stdio.h>
#include <unistd.h>

// FIXME Remove this define once we remove the old way symbolicate
#define _ELFDEFINITIONS_H_
#include <link.h>

// these are not actually system headers, but a TPS library (see ThirdParty/elftoolchain)
THIRD_PARTY_INCLUDES_START
#include <libelf.h>
#include <_libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
THIRD_PARTY_INCLUDES_END

#ifndef DW_AT_MIPS_linkage_name
	#define DW_AT_MIPS_linkage_name		0x2007			// common extension, used before DW_AT_linkage_name became standard
#endif // DW_AT_MIPS_linkage_name

// Init'ed in UnixPlatformMemory for now. Once the old crash symbolicator is gone remove this
extern bool CORE_API GUseNewCrashSymbolicator;
extern bool CORE_API GSuppressDwarfParsing;

// Init'ed in UnixPlatformMemory. Once this is tested more we can remove this fallback flag
extern bool CORE_API GFullCrashCallstack;

namespace UnixStackWalkHelpers
{
	struct UnixBacktraceSymbols
	{
		/** Lock for thread-safe initialization. */
		FCriticalSection CriticalSection;

		/** Initialized flag. If initialization failed, it don't run again. */
		bool Inited;

		/** File descriptor needed for libelf to open (our own) binary */
		int ExeFd;

		/** Elf header as used by libelf (forward-declared in the same way as libelf does it, it's normally of Elf type) */
		struct _Elf* ElfHdr;

		/** DWARF handle used by libdwarf (forward-declared in the same way as libdwarf does it, it's normally of Dwarf_Debug type) */
		struct _Dwarf_Debug	* DebugInfo;

		UnixBacktraceSymbols()
		:	Inited(false)
		,	ExeFd(-1)
		,	ElfHdr(NULL)
		,	DebugInfo(NULL)
		{
		}

		~UnixBacktraceSymbols();

		void Init();

		/**
		 * Gets information for the crash.
		 *
		 * @param Address the address to look up info for
		 * @param OutModuleNamePtr pointer to module name (may be null). Caller doesn't have to free it, but need to consider it temporary (i.e. next GetInfoForAddress() call on any thread may change it).
		 * @param OutFunctionNamePtr pointer to function name (may be null). Caller doesn't have to free it, but need to consider it temporary (i.e. next GetInfoForAddress() call on any thread may change it).
		 * @param OutSourceFilePtr pointer to source filename (may be null). Caller doesn't have to free it, but need to consider it temporary (i.e. next GetInfoForAddress() call on any thread may change it).
		 * @param OutLineNumberPtr pointer to line in a source file (may be null). Caller doesn't have to free it, but need to consider it temporary (i.e. next GetInfoForAddress() call on any thread may change it).
		 *
		 * @return true if succeeded in getting the info. If false is returned, none of above parameters should be trusted to contain valid data!
		 */
		bool GetInfoForAddress(void* Address, const char** OutModuleNamePtr, const char** OutFunctionNamePtr, const char** OutSourceFilePtr, int* OutLineNumberPtr);

		/**
		 * Checks check if address is inside this entry.
		 *
		 * @param DebugInfo DWARF debug info point
		 * @param Die pointer to Debugging Information Entry
		 * @param Addr address to check
		 *
		 * @return true if the address is in range
		 */
		static bool CheckAddressInRange(Dwarf_Debug DebugInfo, Dwarf_Die Die, Dwarf_Unsigned Addr);

		/**
		 * Finds a function name in DWARF DIE (Debug Information Entry) and its children.
		 * For more info on DWARF format, see http://www.dwarfstd.org/Download.php , http://www.ibm.com/developerworks/library/os-debugging/
		 * Note: that function is not exactly traversing the tree, but this "seems to work"(tm). Not sure if we need to descend properly (taking child of every sibling), this
		 * takes too much time (and callstacks seem to be fine without it).
		 *
		 * The function will always deallocate the DIE passed to it.
		 *
		 * @param DebugInfo handle to the opened DWARF data
		 * @param InDiePtr Pointer to Debugging Infromation Entry that this function will use. This DIE will be deallocated and zeroed upon function return.
		 * @param Addr address for which we're looking for the function name
		 * @param OutFuncName pointer to the return function name
		 *
		 */
		static void FindFunctionNameInDIEAndChildren(Dwarf_Debug DebugInfo, Dwarf_Die* InDiePtr, Dwarf_Addr Addr, const char **OutFuncName);

		/**
		 * Finds a function name in DWARF DIE (Debug Information Entry).
		 * For more info on DWARF format, see http://www.dwarfstd.org/Download.php , http://www.ibm.com/developerworks/library/os-debugging/
		 *
		 * The function will deallocate the DIE passed if it returns true.
		 *
		 * @param DebugInfo handle to the opened DWARF data
		 * @param InDiePtr Pointer to Debugging Infromation Entry that this function will use. This DIE will be deallocated and zeroed if function is successful.
		 * @param Addr address for which we're looking for the function name
		 * @param OutFuncName pointer to the return function name
		 *
		 * @return true if we need to stop search (i.e. either found it or some error happened)
		 */
		static bool FindFunctionNameInDIE(Dwarf_Debug DebugInfo, Dwarf_Die* InDiePtr, Dwarf_Addr Addr, const char **OutFuncName);

		/**
		 * Tries all usable attributes in DIE to determine function name (i.e. DW_AT_MIPS_linkage_name, DW_AT_linkage_name, DW_AT_name)
		 *
		 * The function will deallocate the DIE passed if it returns true.
		 *
		 * @param DebugInfo handle to the opened DWARF data
		 * @param InDiePtr Pointer to Debugging Infromation Entry that this function will use. This DIE will be deallocated and zeroed if function is successful.
		 * @param OutFuncName Pointer to function name (volatile, copy it after call). Will not be touched if function returns false
		 *
		 * @return true if found
		 */
		static bool FindNameAttributeInDIE(Dwarf_Debug DebugInfo, Dwarf_Die* InDiePtr, const char **OutFuncName);
	};

	enum
	{
		MaxMangledNameLength = 1024,
		MaxDemangledNameLength = 1024
	};

	void UnixBacktraceSymbols::Init()
	{
		FScopeLock ScopeLock( &CriticalSection );

		if (!Inited)
		{
			// open ourselves for examination
			if (!GSuppressDwarfParsing)
			{
				char ElfPath[512] = { 0 };
				char DebugPath[512] = { 0 };
				
				ssize_t len = readlink("/proc/self/exe", ElfPath, sizeof(ElfPath) - 1);
				if (len > 0)
				{
					snprintf(DebugPath, ARRAY_COUNT(DebugPath), "%s.debug", ElfPath);

					// First try separate .debug file
					ExeFd = ::open(DebugPath, O_RDONLY);

					if (ExeFd < 0)
					{
						// Try the .elf itself
						ExeFd = open(ElfPath, O_RDONLY);
					}
				}

				if (ExeFd >= 0)
				{
					Dwarf_Error ErrorInfo;
					// allocate DWARF debug descriptor
					if (dwarf_init(ExeFd, DW_DLC_READ, NULL, NULL, &DebugInfo, &ErrorInfo) == DW_DLV_OK)
					{
						// get ELF descritor
						if (dwarf_get_elf(DebugInfo, &ElfHdr, &ErrorInfo) != DW_DLV_OK)
						{
							dwarf_finish(DebugInfo, &ErrorInfo);
							DebugInfo = NULL;

							close(ExeFd);
							ExeFd = -1;
						}
					}
					else
					{
						DebugInfo = NULL;
						close(ExeFd);
						ExeFd = -1;
					}
				}			
			}
			Inited = true;
		}
	}

	UnixBacktraceSymbols::~UnixBacktraceSymbols()
	{
		if (DebugInfo)
		{
			Dwarf_Error ErrorInfo;
			dwarf_finish(DebugInfo, &ErrorInfo);
			DebugInfo = NULL;
		}

		if (ElfHdr)
		{
			elf_end_workaround(ElfHdr);
			ElfHdr = NULL;
		}

		if (ExeFd >= 0)
		{
			close(ExeFd);
			ExeFd = -1;
		}
	}

	bool UnixBacktraceSymbols::GetInfoForAddress(void* Address, const char** OutModuleNamePtr, const char** OutFunctionNamePtr, const char** OutSourceFilePtr, int* OutLineNumberPtr)
	{
		if (DebugInfo == NULL)
		{
			return false;
		}

		Dwarf_Die Die = nullptr;
		Dwarf_Unsigned Addr = reinterpret_cast< Dwarf_Unsigned >( Address ), LineNumber = 0;
		const char * SrcFile = NULL;

		static_assert(sizeof(Dwarf_Unsigned) >= sizeof(Address), "Dwarf_Unsigned type should be long enough to represent pointers. Check libdwarf bitness.");

		int ReturnCode = DW_DLV_OK;
		Dwarf_Error ErrorInfo;
		bool bExitHeaderLoop = false;
		int32 MaxCompileUnitsAllowed = 16 * 1024 * 1024;	// safeguard to make sure we never get into an infinite loop
		const int32 kMaxBufferLinesAllowed = 16 * 1024 * 1024;	// safeguard to prevent too long line loop
		for(;;)
		{
			if (UNLIKELY(--MaxCompileUnitsAllowed <= 0))
			{
				fprintf(stderr, "Breaking out from what seems to be an infinite loop during DWARF parsing (too many compile units).\n");
				ReturnCode = DW_DLE_DIE_NO_CU_CONTEXT;	// invalidate
				break;
			}

			if (UNLIKELY(bExitHeaderLoop))
				break;

			ReturnCode = dwarf_next_cu_header(DebugInfo, NULL, NULL, NULL, NULL, NULL, &ErrorInfo);
			if (UNLIKELY(ReturnCode != DW_DLV_OK))
			{
				break;
			}

			if (LIKELY(Die))
			{
				dwarf_dealloc(DebugInfo, Die, DW_DLA_DIE);
				Die = nullptr;
			}

			// find compile unit
			for (;;)
			{
				Dwarf_Die SiblingDie = nullptr;
				bool bStopTraversingSiblings = dwarf_siblingof(DebugInfo, Die, &SiblingDie, &ErrorInfo) != DW_DLV_OK;
				if (LIKELY(Die))
				{
					dwarf_dealloc(DebugInfo, Die, DW_DLA_DIE);
				}
				Die = SiblingDie;

				if (UNLIKELY(bStopTraversingSiblings))
				{
					break;
				}

				// move on to the next sibling
				Dwarf_Half Tag;
				if (UNLIKELY(dwarf_tag(Die, &Tag, &ErrorInfo) != DW_DLV_OK))
				{
					bExitHeaderLoop = true;
					break;
				}

				if (Tag == DW_TAG_compile_unit)
				{
					break;
				}
			}

			if (Die == NULL)
			{
				break;
			}

			// check if address is inside this CU
			if (LIKELY(!CheckAddressInRange(DebugInfo, Die, Addr)))
			{
				dwarf_dealloc(DebugInfo, Die, DW_DLA_DIE);
				Die = nullptr;
				continue;
			}

			Dwarf_Line * LineBuf;
			Dwarf_Signed NumLines = kMaxBufferLinesAllowed;
			if (UNLIKELY(dwarf_srclines(Die, &LineBuf, &NumLines, &ErrorInfo) != DW_DLV_OK))
			{
				// could not get line info for some reason
				dwarf_dealloc(DebugInfo, Die, DW_DLA_DIE);
				Die = nullptr;
				continue;
			}

			if (UNLIKELY(NumLines >= kMaxBufferLinesAllowed))
			{
				fprintf(stderr, "Number of lines associated with a DIE looks unreasonable (%d), early quitting.\n", static_cast<int32>(NumLines));
				ReturnCode = DW_DLE_DIE_NO_CU_CONTEXT;	// invalidate
				break;
			}

			// look which line is that
			Dwarf_Addr LineAddress, PrevLineAddress = ~0ULL;
			Dwarf_Unsigned LineIdx = NumLines;
			for (int Idx = 0; Idx < NumLines; ++Idx)
			{
				if (dwarf_lineaddr(LineBuf[Idx], &LineAddress, &ErrorInfo) != 0)
				{
					bExitHeaderLoop = true;
					break;
				}
				// check if we hit the exact line
				if (Addr == LineAddress)
				{
					LineIdx = Idx;
					bExitHeaderLoop = true;
					break;
				}
				else if (PrevLineAddress < Addr && Addr < LineAddress)
				{
					LineIdx = Idx - 1;
					break;
				}
				PrevLineAddress = LineAddress;
			}
			if (LineIdx < NumLines)
			{
				if (dwarf_lineno(LineBuf[LineIdx], &LineNumber, &ErrorInfo) != 0)
				{
					fprintf(stderr, "Can't get line number by dwarf_lineno.\n");
					break;
				}
				for (int Idx = LineIdx; Idx >= 0; --Idx)
				{
					char * SrcFileTemp = NULL;
					if (!dwarf_linesrc(LineBuf[Idx], &SrcFileTemp, &ErrorInfo))
					{
						SrcFile = SrcFileTemp;
						break;
					}
				}
				bExitHeaderLoop = true;
			}
		}

		bool bSuccess = (ReturnCode == DW_DLV_OK);

		if (LIKELY(bSuccess))
		{
			if (LIKELY(OutFunctionNamePtr != nullptr))
			{
				const char * FunctionName = nullptr;
				// this function will deallocate the die
				FindFunctionNameInDIEAndChildren(DebugInfo, &Die, Addr, &FunctionName);
				if (LIKELY(FunctionName != nullptr))
				{
					*OutFunctionNamePtr = FunctionName;
				}
				else
				{
					// make sure it's not null
					*OutFunctionNamePtr = "Unknown";
				}
			}

			if (LIKELY(OutSourceFilePtr != nullptr && OutLineNumberPtr != nullptr))
			{
				if (SrcFile != nullptr)
				{
					*OutSourceFilePtr = SrcFile;
					*OutLineNumberPtr = LineNumber;
				}
				else
				{
					*OutSourceFilePtr = "Unknown";
					*OutLineNumberPtr = -1;
				}
			}

			if (LIKELY(OutModuleNamePtr != nullptr))
			{
				const char* ModuleName = nullptr;

				Dl_info DlInfo;
				if (dladdr(Address, &DlInfo) != 0)
				{
					if (DlInfo.dli_fname != nullptr)
					{
						ModuleName = DlInfo.dli_fname;	// this is a pointer we don't own, but assuming it's good until at least the next dladdr call
					}
				}

				if (LIKELY(ModuleName != nullptr))
				{
					*OutModuleNamePtr = ModuleName;
				}
				else
				{
					*OutModuleNamePtr = "Unknown";
				}
			}
		}

		// catch-all
		if (Die)
		{
			dwarf_dealloc(DebugInfo, Die, DW_DLA_DIE);
			Die = nullptr;
		}

		// Resets internal CU pointer, so next time we get here it begins from the start
		while (ReturnCode != DW_DLV_NO_ENTRY) 
		{
			if (ReturnCode == DW_DLV_ERROR)
			{
				break;
			}
			ReturnCode = dwarf_next_cu_header(DebugInfo, NULL, NULL, NULL, NULL, NULL, &ErrorInfo);
		}

		return bSuccess;
	}

	bool UnixBacktraceSymbols::CheckAddressInRange(Dwarf_Debug DebugInfo, Dwarf_Die Die, Dwarf_Unsigned Addr)
	{
		Dwarf_Attribute *AttrList;
		Dwarf_Signed AttrCount;

		if (UNLIKELY(dwarf_attrlist(Die, &AttrList, &AttrCount, NULL) != DW_DLV_OK))
		{
			// assume not in range if we couldn't get the information
			return false;
		}

		Dwarf_Addr LowAddr = 0, HighAddr = 0, HighOffset = 0;

		for (int i = 0; i < AttrCount; i++)
		{
			Dwarf_Half Attr;
			if (dwarf_whatattr(AttrList[i], &Attr, nullptr) != DW_DLV_OK)
			{
				continue;
			}

			switch (Attr)
			{
				case DW_AT_low_pc:
					{
						Dwarf_Addr TempLowAddr;
						if (dwarf_formaddr(AttrList[i], &TempLowAddr, nullptr) == DW_DLV_OK)
						{
							if (LIKELY(TempLowAddr > Addr))	// shortcut
							{
								return false;
							}

							LowAddr = TempLowAddr;
						}
					}
					break;

				case DW_AT_high_pc:
					{
						Dwarf_Addr TempHighAddr;
						if (dwarf_formaddr(AttrList[i], &TempHighAddr, nullptr) == DW_DLV_OK)
						{
							if (LIKELY(TempHighAddr <= Addr))	// shortcut
							{
								return false;
							}

							HighAddr = TempHighAddr;
						}

						// Offset is used since DWARF-4. Store it, but don't compare right now in case
						// we haven't yet initialized LowAddr
						Dwarf_Unsigned TempHighOffset;
						if (dwarf_formudata(AttrList[i], &TempHighOffset, nullptr) == DW_DLV_OK)
						{
							HighOffset = TempHighOffset;
						}
					}
					break;

				case DW_AT_ranges:
					{
						Dwarf_Unsigned Offset;
						if (dwarf_formudata(AttrList[i], &Offset, NULL) != DW_DLV_OK)
						{
							continue;
						}

						Dwarf_Ranges *Ranges;
						Dwarf_Signed Count;
						if (dwarf_get_ranges(DebugInfo, (Dwarf_Off) Offset, &Ranges, &Count, nullptr, nullptr) != DW_DLV_OK)
						{
							continue;
						}

						for (int j = 0; j < Count; j++)
						{
							if (Ranges[j].dwr_type == DW_RANGES_END)
							{
								break;
							}
							if (Ranges[j].dwr_type == DW_RANGES_ENTRY)
							{
								if ((Ranges[j].dwr_addr1 <= Addr) && (Addr < Ranges[j].dwr_addr2))
								{
									return true;
								}
								continue;
							}
						}
						return false;
					}
					break;

				default:
					break;
			}
		}

		if (UNLIKELY(HighAddr == 0 && HighOffset != 0))
		{
			HighAddr = LowAddr + HighOffset;
		}

		return LowAddr <= Addr && Addr < HighAddr;
	}

	bool UnixBacktraceSymbols::FindNameAttributeInDIE(Dwarf_Debug DebugInfo, Dwarf_Die* InDiePtr, const char **OutFuncName)
	{
		Dwarf_Error ErrorInfo;
		int ReturnCode;
		Dwarf_Die Die = *InDiePtr;

		// look first for DW_AT_linkage_name or DW_AT_MIPS_linkage_name, since they hold fully qualified (albeit mangled) name
		Dwarf_Attribute LinkageNameAt;
		// DW_AT_MIPS_linkage_name is preferred because we're using DWARF2 by default
		ReturnCode = dwarf_attr(Die, DW_AT_MIPS_linkage_name, &LinkageNameAt, &ErrorInfo);
		if (UNLIKELY(ReturnCode == DW_DLV_NO_ENTRY))
		{
			// retry with newer DW_AT_linkage_name
			ReturnCode = dwarf_attr(Die, DW_AT_linkage_name, &LinkageNameAt, &ErrorInfo);
		}

		if (LIKELY(ReturnCode == DW_DLV_OK))
		{
			char *TempFuncName;
			if (LIKELY(dwarf_formstring(LinkageNameAt, &TempFuncName, &ErrorInfo) == DW_DLV_OK))
			{
				// try to demangle
				int DemangleStatus = 0xBAD;
				char *Demangled = abi::__cxa_demangle(TempFuncName, nullptr, nullptr, &DemangleStatus);
				if (DemangleStatus == 0 && Demangled != nullptr)
				{
					// cache the demangled name
					static char CachedDemangledName[1024];
					FCStringAnsi::Strcpy(CachedDemangledName, sizeof(CachedDemangledName), Demangled);

					*OutFuncName = CachedDemangledName;
				}
				else
				{
					*OutFuncName = TempFuncName;
				}

				if (Demangled)
				{
					free(Demangled);
				}

				// deallocate the DIE
				dwarf_dealloc(DebugInfo, Die, DW_DLA_DIE);
				*InDiePtr = nullptr;
				return true;
			}
		}

		// if everything else fails, just take DW_AT_name, but in case of class methods, it is only a method name, so the information will be incomplete and almost useless
		const char *TempMethodName;
		if (LIKELY(dwarf_attrval_string(Die, DW_AT_name, &TempMethodName, &ErrorInfo) == DW_DLV_OK))
		{
			*OutFuncName = TempMethodName;

			// deallocate the DIE
			dwarf_dealloc(DebugInfo, Die, DW_DLA_DIE);
			*InDiePtr = nullptr;
			return true;
		}

		return false;
	}

	bool UnixBacktraceSymbols::FindFunctionNameInDIE(Dwarf_Debug DebugInfo, Dwarf_Die* InDiePtr, Dwarf_Addr Addr, const char **OutFuncName)
	{
		Dwarf_Error ErrorInfo;
		Dwarf_Half Tag;
		Dwarf_Die Die = *InDiePtr;

		if (dwarf_tag(Die, &Tag, &ErrorInfo) != DW_DLV_OK || Tag != DW_TAG_subprogram)
		{
			return false;
		}

		// check if address is inside this entry
		if (!CheckAddressInRange(DebugInfo, Die, Addr))
		{
			return false;
		}

		// attempt to find the name in DW_TAG_subprogram DIE
		if (FindNameAttributeInDIE(DebugInfo, &Die, OutFuncName))	// this function will allocate the DIE on success
		{
			*InDiePtr = nullptr;
			return true;
		}

		// If not found, navigate to specification DIE and look there
		Dwarf_Attribute SpecAt;
		if (UNLIKELY(dwarf_attr(Die, DW_AT_specification, &SpecAt, &ErrorInfo) != DW_DLV_OK))
		{
			// no specification die
			return false;
		}

		Dwarf_Off Offset;
		if (UNLIKELY(dwarf_global_formref(SpecAt, &Offset, &ErrorInfo) != DW_DLV_OK))
		{
			return false;
		}

		Dwarf_Die SpecDie;
		if (UNLIKELY(dwarf_offdie(DebugInfo, Offset, &SpecDie, &ErrorInfo) != DW_DLV_OK))
		{
			return false;
		}

		if (FindNameAttributeInDIE(DebugInfo, &SpecDie, OutFuncName))	// this function will allocate the DIE on success
		{
			// but we still need to deallocate our original DIE
			dwarf_dealloc(DebugInfo, Die, DW_DLA_DIE);
			*InDiePtr = nullptr;

			return true;
		}

		return false;
	}

	void UnixBacktraceSymbols::FindFunctionNameInDIEAndChildren(Dwarf_Debug DebugInfo, Dwarf_Die* InDiePtr, Dwarf_Addr Addr, const char **OutFuncName)
	{
		if (OutFuncName == NULL || *OutFuncName != NULL)
		{
			dwarf_dealloc(DebugInfo, *InDiePtr, DW_DLA_DIE);
			*InDiePtr = nullptr;
			return;
		}

		// search for this Die  (FFNID will deallocate the Die if successful)
		if (FindFunctionNameInDIE(DebugInfo, InDiePtr, Addr, OutFuncName))
		{
			return;
		}

		Dwarf_Die PrevChild = *InDiePtr, Current = nullptr;
		*InDiePtr = nullptr;	// mark input Die as deallocated so the caller doesn't use it
		Dwarf_Error ErrorInfo;

		int32 MaxChildrenAllowed = 32 * 1024 * 1024;	// safeguard to make sure we never get into an infinite loop
		for(;;)
		{
			if (UNLIKELY(--MaxChildrenAllowed <= 0))
			{
				fprintf(stderr, "Breaking out from what seems to be an infinite loop during DWARF parsing (too many children).\n");
				dwarf_dealloc(DebugInfo, PrevChild, DW_DLA_DIE);
				return;
			}

			// Get the child
			if (UNLIKELY(dwarf_child(PrevChild, &Current, &ErrorInfo) != DW_DLV_OK))
			{
				dwarf_dealloc(DebugInfo, PrevChild, DW_DLA_DIE);
				return;	// bail out
			}

			// Current cannot be nullptr because if we had no child, dwarf_child() would not return Ok

			// prev child needs to be disposed of first.
			dwarf_dealloc(DebugInfo, PrevChild, DW_DLA_DIE);
			PrevChild = Current;

			// look for in the child
			if (UNLIKELY(FindFunctionNameInDIE(DebugInfo, &Current, Addr, OutFuncName)))
			{
				return;	// got the function name!
			}

			// search among Current's siblings. Do not deallocate Current (== PrevChild), because we may need it if we don't find
			int32 MaxSiblingsAllowed = 64 * 1024 * 1024;	// safeguard to make sure we never get into an infinite loop
			Dwarf_Die CurSibling = nullptr;
			if (dwarf_siblingof(DebugInfo, Current, &CurSibling, &ErrorInfo) == DW_DLV_OK)
			{
				for (;;)
				{
					if (UNLIKELY(--MaxSiblingsAllowed <= 0))
					{
						fprintf(stderr, "Breaking out from what seems to be an infinite loop during DWARF parsing (too many siblings).\n");
						break;
					}

					Dwarf_Die NewSibling;
					bool bStopLookingForSiblings = dwarf_siblingof(DebugInfo, CurSibling, &NewSibling, &ErrorInfo) != DW_DLV_OK;
					dwarf_dealloc(DebugInfo, CurSibling, DW_DLA_DIE);
					if (UNLIKELY(bStopLookingForSiblings))
					{
						break;
					}
					CurSibling = NewSibling;

					// this function will deallocate cursibling on success
					if (UNLIKELY(FindFunctionNameInDIE(DebugInfo, &CurSibling, Addr, OutFuncName)))
					{
						// deallocate Current as we don't need it anymore
						dwarf_dealloc(DebugInfo, Current, DW_DLA_DIE);
						return;	// got the function name!
					}
				}
			}
		};
	}

	/**
	 * Finds mangled name and returns it in internal buffer.
	 * Caller doesn't have to deallocate that.
	 */
	const char * GetMangledName(const char * SourceInfo)
	{
		static char MangledName[MaxMangledNameLength + 1];
		const char * Current = SourceInfo;

		MangledName[0] = 0;
		if (Current == NULL)
		{
			return MangledName;
		}

		// find '('
		for (; *Current != 0 && *Current != '('; ++Current);

		// if unable to find, return original
		if (*Current == 0)
		{
			return SourceInfo;
		}

		// copy everything until '+'
		++Current;
		size_t BufferIdx = 0;
		for (; *Current != 0 && *Current != '+' && BufferIdx < MaxMangledNameLength; ++Current, ++BufferIdx)
		{
			MangledName[BufferIdx] = *Current;
		}

		// if unable to find, return original
		if (*Current == 0)
		{
			return SourceInfo;
		}

		MangledName[BufferIdx] = 0;
		return MangledName;
	}

	/**
	 * Returns source filename for particular callstack depth (or NULL).
	 * Caller doesn't have to deallocate that.
	 */
	const char * GetFunctionName(FGenericCrashContext* Context, int32 CurrentCallDepth)
	{
		static char DemangledName[MaxDemangledNameLength + 1];
		if (Context == NULL || CurrentCallDepth < 0)
		{
			return NULL;
		}

		FUnixCrashContext* UnixContext = static_cast< FUnixCrashContext* >( Context );

		if (UnixContext->BacktraceSymbols == NULL)
		{
			return NULL;
		}

		const char * SourceInfo = UnixContext->BacktraceSymbols[CurrentCallDepth];
		if (SourceInfo == NULL)
		{
			return NULL;
		}

		// see http://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html for C++ demangling
		int DemangleStatus = 0xBAD;
		char * Demangled = abi::__cxa_demangle(GetMangledName( SourceInfo ), NULL, NULL, &DemangleStatus);
		if (Demangled != NULL && DemangleStatus == 0)
		{
			FCStringAnsi::Strncpy(DemangledName, Demangled, ARRAY_COUNT(DemangledName) - 1);
		}
		else
		{
			FCStringAnsi::Strncpy(DemangledName, SourceInfo, ARRAY_COUNT(DemangledName) - 1);
		}

		if (Demangled)
		{
			free(Demangled);
		}
		return DemangledName;
	}

	void AppendToString(ANSICHAR * HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext * Context, const ANSICHAR * Text)
	{
		FCStringAnsi::Strncat(HumanReadableString, Text, HumanReadableStringSize);
	}

	void AppendFunctionNameIfAny(FUnixCrashContext & UnixContext, const char * FunctionName, uint64 ProgramCounter)
	{
		if (FunctionName && FunctionName[0] != '\0')
		{
			FCStringAnsi::Strncat(UnixContext.MinidumpCallstackInfo, FunctionName, ARRAY_COUNT( UnixContext.MinidumpCallstackInfo ) - 1);
			FCStringAnsi::Strncat(UnixContext.MinidumpCallstackInfo, " + some bytes", ARRAY_COUNT( UnixContext.MinidumpCallstackInfo ) - 1);	// this is just to conform to crashreporterue4 standard
		}
		else
		{
			ANSICHAR TempArray[MAX_SPRINTF];

			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "0x%016llx", ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "0x%08x", (uint32)ProgramCounter);
			}
			FCStringAnsi::Strncat(UnixContext.MinidumpCallstackInfo, TempArray, ARRAY_COUNT( UnixContext.MinidumpCallstackInfo ) - 1);
		}
	}

	UnixBacktraceSymbols *GetBacktraceSymbols()
	{
		static UnixBacktraceSymbols Symbols;
		Symbols.Init();
		return &Symbols;
	}
}

namespace
{
	// These structures are copied from Engine/Source/Programs/BreakpadSymbolEncoder/BreakpadSymbolEncoder.h
	// DO NOT CHANGE THE SIZE OF THESES STRUCTURES (Unless the BreakpadSymbolEncoder.h structures have changed)
#pragma pack(push, 1)
	struct RecordsHeader
	{
		uint32_t RecordCount;
	};

	struct Record
	{
		uint64_t Address    = static_cast<uint64_t>(-1);
		uint32_t LineNumber = static_cast<uint32_t>(-1);
		uint32_t FileRelativeOffset   = static_cast<uint32_t>(-1);
		uint32_t SymbolRelativeOffset = static_cast<uint32_t>(-1);
	};
#pragma pack(pop)

	class RecordReader
	{
	public:
		explicit RecordReader(const char* Path)
			: SymbolFileFD(open(Path, O_RDONLY)),
			  StartOffset(sizeof(RecordsHeader))
		{
			if (SymbolFileFD)
			{
				// TODO check for EINTR
				read(SymbolFileFD, static_cast<void*>(&RecordCount), sizeof(RecordsHeader));
			}
		}

		~RecordReader()
		{
			if (SymbolFileFD != -1)
			{
				close(SymbolFileFD);
			}
		}

		bool IsValid() const
		{
			return SymbolFileFD != -1 && RecordCount > 0;
		}

		uint32_t GetRecordCount() const
		{
			return RecordCount;
		}

		Record GetRecord(int Index) const
		{
			// When we remove this check, make sure we handle possible out of bounds cases
			if (Index > RecordCount || Index < 0)
			{
				return {};
			}

			Record Out;

			lseek(SymbolFileFD, StartOffset + Index * sizeof(Record), SEEK_SET);
			read(SymbolFileFD, static_cast<void*>(&Out), sizeof(Record));

			return Out;
		}

		void ReadOffsetIntoMemory(char* Buffer, size_t MaxSize, uint32_t Offset)
		{
			// Offset unsigned -1 (0xffffffff) == Invalid
			if (Offset == static_cast<uint32_t>(-1))
			{
				return;
			}

			lseek(SymbolFileFD, StartOffset + RecordCount * sizeof(Record) + Offset, SEEK_SET);
			read(SymbolFileFD, Buffer, MaxSize);

			// Read the max chunk we can read, then find the next '\n' and replace that with '\0'
			for (int i = 0; i < MaxSize; i++)
			{
				if (Buffer[i] == '\n')
				{
					Buffer[i] = '\0';
					return;
				}
			}

			// We couldnt find the end of the line, lets assume we failed to read in a line
			Buffer[0] = '\0';
		}

	private:
		int SymbolFileFD;

		// For now can only be up to 4GB
		uint32_t StartOffset = 0;
		uint32_t RecordCount = 0;
	};

	bool PopulateProgramCounterSymbolInfoFromSymbolFile(uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo)
	{
		Dl_info info;
		if (dladdr(reinterpret_cast<void*>(ProgramCounter), &info) != 0)
		{
			out_SymbolInfo.ProgramCounter = ProgramCounter;

			if (UNLIKELY(info.dli_fname == nullptr))
			{
				// If we cannot find the Module name lets return early
				return false;
			}
			else if (UNLIKELY(info.dli_fbase == nullptr))
			{
				// If we cannot find the module base lets return early
				return false;
			}

			const ANSICHAR* SOPath = info.dli_fname;
			const ANSICHAR* SOName = FCStringAnsi::Strrchr(SOPath, '/');
			if (SOName)
			{
				SOName += 1;
			}
			else
			{
				SOName = SOPath;
			}

			FCStringAnsi::Strcpy(out_SymbolInfo.ModuleName, SOName);
			out_SymbolInfo.OffsetInModule = ProgramCounter - reinterpret_cast<uint64>(info.dli_fbase);

			if (info.dli_saddr != nullptr)
			{
				out_SymbolInfo.SymbolDisplacement = ProgramCounter - reinterpret_cast<uint64>(info.dli_saddr);
			}
			// If we cant find the function either lets just give it the offset into the module
			else if (info.dli_sname == nullptr)
			{
				out_SymbolInfo.SymbolDisplacement = out_SymbolInfo.OffsetInModule;
			}

			if (info.dli_sname != nullptr)
			{
				FCStringAnsi::Strcpy(out_SymbolInfo.FunctionName, info.dli_sname);
			}

			ANSICHAR ModuleSymbolPath[UNIX_MAX_PATH + 1];

			// We cant assume if we are relative we have not chdir to a different working dir.
			if (FPaths::IsRelative(info.dli_fname))
			{
				FCStringAnsi::Strcpy(ModuleSymbolPath, TCHAR_TO_ANSI(FPlatformProcess::BaseDir()));
				FCStringAnsi::Strcat(ModuleSymbolPath, TCHAR_TO_ANSI(*FPaths::GetBaseFilename(out_SymbolInfo.ModuleName)));
				FCStringAnsi::Strcat(ModuleSymbolPath, ".sym");
			}
			else
			{
				FCStringAnsi::Strcpy(ModuleSymbolPath, TCHAR_TO_ANSI(*FPaths::GetBaseFilename(info.dli_fname, false)));
				FCStringAnsi::Strcat(ModuleSymbolPath, ".sym");
			}

			// TODO We should look at only opening the file once per entire callstack (but it depends on the module names)
			RecordReader Reader(ModuleSymbolPath);

			if (Reader.IsValid())
			{
				size_t Start = 0;
				size_t End   = Reader.GetRecordCount() - 1;
				bool bFound  = false;
				uint64 AddressToFind = out_SymbolInfo.OffsetInModule;

				// Make sure we arent not trying the same middle index over and over again
				size_t LastMiddle = 1;
				size_t Middle = 0;

				while (End - Start > 0 && LastMiddle != Middle)
				{
					LastMiddle = Middle;
					Middle     = (Start + End) / 2;

					if (Middle + 1 >= Reader.GetRecordCount())
					{
						// We have placed a dummy record at the end of all the records. So if our Middle index
						// happens to be that dummy record we are in a bad spot
						break;
					}

					Record Current = Reader.GetRecord(Middle);
					Record Next    = Reader.GetRecord(Middle + 1);
					size_t Size    = Next.Address - Current.Address;

					if (AddressToFind >= Current.Address && AddressToFind < Current.Address + Size)
					{
						// Hack when we have a zero line number to attempt to use the previous record for a better guess.
						// non-virtual thunks seem to cause a bunch of these but this will not fix those.
						if (Current.LineNumber == 0)
						{
							Record Previous = Reader.GetRecord(Middle - 1);
							if (Previous.LineNumber > 0 && Previous.LineNumber != static_cast<uint32_t>(-1))
							{
								Current.LineNumber = Previous.LineNumber;
							}
						}

						Reader.ReadOffsetIntoMemory(out_SymbolInfo.Filename, sizeof(out_SymbolInfo.Filename), Current.FileRelativeOffset);
						Reader.ReadOffsetIntoMemory(out_SymbolInfo.FunctionName, sizeof(out_SymbolInfo.FunctionName), Current.SymbolRelativeOffset);
						out_SymbolInfo.LineNumber = Current.LineNumber;

						// If we find a function but no sname from dladdr we cannot make assumptions about its symbol displacement.
						// And a Function name is better then the OffsetInModule address
						if (info.dli_sname == nullptr)
						{
							out_SymbolInfo.SymbolDisplacement = 0x0;
						}

						// If we dont have a file name we have to assume its just a public symbol and use the old way to demangle the backtrace info
						if (out_SymbolInfo.Filename[0] == '\0')
						{
							return false;
						}

						return true;
					}
					else if (AddressToFind > Current.Address)
					{
						Start = Middle;
					}
					else
					{
						End = Middle;
					}
				}
			}
			// We only care if we fail to find our own *.sym file
			else if (!FCStringAnsi::Strcmp(SOName, TCHAR_TO_ANSI(FPlatformProcess::ExecutableName())))
			{
				static bool bReported = false;
				if (!bReported)
				{
					// This will likely happen multiple times, so only write out once
					bReported = true;

					// Will not be part of UE_LOG as it would potentially allocate memory
					const ANSICHAR* Message = "Failed to find symbol file, expected location:\n\"";
					write(STDOUT_FILENO, Message, FCStringAnsi::Strlen(Message));
					write(STDOUT_FILENO, ModuleSymbolPath, FCStringAnsi::Strlen(ModuleSymbolPath));
					write(STDOUT_FILENO, "\"\n", 2);
				}
			}
		}

		return false;
	}
}

void FUnixPlatformStackWalk::ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo )
{
	if (GUseNewCrashSymbolicator)
	{
		PopulateProgramCounterSymbolInfoFromSymbolFile(ProgramCounter, out_SymbolInfo);
	}
	else
	{
		// Set the program counter.
		out_SymbolInfo.ProgramCounter = ProgramCounter;

		// Get function, filename and line number.
		const char* ModuleName = nullptr;
		const char* FunctionName = nullptr;
		const char* SourceFilename = nullptr;
		int LineNumber = 0;

		if (UnixStackWalkHelpers::GetBacktraceSymbols()->GetInfoForAddress(reinterpret_cast<void*>(ProgramCounter), &ModuleName, &FunctionName, &SourceFilename, &LineNumber))
		{
			out_SymbolInfo.LineNumber = LineNumber;

			if (LIKELY(ModuleName != nullptr))
			{
				FCStringAnsi::Strcpy(out_SymbolInfo.ModuleName, sizeof(out_SymbolInfo.ModuleName), ModuleName);
			}

			if (LIKELY(SourceFilename != nullptr))
			{
				FCStringAnsi::Strcpy(out_SymbolInfo.Filename, sizeof(out_SymbolInfo.Filename), SourceFilename);
			}

			if (FunctionName != nullptr)
			{
				FCStringAnsi::Strcpy(out_SymbolInfo.FunctionName, sizeof(out_SymbolInfo.Filename), FunctionName);
			}
			else
			{
				sprintf(out_SymbolInfo.FunctionName, "0x%016llx", ProgramCounter);
			}
		}
		// Temp fix to get modules names for portable callstack. Once we have the new way to symbolicate crashes we can remove this
		else
		{
			Dl_info info;
			if (dladdr(reinterpret_cast<void*>(ProgramCounter), &info) != 0)
			{
				// Only need the Module name for the portable bits
				if (LIKELY(info.dli_fname != nullptr))
				{
					const ANSICHAR* SOPath = info.dli_fname;
					const ANSICHAR* SOName = FCStringAnsi::Strrchr(SOPath, '/');
					if (SOName)
					{
						SOName += 1;
					}
					else
					{
						SOName = SOPath;
					}

					FCStringAnsi::Strcpy(out_SymbolInfo.ModuleName, SOName);
				}
			}
		}
	}
}

bool FUnixPlatformStackWalk::ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context )
{
	//
	// Callstack lines should be written in this standard format
	//
	//	0xaddress module!func [file]
	// 
	// E.g. 0x045C8D01 OrionClient.self!UEngine::PerformError() [D:\Epic\Orion\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp:6481]
	//
	// Module may be omitted, everything else should be present, or substituted with a string that conforms to the expected type
	//
	// E.g 0x00000000 UnknownFunction []
	//
	// 

	if (HumanReadableString && HumanReadableStringSize > 0)
	{
		ANSICHAR TempArray[MAX_SPRINTF];
		if (CurrentCallDepth < 0)
		{
			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "0x%016llx ", ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "0x%08x ", (uint32) ProgramCounter);
			}
			UnixStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);

			// won't be able to display names here
		}
		else
		{
			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "0x%016llx ", ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "0x%08x ", (uint32) ProgramCounter);
			}
			UnixStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);

			// Get filename, source file and line number
			FUnixCrashContext* UnixContext = static_cast< FUnixCrashContext* >( Context );
			if (UnixContext)
			{
				// for ensure, use the fast path - do not even attempt to get detailed info as it will result in long hitch
				bool bAddDetailedInfo = !UnixContext->GetIsEnsure();

				if (GUseNewCrashSymbolicator)
				{
					// Program counters in the backtrace point to the location from where the execution will be resumed (in all frames except the one where we crashed),
					// which results in callstack pointing to the next lines in code. In order to determine the source line where the actual call happened, we need to go
					// back to the line that had the "call" instruction. Since x86(-64) instructions vary in length, we cannot do it reliably without disassembling,
					// just go back one byte - even if it's not the actual address of the call site.
					int OffsetToCallsite = CurrentCallDepth > 0 ? 1 : 0;

					FProgramCounterSymbolInfo TempSymbolInfo;

					// We can print detail info out during ensures, the only reason not to is if fail to populate the symbol info all the way
					bAddDetailedInfo = PopulateProgramCounterSymbolInfoFromSymbolFile(ProgramCounter - OffsetToCallsite, TempSymbolInfo);

					if (bAddDetailedInfo)
					{
						// append Module!FunctionName() [Source.cpp:X] to HumanReadableString
						FCStringAnsi::Strncat(HumanReadableString, TempSymbolInfo.ModuleName, HumanReadableStringSize);
						FCStringAnsi::Strncat(HumanReadableString, "!", HumanReadableStringSize);
						FCStringAnsi::Strncat(HumanReadableString, TempSymbolInfo.FunctionName, HumanReadableStringSize);
						FCStringAnsi::Sprintf(TempArray, " [%s:%d]", TempSymbolInfo.Filename, TempSymbolInfo.LineNumber);
						FCStringAnsi::Strncat(HumanReadableString, TempArray, HumanReadableStringSize);

						// append Module!FunctioName [Source.cpp:X] to MinidumpCallstackInfo
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, TempSymbolInfo.ModuleName, ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, "!", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, TempSymbolInfo.FunctionName, ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, TempArray, ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
					}
					else
					{
						const char* ModuleName   = nullptr;
						const char* FunctionName = nullptr;

						// We have failed to fully populate the SymbolInfo, but we could still have basic information. Lets try to print as much info as possible
						if (TempSymbolInfo.ModuleName[0] != '\0')
						{
							ModuleName = TempSymbolInfo.ModuleName;
						}

						if (TempSymbolInfo.FunctionName[0] != '\0')
						{
							FunctionName = TempSymbolInfo.FunctionName;
						}

						FCStringAnsi::Strncat(HumanReadableString, ModuleName != nullptr ? ModuleName : "", HumanReadableStringSize);
						FCStringAnsi::Strncat(HumanReadableString, "!", HumanReadableStringSize);
						FCStringAnsi::Strncat(HumanReadableString, FunctionName != nullptr ? FunctionName : "UnknownFunction", HumanReadableStringSize);
						FCStringAnsi::Strncat(HumanReadableString, FunctionName && TempSymbolInfo.SymbolDisplacement ? "(+": "(", HumanReadableStringSize);

						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, ModuleName != nullptr ? ModuleName : "Unknown", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, "!", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, FunctionName != nullptr ? FunctionName : "UnknownFunction", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, FunctionName && TempSymbolInfo.SymbolDisplacement ? "(+": "(", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);

						if (TempSymbolInfo.SymbolDisplacement > 0x0)
						{
							FCStringAnsi::Sprintf(TempArray, "%p", TempSymbolInfo.SymbolDisplacement);
							FCStringAnsi::Strncat(HumanReadableString, TempArray, HumanReadableStringSize);
							FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, TempArray, ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						}

						FCStringAnsi::Strncat(HumanReadableString, ")", HumanReadableStringSize);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, ")", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
					}

				}
				else
				{
					const char * ModuleName = nullptr;
					const char * FunctionName = nullptr;
					const char * SourceFilename = nullptr;
					int LineNumber;

					// attempt to get the said detailed info
					bAddDetailedInfo = bAddDetailedInfo && UnixStackWalkHelpers::GetBacktraceSymbols()->GetInfoForAddress(reinterpret_cast<void*>(ProgramCounter), &ModuleName, &FunctionName, &SourceFilename, &LineNumber);

					if (bAddDetailedInfo)
					{
						// append FunctionName() [Source.cpp:X] to HumanReadableString
						UnixStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, FunctionName);
						FCStringAnsi::Sprintf(TempArray, " [%s:%d]", SourceFilename, LineNumber);
						UnixStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);

						// append Module!FunctioName [Source.cpp:X] to MinidumpCallstackInfo
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, ModuleName, ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, "!", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						UnixStackWalkHelpers::AppendFunctionNameIfAny(*UnixContext, FunctionName, ProgramCounter);
						FCStringAnsi::Sprintf(TempArray, " [%s:%d]", SourceFilename, LineNumber);
						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, TempArray, ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
					}
					else
					{
						// get the function name for backtrace, may be incorrect
						FunctionName = UnixStackWalkHelpers::GetFunctionName(Context, CurrentCallDepth);
					
						UnixStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, FunctionName != nullptr ? FunctionName : "UnknownFunction");	

						FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, "Unknown!", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);
						UnixStackWalkHelpers::AppendFunctionNameIfAny(*UnixContext, FunctionName, ProgramCounter);
					}
				}

				FCStringAnsi::Strncat(UnixContext->MinidumpCallstackInfo, "\r\n", ARRAY_COUNT( UnixContext->MinidumpCallstackInfo ) - 1);	// this one always uses Windows line terminators
			}
		}
		return true;
	}
	return true;
}

void FUnixPlatformStackWalk::StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context )
{
	if (Context == nullptr)
	{
		FUnixCrashContext CrashContext;
		CrashContext.InitFromSignal(0, nullptr, nullptr);
		CrashContext.FirstCrashHandlerFrame = static_cast<uint64*>(__builtin_return_address(0));
		FGenericPlatformStackWalk::StackWalkAndDump(HumanReadableString, HumanReadableStringSize, IgnoreCount, &CrashContext);
	}
	else
	{
		FGenericPlatformStackWalk::StackWalkAndDump(HumanReadableString, HumanReadableStringSize, IgnoreCount, Context);
	}
}

void FUnixPlatformStackWalk::StackWalkAndDumpEx(ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, uint32 Flags, void* Context)
{
	const bool bHandlingEnsure = (Flags & EStackWalkFlags::FlagsUsedWhenHandlingEnsure) == EStackWalkFlags::FlagsUsedWhenHandlingEnsure;
	if (Context == nullptr)
	{
		FUnixCrashContext CrashContext(bHandlingEnsure);
		CrashContext.InitFromSignal(0, nullptr, nullptr);
		CrashContext.FirstCrashHandlerFrame = static_cast<uint64*>(__builtin_return_address(0));
		FPlatformStackWalk::StackWalkAndDump(HumanReadableString, HumanReadableStringSize, IgnoreCount, &CrashContext);
	}
	else
	{
		/** Helper sets the ensure value in the context and guarantees it gets reset afterwards (even if an exception is thrown) */
		struct FLocalGuardHelper
		{
			FLocalGuardHelper(FUnixCrashContext* InContext, bool bNewEnsureValue)
				: Context(InContext), bOldEnsureValue(Context->GetIsEnsure())
			{
				Context->SetIsEnsure(bNewEnsureValue);
			}
			~FLocalGuardHelper()
			{
				Context->SetIsEnsure(bOldEnsureValue);
			}

		private:
			FUnixCrashContext* Context;
			bool bOldEnsureValue;
		};

		FLocalGuardHelper Guard(reinterpret_cast<FUnixCrashContext*>(Context), bHandlingEnsure);
		FPlatformStackWalk::StackWalkAndDump(HumanReadableString, HumanReadableStringSize, IgnoreCount, Context);
	}
}

namespace
{
	uint32 OverwriteBacktraceWithRealCallstack(uint64* BackTrace, uint32 Size, uint64* FirstCrashHandlerFrame)
	{
		if (!GFullCrashCallstack && Size && FirstCrashHandlerFrame != nullptr)
		{
			for (int i = 0; i < Size - 1; i++)
			{
				if (FirstCrashHandlerFrame == reinterpret_cast<uint64*>(BackTrace[i]))
				{
					i++;
					uint64* OverwriteBackTrace = BackTrace;

					for (int j = i; j < Size; j++)
					{
						*OverwriteBackTrace = BackTrace[j];
						OverwriteBackTrace++;
					}

					return Size - i;
				}
			}
		}

		return Size;
	}
}

uint32 FUnixPlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
{
	// Make sure we have place to store the information before we go through the process of raising
	// an exception and handling it.
	if( BackTrace == nullptr || MaxDepth == 0 )
	{
		return 0;
	}

	size_t Size = backtrace(reinterpret_cast<void**>(BackTrace), MaxDepth);

	FUnixCrashContext* UnixContext = reinterpret_cast<FUnixCrashContext*>(Context);

	if (GUseNewCrashSymbolicator)
	{
		if (UnixContext)
		{
			return OverwriteBacktraceWithRealCallstack(BackTrace, Size, UnixContext->FirstCrashHandlerFrame);
		}
	}
	// Remove once we remove the old crash symbolicator
	else
	{
		if (UnixContext->BacktraceSymbols == nullptr)
		{
			// #CrashReport: 2014-09-29 Replace with backtrace_symbols_fd due to malloc()
			UnixContext->BacktraceSymbols = backtrace_symbols(reinterpret_cast< void** >( BackTrace ), MaxDepth);
		}
	}

	return (uint32)Size;
}

void FUnixPlatformStackWalk::ThreadStackWalkAndDump(ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, uint32 ThreadId)
{
	// This is intentional on servers. Right now we cannot symbolicate the other thread, so we crash it instead, which also helps in identifying lock ups
	if (UE_SERVER)
	{
		if (kill(ThreadId, SIGQUIT) == 0)
		{
			// do not exit, crash is imminent anyway (signals are delivered asynchronously)
			for(;;)
			{
			}
		}
	}
}

namespace
{
	int NumberOfDynamicLibrariesCallback(struct dl_phdr_info* Info, size_t /*size*/, void* Data)
	{
		int* Size = static_cast<int*>(Data);
		if (Info->dlpi_name)
		{
			(*Size)++;
		}

		// Continute until no more callbacks
		return 0;
	}
}

int32 FUnixPlatformStackWalk::GetProcessModuleCount()
{
	// If we are not using the new crash symbolicator we want to avoid generating portable callstacks these
	// as they may cause dwarf/elf symbols to be loaded dynamically which can cause hitching during an ensure
	if (!GUseNewCrashSymbolicator)
	{
		return 0;
	}

	int Size = 0;
	dl_iterate_phdr(NumberOfDynamicLibrariesCallback, &Size);

	return Size;
}

namespace
{
	struct ProcessModuleSignatures
	{
		FStackWalkModuleInfo* ModuleSignatures;
		int32 ModuleSignaturesSize;
		int32 Index;
	};

	int CollectModuleSignatures(struct dl_phdr_info* Info, size_t /*size*/, void* Data)
	{
		ProcessModuleSignatures* Modules = static_cast<ProcessModuleSignatures*>(Data);

		if (Info->dlpi_name)
		{
			int TotalMemSize = 0;
			uint64 RealBase = 0;
			bool bRealBaseSet = false;
			for (int i = 0; i < Info->dlpi_phnum; i++)
			{
				TotalMemSize += Info->dlpi_phdr[i].p_memsz;

				// Lets get our real base from the BASE + first LOAD segment
				if (!bRealBaseSet && Info->dlpi_phdr[i].p_type == PT_LOAD)
				{
					RealBase = (uint64)(Info->dlpi_addr + Info->dlpi_phdr[i].p_vaddr);
					bRealBaseSet = true;
				}
			}

			FString ImageName = FPaths::GetCleanFilename(Info->dlpi_name);

			// If Info->dlpi_name is empty then it is the binary name
			if (ImageName.IsEmpty())
			{
				ImageName = FPlatformProcess::ExecutableName(false);
			}

			FStackWalkModuleInfo StackInfo;
			memset(&StackInfo, 0, sizeof(StackInfo));
			StackInfo.BaseOfImage = RealBase;
			// TODO Check if ImageName is greater then 32 bytes, if so we need to look at increasing the struct size
			FCString::Strcpy(StackInfo.ImageName, *ImageName);
			StackInfo.ImageSize = TotalMemSize;
			FCString::Strcpy(StackInfo.LoadedImageName, *ImageName);
			FCString::Strcpy(StackInfo.ModuleName, *ImageName);
			FMemory::Memzero(&StackInfo.PdbSig70, sizeof(StackInfo.PdbSig70));

			Modules->ModuleSignatures[Modules->Index] = StackInfo;
			Modules->Index++;
		}

		// Continute until our index is less then our size or no more callbacks
		return Modules->Index >= Modules->ModuleSignaturesSize;
	}
}

int32 FUnixPlatformStackWalk::GetProcessModuleSignatures(FStackWalkModuleInfo *ModuleSignatures, const int32 ModuleSignaturesSize)
{
	if (ModuleSignatures == nullptr || ModuleSignaturesSize == 0 || !GUseNewCrashSymbolicator)
	{
		return 0;
	}

	ProcessModuleSignatures Signatures{ModuleSignatures, ModuleSignaturesSize, 0};
	dl_iterate_phdr(CollectModuleSignatures, &Signatures);

	return Signatures.Index;
}

static FCriticalSection EnsureLock;
static bool bReentranceGuard = false;

void NewReportEnsure(const TCHAR* ErrorMessage)
{
	// Simple re-entrance guard.
	EnsureLock.Lock();

	if (bReentranceGuard)
	{
		EnsureLock.Unlock();
		return;
	}

	bReentranceGuard = true;

	const bool bIsEnsure = true;
	FUnixCrashContext EnsureContext(bIsEnsure);
	EnsureContext.InitFromEnsureHandler(ErrorMessage, __builtin_return_address(0));

	EnsureContext.CaptureStackTrace();
	EnsureContext.GenerateCrashInfoAndLaunchReporter(true);

	bReentranceGuard = false;
	EnsureLock.Unlock();
}
