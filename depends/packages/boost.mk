package=boost
$(package)_version=1.89.0
$(package)_download_path = https://github.com/boostorg/boost/releases/download/boost-$($(package)_version)
$(package)_file_name = boost-$($(package)_version)-cmake.tar.gz
$(package)_sha256_hash=954a01219bf818c7fb850fa610c2c8c71a4fa28fa32a1900056bcb6ff58cf908
$(package)_patches = skip_compiled_targets.patch
$(package)_build_subdir = build
$(package)_dependencies := zlib bzip2 xz

define $(package)_set_vars
  $(package)_config_opts = -DBOOST_INCLUDE_LIBRARIES="assign;multi_index;signals2;test;filesystem;system;thread;iostreams;asio;date_time;interprocess"
  $(package)_config_opts += -DBOOST_TEST_HEADERS_ONLY=OFF
  $(package)_config_opts += -DBOOST_ENABLE_MPI=OFF
  $(package)_config_opts += -DBOOST_ENABLE_PYTHON=OFF
  $(package)_config_opts += -DBOOST_INSTALL_LAYOUT=system
  $(package)_config_opts += -DBUILD_TESTING=OFF
  $(package)_config_opts += -DCMAKE_DISABLE_FIND_PACKAGE_ICU=ON
  $(package)_config_opts += -DCMAKE_DISABLE_FIND_PACKAGE_zstd=ON
  $(package)_config_opts += -DZLIB_INCLUDE_DIR=$(host_prefix)/include
  $(package)_config_opts += -DZLIB_LIBRARY=$(host_prefix)/lib/libz.a
  $(package)_config_opts += -DBZIP2_INCLUDE_DIR=$(host_prefix)/include
  $(package)_config_opts += -DBZIP2_LIBRARIES=$(host_prefix)/lib/libbz2.a
  $(package)_config_opts += -DLIBLZMA_INCLUDE_DIR=$(host_prefix)/include
  $(package)_config_opts += -DLIBLZMA_LIBRARY=$(host_prefix)/lib/liblzma.a
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/skip_compiled_targets.patch
endef

define $(package)_config_cmds
  $($(package)_cmake) -S .. -B . $($(package)_config_opts)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
