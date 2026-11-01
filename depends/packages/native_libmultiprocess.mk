package=native_libmultiprocess
# Build the native mpgen code generator from the vendored in-tree subtree
# (src/ipc/libmultiprocess), the same way Bitcoin Core does. The runtime library
# itself is NOT a depends package: it is compiled in-tree via add_subdirectory
# (see cmake/libmultiprocess.cmake), so depends only needs to provide the native
# generator for cross builds.
$(package)_local_dir=../src/ipc/libmultiprocess
$(package)_dependencies=native_capnp

define $(package)_config_cmds
  $($(package)_cmake) .
endef

define $(package)_build_cmds
  $(MAKE)
endef

# install-bin stages only the native code generator (mpgen) and its shared
# schema/header bits -- this package exists to provide the build machine's mpgen.
define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-bin
endef
