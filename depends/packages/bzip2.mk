package=bzip2
$(package)_version=1.0.8
$(package)_download_path=https://sourceware.org/pub/bzip2/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269
$(package)_patches=Makefile.patch

define $(package)_set_vars
  $(package)_build_opts= CC="$($(package)_cc)"
  $(package)_build_opts+=CFLAGS="$($(package)_cflags) $($(package)_cppflags) -fPIC"
  $(package)_build_opts+=RANLIB="$($(package)_ranlib)"
  $(package)_build_opts+=AR="$($(package)_ar)"
  # $(package)_build_opts_darwin+=AR="$($(package)_libtool)"
  # $(package)_build_opts_darwin+=ARFLAGS="-o"
  $(package)_cxxflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cxxflags_arm_linux = $(GCCFLAGS)
  $(package)_cflags_arm_linux = $(GCCFLAGS)
endef

define $(package)_preprocess_cmds
    patch -p0 < $($(package)_patch_dir)/Makefile.patch
endef

define $(package)_config_cmds
endef

define $(package)_build_cmds
  $($(package)_build_opts) $(MAKE) libbz2.a
endef

define $(package)_stage_cmds
  mkdir $($(package)_staging_prefix_dir)/include && \
  cp -f bzlib.h $($(package)_staging_prefix_dir)/include && \
  mkdir $($(package)_staging_prefix_dir)/lib && \
  cp -f libbz2.a $($(package)_staging_prefix_dir)/lib
endef
