packages:=boost openssl curl zlib bzip2 libzip
native_packages := native_ccache

qt_packages = qrencode

ifeq ($(QT_59),1)
qt_linux_packages:=qt59 expat dbus libxcb xcb_proto libXau xproto freetype fontconfig libX11 xextproto libXext xtrans libzip
qt_darwin_packages=qt59 libzip
qt_mingw32_packages=qt59 libzip
else
qt_linux_packages:=qt expat dbus libxcb xcb_proto libXau xproto freetype fontconfig libX11 xextproto libXext xtrans libzip
qt_darwin_packages=qt libzip
qt_mingw32_packages=qt libzip
endif

ifeq ($(BDB_53),1)
wallet_packages=bdb53
else
wallet_packages=bdb
endif

upnp_packages=miniupnpc

darwin_native_packages = native_biplist native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_cdrkit native_libdmg-hfsplus
endif
