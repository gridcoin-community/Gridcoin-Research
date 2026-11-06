TEST-ONLY. test-key.pem is a throwaway P-256 key whose private half is committed on purpose
so the cross-language checks (Python and Windows PowerShell 5.1) are reproducible. It is never
a release key and its key id must never appear in contrib/windows/release-keys.txt (the lint
test/lint/lint-release-signature.sh fails if it does).

SHA256SUMS                 sample list
SHA256SUMS.sig             valid signature by the test key (verifies)
SHA256SUMS.tampered        the list with one digit changed (must be refused)
SHA256SUMS.sig.wrongkey    claims the test key id but signed by another key (must be refused)
test-release-keys.txt      keys file pinning the test key, for -KeysPath in standalone checks
