package=qrencode
GCCFLAGS?=
$(package)_version=3.4.4
$(package)_download_path=https://fukuchi.org/works/qrencode/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=efe5188b1ddbcbf98763b819b146be6a90481aac30cfc8d858ab78a19cde1fa5

define $(package)_set_vars
  $(package)_config_opts=--disable-shared -without-tools --disable-sdltest --libdir="$($($(package)_type)_prefix)/lib"
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
