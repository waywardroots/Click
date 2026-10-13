#!/usr/bin/env python3
"""Reflow the EULA PDF into the plain-text copy the installers display.

Run from the repository root after changing the PDF:

    python3 tools/make_eula_txt.py

pdftotext's default mode emits a few hanging-indent lines out of reading
order, so this uses -layout and recovers the structure from the indentation:
a line indented past the body margin starts a new paragraph, anything else
continues the current one.
"""

import re
import subprocess
import sys
import textwrap

PDF = "End User License Agreement - FuzzyAudio - Click.pdf"
OUT = "packaging/eula.txt"
TITLE = "Software End User License Agreement"
WIDTH = 76
BODY_MARGIN = 8      # continuation lines sit at or inside this column

SECTION = re.compile(r"^(\d{1,2})\.\s")
LETTER = re.compile(r"^\(([a-z])\)\s")
ROMAN = re.compile(r"^\((i|ii|iii|iv|v|vi)\)\s")
PAGE_NUMBER = re.compile(r"^\s*\d{1,2}\s*$")
SENTENCE_END = (".", ";", ":")


def extract(layout):
    argv = ["pdftotext", "-enc", "UTF-8"] + (["-layout"] if layout else []) + [PDF, "-"]
    text = subprocess.run(argv, capture_output=True, text=True, check=True).stdout
    return text.replace("\u200b", " ").replace("\u00a0", " ")


def shouting(text):
    letters = [c for c in text if c.isalpha()]
    return bool(letters) and all(c.isupper() for c in letters)


def prose(text):
    """True for ordinary mixed-case text, false for a block of capitals."""
    letters = [c for c in text if c.isalpha()]
    return sum(c.islower() for c in letters) > len(letters) * 0.2


pages = extract(layout=True).split("\f")
blocks = []          # (indent, text)
current = None
last_letter = None

for page, raw in enumerate(pages):
    lines = [ln for ln in raw.split("\n") if ln.strip() and not PAGE_NUMBER.match(ln)]

    for i, line in enumerate(lines):
        column = len(line) - len(line.lstrip())
        text = re.sub(r"\s+", " ", line.strip())

        if not blocks and current is None and text == TITLE:
            continue

        roman = ROMAN.match(text)
        letter = LETTER.match(text)

        # "(i)" is the ninth letter in section 2's list but roman numeral one
        # in section 8's: it continues a letter list only straight after "(h)".
        if roman and not (roman.group(1) == "i" and last_letter == "h"):
            indent = 8
        elif letter:
            indent = 4
            last_letter = letter.group(1)
        elif SECTION.match(text):
            indent = 0
            last_letter = None
        elif text[:1].islower():
            indent = None           # mid-sentence: continues the paragraph above
        elif column >= BODY_MARGIN:
            indent = 0              # an unmarked paragraph's first-line indent
        else:
            indent = None           # continues the paragraph above

        if indent is None and current is not None:
            # A page break, or a run of capitals after ordinary prose, can
            # still start a paragraph the indentation alone does not mark.
            across_break = i == 0 and page > 0
            capitalised = text[:1].isupper()
            finished = current[1].endswith(SENTENCE_END)

            if across_break and finished and capitalised:
                indent = 0
            elif shouting(text) and finished and prose(current[1]):
                indent = 0

        if indent is None:
            if current is None:
                current = (0, text)
            else:
                current = (current[0], current[1] + " " + text)
            continue

        if current is not None:
            blocks.append(current)

        current = (indent, text)

if current is not None:
    blocks.append(current)

out = [TITLE, "=" * len(TITLE)]

for indent, text in blocks:
    pad = " " * indent
    marker = re.match(r"(\(\w+\)|\d{1,2}\.)\s", text)
    hanging = pad + " " * (len(marker.group(1)) + 1) if marker else pad
    out.append("\n".join(textwrap.wrap(text, WIDTH,
                                       initial_indent=pad,
                                       subsequent_indent=hanging,
                                       break_long_words=False,
                                       break_on_hyphens=False)))

# Inno Setup reads a .txt licence as UTF-8 only when it starts with a BOM.
body = "\n\n".join(out) + "\n"

with open(OUT, "w", encoding="utf-8-sig", newline="\r\n") as f:
    f.write(body)

# Nothing may be dropped, reordered or invented by the reflow, and -layout
# must not have lost anything the default extraction found.
words = lambda s: re.sub(r"\s+", " ", s).split()
source = words("\n".join(ln for ln in extract(layout=True).split("\n")
                         if not PAGE_NUMBER.match(ln)))
result = words(body.replace("=" * len(TITLE), ""))

if source != result:
    import difflib
    sys.exit("\n".join(list(difflib.unified_diff(source, result, lineterm=""))[:40]))

if sorted(words(extract(layout=False))) != sorted(words(extract(layout=True))):
    sys.exit("the two pdftotext modes disagree on the document's words")

print(f"{OUT}: {len(blocks)} paragraphs, {len(result)} words, verified against the PDF")
