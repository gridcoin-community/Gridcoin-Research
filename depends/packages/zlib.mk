package=zlib
GCCFLAGS?=
$(package)_version=1.2.11
$(package)_download_path=http://www.zlib.net
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1

define $(package)_set_vars
  $(package)_build_opts= CC="$($(package)_cc)"
  $(package)_build_opts+=CFLAGS="$($(package)_cflags) $($(package)_cppflags) -fPIC"
  $(package)_build_opts+=RANLIB="$($(package)_ranlib)"
  $(package)_build_opts+=AR="$($(package)_ar)"
  $(package)_build_opts_darwin+=AR="$($(package)_libtool)"
  $(package)_build_opts_darwin+=ARFLAGS="-o"
  $(package)_cxxflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cxxflags_arm_linux = $(GCCFLAGS)
  $(package)_cflags_arm_linux = $(GCCFLAGS)
endef

define $(package)_config_cmds
  ./configure --static --prefix=$(host_prefix)
endef

define $(package)_build_cmds
  $(MAKE) $($(package)_build_opts) all
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install $($(package)_build_opts) &&\
  mkdir $($(package)_staging_dir)/$(host_prefix)/lib/zlib_source &&\
  cp -r * $($(package)_staging_dir)/$(host_prefix)/lib/zlib_source
endef
