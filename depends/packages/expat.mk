package=expat
GCCFLAGS?=
$(package)_version=2.2.7
$(package)_download_path=https://github.com/libexpat/libexpat/releases/download/R_2_2_7/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=cbc9102f4a31a8dafd42d642e9a3aa31e79a0aedaa1f6efd2795ebc83174ec18

define $(package)_set_vars
  $(package)_config_opts=--disable-static --without-docbook
  $(package)_config_opts+=--libdir=$($($(package)_type)_prefix)/lib
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
