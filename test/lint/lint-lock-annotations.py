#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Lock annotation lint.

The Clang thread-safety analysis checks a call against the lock annotations it
can see at that call. An annotation that is only on a function's definition in
a .cpp file is seen only below the definition in that one file, so callers in
every other file, and callers above the definition, go unchecked. AssertLockHeld
checks only at run time, only in lock-order builds and only on the paths a test
happens to run, where an annotation is checked by every Clang build. So:

  * A lock annotation (EXCLUSIVE_LOCKS_REQUIRED, SHARED_LOCKS_REQUIRED,
    LOCKS_EXCLUDED) on a function definition in a .cpp file is also on every
    separate declaration of that function, naming the same locks: in its class,
    in a header, earlier in the same file, or, for a function that is not
    static or in an anonymous namespace, in another .cpp file.
  * AssertLockHeld is not used outside src/sync.h and src/sync.cpp. A function
    that needs a lock held states it with an annotation.

A declaration is matched to a definition by name, by class for a member, and by
the number of parameters. Locks are compared as whole expressions, with class
and namespace qualifiers and this-> left out and each parameter named by its
position, so a declaration may name its parameters differently from the
definition.

Overloads with the same number of parameters, and functions of the same name in
different namespaces, are not told apart. The rule then asks for an annotation
on a declaration that does not need it: a false report, never a missed one.
There is none in the tree. One that appears is a reason to teach the lint
parameter types and namespaces.

--self-test runs the rules on samples that must fail and samples that must pass.
"""

import bisect
import os
import re
import sys

ANN_NAMES = ("EXCLUSIVE_LOCKS_REQUIRED", "SHARED_LOCKS_REQUIRED", "LOCKS_EXCLUDED")
ANN = re.compile(r"\b(" + "|".join(ANN_NAMES) + r")\s*\(")
QUALIFIERS = re.compile(r"(?:\s*\b(?:const|override|final|noexcept|volatile)\b)*\s*$")
THIRD_PARTY = ("leveldb", "secp256k1", "univalue", "crc32c", "minisketch")
ASSERT_ALLOWED = ("sync.h", "sync.cpp")


NUMBER = re.compile(r"\d(?:[\w.]|'(?=\w)|(?<=[eEpP])[+-])*")
# Where strip() has something to do: a comment, a literal, or a number.
STRIP_START = re.compile(r"""//|/\*|["']|(?<![A-Za-z0-9_])\d""")


def strip(text):
    """Blank comments and string and character literals, keeping offsets and newlines."""
    out = []
    i, n = 0, len(text)
    while i < n:
        m = STRIP_START.search(text, i)
        if not m:
            out.append(text[i:])
            break
        out.append(text[i:m.start()])
        i = m.start()
        c = text[i]
        if c.isdigit():
            # A number, whose digit separators (1'000) do not open a character literal.
            j = NUMBER.match(text, i).end()
            out.append(text[i:j])
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c in "\"'":
            # To the closing quote, or to the end of the line for one that is not closed. The newline
            # is never part of the literal, unless escaped, and is kept either way.
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j, n)
            closed = j < n and text[j] == c
            out.append(c + re.sub(r"[^\n]", " ", text[i + 1:j]) + (c if closed else ""))
            i = j + 1 if closed else j
    return "".join(out)


def match_close(t, i, o, c):
    """t[i] is the opening bracket o; return the index just after its matching c."""
    d = 0
    for j in range(i, len(t)):
        if t[j] == o:
            d += 1
        elif t[j] == c:
            d -= 1
            if d == 0:
                return j + 1
    return len(t)


def match_open(t, j, o, c):
    """t[j] is the closing bracket c; return the index of its matching o."""
    d = 0
    for k in range(j, -1, -1):
        if t[k] == c:
            d += 1
        elif t[k] == o:
            d -= 1
            if d == 0:
                return k
    return -1


def split_top(inner):
    """inner split at the commas outside brackets."""
    out, d, cur = [], 0, []
    for ch in inner:
        if ch in "(<[{":
            d += 1
        elif ch in ")>]}":
            d -= 1
        if ch == "," and d == 0:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    out.append("".join(cur))
    return out


def split_params(params):
    inner = params.strip()
    if inner in ("", "void"):
        return []
    return split_top(inner)


def param_count(params):
    return len(split_params(params))


def param_names(params):
    """{parameter name: its position, as $0, $1, ...}."""
    names = {}
    for i, x in enumerate(split_params(params)):
        x = re.sub(r"\[[^\]]*\]\s*$", "", x.split("=", 1)[0].strip())
        m = re.search(r"[\s*&]([A-Za-z_]\w*)$", x)
        if m:
            names[m.group(1)] = f"${i}"
    return names


def lock_expr(x, names):
    """A lock expression without whitespace, this->, or class and namespace qualifiers, its parameters by position."""
    x = re.sub(r"\s+", "", x)
    x = re.sub(r"\bthis->", "", x)
    x = re.sub(r"(?:\b[A-Za-z_]\w*)?::", "", x)
    return re.sub(r"(?<![\w.>$])[A-Za-z_]\w*", lambda m: names.get(m.group(0), m.group(0)), x)


def lock_set(segment, params):
    """{annotation kind: set of lock expressions} for the annotations in segment, of a function with params."""
    names = param_names(params)
    out = {}
    for m in ANN.finditer(segment):
        e = match_close(segment, m.end() - 1, "(", ")")
        locks = {lock_expr(x, names) for x in split_top(segment[m.end():e - 1]) if x.strip()}
        out.setdefault(m.group(1), set()).update(locks)
    return out


def internal_linkage(t, pos, scopes):
    """True if the declaration or definition whose name starts at pos is static or in an anonymous namespace."""
    start = max(t.rfind(";", 0, pos), t.rfind("{", 0, pos), t.rfind("}", 0, pos)) + 1
    return bool(re.search(r"\bstatic\b", t[start:pos])) or scopes.in_anonymous_namespace(pos)


class Scopes:
    """The brace structure of one stripped file: class bodies by name, and which braces are namespaces."""

    def __init__(self, t):
        self.t = t
        self.classes = {}       # name -> [(open, close)]
        self.nonns = []         # (open, close) of every brace that is not a namespace or extern "C"
        self.anon = []          # (open, close) of every anonymous namespace
        stack = []
        for i, ch in enumerate(t):
            if ch == "{":
                head = t[max(0, i - 300):i]
                kind = "other"
                if re.search(r"\bnamespace\s*$", head):
                    kind = "anon"
                elif (re.search(r"\bnamespace(\s+[A-Za-z_][\w:]*)?\s*$", head)
                      or re.search(r'\bextern\s+"\s*"\s*$', head)):
                    kind = "ns"
                else:
                    m = re.search(r"\b(?:class|struct)\s+(?:[A-Z_]+\s+)?([A-Za-z_]\w*)\b[^;{()]*$", head)
                    if m and not re.search(r"\benum\s+(?:class|struct)\s+" + m.group(1), head):
                        kind = ("class", m.group(1))
                stack.append((i, kind))
            elif ch == "}" and stack:
                o, kind = stack.pop()
                if kind == "anon":
                    self.anon.append((o, i))
                elif kind != "ns":
                    self.nonns.append((o, i))
                if isinstance(kind, tuple):
                    self.classes.setdefault(kind[1], []).append((o, i))
        # The outermost of those braces, which do not overlap, for a bisect.
        self.roots_open, self.roots_close = [], []
        for o, c in sorted(self.nonns):
            if not self.roots_close or o > self.roots_close[-1]:
                self.roots_open.append(o)
                self.roots_close.append(c)

    def at_namespace_scope(self, pos):
        k = bisect.bisect_left(self.roots_open, pos) - 1
        return k < 0 or pos >= self.roots_close[k]

    def in_anonymous_namespace(self, pos):
        return any(o < pos < c for o, c in self.anon)

    def member_level(self, body, pos):
        """True if pos is directly in the class body (o, c), not inside a nested brace."""
        o, c = body
        return not any(o < oo < pos < cc < c for oo, cc in self.nonns)


def definitions(t):
    """Annotated function definitions: (name, param count, lock set, offset, offset of the name)."""
    out = []
    for m in ANN.finditer(t):
        # The annotations run on to the body's '{' (or a constructor's ':').
        k = m.start()
        e = k
        while True:
            mm = ANN.search(t, e)
            if mm and t[e:mm.start()].strip() == "":
                e = match_close(t, mm.end() - 1, "(", ")")
                continue
            break
        rest = t[e:e + 200]
        if not re.match(r"\s*(?:->\s*[\w:<>,\s\*&]+?\s*)?(?::[^;{]*)?\{", rest):
            continue
        # Only the first annotation of a run starts a definition record.
        prev = ANN.search(t, max(0, k - 400), k)
        if prev and t[match_close(t, prev.end() - 1, "(", ")"):k].strip() == "":
            continue
        # The declarator before the annotations, in a window of the text before them; base is where the window
        # starts in t.
        base = max(0, k - 4000)
        head = re.sub(QUALIFIERS, "", t[base:k].rstrip())
        head = re.sub(r"->\s*[\w:<>,\s\*&]+$", "", head).rstrip()
        if not head.endswith(")"):
            continue
        o = match_open(head, len(head) - 1, "(", ")")
        if o < 0:
            continue
        pre = head[:o].rstrip()
        if pre.endswith("]"):
            continue  # a lambda
        mi = re.search(r"([A-Za-z_~]\w*(?:\s*::\s*[A-Za-z_~]\w*)*)\s*$", pre)
        if not mi:
            continue
        name = re.sub(r"\s+", "", mi.group(1))
        if name.split("::")[-1] in ("if", "for", "while", "switch", "catch", "return", "sizeof"):
            continue
        params = head[o + 1:-1]
        out.append((name, param_count(params), lock_set(t[k:e], params), k, base + mi.start()))
    return out


# A name followed by '(', not after '.', '->' or '::': a call or a declaration.
NAME_PAREN = re.compile(r"(?<![\w.>:])([A-Za-z_~]\w*)\s*\(")


def name_index(t):
    """{name: [(offset of the name, offset just after its '(')]} for every NAME_PAREN in t."""
    index = {}
    for m in NAME_PAREN.finditer(t):
        index.setdefault(m.group(1), []).append((m.start(), m.end()))
    return index


def declarations(short, nparams, t, region, scopes, member, index, upto=None, external=False):
    """Declarations of `short` with nparams parameters in t[region]: (offset, lock set). index is t's
    name_index. With external, only those that are not static or in an anonymous namespace."""
    lo, hi = region
    found = []
    for p, after in index.get(short, []):
        if p < lo or after > hi or (upto is not None and p >= upto):
            continue
        if member is not None:
            if not scopes.member_level(member, p):
                continue
        elif not scopes.at_namespace_scope(p):
            continue
        e = match_close(t, after - 1, "(", ")")
        tail = t[e:e + 600]
        mt = re.match(r"(?:[^;{}()]|\((?:[^()]|\([^()]*\))*\))*?;", tail)
        if not mt or "{" in tail[:mt.end()]:
            continue
        lastline = t[max(0, p - 200):p].split("\n")[-1]
        if re.search(r"(?:\.|->|\breturn|=|\(|,|!|&&|\|\||\))\s*$", lastline):
            continue
        params = t[after:e - 1]
        if param_count(params) != nparams:
            continue
        if external and internal_linkage(t, p, scopes):
            continue
        found.append((p, lock_set(tail[:mt.end()], params)))
    return found


def check_tree(files):
    """files: {path: text}. Returns a list of error strings."""
    errors = []
    stripped = {p: strip(t) for p, t in files.items()}
    scopes = {p: Scopes(t) for p, t in stripped.items()}
    indexes = {p: name_index(t) for p, t in stripped.items()}

    for p, t in stripped.items():
        if p in ASSERT_ALLOWED:
            continue
        for m in re.finditer(r"\bAssertLockHeld\s*\(", t):
            line = t.count("\n", 0, m.start()) + 1
            errors.append(f"{p}:{line}: AssertLockHeld: state the requirement with an annotation on the "
                          "function's declaration instead")

    for p, t in stripped.items():
        if not p.endswith(".cpp"):
            continue
        for name, nparams, locks, pos, name_pos in definitions(t):
            parts = name.split("::")
            short = parts[-1]
            owner = parts[-2] if len(parts) > 1 else None
            decls = []
            if owner is not None and any(owner in s.classes for s in scopes.values()):
                for q, s in scopes.items():
                    for body in s.classes.get(owner, []):
                        for off, ls in declarations(short, nparams, stripped[q], body, s, body, indexes[q]):
                            decls.append((q, off, ls))
            else:
                if owner is None and not scopes[p].at_namespace_scope(pos):
                    continue  # a member defined inside its class
                internal = owner is None and internal_linkage(t, name_pos, scopes[p])
                for q, s in scopes.items():
                    external = False
                    if q.endswith(".h"):
                        region = (0, len(stripped[q]))
                        upto = None
                    elif q == p:
                        region = (0, pos)
                        upto = pos
                    elif not internal:
                        # Another .cpp file: an extern declaration of the same function there.
                        region = (0, len(stripped[q]))
                        upto = None
                        external = True
                    else:
                        continue
                    for off, ls in declarations(short, nparams, stripped[q], region, s, None, indexes[q], upto,
                                                external):
                        decls.append((q, off, ls))
            def_line = t.count("\n", 0, pos) + 1
            for q, off, ls in decls:
                missing = [k for k, names in locks.items() if not names <= ls.get(k, set())]
                if missing:
                    dline = stripped[q].count("\n", 0, off) + 1
                    errors.append(f"{q}:{dline}: {name}: this declaration lacks the "
                                  f"{', '.join(sorted(missing))} annotation that its definition at {p}:{def_line} "
                                  "carries; put it on the declaration")
    return errors


def load(src):
    files = {}
    for root, dirs, names in os.walk(src):
        rel = os.path.relpath(root, src)
        if rel.split(os.sep)[0] in THIRD_PARTY:
            continue
        for n in names:
            if n.endswith((".cpp", ".h")):
                path = os.path.join(root, n)
                key = os.path.relpath(path, src).replace(os.sep, "/")
                files[key] = open(path, encoding="utf-8", errors="surrogateescape").read()
    return files


SAMPLES_BAD = {
    "header only on the definition": {
        "a.h": "int f(int x);\n",
        "a.cpp": "int f(int x) EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n    return x;\n}\n",
    },
    "member only on the definition": {
        "s.h": "struct S {\n    void g(const int& y) const;\n};\n",
        "s.cpp": "void S::g(const int& y) const EXCLUSIVE_LOCKS_REQUIRED(m_cs)\n{\n}\n",
    },
    "forward declaration in the same file": {
        "b.cpp": "static void h();\nvoid k() { h(); }\nstatic void h() EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n}\n",
    },
    "different lock on the declaration": {
        "c.h": "void q() EXCLUSIVE_LOCKS_REQUIRED(cs_a);\n",
        "c.cpp": "void q() EXCLUSIVE_LOCKS_REQUIRED(cs_b)\n{\n}\n",
    },
    "AssertLockHeld": {
        "d.cpp": "void r()\n{\n    AssertLockHeld(cs_main);\n}\n",
    },
    "AssertLockHeld in a test": {
        "test/e_tests.cpp": "void t()\n{\n    AssertLockHeld(cs_main);\n}\n",
    },
    "the lock of only one of two instances": {
        "m.h": "void merge(CWallet* left, CWallet* right) EXCLUSIVE_LOCKS_REQUIRED(left->cs_wallet);\n",
        "m.cpp": "void merge(CWallet* left, CWallet* right)\n"
                 "EXCLUSIVE_LOCKS_REQUIRED(left->cs_wallet, right->cs_wallet)\n{\n}\n",
    },
    "AssertLockHeld after a digit separator": {
        "n.cpp": "void f() { const int n = 1'000; AssertLockHeld(cs_main); }\n",
    },
    "an extern declaration in another file": {
        "x.cpp": "extern int g(int x);\nint use() { return g(1); }\n",
        "y.cpp": "int g(int x) EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n    return x;\n}\n",
    },
}

# Bad samples whose report must name this file and line.
SAMPLES_BAD_AT = {
    "the line of an AssertLockHeld after a digit separator": (
        {"n.cpp": "void f()\n{\n    const int n = 1'000;\n    AssertLockHeld(cs_main);\n}\n"}, "n.cpp:4:"),
}

SAMPLES_GOOD = {
    "on the declaration and the definition": {
        "a.h": "int f(int x) EXCLUSIVE_LOCKS_REQUIRED(cs_main);\n",
        "a.cpp": "int f(int x) EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n    return x;\n}\n",
    },
    "on the declaration only": {
        "a.h": "int f(int x) EXCLUSIVE_LOCKS_REQUIRED(cs_main);\n",
        "a.cpp": "int f(int x)\n{\n    return x;\n}\n",
    },
    "member with a qualified lock name": {
        "s.h": "struct S {\n    void g() const EXCLUSIVE_LOCKS_REQUIRED(cs_x);\n};\n",
        "s.cpp": "void S::g() const EXCLUSIVE_LOCKS_REQUIRED(S::cs_x)\n{\n}\n",
    },
    "no separate declaration": {
        "b.cpp": "static void h() EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n}\nvoid k() EXCLUSIVE_LOCKS_REQUIRED(cs_main) { h(); }\n",
    },
    "calls are not declarations": {
        "a.h": "void f() EXCLUSIVE_LOCKS_REQUIRED(cs_main);\n",
        "a.cpp": "void f() EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n}\nvoid u() EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n    f();\n    if (true) f();\n    (void)f();\n}\n",
    },
    "an overload with other parameters": {
        "o.h": "void v(int a) EXCLUSIVE_LOCKS_REQUIRED(cs_main);\nvoid v(int a, int b);\n",
        "o.cpp": "void v(int a) EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n}\n",
    },
    "a lambda": {
        "l.cpp": "void w()\n{\n    auto fn = [&]() EXCLUSIVE_LOCKS_REQUIRED(cs_main) { return 1; };\n}\n",
    },
    "parameters named differently": {
        "m.h": "void merge(CWallet* a, CWallet* b) EXCLUSIVE_LOCKS_REQUIRED(a->cs_wallet, b->cs_wallet);\n",
        "m.cpp": "void merge(CWallet* left, CWallet* right)\n"
                 "EXCLUSIVE_LOCKS_REQUIRED(left->cs_wallet, right->cs_wallet)\n{\n}\n",
    },
    "this-> on the declaration": {
        "s.h": "struct S {\n    void g() EXCLUSIVE_LOCKS_REQUIRED(this->m_cs);\n};\n",
        "s.cpp": "void S::g() EXCLUSIVE_LOCKS_REQUIRED(m_cs)\n{\n}\n",
    },
    "a static function of the same name in another file": {
        "x.cpp": "static int g(int x);\n",
        "y.cpp": "int g(int x) EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n    return x;\n}\n",
    },
    "a function of the same name in an anonymous namespace in another file": {
        "x.cpp": "namespace {\nint g(int x);\n}\n",
        "y.cpp": "int g(int x) EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n    return x;\n}\n",
    },
    "a static definition and an extern declaration in another file": {
        "x.cpp": "extern int g(int x);\n",
        "y.cpp": "static int g(int x) EXCLUSIVE_LOCKS_REQUIRED(cs_main)\n{\n    return x;\n}\n",
    },
    "AssertLockHeld in sync": {
        "sync.h": "#define AssertLockHeld(cs) AssertLockHeldInternal(#cs, __FILE__, __LINE__, &cs)\n",
        "sync.cpp": "void x()\n{\n    // AssertLockHeld(cs) is defined in sync.h.\n}\n",
    },
}


def self_test():
    ok = True
    for name, files in SAMPLES_BAD.items():
        if not check_tree(files):
            print(f"self-test: the bad sample '{name}' was not flagged")
            ok = False
    for name, (files, at) in SAMPLES_BAD_AT.items():
        errs = check_tree(files)
        if not any(e.startswith(at) for e in errs):
            print(f"self-test: the bad sample '{name}' was not flagged at {at}: {errs}")
            ok = False
    for name, files in SAMPLES_GOOD.items():
        errs = check_tree(files)
        if errs:
            print(f"self-test: the good sample '{name}' was flagged: {errs}")
            ok = False
    if ok:
        print(f"lint-lock-annotations self-test: {len(SAMPLES_BAD) + len(SAMPLES_BAD_AT)} bad samples flagged, "
              f"{len(SAMPLES_GOOD)} good samples clean")
    return ok


def main():
    if "--self-test" in sys.argv[1:]:
        sys.exit(0 if self_test() else 1)
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src")
    if len(sys.argv) > 1 and not sys.argv[1].startswith("--"):
        src = sys.argv[1]
    errors = check_tree(load(src))
    for e in errors:
        print(e)
    if errors:
        print(f"lint-lock-annotations: {len(errors)} problem(s)")
        sys.exit(1)
    print("lint-lock-annotations: OK")


if __name__ == "__main__":
    main()
