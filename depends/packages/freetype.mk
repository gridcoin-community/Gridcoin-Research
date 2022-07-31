package=freetype
GCCFLAGS?=
$(package)_version=2.11.0
$(package)_download_path=https://download.savannah.gnu.org/releases/$(package)
$(package)_file_name=$(package)-$($(package)_version).tar.xz
$(package)_sha256_hash=8bee39bd3968c4804b70614a0a3ad597299ad0e824bc8aad5ce8aaf48067bde7

define $(package)_set_vars
  $(package)_config_opts=--without-zlib --without-png --without-harfbuzz --without-bzip2 --disable-static --without-brotli
  $(package)_config_opts+=--libdir=$($($(package)_type)_prefix)/lib
  $(package)_config_opts_linux=--with-pic
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
