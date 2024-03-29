package=xcb_proto
GCCFLAGS?=
$(package)_version=1.14.1
$(package)_download_path=https://xorg.freedesktop.org/archive/individual/proto
$(package)_file_name=xcb-proto-$($(package)_version).tar.xz
$(package)_sha256_hash=f04add9a972ac334ea11d9d7eb4fc7f8883835da3e4859c9afa971efdf57fcc3

define $(package)_set_vars
  $(package)_config_opts +=--libdir=$($($(package)_type)_prefix)/lib
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

define $(package)_postprocess_cmds
  rm -rf lib/python*/site-packages/xcbgen/__pycache__
endef
