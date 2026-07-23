package=libmultiprocess
# The cross-compiled libmultiprocess runtime library (linked into the target
# binaries). Bitcoin Core does not have this package -- it compiles the runtime
# from its in-tree subtree via add_subdirectory. Because Gridcoin has no vendored
# subtree, depends builds the runtime here for the target host. Reuse the pinned
# revision from native_libmultiprocess so the code generator and runtime match.
$(package)_version=$(native_$(package)_version)
$(package)_download_path=$(native_$(package)_download_path)
$(package)_download_file=$(native_$(package)_download_file)
$(package)_file_name=$(native_$(package)_file_name)
$(package)_sha256_hash=$(native_$(package)_sha256_hash)
$(package)_dependencies=capnp

define $(package)_config_cmds
  $($(package)_cmake) .
endef

define $(package)_build_cmds
  $(MAKE)
endef

# install-lib stages the static runtime library, its public headers and the
# Libmultiprocess CMake package (so find_package(Libmultiprocess) resolves against
# the depends prefix), without the native-only mpgen binary.
define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-lib
endef
