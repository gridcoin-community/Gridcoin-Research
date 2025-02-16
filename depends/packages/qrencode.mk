package=qrencode
GCCFLAGS?=
$(package)_version=4.1.1
$(package)_download_path=https://fukuchi.org/works/qrencode/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=e455d9732f8041cf5b9c388e345a641fd15707860f928e94507b1961256a6923

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --without-tools --without-tests --disable-sdltest --libdir="$($($(package)_type)_prefix)/lib"
  $(package)_config_opts_linux=--with-pic
  $(package)_cxxflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cxxflags_arm_linux = $(GCCFLAGS)
  $(package)_cflags_arm_linux = $(GCCFLAGS)
endef

define $(package)_config_cmds
  autoreconf -fi && \
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
