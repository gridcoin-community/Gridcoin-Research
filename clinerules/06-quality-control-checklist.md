# Quality Control Checklist for Gridcoin Contributions

## Purpose

This checklist ensures contributions meet quality standards before submission. Use it as a pre-commit and pre-PR review guide to catch common issues early.

---

## Documentation Changes

### Before Committing

- [ ] **No trailing whitespace**
  - Check: `grep -En '[[:space:]]+$' *.md`
  - Should return nothing
  - Fix: `sed -i 's/[[:space:]]*$//' *.md` (or `sed -i '' 's/[[:space:]]*$//' *.md` on macOS)

- [ ] **No tab characters in markdown**
  - Run: `grep -n $'\t' *.md`
  - Should return nothing

- [ ] **Code examples use 4 spaces (not tabs)**
  - Check all code blocks for consistent indentation

- [ ] **Cross-references are valid**
  - All links to other files/sections work correctly
  - No broken internal links

- [ ] **Examples have been tested**
  - Command examples run without errors
  - Code examples compile/work as shown

- [ ] **Follows Baby Steps philosophy**
  - Changes are focused and incremental
  - Each change has clear purpose

### Before Submitting PR

- [ ] **Run full lint suite**
  - From repo root: `test/lint/lint-all.sh`
  - Or run full local CI: `contrib/devtools/run-local-ci.sh`
  - All checks pass

- [ ] **Changes documented appropriately**
  - Updated relevant .md files
  - Added to changelog if significant

- [ ] **README.md updated (if needed)**
  - New files listed in appropriate section
  - Table of contents reflects changes

---

## Code Changes

### Before Committing

- [ ] **Follows coding standards**
  - See `01-coding.md` for style guide
  - ANSI/Allman block style
  - 4-space indentation, no tabs
  - Modern variable naming: `lower_snake_case` for locals, `m_` prefix for members, `UPPER_SNAKE_CASE` for global constants
  - Legacy Hungarian notation only for consistency within existing modules

- [ ] **Comments explain "why", not just "what"**
  - Complex logic has explanatory comments
  - Non-obvious decisions documented

- [ ] **One substantive change per commit**
  - Each commit represents one logical change
  - No mixing unrelated changes

- [ ] **Lock order respected**
  - Always: `cs_main` → `cs_wallet`
  - No potential deadlocks introduced
  - Tip: Configure with `-DENABLE_DEBUG_LOCKORDER=ON` to detect violations at runtime

- [ ] **No trailing whitespace in code**
  - Check: `grep -En '[[:space:]]+$' *.cpp *.h`
  - Fix: `sed -i 's/[[:space:]]*$//' *.cpp *.h` (or `sed -i '' 's/[[:space:]]*$//' *.cpp *.h` on macOS)
  - Clean up before commit

- [ ] **No debug code left in**
  - Remove temporary print statements
  - Remove commented-out debugging code

### Before Submitting PR

- [ ] **Unit tests added/updated**
  - New features have test coverage
  - Changed features have updated tests
  - All tests pass: `./build/src/test/test_gridcoin`

- [ ] **Manual testing completed**
  - Tested on testnet first
  - Verified expected behavior
  - Checked edge cases

- [ ] **Documentation updated**
  - Code comments added/updated
  - User-facing docs updated if needed
  - RPC help text accurate

- [ ] **CHANGELOG.md entry added**
  - For user-facing changes
  - Clear description of change
  - Proper category (Added/Changed/Fixed/etc.)

- [ ] **Consensus changes properly gated**
  - Height-gated activation if needed
  - Version bumps if appropriate
  - Community coordination planned

---

## Consensus Changes (Critical)

### Special Requirements

- [ ] **Community approval obtained**
  - Discussed on Discord/GitHub
  - General agreement on approach

- [ ] **Hard fork planning complete**
  - Activation height determined
  - Upgrade timeline communicated

- [ ] **Extensive testing performed**
  - Tested on testnet extensively
  - Multiple scenarios verified
  - Edge cases examined

- [ ] **Documentation comprehensive**
  - CHANGELOG.md updated
  - Release notes prepared
  - Migration guide written (if needed)

- [ ] **Backward compatibility considered**
  - Old blocks still validate correctly
  - Database migrations handled
  - Network protocol compatible

---

## GUI Changes

### Before Committing

- [ ] **UI files properly formatted**
  - .ui files valid XML
  - Consistent styling applied

- [ ] **Signals/slots connected correctly**
  - No memory leaks (proper cleanup)
  - Thread safety maintained

- [ ] **Strings marked for translation**
  - User-facing strings use `tr()`
  - No hardcoded English text

### Before Submitting PR

- [ ] **Tested on target platforms**
  - Windows, Linux, macOS (if possible)
  - Different screen resolutions
  - High DPI displays

- [ ] **Accessibility considered**
  - Tab order logical
  - Keyboard shortcuts work
  - Screen reader friendly

---

## RPC Changes

### Before Committing

- [ ] **Help text complete and accurate**
  - All parameters documented
  - Return values explained
  - Examples provided

- [ ] **Parameter validation implemented**
  - Invalid inputs rejected gracefully
  - Clear error messages

- [ ] **Proper locking used**
  - Appropriate locks acquired
  - Locks released properly

### Before Submitting PR

- [ ] **Backward compatibility maintained**
  - Existing functionality unchanged
  - New parameters optional (if possible)

- [ ] **Examples tested**
  - CLI examples work
  - RPC examples work

---

## Contract Changes

### Before Committing

- [ ] **Contract type properly registered**
  - Added to enum
  - Dispatcher updated
  - Handler implemented

- [ ] **Validation comprehensive**
  - Well-formed checks complete
  - Block-context validation correct
  - DoS scoring appropriate

- [ ] **Burn fee appropriate**
  - Fee amount reasonable
  - Prevents spam

### Before Submitting PR

- [ ] **Registry persistence working**
  - Database operations correct
  - Reorg handling proper
  - Migration path clear

- [ ] **Tests comprehensive**
  - Unit tests for validation
  - Integration tests for workflow
  - Edge cases covered

---

## Performance Changes

### Before Committing

- [ ] **Profiling data supports change**
  - Identified actual bottleneck
  - Measured improvement

- [ ] **No premature optimization**
  - Change addresses real issue
  - Complexity justified

### Before Submitting PR

- [ ] **Benchmarks show improvement**
  - Before/after measurements
  - Multiple test cases
  - No regressions elsewhere

- [ ] **Memory usage considered**
  - No significant increase
  - Leaks addressed

---

## General Best Practices

### Code Quality

- [ ] **No compiler warnings**
  - Clean build with warnings enabled
  - No suppressed warnings without reason

- [ ] **No magic numbers**
  - Constants named meaningfully
  - Values explained in comments

- [ ] **Error handling complete**
  - All error paths handled
  - Resources cleaned up properly

- [ ] **Logging appropriate**
  - Important events logged
  - Debug logging for troubleshooting
  - No excessive logging

### Git Practices

- [ ] **Commit message clear**
  - First line: concise summary
  - Body: explains why, not just what
  - References issues if applicable

- [ ] **Commits atomic**
  - Each commit buildable
  - Each commit testable
  - No "fix previous commit" commits in PR

- [ ] **Branch up to date**
  - Rebased on target branch (typically `development`; check PR target)
  - Conflicts resolved
  - Tests still pass

---

## Pre-Submission Final Check

### Required Actions

- [ ] **All lint checks pass**
  ```bash
  test/lint/lint-all.sh
  ```

- [ ] **All tests pass**
  ```bash
  cmake --build . --target test_gridcoin
  ./src/test/test_gridcoin
  ```

- [ ] **No trailing whitespace anywhere**
  ```bash
  # Check for documentation
  grep -rEn '[[:space:]]+$' clinerules/*.md

  # Check for code
  grep -rEn '[[:space:]]+$' src/*.cpp src/*.h

  # Fix documentation (recursive)
  find clinerules/ -name "*.md" -type f -exec sed -i 's/[[:space:]]*$//' {} +
  # On macOS: find clinerules/ -name "*.md" -type f -exec sed -i '' 's/[[:space:]]*$//' {} +

  # Fix code (recursive)
  find src/ \( -name "*.cpp" -o -name "*.h" \) -type f -exec sed -i 's/[[:space:]]*$//' {} +
  # On macOS: find src/ \( -name "*.cpp" -o -name "*.h" \) -type f -exec sed -i '' 's/[[:space:]]*$//' {} +
  ```

- [ ] **Clean build on multiple platforms**
  - At minimum: your development platform
  - Ideally: Windows, Linux, macOS

### PR Description

- [ ] **Clear title**
  - Summarizes change in one line

- [ ] **Comprehensive description**
  - What: what changes were made
  - Why: motivation for changes
  - How: approach taken

- [ ] **Testing notes included**
  - How to test the changes
  - Expected results
  - Edge cases to check

- [ ] **Screenshots if GUI**
  - Before/after comparisons
  - Different states shown

---

## Remember: Baby Steps

This checklist may seem extensive, but remember:

1. **One change at a time**: Focus on one substantive improvement
2. **Test as you go**: Don't wait until the end
3. **Document immediately**: Write docs while context is fresh
4. **Ask for help**: Better to ask than to guess
5. **Process is product**: Taking time to do it right IS the work

---

## Related Documentation

- **Coding Standards**: `01-coding.md`
- **Common Tasks**: `05-common-tasks.md`
- **Architecture**: `02-architecture-overview.md`
- **Testing**: `05-common-tasks.md` (Section 6: Working with the Test Suite)

---

## Quick Reference Commands

```bash
# Run all linters
test/lint/lint-all.sh

# Run unit tests
./build/src/test/test_gridcoin

# Check for trailing whitespace
grep -rEn '[[:space:]]+$' --include="*.cpp" --include="*.h" src/
grep -rEn '[[:space:]]+$' --include="*.md" clinerules/

# Remove trailing whitespace
find src/ \( -name "*.cpp" -o -name "*.h" \) -type f -exec sed -i 's/[[:space:]]*$//' {} +
find clinerules/ -name "*.md" -type f -exec sed -i 's/[[:space:]]*$//' {} +
# On macOS: add '' after -i (e.g., sed -i '' 's/[[:space:]]*$//')

# Find tab characters
grep -rn $'\t' --include="*.cpp" --include="*.h" src/

# Build with warnings
cmake -DCMAKE_CXX_FLAGS="-Wall -Wextra" ..
cmake --build .
```

---

**Use this checklist to ensure your contributions are high-quality and ready for review!**
