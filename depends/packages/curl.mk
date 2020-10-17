package=curl
GCCFLAGS?=
$(package)_version=7.71.1
$(package)_download_path=https://curl.haxx.se/download/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=59ef1f73070de67b87032c72ee6037cedae71dcb1d7ef2d7f59487704aec069d
$(package)_dependencies=openssl

define $(package)_set_vars
  $(package)_config_opts=--disable-shared
  $(package)_config_opts+= --enable-static
  $(package)_config_opts+= --without-brotli
  $(package)_config_opts+= --libdir=$($($(package)_type)_prefix)/lib
  $(package)_config_opts_release+=--disable-debug-mode
  $(package)_config_opts_linux+=--with-pic
  # Disable OpenSSL for Windows and use native SSL stack (SSPI/Schannel):
  $(package)_config_opts_mingw32+= --with-winssl --without-ssl
  # This extra flag for macOS is necessary as curl will append a -mmacosx-version-min=10.8 otherwise
  # which will cause the linker to fail as it cannot optimize away a __builtin_available(MacOS 10.11...) call
  # which requires a link to compiler runtime library.
  $(package)_cflags_darwin=-mmacosx-version-min=$(OSX_MIN_VERSION)
  $(package)_cxxflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cxxflags_arm_linux = $(GCCFLAGS)
  $(package)_cflags_arm_linux = $(GCCFLAGS)
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
