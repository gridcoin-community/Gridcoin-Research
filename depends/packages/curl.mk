package=curl
GCCFLAGS?=
$(package)_version=7.63.0
$(package)_download_path=https://curl.haxx.se/download/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=d483b89062832e211c887d7cf1b65c902d591b48c11fe7d174af781681580b41
$(package)_dependencies=openssl

define $(package)_set_vars
  $(package)_config_opts=--disable-shared
  $(package)_config_opts+= --enable-static
  $(package)_config_opts_release+=--disable-debug-mode
  $(package)_config_opts_linux+=--with-pic
  # Disable OpenSSL for Windows and use native SSL stack (SSPI/Schannel):
  $(package)_config_opts_mingw32+= --with-winssl --without-ssl
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
