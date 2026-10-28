package=curl
GCCFLAGS?=
$(package)_version=8.23.0
$(package)_download_path=https://curl.se/download/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49
$(package)_dependencies=openssl

define $(package)_set_vars
  $(package)_config_opts=--disable-shared
  $(package)_config_opts+= --enable-static
  $(package)_config_opts += --disable-ldap --disable-ldaps
  $(package)_config_opts += --without-libidn2 --without-libpsl
  $(package)_config_opts += --without-zstd --without-brotli
  $(package)_config_opts+= --without-nghttp2
  # curl 8 builds WebSockets and MQTTS by default; 7.88.1 had neither.
  # Gridcoin fetches only over HTTP(S), so leave them out.
  $(package)_config_opts+= --disable-websockets --disable-mqtt
  $(package)_config_opts+= --libdir=$($($(package)_type)_prefix)/lib
  $(package)_config_opts_release+=--disable-debug
  # Where curl uses OpenSSL it asks OpenSSL to load openssl.cnf, which
  # no-autoload-config in openssl.mk does not stop. Do not load it here either.
  $(package)_config_opts_linux+=--with-pic --with-openssl --disable-openssl-auto-load-config
  # Disable OpenSSL for Windows and use native SSL stack (SSPI/Schannel):
  $(package)_config_opts_mingw32+= --with-schannel
  # curl 8.15 removed Secure Transport. Use OpenSSL on macOS, and point it at the
  # system CA bundle, since OpenSSL does not read the Keychain.
  $(package)_config_opts_darwin+= --with-openssl --with-ca-bundle=/etc/ssl/cert.pem --disable-openssl-auto-load-config
  # This extra flag for macOS is necessary as curl will append a -mmacosx-version-min=10.8 otherwise
  # which will cause the linker to fail as it cannot optimize away a __builtin_available(MacOS 10.11...) call
  # which requires a link to compiler runtime library.
  $(package)_cflags_darwin=-mmacosx-version-min=$(OSX_MIN_VERSION)
  $(package)_cxxflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cxxflags_arm_linux = $(GCCFLAGS)
  $(package)_cflags_arm_linux = $(GCCFLAGS)
  $(package)_cppflags_mingw32 += -DHAVE_IOCTLSOCKET -DHAVE_IOCTLSOCKET_FIONBIO
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
