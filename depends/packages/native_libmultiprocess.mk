package=native_libmultiprocess
# NOTE: Bitcoin Core builds libmultiprocess from an in-tree git subtree
# (depends uses $(package)_local_dir=../src/ipc/libmultiprocess). Gridcoin does
# not (yet) vendor that subtree, so we pin and download a specific upstream
# commit instead. Keep this commit in sync with libmultiprocess.mk (the
# cross-compiled runtime) -- both must be the same revision.
$(package)_version=3f221b5bfd7ee0e7972e3c5ed4bb7ee86e457f6d
$(package)_download_path=https://github.com/bitcoin-core/libmultiprocess/archive
$(package)_download_file=$($(package)_version).tar.gz
$(package)_file_name=libmultiprocess-$($(package)_version).tar.gz
$(package)_sha256_hash=1914a8aca106f787968f8efa0492ee581646d90610e3c8fc3d6292492a30cad5
$(package)_dependencies=native_capnp

define $(package)_config_cmds
  $($(package)_cmake) .
endef

define $(package)_build_cmds
  $(MAKE)
endef

# install-bin stages only the native code generator (mpgen) and the shared
# schema/header bits it needs -- this package exists to provide the build
# machine's mpgen, not a runtime library.
define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-bin
endef
