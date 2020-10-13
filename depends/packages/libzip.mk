package=libzip
$(package)_version=1.3.2
$(package)_download_path=https://libzip.org/download/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=ab4c34eb6c3a08b678cd0f2450a6c57a13e9618b1ba34ee45d00eb5327316457
$(package)_dependencies=zlib bzip2
$(package)_patches=nonrandomopentest.c.patch


define $(package)_set_vars
  $(package)_build_opts= CC="$($(package)_cc)"
  $(package)_build_opts+=CFLAGS="$($(package)_cflags) $($(package)_cppflags) -fPIC"
  $(package)_build_opts+=RANLIB="$($(package)_ranlib)"
  $(package)_build_opts+=AR="$($(package)_ar)"
  $(package)_cxxflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cxxflags_arm_linux = $(GCCFLAGS)
  $(package)_cflags_arm_linux = $(GCCFLAGS)
endef

ifeq ($(host),i686-pc-linux-gnu)
  i686_cflag="$($(package)_cflags) $($(package)_cppflags) -fPIC -m32"
else
  i686_cflag="$($(package)_cflags) $($(package)_cppflags) -fPIC"
endif


define $(package)_preprocess_cmds
  sed -i.old 's/\#  ifdef _WIN32/\#  if defined _WIN32 \&\& defined ZIP_DLL/' lib/zip.h && \
  patch -p1 < $($(package)_patch_dir)/nonrandomopentest.c.patch
endef

define $(package)_config_cmds
  $($(package)_build_opts) CFLAGS=$(i686_cflag)  ./configure --host=$(host) \
  --prefix=$(host_prefix) --with-zlib=$(host_prefix) --with-bzip2=$(host_prefix) \
  --with-pic --enable-static --enable-shared=no  --libdir=$($($(package)_type)_prefix)/lib
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
