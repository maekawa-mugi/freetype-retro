"""Instruction semantics model of the actual preprocessed new MMI asm.

This is not an EE timing emulator. It checks data flow, byte ordering and
load/store bounds; hardware CHECK gates still run before target timing.
Usage: python3 tests/test_mmi_schedules.py /path/to/ee-gcc
"""
import ast
import random
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MASK = (1 << 128) - 1
source = """
typedef unsigned char FT_Byte;
typedef unsigned int FT_UInt; typedef int FT_Int;
typedef unsigned long FT_ULong;
#include "%s/src/base/ftbitmap_mmi.h"
#include "%s/src/base/ftbitmap_convert_mmi.h"
#define FT_CONFIG_OPTION_MMI_GRAY_SPANS
#define FT_MEM_SET(p,v,n) ((void)0)
#include "%s/src/smooth/ftgrays_retro.h"
""" % (ROOT.as_posix(), ROOT.as_posix(), ROOT.as_posix())
pre = subprocess.run([sys.argv[1], "-E", "-x", "c", "-"], input=source,
                     text=True, capture_output=True, check=True).stdout


def assembly(name):
    start = re.search(r"\b" + name + r"\s*\(", pre).start()
    start = pre.index("{", start)
    depth = 1
    end = start + 1
    while depth:
        depth += (pre[end] == "{") - (pre[end] == "}")
        end += 1
    body = pre[start:end]
    strings = re.findall(r'__asm__\s+volatile\s*\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)\s*:', body)
    return ["".join(ast.literal_eval(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', group))
            for group in strings]


def execute(code, values, memory):
    registers = {"$zero": 0, **{f"%{i}": v for i, v in values.items()}}
    instructions = [s.strip() for s in code.splitlines() if s.strip() and not s.strip().startswith(".")]
    labels = {s[:-1]: i for i, s in enumerate(instructions) if s.endswith(":")}
    sa = 0
    pc = 0
    steps = 0

    def val(x):
        return registers.get(x, 0) if x.startswith(("%", "$")) else int(x)

    def lanes(x, bits):
        return [(x >> i) & ((1 << bits) - 1) for i in range(0, 128, bits)]

    def pack(items, bits):
        return sum(x << (i * bits) for i, x in enumerate(items))

    while pc < len(instructions):
        line = instructions[pc]
        pc += 1
        steps += 1
        assert steps < 10000, "nonterminating asm loop"
        if line.endswith(":") or line == "nop":
            continue
        op, args = line.split(None, 1)
        a = [x.strip() for x in args.split(",")]
        d = a[0]
        if op in ("lq", "ld", "lwu", "sq"):
            match = re.fullmatch(r"(-?\d+)\((%\d+)\)", a[1])
            at = int(match[1]) + val(match[2])
            n = {"lq": 16, "sq": 16, "ld": 8, "lwu": 4}[op]
            assert at >= 0 and at + n <= len(memory)
            assert at % n == 0, (op, at)
            if op == "sq":
                memory[at:at+n] = val(d).to_bytes(16, "little")
            else:
                registers[d] = int.from_bytes(memory[at:at+n], "little")
        elif op == "mtsab":
            sa = ((val(a[0]) & 15) ^ (val(a[1]) & 15)) * 8
        elif op == "qfsrv":
            registers[d] = ((val(a[1]) << 128 | val(a[2])) >> sa) & MASK
        elif op == "paddub":
            registers[d] = pack([min(255, x+y) for x, y in zip(lanes(val(a[1]), 8), lanes(val(a[2]), 8))], 8)
        elif op.startswith("pextl"):
            bits = {"pextlb": 8, "pextlh": 16, "pextlw": 32}[op]
            rs, rt = lanes(val(a[1]), bits), lanes(val(a[2]), bits)
            registers[d] = pack([v for i in range(len(rs)//2) for v in (rt[i], rs[i])], bits)
        elif op == "pcpyld":
            registers[d] = ((val(a[1]) & ((1 << 64)-1)) << 64) | (val(a[2]) & ((1 << 64)-1))
        elif op in ("psrlh", "psllh", "psrlw", "psllw"):
            bits = 16 if op.endswith("h") else 32
            registers[d] = pack([((x << val(a[2])) if op.startswith("psll") else (x >> val(a[2]))) & ((1 << bits)-1)
                                 for x in lanes(val(a[1]), bits)], bits)
        elif op in ("por", "or", "pand"):
            registers[d] = val(a[1]) & val(a[2]) if op == "pand" else val(a[1]) | val(a[2])
        elif op == "lui":
            registers[d] = val(a[1]) << 16
        elif op == "ori":
            registers[d] = val(a[1]) | val(a[2])
        elif op in ("dsll32", "dsrl32"):
            registers[d] = ((val(a[1]) << (32+val(a[2]))) if op == "dsll32" else
                            (val(a[1]) & ((1 << 64)-1)) >> (32+val(a[2]))) & ((1 << 64)-1)
        elif op == "addiu":
            registers[d] = val(a[1]) + val(a[2])
        elif op == "bne":
            taken = val(a[0]) != val(a[1])
            delay = instructions[pc]
            pc += 1
            if delay != "nop":
                match = re.fullmatch(r"addiu (%\d+), (%\d+), (-?\d+)", delay)
                assert match, delay
                registers[match[1]] = val(match[2]) + int(match[3])
            if taken:
                pc = labels[a[2][:-1]] + 1
        else:
            raise AssertionError(op)
    return memory


rng = random.Random(91203)
cases = 0
for strength, code in enumerate(assembly("ft_bitmap_mmi_gray8_embolden_small"), 1):
    for blocks in (1, 2, 3, 7, 31):
        for pattern in (0, 1, 2):
            n = (blocks+1)*16
            original = bytearray(rng.randrange(256 if pattern == 0 else 16) if pattern != 2 else 255 for _ in range(n))
            got = execute(code, {4: n-16, 5: blocks}, original.copy())
            expected = original.copy()
            for x in range(16, n):
                expected[x] = min(255, sum(original[max(0, x-strength):x+1]))
            assert got == expected, ("gray8", strength, blocks, pattern)
            cases += 1

for name, bits in (("mono", 1), ("gray2", 2), ("gray4", 4)):
    for direct in (False, True) if bits != 4 else (False,):
        code = assembly(f"ft_bitmap_mmi_convert_{name}_row" + ("_direct" if direct else ""))[0]
        for trial in range(256):
            memory = bytearray(4096)
            width = 16 if direct else 32
            raw = bytes([trial] + [rng.randrange(256) for _ in range(width*bits//8-1)])
            memory[:len(raw)] = raw
            if direct:
                values = {5: int.from_bytes(raw, "little"), 6: 2048}
            elif bits == 4:
                values = {4: 0, 5: 2048}
            else:
                table_stride = 8 if bits == 1 else 4
                for v in range(256):
                    at = 16 + v*table_stride
                    memory[at:at+table_stride] = bytes((v >> (8-bits*(i+1))) & ((1 << bits)-1) for i in range(table_stride))
                first = 4 if bits == 1 else 8
                values = {first+i: 16+v*table_stride for i, v in enumerate(raw)}
                values[first+len(raw)] = 2048
            expected = bytes((raw[i*bits//8] >> (8-bits-(i*bits)%8)) & ((1 << bits)-1) for i in range(width))
            before = memory.copy()
            got = execute(code, values, memory)
            assert got[2048:2048+width] == expected, (name, direct, trial)
            assert got[:2048] == before[:2048] and got[2048+width:] == before[2048+width:]
            cases += 1
code = assembly("ft_gray_retro_fill")[0]
for byte in range(256):
    for blocks in (1, 2, 7):
        memory = bytearray([0xA5] * (blocks*64+32))
        got = execute(code, {2: 16, 3: blocks, 4: byte*0x01010101}, memory)
        assert got[:16] == bytes([0xA5])*16
        assert got[16:16+blocks*64] == bytes([byte])*(blocks*64)
        assert got[16+blocks*64:] == bytes([0xA5])*16
        cases += 1
print(f"PASS actual preprocessed MMI schedules: {cases} cases (semantics only)")
