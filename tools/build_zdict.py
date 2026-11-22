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
import csv
import io
import struct
import sys
import urllib.request
from pathlib import Path
from typing import Dict, List, Tuple

from parse_dict import encode_zhuyin, load_imechlex


CHEWING_WORD_CSV_URL = (
    "https://codeberg.org/chewing/libchewing-data/raw/branch/main/dict/chewing/word.csv"
)
CHEWING_TSI_CSV_URL = (
    "https://codeberg.org/chewing/libchewing-data/raw/branch/main/dict/chewing/tsi.csv"
)


def _charset_flags_for_char(ch: str) -> int:
    cp = ord(ch)
    flags = 0
    try:
        ch.encode("cp950")
        flags |= 0x02  # Big5
    except UnicodeEncodeError:
        pass
    if 0x3400 <= cp <= 0x4DBF:
        flags |= 0x04  # CJK Ext-A
    return flags


def _download_utf8(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "ZZhuyin-Builder/1.0"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return resp.read().decode("utf-8")


def build_from_chewing(
    word_csv_text: str, tsi_csv_text: str, max_assoc_per_lead: int = 36
) -> Tuple[Dict[int, List[Tuple[int, int]]], Dict[int, List[str]]]:
    # 1. Extract single-char frequencies and multi-char associate phrases from tsi.csv
    char_freq: Dict[Tuple[int, int], int] = {}
    assoc_raw: Dict[int, List[Tuple[int, str]]] = {}

    for row in csv.reader(io.StringIO(tsi_csv_text)):
        if not row or row[0].startswith("#") or len(row) < 3:
            continue
        phrase = row[0].strip()
        try:
            freq = int(row[1].strip())
        except ValueError:
            freq = 0
        reading = row[2].strip()
        if not phrase:
            continue

        if len(phrase) == 1 and ord(phrase) <= 0xFFFF:
            try:
                bucket = encode_zhuyin(reading) >> 2
            except ValueError:
                continue
            key = (bucket, ord(phrase))
            if freq > char_freq.get(key, -1):
                char_freq[key] = freq
        elif 2 <= len(phrase) <= 4 and all(ord(c) <= 0xFFFF for c in phrase):
            lead = ord(phrase[0])
            tail = phrase[1:]
            assoc_raw.setdefault(lead, []).append((freq, tail))

    # 2. Collect all BMP single characters from word.csv (+ any single-char entries in tsi.csv)
    bucket_entries: Dict[int, List[Tuple[int, int, int, int]]] = {}
    seen: Dict[int, set] = {}
    order_idx = 0

    for row in csv.reader(io.StringIO(word_csv_text)):
        if not row or row[0].startswith("#") or len(row) < 3:
            continue
        ch = row[0].strip()
        if len(ch) != 1 or ord(ch) > 0xFFFF:
            continue
        reading = row[2].strip()
        try:
            bucket = encode_zhuyin(reading) >> 2
        except ValueError:
            continue
        cp = ord(ch)
        s = seen.setdefault(bucket, set())
        if cp in s:
            continue
        s.add(cp)
        freq = char_freq.get((bucket, cp), 0)
        flags = _charset_flags_for_char(ch)
        # Prefer Big5 + higher tsi.csv frequency, preserving word.csv order on ties
        is_big5 = 1 if (flags & 0x02) else 0
        bucket_entries.setdefault(bucket, []).append((freq, is_big5, -order_idx, (cp, flags)))
        order_idx += 1

    # Add any single-char reading from tsi.csv that was not in word.csv
    for (bucket, cp), freq in char_freq.items():
        ch = chr(cp)
        # Skip standalone Bopomofo/tone symbols
        if 0x3105 <= cp <= 0x312F or ch in "ˊˇˋ˙":
            continue
        s = seen.setdefault(bucket, set())
        if cp in s:
            continue
        s.add(cp)
        flags = _charset_flags_for_char(ch)
        is_big5 = 1 if (flags & 0x02) else 0
        bucket_entries.setdefault(bucket, []).append((freq, is_big5, -order_idx, (cp, flags)))
        order_idx += 1

    buckets: Dict[int, List[Tuple[int, int]]] = {}
    for bucket, items in bucket_entries.items():
        items.sort(key=lambda x: (x[0], x[1], x[2]), reverse=True)
        buckets[bucket] = [x[3] for x in items]

    # 3. Build top-N associate phrases per lead character sorted by frequency
    assoc_map: Dict[int, List[str]] = {}
    for lead, pairs in assoc_raw.items():
        pairs.sort(key=lambda x: x[0], reverse=True)
        dedup: List[str] = []
        seen_tails = set()
        for _, tail in pairs:
            if tail not in seen_tails:
                seen_tails.add(tail)
                dedup.append(tail)
                if len(dedup) >= max_assoc_per_lead:
                    break
        if dedup:
            assoc_map[lead] = dedup

    return buckets, assoc_map


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
    group.add_argument(
        "--fetch-chewing",
        action="store_true",
        help="Download open-source libchewing-data (LGPL-2.1) and build .zdict",
    )
    parser.add_argument("--assoc-text", type=Path, help="Optional UTF-8 TSV associate table")
    parser.add_argument("--lcptr", type=Path, help="Optional lcptr.tbl (with --from-imd)")
    parser.add_argument("--lcphrase", type=Path, help="Optional lcphrase.tbl (with --from-imd)")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .zdict path")
    args = parser.parse_args()

    if args.from_text:
        buckets, assoc_map = build_from_text(args.from_text, args.assoc_text)
    elif args.fetch_chewing:
        print("Downloading libchewing-data (word.csv + tsi.csv)...")
        word_csv = _download_utf8(CHEWING_WORD_CSV_URL)
        tsi_csv = _download_utf8(CHEWING_TSI_CSV_URL)
        buckets, assoc_map = build_from_chewing(word_csv, tsi_csv)
    else:
        buckets, assoc_map = build_from_imd(args.from_imd, args.lcptr, args.lcphrase)

    write_zdict(args.output, buckets, assoc_map)


if __name__ == "__main__":
    main()
