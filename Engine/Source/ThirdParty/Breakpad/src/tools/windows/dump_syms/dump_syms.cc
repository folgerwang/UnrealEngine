// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Windows utility to dump the line number data from a pdb file to
// a text-based format that we can use from the minidump processor.

#include <stdio.h>
#include <wchar.h>

#include <string>
#include <fstream>

#include "common/linux/dump_symbols.h"
#include "common/windows/pdb_source_line_writer.h"

using std::wstring;
using google_breakpad::PDBSourceLineWriter;

int wmain(int argc, wchar_t **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %ws <file.[pdb|exe|dll]>\n", argv[0]);
    return 1;
  }

  PDBSourceLineWriter writer;
  bool cfi = true;
  bool handle_inter_cu_refs = true;
  if (!writer.Open(wstring(argv[1]), PDBSourceLineWriter::ANY_FILE)) {
    std::vector<string> debug_dirs;
    std::wstring s(argv[1]);

    std::streambuf* os = std::cout.rdbuf();
    std::ofstream ofs;
    if (argc > 2)
    {
      std::wstring output(argv[2]);
      ofs.open(output, std::ofstream::out);
      if (ofs.is_open()) {
        os = ofs.rdbuf();
      }
    }

    std::ostream out(os);
    SymbolData symbol_data = cfi ? ALL_SYMBOL_DATA : NO_CFI;
    google_breakpad::DumpOptions options(symbol_data, handle_inter_cu_refs);
    if (!WriteSymbolFile(std::string(s.begin(), s.end()), debug_dirs, options, out)) {
      fprintf(stderr, "Failed to write symbol file.\n");
      ofs.close();
      return 1;
    }

    ofs.close();
    return 0;
  }

  if (!writer.WriteMap(stdout)) {
    fprintf(stderr, "WriteMap failed\n");
    return 1;
  }

  writer.Close();
  return 0;
}
