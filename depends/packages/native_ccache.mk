package=native_ccache
$(package)_version=3.4.3
$(package)_download_path=https://samba.org/ftp/ccache
$(package)_file_name=ccache-$($(package)_version).tar.bz2
$(package)_sha256_hash=38decec3eed6524a002ffae52b25781843553c28e2d840e7e55e6831c40264d2

define $(package)_set_vars
  $(package)_config_opts=
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
  rm -rf lib include
endef
