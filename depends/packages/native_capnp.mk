# Cap'n Proto version must match what the pinned libmultiprocess
# (native_libmultiprocess.mk / libmultiprocess.mk) was generated against, or the
# build fails with "Version mismatch between generated code and library headers".
# libmultiprocess 3f221b5 targets Cap'n Proto 1.5.0.
package=native_capnp
$(package)_version=1.5.0
$(package)_download_path=https://capnproto.org/
$(package)_download_file=capnproto-c++-$($(package)_version).tar.gz
$(package)_file_name=capnproto-cxx-$($(package)_version).tar.gz
$(package)_sha256_hash=77dbc13ca82d9c87ddb4581dd49559d45b63096433d3dadea08b7f31b360a5ba

define $(package)_set_vars
  $(package)_config_opts := -DBUILD_TESTING=OFF
  $(package)_config_opts += -DWITH_OPENSSL=OFF
  $(package)_config_opts += -DWITH_ZLIB=OFF
endef

define $(package)_config_cmds
  $($(package)_cmake) .
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf lib/pkgconfig
endef
