package=bdb53
GCCFLAGS?=
$(package)_version=5.3.28
$(package)_download_path=https://download.oracle.com/berkeley-db
$(package)_file_name=db-$($(package)_version).NC.tar.gz
$(package)_sha256_hash=76a25560d9e52a198d37a31440fd07632b5f1f8f9f2b6d5438f4bc3e7c9013ef
$(package)_build_subdir=build_unix

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --enable-cxx --disable-replication
  $(package)_config_opts+=--libdir=$($($(package)_type)_prefix)/lib
  $(package)_config_opts_mingw32=--enable-mingw
  $(package)_config_opts_linux=--with-pic
  $(package)_cflags+=-Wno-error=implicit-function-declaration
  $(package)_cxxflags=-std=c++17
  $(package)_cxxflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cxxflags_arm_linux = $(GCCFLAGS)
  $(package)_cflags_arm_linux = $(GCCFLAGS)
endef

define $(package)_preprocess_cmds
  sed -i.old 's/__atomic_compare_exchange/__atomic_compare_exchange_db/' src/dbinc/atomic.h && \
  sed -i.old 's/atomic_init/atomic_init_db/' src/dbinc/atomic.h src/mp/mp_region.c src/mp/mp_mvcc.c src/mp/mp_fget.c src/mutex/mut_method.c src/mutex/mut_tas.c && \
  sed -i.old "s/WinIoCtl.h/winioctl.h/g" src/dbinc/win_db.h && \
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub dist
endef

define $(package)_config_cmds
  ../dist/$($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) libdb_cxx-5.3.a libdb-5.3.a
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install_lib install_include
endef
