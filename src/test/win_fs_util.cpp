// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "test/win_fs_util.h"

#ifdef WIN32
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <winioctl.h>
#endif

namespace grc_test {

using util::winacl::ObservedAce;
using util::winacl::ObservedAcl;

bool SameAcl(const ObservedAcl& a, const ObservedAcl& b)
{
    if (a.owner != b.owner || a.dacl_present != b.dacl_present || a.is_protected != b.is_protected ||
        a.aces.size() != b.aces.size()) {
        return false;
    }
    for (size_t i = 0; i < a.aces.size(); ++i) {
        const ObservedAce& x = a.aces[i];
        const ObservedAce& y = b.aces[i];
        if (x.kind != y.kind || x.trustee != y.trustee || x.mask != y.mask || x.inherit_only != y.inherit_only ||
            x.inherited != y.inherited || x.inherit != y.inherit) {
            return false;
        }
    }

    return true;
}

#ifdef WIN32
void MakeJunction(const fs::path& link, const fs::path& target)
{
    BOOST_REQUIRE(::CreateDirectoryW(link.wstring().c_str(), nullptr));
    const HANDLE handle = ::CreateFileW(link.wstring().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    BOOST_REQUIRE(handle != INVALID_HANDLE_VALUE);

    // REPARSE_DATA_BUFFER's mount-point form, which no user-mode header declares:
    // the tag, the data length and a reserved word, then the offsets and lengths of
    // the two names, then the names, each followed by a NUL.
    const std::wstring substitute = L"\\??\\" + target.wstring();
    const std::wstring print = target.wstring();
    const size_t names = (substitute.size() + 1 + print.size() + 1) * sizeof(wchar_t);
    std::vector<unsigned char> buffer(8 + 8 + names, 0);
    const auto put16 = [&](size_t offset, size_t value) {
        const auto v = static_cast<uint16_t>(value);
        std::memcpy(buffer.data() + offset, &v, sizeof(v));
    };
    const uint32_t tag = IO_REPARSE_TAG_MOUNT_POINT;
    std::memcpy(buffer.data(), &tag, sizeof(tag));
    put16(4, 8 + names);
    put16(8, 0);
    put16(10, substitute.size() * sizeof(wchar_t));
    put16(12, (substitute.size() + 1) * sizeof(wchar_t));
    put16(14, print.size() * sizeof(wchar_t));
    std::memcpy(buffer.data() + 16, substitute.c_str(), (substitute.size() + 1) * sizeof(wchar_t));
    std::memcpy(buffer.data() + 16 + (substitute.size() + 1) * sizeof(wchar_t), print.c_str(),
                (print.size() + 1) * sizeof(wchar_t));

    DWORD returned = 0;
    const BOOL set = ::DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(),
                                       static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr);
    const DWORD error = ::GetLastError();
    ::CloseHandle(handle);
    BOOST_REQUIRE_MESSAGE(set, "FSCTL_SET_REPARSE_POINT failed with Windows error " << error);
}
#endif // WIN32

} // namespace grc_test
