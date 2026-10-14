"""Build the overlay word lists from the Wiktionary extracts.

English-Vietnamese comes from en-vi-enwiktionary.txt.
Japanese-Vietnamese is joined through the shared English headword in
en-ja-enwiktionary.txt, so a Japanese word on screen maps to the
Vietnamese gloss of the same English word.

Both source files are CC BY-SA 3.0 and GNU FDL, Matthias Buchmeier, 2015-01-02.
"""

import re
import sys
from pathlib import Path

HEAD = re.compile(r"^([^ {]+(?: [^ {]+)*)")
PAREN = re.compile(r"\([^)]*\)")
SPACE = re.compile(r"\s+")
HAS_JP = re.compile(r"[\u3040-\u30ff\u4e00-\u9fff]")
KANA = re.compile(r"[\u3040-\u30ff]{2,}")


def clean_vi(text: str) -> str:
    piece = text.split(",")[0]
    piece = PAREN.sub(" ", piece)
    piece = piece.replace("/", " ")
    piece = SPACE.sub(" ", piece).strip(" .")
    return piece


def english_key(left: str) -> str:
    match = HEAD.match(left.strip())
    if not match:
        return ""
    key = match.group(1).strip().lower()
    if len(key) < 2 or len(key) > 40:
        return ""
    if not all(("a" <= c <= "z") or c in " '-" or c.isdigit() for c in key):
        return ""
    return key


def remember(table: dict[str, str], key: str, value: str) -> None:
    old = table.get(key)
    if old is None or len(value) < len(old):
        table[key] = value


def japanese_forms(right: str) -> list[str]:
    forms: list[str] = []
    if "/" in right:
        for head in re.findall(r"(?:^|,)\s*([^/]+?)\s*/", right):
            head = SPACE.sub(" ", head).strip(" .")
            if HAS_JP.search(head) and 1 < len(head) <= 40:
                forms.append(head)
        for reading in re.findall(r"/([^/]*)/", right):
            for kana in KANA.findall(reading):
                if 1 < len(kana) <= 40:
                    forms.append(kana)
    else:
        for piece in right.split(","):
            piece = SPACE.sub(" ", piece).strip(" .")
            if HAS_JP.search(piece) and 1 < len(piece) <= 40:
                forms.append(piece)
    return forms


def main() -> None:
    en_vi_src = Path(sys.argv[1])
    en_ja_src = Path(sys.argv[2])
    en_vi_dst = Path(sys.argv[3])
    ja_vi_dst = Path(sys.argv[4])

    english_to_vi: dict[str, str] = {}
    for raw in en_vi_src.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "::" not in line:
            continue
        left, right = line.split("::", 1)
        key = english_key(left)
        vi = clean_vi(right)
        if key and vi and len(vi) <= 80:
            remember(english_to_vi, key, vi)

    japanese_to_vi: dict[str, str] = {}
    for raw in en_ja_src.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "::" not in line:
            continue
        left, right = line.split("::", 1)
        key = english_key(left)
        vi = english_to_vi.get(key, "")
        if not vi:
            continue
        for form in japanese_forms(right):
            remember(japanese_to_vi, form, vi)

    en_header = [
        "# English-Vietnamese word list for the translate overlay.",
        "# Dictionary data: CC BY-SA 3.0 and GNU FDL.",
        "# Extracted from Wiktionary by Matthias Buchmeier, version 20150102.",
        "# https://en.wiktionary.org/wiki/User:Matthias_Buchmeier",
        "# Form: english<TAB>vietnamese",
    ]
    ja_header = [
        "# Japanese-Vietnamese word list for the translate overlay.",
        "# Dictionary data: CC BY-SA 3.0 and GNU FDL.",
        "# Japanese and Vietnamese glosses extracted from Wiktionary by Matthias Buchmeier, version 20150102.",
        "# A Japanese word is paired with the Vietnamese gloss of the same English word.",
        "# https://en.wiktionary.org/wiki/User:Matthias_Buchmeier",
        "# Form: japanese<TAB>vietnamese",
    ]
    en_vi_dst.parent.mkdir(parents=True, exist_ok=True)
    en_vi_dst.write_text("\n".join(en_header + [f"{k}\t{english_to_vi[k]}" for k in sorted(english_to_vi)]) + "\n", encoding="utf-8")
    ja_vi_dst.write_text("\n".join(ja_header + [f"{k}\t{japanese_to_vi[k]}" for k in sorted(japanese_to_vi)]) + "\n", encoding="utf-8")
    print(f"en-vi {len(english_to_vi)} -> {en_vi_dst}")
    print(f"ja-vi {len(japanese_to_vi)} -> {ja_vi_dst}")


if __name__ == "__main__":
    main()
