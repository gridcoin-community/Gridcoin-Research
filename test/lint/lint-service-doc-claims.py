#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Keep a fixed list of phrases out of the service docs.

The lint reads every text file under doc/, contrib/init/ and contrib/docker/ and
fails when one of the phrases in PHRASES appears, naming the file and the line. It
matches each phrase without regard to case, line breaks, the comment and quote
markers that start a line (#, >, ;) and the Markdown marks ` and *, so a phrase
wrapped over two lines of a unit header or a block quote is still found.

If you are here because the lint failed, reword the sentence.
doc/running-unattended.md describes the current setup.
"""

import bisect
import os
import re
import sys

ROOTS = ("doc", "contrib/init", "contrib/docker", "contrib/windows", "debian", "debian-testnet")
SKIP_DIRS = {"__pycache__", ".git"}
HINT = "see doc/running-unattended.md for the current setup"

PHRASES = (
    "nearly turnkey",
    "nothing elsewhere can",
    "dumpprivkey <one of your",
    "Autounlock re-runs on the fresh instance",
    "chown -R 1000:1000",
)

# What starts a line in a unit header, a shell or conf comment, or a Markdown quote.
LINE_MARKERS = re.compile(r"^\s*[#>;]*\s*")
DROPPED_MARKS = str.maketrans("", "", "`*")


def normalize(text):
    """Lower case, without ` and *, with every run of whitespace one space."""
    return " ".join(text.translate(DROPPED_MARKS).lower().split())


class Document:
    """The normalized text of a file, and where each of its lines starts in it."""

    def __init__(self, text):
        parts = []
        self.starts = []
        self.line_numbers = []
        offset = 0
        for number, line in enumerate(text.splitlines(), start=1):
            content = normalize(LINE_MARKERS.sub("", line, count=1))
            if not content:
                continue
            self.starts.append(offset)
            self.line_numbers.append(number)
            parts.append(content)
            offset += len(content) + 1
        self.text = " ".join(parts)

    def line_of(self, offset):
        return self.line_numbers[bisect.bisect_right(self.starts, offset) - 1]

    def find(self, phrase):
        """The line numbers where the normalized phrase starts."""
        needle = normalize(phrase)
        found = []
        start = self.text.find(needle)
        while start >= 0:
            found.append(self.line_of(start))
            start = self.text.find(needle, start + 1)
        return found


def check_text(name, text):
    """The findings for one file, as printable lines."""
    document = Document(text)
    found = []
    for phrase in PHRASES:
        for line in document.find(phrase):
            found.append((line, phrase))
    return ["%s:%d: listed phrase '%s': %s" % (name, line, phrase, HINT) for line, phrase in sorted(found)]


def files_to_check():
    for root in ROOTS:
        for directory, dirs, files in os.walk(root):
            dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
            for name in sorted(files):
                yield os.path.join(directory, name)


def read_text(path):
    """The file as text, or None for a binary file."""
    with open(path, "rb") as f:
        data = f.read()
    if b"\0" in data:
        return None
    return data.decode("utf8", errors="replace")


def self_test():
    """Each phrase must be found wrapped over a comment, a quote and a plain line, and a text
    without any must give nothing."""
    failures = 0
    for phrase in PHRASES:
        words = phrase.split(" ")
        half = max(1, len(words) // 2)
        first, rest = " ".join(words[:half]), " ".join(words[half:])
        samples = {
            "plain": "Some text, %s, more text.\n" % phrase.upper(),
            "unit header": "# first line %s\n#   %s and the rest\n" % (first, rest),
            "quote": "> start %s\n> %s end\n" % (first, rest),
            "marks removed": "x %s y\n" % phrase.replace("`", ""),
        }
        for kind, sample in samples.items():
            if not any(("'%s'" % phrase) in line for line in check_text("sample", sample)):
                print("self-test: '%s' was not found in the %s sample" % (phrase, kind))
                failures += 1
    clean = ("# The credential stays owned by root, mode 0600.\n"
             "Check unlock_scope in getwalletinfo.\n")
    unexpected = check_text("clean sample", clean)
    for line in unexpected:
        print("self-test: unexpected finding: %s" % line)
        failures += 1
    if failures:
        print("self-test: %d failure(s)" % failures)
        return 1
    print("lint-service-doc-claims self-test: each of %d phrases found in every sample" % len(PHRASES))
    return 0


def main(argv):
    if argv == ["--self-test"]:
        return self_test()
    if argv:
        print("usage: %s [--self-test]" % os.path.basename(sys.argv[0]))
        return 64
    top = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    os.chdir(top)
    findings = []
    checked = 0
    for path in files_to_check():
        text = read_text(path)
        if text is not None:
            checked += 1
            findings.extend(check_text(path, text))
    for line in findings:
        print(line)
    if findings:
        print("%d listed phrase(s) found; %s" % (len(findings), HINT))
        return 1
    print("lint-service-doc-claims: OK: %d files under %s checked" % (checked, ", ".join(ROOTS)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
