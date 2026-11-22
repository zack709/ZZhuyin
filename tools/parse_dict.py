#!/usr/bin/env python3
"""
Verify and query Windows 7 IMECHLEX v2 dictionary files (e.g., IMTCPH.IMD).
Based on Phase 2 reverse engineering of IMTCCORE.DLL (FUN_7ff40861c64, FUN_7ff408603f4, FUN_7ff4086457c).

Usage:
  python tools/parse_dict.py targets/DICTS/IMTCPH.IMD
  python tools/parse_dict.py targets/DICTS/IMTCPH.IMD ㄓㄨˋ ㄧㄣ ㄓㄨㄥ
"""

import struct
import sys
from pathlib import Path

INITIALS = " ㄅㄆㄇㄈㄉㄊㄋㄌㄍㄎㄏㄐㄑㄒㄓㄔㄕㄖㄗㄘㄙ"
MEDIALS = " ㄧㄨㄩ"
FINALS = " ㄚㄛㄜㄝㄞㄟㄠㄡㄢㄣㄤㄥㄦ"
TONES = {"": 1, " ": 1, "ˊ": 2, "ˇ": 3, "ˋ": 4, "˙": 5}


def encode_zhuyin(syllable: str) -> int:
    init = med = fin = 0
    tone = 1
    for ch in syllable:
        if ch in INITIALS[1:]:
            init = INITIALS.index(ch)
        elif ch in MEDIALS[1:]:
            med = MEDIALS.index(ch)
        elif ch in FINALS[1:]:
            fin = FINALS.index(ch)
        elif ch in TONES:
            tone = TONES[ch]
        else:
            raise ValueError(f"Unknown Bopomofo symbol: {ch!r}")
    return (init << 11) | (med << 9) | (fin << 5) | (tone << 2)


def load_imechlex(path: Path):
    data = path.read_bytes()
    if len(data) < 0x38 or data[0:8] != b"IMECHLEX":
        raise ValueError(f"Not an IMECHLEX file: {path}")
    minor_ver, major_ver = struct.unpack_from("<II", data, 0x10)
    sec1, sec2, sec3, sec4, total_size = struct.unpack_from("<5I", data, 0x18)
    max_cid, min_cid, flags = struct.unpack_from("<3I", data, 0x2C)
    count = max_cid - min_cid
    assert major_ver == 2 and minor_ver == 0 and total_size <= len(data)
    u_tbl = struct.unpack_from(f"<{count}H", data, sec2)
    r_tbl = struct.unpack_from(f"<{count}H", data, sec3)
    b_off = struct.unpack_from("<16385H", data, sec4)
    cands = struct.unpack_from(f"<{(total_size - sec4 - 0x8002) // 2}H", data, sec4 + 0x8002)
    return {
        "name": path.name,
        "size": len(data),
        "min_cid": min_cid,
        "max_cid": max_cid,
        "flags": flags,
        "sections": (sec1, sec2, sec3, sec4, total_size),
        "unicode": u_tbl,
        "reading": r_tbl,
        "bucket_offsets": b_off,
        "cands": cands,
    }


def query_zhuyin(dic: dict, syllable: str) -> str:
    raw = encode_zhuyin(syllable)
    b = raw >> 2
    s, e = dic["bucket_offsets"][b], dic["bucket_offsets"][b + 1]
    min_cid = dic["min_cid"]
    u_tbl = dic["unicode"]
    return "".join(chr(u_tbl[cid - min_cid]) for cid in dic["cands"][s:e])


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python tools/parse_dict.py <IMTCPH.IMD> [zhuyin ...]")
        sys.exit(1)

    dic = load_imechlex(Path(sys.argv[1]))
    count = dic["max_cid"] - dic["min_cid"]
    if len(sys.argv) == 2:
        sec1, sec2, sec3, sec4, total = dic["sections"]
        non_empty = sum(
            1 for i in range(16384) if dic["bucket_offsets"][i + 1] > dic["bucket_offsets"][i]
        )
        print(f"File: {dic['name']} ({dic['size']:,} bytes) | IMECHLEX v2.0")
        print(f"CID range: [{dic['min_cid']}, {dic['max_cid']}) -> {count:,} entries | flags=0x{dic['flags']:x}")
        print(f"Sections: sec1=0x{sec1:x}, sec2=0x{sec2:x}, sec3=0x{sec3:x}, sec4=0x{sec4:x}, total=0x{total:x}")
        print(f"Inverted index: {non_empty:,} non-empty buckets, {len(dic['cands']):,} candidate CIDs")
        for sample in ["ㄓㄨˋ", "ㄧㄣ", "ㄓㄨㄥ", "ㄨㄛˇ", "ㄉㄜ˙"]:
            print(f"  {sample:5s} -> {query_zhuyin(dic, sample)[:20]}")
    else:
        for syl in sys.argv[2:]:
            raw = encode_zhuyin(syl)
            print(f"{syl} (0x{raw:04X}, bucket={raw >> 2}): {query_zhuyin(dic, syl)}")


if __name__ == "__main__":
    main()
