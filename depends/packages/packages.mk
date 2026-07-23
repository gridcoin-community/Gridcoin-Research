packages:=boost openssl curl zlib bzip2 xz
native_packages := native_ccache native_xxd

qt_packages = qrencode

qt_linux_packages:=qt expat libxcb xcb_proto libXau libXdmcp xproto freetype fontconfig libxkbcommon libxcb_util libxcb_util_cursor libxcb_util_render libxcb_util_keysyms libxcb_util_image libxcb_util_wm util-macros xkeyboard-config libevent libpng harfbuzz pcre2
qt_darwin_packages=qt
qt_mingw32_packages=qt
ifneq ($(host),$(build))
qt_native_packages := native_qt
endif

upnp_packages=miniupnpc

# Multiprocess (IPC) toolchain. The cross-compiled Cap'n Proto runtime plus the
# native code generators (capnp / capnpc-c++ and mpgen). The libmultiprocess
# runtime is NOT a depends package -- it is compiled in-tree from the vendored
# subtree (src/ipc/libmultiprocess) via add_subdirectory; depends only supplies
# the native mpgen (native_libmultiprocess, built from that same subtree via
# local_dir). Added only when MULTIPROCESS=1 is passed to depends (see the
# Makefile), so the default depends build is unchanged.
multiprocess_packages = capnp
multiprocess_native_packages = native_capnp native_libmultiprocess

darwin_native_packages = native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_libdmg-hfsplus
endif
