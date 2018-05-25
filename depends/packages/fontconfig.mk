package=fontconfig
$(package)_version=2.12.1
$(package)_download_path=http://www.freedesktop.org/software/fontconfig/release/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=b449a3e10c47e1d1c7a6ec6e2016cca73d3bd68fbbd4f0ae5cc6b573f7d6c7f3
$(package)_dependencies=freetype expat
$(package)_patches=0001-Avoid-conflicts-with-integer-width-macros-from-TS-18.patch

define $(package)_set_vars
  $(package)_config_opts=--disable-docs --disable-static
endef

define $(package)_config_cmds
  patch -p1 < $($(package)_patch_dir)/0001-Avoid-conflicts-with-integer-width-macros-from-TS-18.patch &&\
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
