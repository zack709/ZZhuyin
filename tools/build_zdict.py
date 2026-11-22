#!/usr/bin/env python3
"""
Build standalone ZZhuyin .zdict (ZZDICT01) dictionary files.

Modes:
  1. From open-source TSV word table:
     python tools/build_zdict.py --from-text table.tsv [--assoc-text assoc.tsv] -o zzhuyin.zdict
     TSV format (UTF-8, lines starting with # ignored):
       <zhuyin_syllable>\t<single_bmp_char>[\t<hex_flags>]
     Associate TSV format:
       <lead_char>\t<phrase1,phrase2,...>

  2. From local Windows 7 IMTCPH.IMD (+ optional lcptr.tbl / lcphrase.tbl):
     python tools/build_zdict.py --from-imd targets/DICTS/IMTCPH.IMD \
         --lcptr targets/lcptr.tbl --lcphrase targets/lcphrase.tbl -o zzhuyin.zdict
"""

import argparse
import struct
import sys
from pathlib import Path
from typing import Dict, List, Tuple

from parse_dict import encode_zhuyin, load_imechlex


def write_zdict(
    out_path: Path,
    buckets: Dict[int, List[Tuple[int, int]]],
    assoc_map: Dict[int, List[str]],
) -> None:
    bucket_offsets: List[int] = [0] * 16385
    entries: List[Tuple[int, int]] = []

    for b in range(16384):
        bucket_offsets[b] = len(entries)
        if b in buckets:
            entries.extend(buckets[b])
    bucket_offsets[16384] = len(entries)

    assoc_ptrs: List[Tuple[int, int]] = []
    assoc_pool: List[int] = []
    for lead in sorted(assoc_map.keys()):
        phrases = [p for p in assoc_map[lead] if p]
        if not phrases:
            continue
        assoc_ptrs.append((lead, len(assoc_pool)))
        for phrase in phrases:
            u16 = phrase.encode("utf-16-le")
            assoc_pool.extend(struct.unpack(f"<{len(u16) // 2}H", u16))
            assoc_pool.append(0)

    header = struct.pack(
        "<8s6I",
        b"ZZDICT01",
        1,
        len(entries),
        len(assoc_ptrs),
        len(assoc_pool),
        0,
        0,
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as f:
        f.write(header)
        f.write(struct.pack("<16385I", *bucket_offsets))
        for codepoint, flags in entries:
            f.write(struct.pack("<HBB", codepoint, flags & 0xFF, 0))
        for lead, wchar_off in assoc_ptrs:
            f.write(struct.pack("<HHI", lead, 0, wchar_off))
        if assoc_pool:
            f.write(struct.pack(f"<{len(assoc_pool)}H", *assoc_pool))

    print(
        f"Wrote {out_path} ({out_path.stat().st_size:,} bytes): "
        f"{len(entries):,} entries, {len(assoc_ptrs):,} associate leads"
    )


def build_from_text(
    tsv_path: Path, assoc_path: Path | None
) -> Tuple[Dict[int, List[Tuple[int, int]]], Dict[int, List[str]]]:
    buckets: Dict[int, List[Tuple[int, int]]] = {}
    for raw_line in tsv_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) < 2:
            continue
        syllable, ch = parts[0].strip(), parts[1].strip()
        if len(ch) != 1 or ord(ch) > 0xFFFF:
            continue
        flags = int(parts[2].strip(), 0) if len(parts) >= 3 and parts[2].strip() else 0x02
        bucket = encode_zhuyin(syllable) >> 2
        buckets.setdefault(bucket, []).append((ord(ch), flags))

    assoc_map: Dict[int, List[str]] = {}
    if assoc_path and assoc_path.exists():
        for raw_line in assoc_path.read_text(encoding="utf-8").splitlines():
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 2:
                continue
            lead = parts[0].strip()
            if len(lead) != 1 or ord(lead) > 0xFFFF:
                continue
            phrases = [p.strip() for p in parts[1].split(",") if p.strip()]
            if phrases:
                assoc_map[ord(lead)] = phrases

    return buckets, assoc_map


def build_from_imd(
    imd_path: Path, lcptr_path: Path | None, lcphrase_path: Path | None
) -> Tuple[Dict[int, List[Tuple[int, int]]], Dict[int, List[str]]]:
    dic = load_imechlex(imd_path)
    raw_data = imd_path.read_bytes()
    sec1_off = dic["sections"][0]
    min_cid = dic["min_cid"]
    u_tbl = dic["unicode"]
    b_off = dic["bucket_offsets"]
    cands = dic["cands"]

    buckets: Dict[int, List[Tuple[int, int]]] = {}
    for b in range(16384):
        s, e = b_off[b], b_off[b + 1]
        if s >= e:
            continue
        items: List[Tuple[int, int]] = []
        for cid in cands[s:e]:
            idx = cid - min_cid
            items.append((u_tbl[idx], raw_data[sec1_off + idx]))
        buckets[b] = items

    assoc_map: Dict[int, List[str]] = {}
    if lcptr_path and lcphrase_path and lcptr_path.exists() and lcphrase_path.exists():
        ptr_bytes = lcptr_path.read_bytes()
        phr_bytes = lcphrase_path.read_bytes()
        count = (len(ptr_bytes) - 6) // 6
        entries = [
            struct.unpack_from("<HI", ptr_bytes, 6 + i * 6) for i in range(count)
        ]
        total_wchars = len(phr_bytes) // 2
        for i, (lead, start_w) in enumerate(entries):
            end_w = entries[i + 1][1] if i + 1 < count else total_wchars
            chunk = phr_bytes[start_w * 2 : end_w * 2].decode("utf-16-le")
            phrases = [p for p in chunk.split("\x00") if p]
            if phrases:
                assoc_map[lead] = phrases

    return buckets, assoc_map


def main() -> None:
    parser = argparse.ArgumentParser(description="Build ZZhuyin .zdict dictionary")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--from-text", type=Path, help="Input UTF-8 TSV Bopomofo table")
    group.add_argument("--from-imd", type=Path, help="Input Windows 7 IMTCPH.IMD file")
    parser.add_argument("--assoc-text", type=Path, help="Optional UTF-8 TSV associate table")
    parser.add_argument("--lcptr", type=Path, help="Optional lcptr.tbl (with --from-imd)")
    parser.add_argument("--lcphrase", type=Path, help="Optional lcphrase.tbl (with --from-imd)")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .zdict path")
    args = parser.parse_args()

    if args.from_text:
        buckets, assoc_map = build_from_text(args.from_text, args.assoc_text)
    else:
        buckets, assoc_map = build_from_imd(args.from_imd, args.lcptr, args.lcphrase)

    write_zdict(args.output, buckets, assoc_map)


if __name__ == "__main__":
    main()
