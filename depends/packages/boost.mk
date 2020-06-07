package=boost
GCCFLAGS?=
$(package)_version=1_73_0
$(package)_download_path=https://downloads.sourceforge.net/project/boost/boost/1.73.0/
$(package)_file_name=$(package)_$($(package)_version).tar.bz2
$(package)_sha256_hash=4eb3b8d442b426dc35346235c8733b5ae35ba431690e38c6a8263dce9fcbb402
$(package)_dependencies=zlib

define $(package)_set_vars
  $(package)_config_opts_release=variant=release
  $(package)_config_opts_debug=variant=debug
  $(package)_dependencies=zlib
  $(package)_config_opts=--layout=tagged --build-type=complete --user-config=user-config.jam
  $(package)_config_opts+=threading=multi link=static -sNO_BZIP2=1 -sZLIB_SOURCE=$(host_prefix)/lib/zlib_source -sZLIB_INCLUDE=$(host_prefix)/include -sZLIB_LIBPATH=$(host_prefix)/lib
  $(package)_config_opts_linux=threadapi=pthread runtime-link=shared
  $(package)_config_opts_darwin=--toolset=darwin-4.2.1 runtime-link=shared
  $(package)_config_opts_mingw32=binary-format=pe target-os=windows threadapi=win32 runtime-link=static
  $(package)_config_opts_x86_64_mingw32=address-model=64
  $(package)_config_opts_i686_mingw32=address-model=32
  $(package)_config_opts_i686_linux=address-model=32 architecture=x86
  $(package)_toolset_$(host_os)=gcc
  $(package)_archiver_$(host_os)=$($(package)_ar)
  $(package)_toolset_darwin=darwin
  $(package)_archiver_darwin=$($(package)_libtool)
  $(package)_config_libraries=chrono,filesystem,program_options,system,thread,test,iostreams
  $(package)_cxxflags=-std=c++11 -fvisibility=hidden
  $(package)_cxxflags_linux=-fPIC
  $(package)_cxxflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cflags_aarch64_linux = $(GCCFLAGS)
  $(package)_cxxflags_arm_linux = $(GCCFLAGS)
  $(package)_cflags_arm_linux = $(GCCFLAGS)
endef

define $(package)_preprocess_cmds
  echo "using $(boost_toolset_$(host_os)) : : $($(package)_cxx) : <cxxflags>\"$($(package)_cxxflags) $($(package)_cppflags)\" <linkflags>\"$($(package)_ldflags)\" <archiver>\"$(boost_archiver_$(host_os))\" <striper>\"$(host_STRIP)\"  <ranlib>\"$(host_RANLIB)\" <rc>\"$(host_WINDRES)\" : ;" > user-config.jam
endef

define $(package)_config_cmds
  ./bootstrap.sh --without-icu --with-libraries=$(boost_config_libraries)
endef

define $(package)_build_cmds
  ./b2 -d2 -j2 -d1 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) stage
endef

define $(package)_stage_cmds
  ./b2 -d0 -j4 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) install
endef
