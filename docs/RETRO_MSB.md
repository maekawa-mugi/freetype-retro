# Optional FT_MSB paths for PlayStation 2 EE and SPARC32

The internal `FT_MSB(FT_UInt32)` finds the highest set-bit index.
It is used by FreeType's normalization, trigonometry and fixed-point
geometry routines.

**Important:** FreeType already uses `__builtin_clz` or `__builtin_clzl`
under a GCC/Clang build, when assembler support is enabled. These
optional paths are experiments for target-specific code generation,
NOT guaranteed improvements over the existing compiler builtin.

## Select one backend

| Macro, off by default | ISA | Algorithm |
| --- | --- | --- |
| `FT_CONFIG_OPTION_RETRO_MSB_R5900` | PS2 R5900 MMI | `PLZCW` on the low 32-bit lane |
| `FT_CONFIG_OPTION_RETRO_MSB_SPARC32` | GCC-compatible SPARC 32-bit ABI | Five bit-spreading OR/shifts, 32-bit unsigned multiply and a 32-byte De Bruijn lookup table |

Both require 32-bit `FT_Int` and `FT_Long`, and both are
mutually exclusive. `FT_CONFIG_OPTION_NO_ASSEMBLER` disables
both experimental alternatives, retaining FreeType's ordinary
function or builtin. The SPARC implementation uses portable
integer instructions rather than a VIS1 SIMD instruction.
The opt-in switch is deliberately limited to SPARC32 for
controlled comparison, not because De Bruijn itself requires SPARC.

## R5900: PLZCW mapping

The uploaded EE core instruction manual specifies the instruction as:

    PLZCW rd, rs
    GPR[rd].word = LZC(GPR[rs].word) - 1

The count includes leading copies of the **sign bit** and
is calculated separately for the two 32-bit words of each
64-bit GPR. That differs from a conventional unsigned
count-leading-zeros on inputs with their high bit set.

Consequently, the exact 32-bit unsigned MSB mapping is:

    if (x == 0) return 0;
    if (x & 0x80000000) return 31;
    return 30 - PLZCW_low_word(x);

The zero result matches the original FreeType scalar fallback.
The ordinary `__builtin_clz(0)` is undefined, and calling it
with zero is excluded from the on-target scalar comparison.

The target assembler and R5900 register constraints remain
unverified. The output is deliberately taken only from the
low 32-bit result word.

## SPARC32: De Bruijn lookup

The alternative first propagates the most significant set bit
through all lower positions, multiplies the resulting 32-bit
word modulo 2^32 by `0x07C4ACDD`, and uses the top five
bits to index a 32-byte constant lookup table.

The method handles zero explicitly through `table[0] == 0`.
It uses ordinary unsigned arithmetic without overflowing signed
integers. No `POPC`, VIS2 or other newer-than-VIS1 instructions
are required. The chosen table is small, but cache footprint
and one integer multiplication may still cost more than the
compiler's baseline implementation.

## Deferred tests

The existing host batch script now includes:

    sh tests/run_retro_simd_models.sh

The new `tests/msb_semantics_model.c` checks the **actual
De Bruijn helper** and an EE PLZCW word-count model against a
portable bit-scanning reference in 2,048,706 deterministic
patterns. Cases include zero, powers of two, adjacent values,
all 16-bit low values combined with 16 high patterns, and
one million generated random words. It does not execute
target PLZCW instructions.

For target compilation and instruction-level comparison:

    cc -O2 tests/msb_target_compare.c \
       -Iinclude ... -o msb-fingerprint
    ./msb-fingerprint

Use the same target compiler and configuration headers as the
matching FreeType build. The target test includes
`freetype/internal/ftcalc.h` and compares the selected
`FT_MSB` routine with an independent reference. Compare the
result fingerprints against an otherwise identical baseline.
The program avoids testing zero against the default builtin.

Follow with `tests/retro_glyph_hash.c` in all render modes
and profile the actual normalization/trigonometry hot paths.
In particular, inspect whether GCC already emits a suitable
`PLZCW` or a fast builtin expansion on the target before
claiming a benefit.

**No new tests, cross-compilations, disassemblies or performance
measurements were run during this change.**
