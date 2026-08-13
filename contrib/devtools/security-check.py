#!/usr/bin/env python3
# Copyright (c) 2015-present The Bitcoin Core developers
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Perform basic security checks on a series of executables.
Exit status will be 0 if successful, and the program will be silent.
Otherwise the exit status will be 1 and it will log which executables failed which checks.

Example usage:

    python3 contrib/devtools/security-check.py build/bin/gridcoinresearchd build/bin/gridcoinresearch

===============================================================================
PORTING NOTES
===============================================================================

SOURCE
------
Adapted from Bitcoin Core `contrib/guix/security-check.py`.

  upstream path   : contrib/guix/security-check.py
  upstream repo   : https://github.com/bitcoin/bitcoin
  master HEAD     : 128456b62d5e38abea031f97f823d5b28aef9357 (2026-08-08T13:00:53Z)
  file last touch : 594a02c3ae05 (2026-08-03, "lint: re-add guix scripts to mypy linting")
  fetched         : 2026-08-08
  source sha256   : 7304f12cc45a8f5dd59e46c7299d1594726f29562fe11bd0030c34fa40cf28cf
  source length   : 293 lines

Two upstream path facts that matter for future re-syncs:
  * The script used to live at contrib/devtools/security-check.py. It was MOVED to
    contrib/guix/ in 415650cea94f (2025-05-09, "guix: move *-check.py scripts under
    contrib/guix"). contrib/devtools/security-check.py is a 404 on upstream master.
  * Upstream's companion `test-security-check.py` was DELETED on 2025-02-10 in
    76c090145e9b. It has deliberately NOT been ported here (see "NOT PORTED" below).

This is a fresh port from current Bitcoin Core. It is NOT a resurrection of Gridcoin's
own 2017-era security-check.py, which was deleted on 2017-08-06 in 8db4b58b1 and is
far too stale (readelf/objdump scraping, pre-lief) to be a useful base.

WHAT CHANGED (divergences from upstream, each deliberate)
---------------------------------------------------------
(1) Removed the `--monolithic` FORTIFY escape hatch.
    Upstream check_ELF_FORTIFY() begins with:
        if '--monolithic' in binary.strings:
            return True
    which whitelists Bitcoin's multi-call `bitcoin` wrapper binary out of the FORTIFY
    check. Gridcoin has no multi-call wrapper -- `add_executable` produces only
    `gridcoinresearchd` (src/CMakeLists.txt:350) and `gridcoinresearch`
    (src/qt/CMakeLists.txt:232). Keeping the hatch would mean any Gridcoin binary that
    merely happens to contain the string "--monolithic" silently passes FORTIFY, which
    is precisely the class of silent pass this script exists to catch.

(2) The driver now FAILS CLOSED instead of raising.
    Upstream's `__main__` does an unguarded `binary.format` on a possibly-None
    lief.parse() result and an unguarded double lookup `CHECKS[etype][arch]`. Upstream
    gets away with it because Guix hands it a curated ${INSTALLPATH}/bin/* list; the
    `# type: ignore[union-attr]` comments on its lines 283-284 are upstream
    acknowledging the sharp edge. Gridcoin's invocation seam is a CI shell step over a
    glob, so a non-binary or an unexpected arch is reachable. Upstream would emit an
    AttributeError/KeyError traceback; a shell step that treats a traceback as "check
    ran" is a false pass. Here, unparseable input, an unsupported format/arch, and a
    check that raises are all reported as FAILURES.
    This is a safe-direction divergence: it can only ever produce a false alarm, never
    a false pass. The precedent for caring: the "Verify Hermeticity (Linux)" step at
    .github/workflows/cmake_production.yml:144-184 points at
    `${build_dir}/src/gridcoinresearchd` and `${build_dir}/src/qt/gridcoinresearch`,
    but CMake puts both binaries in `${build_dir}/bin/` (RUNTIME_OUTPUT_DIRECTORY at
    src/CMakeLists.txt:395 and src/qt/CMakeLists.txt:309 -- verified against three
    real build trees). Its missing-file branch emits `::warning::` and passes
    (workflow lines 175-178), so that check is a silent no-op today. Do not repeat
    that shape.

(3) Stdout contract preserved, diagnostics moved to stderr.
    Upstream prints exactly `f'{filename}: failed {" ".join(failed)}'`. That is kept
    byte-for-byte so any future re-sync (and any caller parsing it) is unaffected.
    Extra detail from a raising check goes to STDERR only.

(4) Docstring/usage rewritten for Gridcoin paths. Upstream's example invokes
    `find ../path/to/guix/binaries`; Gridcoin has no contrib/guix/ and no Guix
    packaging stage, so the example names the real build-tree paths instead.

WHAT WAS DELIBERATELY KEPT
--------------------------
(a) Every check function, verbatim, with upstream names (check_ELF_RELRO,
    check_ELF_SEPARATE_CODE, check_PE_HIGH_ENTROPY_VA, ...) and upstream ordering.
    Re-syncing against a future Bitcoin Core revision should be a near-clean diff.
(b) The BASE_ELF / BASE_PE / BASE_MACHO / CHECKS table shape.
(c) The PPC64 and RISCV rows in CHECKS, even though Gridcoin ships neither. They cost
    nothing, and deleting them would create gratuitous diff noise against upstream.
(d) The argv[1:]-as-plain-path-list CLI. No argparse, matching upstream.
(e) SEPARATE_CODE and RELOC_SECTION, even though no Gridcoin build flag sets
    `-Wl,-z,separate-code` or `-Wl,--enable-reloc-section`. Both are modern toolchain
    defaults; the checks are free regression detectors. See CAVEATS.

NOT PORTED
----------
  * test-security-check.py. Upstream deleted it 2025-02-10 (76c090145e9b), stating the
    scripts "are becoming more of a nuisance than a value-add" because constructing a
    deliberately-failing binary gets harder as toolchains enable protections by
    default. The last extant copy is also stale against this script (it uses the
    pre-0.15 LIEF enum path `lief.ARCHITECTURES.X86`, whereas this script uses
    `lief.Header.ARCHITECTURES`). Its `pass_flags` list is still useful as an
    independent cross-check of the hardening flags Gridcoin's CMake should set, but
    the test itself is not worth resurrecting.
  * The entire Guix apparatus (contrib/guix/libexec/package.sh, manifest_gui.scm).
    Gridcoin has no Guix build. See INVOCATION.
  * Any coupling to symbol-check.py. Gridcoin's contrib/devtools/symbol-check.py is an
    older, unrelated lineage (subprocess/readelf, no lief) whose own docstring says it
    is unmaintained. It is a separate question; this script does not touch it.

INVOCATION / ORDERING HAZARD  -- READ BEFORE WIRING INTO CI
------------------------------------------------------------
Run this on UNSTRIPPED build-tree binaries, i.e. `${build_dir}/bin/*`.

`CMakeLists.txt:614` sets `CPACK_STRIP_FILES ON`, so binaries inside CPack output
(TGZ, NSIS64) are STRIPPED. The ELF and PE CONTROL_FLOW checks resolve `main` from
the symbol table, so on a stripped binary they fail for a reason that has nothing to
do with `-fcf-protection`. Upstream has the same constraint and handles it by
ordering: security-check runs at contrib/guix/libexec/package.sh:13, before
split-debug.sh strips at line 27.

The artifact-copy steps already use the correct unstripped paths --
cmake_production.yml:687-688 (`build_native/bin/`), :798-799 (`build_arm64/bin/`),
:233-243 (`${build_dir}/bin/*.exe`) -- so those are the seams to hook.

macOS: the Qt GUI is a bundle (MACOSX_BUNDLE TRUE, src/qt/CMakeLists.txt:307). Pass
the inner Mach-O, `<...>/gridcoinresearch.app/Contents/MacOS/gridcoinresearch`, which
is the path cmake_production.yml:404 and :590 already compute. This script does not
walk .app directories, so passing the bundle itself fails as unparseable (loudly, by
design). Note the macOS `deploy` target ad-hoc signs the app; run the check before
signing or re-verify after, since codesigning rewrites the Mach-O.

DEPENDENCY
----------
`lief` only. Requires the modern (>= 0.15) enum layout -- `lief.ELF.Segment.TYPE`,
`lief.ELF.DynamicEntry.TAG`, `lief.Binary.FORMATS`, `lief.Header.ARCHITECTURES`.
Older distro `python3-lief` packages use different enum paths and will raise
AttributeError at import/first use. Pin it; do not accept whatever the distro ships.
Upstream pins `lief==0.17.5` in ci/lint/requirements.txt. Python >= 3.9 for the
`list[str]` annotation.

MEASURED BASELINE (this script, lief 0.17.5, 2026-08-08)
---------------------------------------------------------
Run against the shipped binaries the S18 audit measured by hand, it reports:

    gridcoinresearchd-5.5.1.4-development-multiprocess-11be6dd40:
        failed FORTIFY RELRO CANARY CONTROL_FLOW
    gridcoinresearchd-mitigation:
        failed FORTIFY RELRO CANARY CONTROL_FLOW
    gridcoinresearch-5.5.1.4-development-multiprocess-11be6dd40:
        failed FORTIFY RELRO CANARY CONTROL_FLOW

That independently reproduces S18: FORTIFY absent, RELRO partial (no BIND_NOW),
canary absent. PIE, NX and SEPARATE_CODE pass -- all three from toolchain defaults,
not because the build asks for them. CONTROL_FLOW is an additional datum consistent
with the loss of `-fcf-protection=full`.

CAVEATS -- checks with no corresponding Gridcoin build flag
-----------------------------------------------------------
  * SEPARATE_CODE: no Gridcoin flag requests `-Wl,-z,separate-code`. MEASURED as
    passing on ELF x86_64 (binutils >= 2.31 defaults to it). Kept as a free
    regression detector; not yet confirmed on the aarch64 cross build.
  * PE RELOC_SECTION / DYNAMIC_BASE / HIGH_ENTROPY_VA / NX / PIE: MEASURED as passing
    on stock x86_64-w64-mingw32 output with no hardening flags at all, so the mingw
    defaults already satisfy them. Only CONTROL_FLOW and CANARY failed on those
    probes -- i.e. those two are what the restored `-fcf-protection=full` and
    `-fstack-protector-all` + `-lssp` must supply.
  * MachO arm64 BRANCH_PROTECTION is NOT WIRED UP, deliberately. It requires
    `-mbranch-protection`, and that flag is withheld on Darwin: the pointer
    authentication it emits on plain arm64 defeats the unwinder, so a throw stops
    reaching its handler (MEASURED -- it aborted the macOS arm64 functional tests as
    "uncaught exception" and hung the job; removing the flag turned it green). PAC on
    Darwin is an ABI-level opt-in via arm64e, not something this flag can grant.
    check_MACHO_BRANCH_PROTECTION is kept below but referenced by nothing, so it is
    ready if we ever ship arm64e. If you re-add it to CHECKS, add the flag in the
    same change or the check will demand what the build must not produce.
  * MachO checks generally are UNVERIFIED; there was no Mach-O binary to test against.

A NOTE ON LIEF ERROR SHAPES
---------------------------
LIEF does not raise on a missing symbol -- `binary.get_function_address('main')`
returns a `lief_errors` object, and `binary.section_from_rva()` can return None.
Those then blow up one call later as a TypeError/AttributeError inside the check.
Both were observed while testing (stripped ELF, and a PE probe respectively). The
per-check try/except in the driver is what converts those into an honest
`failed CONTROL_FLOW` plus a stderr diagnostic, rather than a traceback that aborts
the whole run and loses the verdicts for every remaining binary on the command line.
===============================================================================
'''
import re
import sys

import lief

def check_ELF_RELRO(binary) -> bool:
    '''
    Check for read-only relocations.
    GNU_RELRO program header must exist
    Dynamic section must have BIND_NOW flag
    '''
    have_gnu_relro = False
    for segment in binary.segments:
        # Note: not checking p_flags == PF_R: here as linkers set the permission differently
        # This does not affect security: the permission flags of the GNU_RELRO program
        # header are ignored, the PT_LOAD header determines the effective permissions.
        # However, the dynamic linker need to write to this area so these are RW.
        # Glibc itself takes care of mprotecting this area R after relocations are finished.
        # See also https://marc.info/?l=binutils&m=1498883354122353
        if segment.type == lief.ELF.Segment.TYPE.GNU_RELRO:
            have_gnu_relro = True

    have_bindnow = False
    try:
        flags = binary.get(lief.ELF.DynamicEntry.TAG.FLAGS)
        if flags.has(lief.ELF.DynamicEntryFlags.FLAG.BIND_NOW):
            have_bindnow = True
    except Exception:
        have_bindnow = False

    return have_gnu_relro and have_bindnow

def check_ELF_CANARY(binary) -> bool:
    '''
    Check for use of stack canary
    '''
    return binary.has_symbol('__stack_chk_fail')

def check_ELF_SEPARATE_CODE(binary):
    '''
    Check that sections are appropriately separated in virtual memory,
    based on their permissions. This checks for missing -Wl,-z,separate-code
    and potentially other problems.
    '''
    R = lief.ELF.Segment.FLAGS.R
    W = lief.ELF.Segment.FLAGS.W
    E = lief.ELF.Segment.FLAGS.X
    EXPECTED_FLAGS = {
        # Read + execute
        '.init': R | E,
        '.plt': R | E,
        '.plt.got': R | E,
        '.plt.sec': R | E,
        '.text': R | E,
        '.fini': R | E,
        # Read-only data
        '.interp': R,
        '.note.gnu.property': R,
        '.note.gnu.build-id': R,
        '.note.ABI-tag': R,
        '.gnu.hash': R,
        '.dynsym': R,
        '.dynstr': R,
        '.gnu.version': R,
        '.gnu.version_r': R,
        '.rela.dyn': R,
        '.rela.plt': R,
        '.rodata': R,
        '.eh_frame_hdr': R,
        '.eh_frame': R,
        '.qtmetadata': R,
        '.gcc_except_table': R,
        '.stapsdt.base': R,
        # Writable data
        '.init_array': R | W,
        '.fini_array': R | W,
        '.dynamic': R | W,
        '.got': R | W,
        '.data': R | W,
        '.bss': R | W,
    }
    if binary.header.machine_type == lief.ELF.ARCH.PPC64:
        # .plt is RW on ppc64 even with separate-code
        EXPECTED_FLAGS['.plt'] = R | W
    # For all LOAD program headers get mapping to the list of sections,
    # and for each section, remember the flags of the associated program header.
    flags_per_section = {}
    for segment in binary.segments:
        if segment.type == lief.ELF.Segment.TYPE.LOAD:
            for section in segment.sections:
                flags_per_section[section.name] = segment.flags
    # Spot-check ELF LOAD program header flags per section
    # If these sections exist, check them against the expected R/W/E flags
    for (section, flags) in flags_per_section.items():
        if section in EXPECTED_FLAGS:
            if int(EXPECTED_FLAGS[section]) != int(flags):
                return False
    return True

def check_ELF_CONTROL_FLOW(binary) -> bool:
    '''
    Check for control flow instrumentation

    NOTE (Gridcoin): resolves `main` from the symbol table, so this fails on a
    stripped binary regardless of -fcf-protection. See the PORTING NOTES ordering
    hazard: run against ${build_dir}/bin/*, not CPack output (CPACK_STRIP_FILES ON,
    CMakeLists.txt:614).
    '''
    main = binary.get_function_address('main')
    content = binary.get_content_from_virtual_address(main, 4, lief.Binary.VA_TYPES.AUTO)

    if content.tolist() == [243, 15, 30, 250]: # endbr64
        return True
    return False

def check_ELF_FORTIFY(binary) -> bool:
    # Gridcoin divergence (1): upstream's `--monolithic` escape hatch is removed here.
    # Gridcoin has no multi-call wrapper binary, and the hatch would let any binary
    # containing that string silently pass FORTIFY.
    chk_funcs = set()

    for sym in binary.imported_symbols:
        match = re.search(r'__[a-z]*_chk', sym.name)
        if match:
            chk_funcs.add(match.group(0))

    # ignore stack-protector
    chk_funcs.discard('__stack_chk')

    return len(chk_funcs) >= 1

def check_PE_DYNAMIC_BASE(binary) -> bool:
    '''PIE: DllCharacteristics bit 0x40 signifies dynamicbase (ASLR)'''
    return lief.PE.OptionalHeader.DLL_CHARACTERISTICS.DYNAMIC_BASE in binary.optional_header.dll_characteristics_lists

# Must support high-entropy 64-bit address space layout randomization
# in addition to DYNAMIC_BASE to have secure ASLR.
def check_PE_HIGH_ENTROPY_VA(binary) -> bool:
    '''PIE: DllCharacteristics bit 0x20 signifies high-entropy ASLR'''
    return lief.PE.OptionalHeader.DLL_CHARACTERISTICS.HIGH_ENTROPY_VA in binary.optional_header.dll_characteristics_lists

def check_PE_RELOC_SECTION(binary) -> bool:
    '''Check for a reloc section. This is required for functional ASLR.'''
    return binary.has_relocations

def check_PE_CONTROL_FLOW(binary) -> bool:
    '''
    Check for control flow instrumentation

    NOTE (Gridcoin): as with the ELF variant, this resolves `main` and therefore
    requires an unstripped binary.
    '''
    main = binary.get_symbol('main').value

    section_addr = binary.section_from_rva(main).virtual_address
    virtual_address = binary.optional_header.imagebase + section_addr + main

    content = binary.get_content_from_virtual_address(virtual_address, 4, lief.Binary.VA_TYPES.VA)

    if content.tolist() == [243, 15, 30, 250]: # endbr64
        return True
    return False

def check_PE_CANARY(binary) -> bool:
    '''
    Check for use of stack canary
    '''
    return binary.has_symbol('__stack_chk_fail')

def check_MACHO_NOUNDEFS(binary) -> bool:
    '''
    Check for no undefined references.
    '''
    return binary.header.has(lief.MachO.Header.FLAGS.NOUNDEFS)

def check_MACHO_FIXUP_CHAINS(binary) -> bool:
    '''
    Check for use of chained fixups.
    '''
    return binary.has_dyld_chained_fixups

def check_MACHO_CANARY(binary) -> bool:
    '''
    Check for use of stack canary
    '''
    return binary.has_symbol('___stack_chk_fail')

def check_PIE(binary) -> bool:
    '''
    Check for position independent executable (PIE),
    allowing for address space randomization.
    '''
    return binary.is_pie

def check_NX(binary) -> bool:
    '''
    Check for no stack execution
    '''

    # binary.has_nx checks are only for the stack, but MachO binaries might
    # have executable heaps.
    if binary.format == lief.Binary.FORMATS.MACHO:
        return binary.concrete.has_nx_stack and binary.concrete.has_nx_heap
    else:
        return binary.has_nx

def check_MACHO_CONTROL_FLOW(binary) -> bool:
    '''
    Check for control flow instrumentation
    '''
    content = binary.get_content_from_virtual_address(binary.entrypoint, 4, lief.Binary.VA_TYPES.AUTO)

    if content.tolist() == [243, 15, 30, 250]: # endbr64
        return True
    return False

def check_MACHO_BRANCH_PROTECTION(binary) -> bool:
    '''
    Check for branch protection instrumentation

    NOTE (Gridcoin): intentionally not referenced by CHECKS. -mbranch-protection is
    withheld on Darwin because its pointer-authentication codegen breaks unwinding on
    plain arm64; see the CAVEATS section above and the hardening block in
    CMakeLists.txt. Retained for a future arm64e target.
    '''
    content = binary.get_content_from_virtual_address(binary.entrypoint, 4, lief.Binary.VA_TYPES.AUTO)

    if content.tolist() == [95, 36, 3, 213]: # bti
        return True
    return False

BASE_ELF = [
    ('FORTIFY', check_ELF_FORTIFY),
    ('PIE', check_PIE),
    ('NX', check_NX),
    ('RELRO', check_ELF_RELRO),
    ('CANARY', check_ELF_CANARY),
    ('SEPARATE_CODE', check_ELF_SEPARATE_CODE),
]

BASE_PE = [
    ('PIE', check_PIE),
    ('DYNAMIC_BASE', check_PE_DYNAMIC_BASE),
    ('HIGH_ENTROPY_VA', check_PE_HIGH_ENTROPY_VA),
    ('NX', check_NX),
    ('RELOC_SECTION', check_PE_RELOC_SECTION),
    ('CONTROL_FLOW', check_PE_CONTROL_FLOW),
    ('CANARY', check_PE_CANARY),
]

BASE_MACHO = [
    ('NOUNDEFS', check_MACHO_NOUNDEFS),
    ('CANARY', check_MACHO_CANARY),
    ('FIXUP_CHAINS', check_MACHO_FIXUP_CHAINS),
]

# Architectures Gridcoin actually ships (verified against
# .github/workflows/cmake_production.yml):
#   ELF   x86_64 -- depends-builds "Linux Static" (host x86_64-pc-linux-gnu, :38),
#                   amd64-native-build (:641), flatpak-build (:812)
#   ELF   ARM64  -- arm64-cross-build (aarch64-linux-gnu, :701)
#   PE    x86_64 -- depends-builds "Windows Cross-Compile" (x86_64-w64-mingw32, :46).
#                   There is no 32-bit target; the `dpkg --add-architecture i386` at
#                   :68 is only to install 32-bit wine for running the test suite.
#   MACHO x86_64 -- macos-native-intel (:452)
#   MACHO ARM64  -- macos-native-arm64 (:267)
# PPC64/RISCV are kept purely for upstream parity; see PORTING NOTES (c).
CHECKS = {
    lief.Binary.FORMATS.ELF: {
        lief.Header.ARCHITECTURES.X86_64: BASE_ELF + [('CONTROL_FLOW', check_ELF_CONTROL_FLOW)],
        lief.Header.ARCHITECTURES.ARM: BASE_ELF,
        lief.Header.ARCHITECTURES.ARM64: BASE_ELF,
        lief.Header.ARCHITECTURES.PPC64: BASE_ELF,
        lief.Header.ARCHITECTURES.RISCV: BASE_ELF,
    },
    lief.Binary.FORMATS.PE: {
        lief.Header.ARCHITECTURES.X86_64: BASE_PE,
    },
    lief.Binary.FORMATS.MACHO: {
        lief.Header.ARCHITECTURES.X86_64: BASE_MACHO + [('PIE', check_PIE),
                                              ('NX', check_NX),
                                              ('CONTROL_FLOW', check_MACHO_CONTROL_FLOW)],
        # No BRANCH_PROTECTION: Gridcoin deliberately does not pass
        # -mbranch-protection on Darwin (see the hardening block in CMakeLists.txt),
        # because the pointer-authentication codegen it produces on plain arm64
        # breaks exception unwinding. Requiring the check here would demand a flag
        # the build is required not to set.
        lief.Header.ARCHITECTURES.ARM64: BASE_MACHO,
    }
}

if __name__ == '__main__':
    # Gridcoin divergence (2): fail closed. Upstream lets a None parse result or an
    # unknown format/arch escape as an AttributeError/KeyError traceback, which a CI
    # shell step can misread as "the check ran". Every path below that cannot produce
    # a real verdict is reported as a failure instead.
    if len(sys.argv) < 2:
        print(f'usage: {sys.argv[0]} <executable> [<executable> ...]', file=sys.stderr)
        sys.exit(2)

    retval: int = 0
    for filename in sys.argv[1:]:
        try:
            binary = lief.parse(filename)
        except Exception as exc:
            print(f'{filename}: failed PARSE')
            print(f'{filename}: lief.parse raised {type(exc).__name__}: {exc}', file=sys.stderr)
            retval = 1
            continue

        if binary is None:
            print(f'{filename}: failed PARSE')
            print(f'{filename}: not a recognised executable format (or unreadable)', file=sys.stderr)
            retval = 1
            continue

        # These two reads were outside the guard above, which is where the
        # fail-closed promise leaked: lief.parse() on a macOS universal binary
        # returns a FatBinary, which has neither .format nor .abstract, so an
        # AttributeError escaped and killed the whole run -- losing the verdicts
        # for every remaining file on the command line, which a shell step can
        # easily read as "the check ran".
        try:
            etype = binary.format
            arch = binary.abstract.header.architecture
        except AttributeError:
            print(f'{filename}: failed PARSE')
            print(f'{filename}: parsed to {type(binary).__name__}, not a single-architecture '
                  f'binary (a universal/fat Mach-O must be thinned first)', file=sys.stderr)
            retval = 1
            continue

        if etype not in CHECKS or arch not in CHECKS[etype]:
            print(f'{filename}: failed UNSUPPORTED')
            print(f'{filename}: no check set for format={etype!s} arch={arch!s}', file=sys.stderr)
            retval = 1
            continue

        failed: list[str] = []
        for (name, func) in CHECKS[etype][arch]:
            try:
                passed = func(binary)
            except Exception as exc:
                # A check that cannot run is not a check that passed. Detail goes to
                # stderr so the stdout contract stays byte-identical to upstream.
                print(f'{filename}: {name} check raised {type(exc).__name__}: {exc}', file=sys.stderr)
                passed = False
            if not passed:
                failed.append(name)
        if failed:
            print(f'{filename}: failed {" ".join(failed)}')
            retval = 1
    sys.exit(retval)
