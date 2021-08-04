#!/usr/bin/env bash
#
# Copyright (c) 2018-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

GRIDCOIN_CONFIG_ALL="--disable-dependency-tracking --prefix=$DEPENDS_DIR/$HOST --bindir=$BASE_OUTDIR/bin --libdir=$BASE_OUTDIR/lib"
DOCKER_EXEC "ccache --zero-stats --max-size=$CCACHE_SIZE"

DOCKER_EXEC mkdir -p "${BASE_BUILD_DIR}/gridcoin-$HOST"
export P_CI_DIR="${BASE_BUILD_DIR}/gridcoin-$HOST"

BEGIN_FOLD autogen
if [ -n "$CONFIG_SHELL" ]; then
  DOCKER_EXEC "$CONFIG_SHELL" -c "{BASE_ROOT_DIR}/autogen.sh"
else
  DOCKER_EXEC ${BASE_ROOT_DIR}/autogen.sh
fi
END_FOLD

#export P_CI_DIR="${BASE_BUILD_DIR}"

#BEGIN_FOLD configure
#DOCKER_EXEC "${BASE_ROOT_DIR}/configure" --cache-file=config.cache $GRIDCOIN_CONFIG_ALL $GRIDCOIN_CONFIG || ( (DOCKER_EXEC cat config.log) && false)
#END_FOLD

#BEGIN_FOLD distdir
#DOCKER_EXEC make distdir VERSION=$HOST
#END_FOLD


BEGIN_FOLD configure
DOCKER_EXEC ../configure $GRIDCOIN_CONFIG_ALL $GRIDCOIN_CONFIG || ( (DOCKER_EXEC cat config.log) && false)
END_FOLD

BEGIN_FOLD build
DOCKER_EXEC make $MAKEJOBS deploy || ( echo "Build failure. Verbose build follows." && DOCKER_EXEC make $GOAL V=1 ; false )
END_FOLD

BEGIN_FOLD cache_stats
DOCKER_EXEC "ccache --version | head -n 1 && ccache --show-stats"
DOCKER_EXEC du -sh "${DEPENDS_DIR}"/*/
END_FOLD
