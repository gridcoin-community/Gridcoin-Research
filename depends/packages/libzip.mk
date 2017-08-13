package=libzip
$(package)_version=1.2.0
$(package)_download_path=https://nih.at/$(package)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=6cf9840e427db96ebf3936665430bab204c9ebbd0120c326459077ed9c907d9f


define $(package)_set_vars
  $(package)_config_opts_release=--disable-debug-mode
  $(package)_config_opts_linux=--with-pic
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
endef
