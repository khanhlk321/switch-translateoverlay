"""Turn the Wiktionary English–Vietnamese extract into a two-column list.

Source file header states the license: CC BY-SA 3.0 and the GNU Free
Documentation License, Matthias Buchmeier / Wiktionary, 2015-01-02.
"""

import re
import sys
from pathlib import Path

HEAD = re.compile(r"^([^ {]+(?: [^ {]+)*)")
PAREN = re.compile(r"\([^)]*\)")
SPACE = re.compile(r"\s+")


def clean_vi(text: str) -> str:
    piece = text.split(",")[0]
    piece = PAREN.sub(" ", piece)
    piece = piece.replace("/", " ")
    piece = SPACE.sub(" ", piece).strip(" .")
    return piece


def main() -> None:
    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])
    best: dict[str, str] = {}
    for raw in src.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("- ") or "::" not in line:
            continue
        left, right = line.split("::", 1)
        match = HEAD.match(left.strip())
        if not match:
            continue
        key = match.group(1).strip().lower()
        if len(key) < 2 or len(key) > 40:
            continue
        if not all(("a" <= c <= "z") or c in " '-" or c.isdigit() for c in key):
            continue
        vi = clean_vi(right)
        if not vi or len(vi) > 80:
            continue
        old = best.get(key)
        if old is None or len(vi) < len(old):
            best[key] = vi

    lines = [
        "# English-Vietnamese word list for the translate overlay.",
        "# Dictionary data: CC BY-SA 3.0 and GNU FDL.",
        "# Extracted from Wiktionary by Matthias Buchmeier, version 20150102.",
        "# https://en.wiktionary.org/wiki/User:Matthias_Buchmeier",
        "# Form: english<TAB>vietnamese",
    ]
    for key in sorted(best):
        lines.append(f"{key}\t{best[key]}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{len(best)} entries -> {dst}")


if __name__ == "__main__":
    main()
