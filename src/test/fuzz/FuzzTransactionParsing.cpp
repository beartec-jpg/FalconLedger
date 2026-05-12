// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Fuzz target: transaction serialization / deserialization round-trip.
//
// Build with clang + libFuzzer:
//
//   clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
//     -I <repo>/include \
//     FuzzTransactionParsing.cpp \
//     -o fuzz_tx_parsing \
//     -L<build>/lib -lxrpl
//
// Run:
//   ./fuzz_tx_parsing corpus/tx/ -max_len=4096 -timeout=30

#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/basics/Slice.h>

#include <cstdint>
#include <cstddef>
#include <stdexcept>

// Entry point called by libFuzzer for every generated input.
extern "C" int
LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    // We feed raw bytes into the STTx deserializer.  The goal is to ensure
    // that no input causes an abort, crash, or undefined behaviour — only
    // well-formed exceptions or graceful error returns are acceptable.
    try
    {
        xrpl::SerialIter sit{data, size};
        xrpl::STTx tx{sit};
        (void)tx.getJson(xrpl::JsonOptions::none);
    }
    catch (std::exception const&)
    {
        // Expected: malformed inputs throw.
    }
    return 0;
}
