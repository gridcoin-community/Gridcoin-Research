package=curl
$(package)_version=7.63.0
$(package)_download_path=https://curl.haxx.se/download/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=d483b89062832e211c887d7cf1b65c902d591b48c11fe7d174af781681580b41
$(package)_dependencies=openssl
define $(package)_set_vars
  $(package)_config_opts=--disable-shared
  $(package)_config_opts+= --enable-static
  $(package)_config_opts+= --enable-ssl
  $(package)_config_opts+= --with-ca-bundle=./TLS/cacert.pem
  $(package)_config_opts+= --with-ca-path=./TLS/certs
#  $(package)_config_opts+= --with-winssl
  $(package)_config_opts_release+=--disable-debug-mode
  $(package)_config_opts_linux+=--with-pic
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
