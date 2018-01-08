set -o errexit

if [ -d "${ROOTPATHSH}/src/leveldb" ]; then
    echo leveldb...
    cd $ROOTPATHSH/src/leveldb
    make memenv_test TARGET_OS=OS_WINDOWS_CROSSCOMPILE \
     OPT="${ADDITIONALCCFLAGS}"
    echo
    cd ../../$EWBPATH
fi

cd $ROOTPATHSH/$EWBLIBS

echo libpng..
cd ${LIBPNG}
make -j2 -f scripts/makefile.gcc
cd ..
echo

echo qrencode..
cd ${QRENCODE}
png_CFLAGS="-I${LIBPNG}" \
png_LIBS="-L${LIBPNG}/.libs -lpng" \
./configure --enable-static --disable-shared --without-tools
make -j2
cd ..
echo

echo db...
cd $BERKELEYDB/build_unix
../dist/configure --disable-replication --enable-mingw --enable-cxx \
 CXXFLAGS="${ADDITIONALCCFLAGS}" \
 CFLAGS="${ADDITIONALCCFLAGS}"
sed -i 's/typedef pthread_t db_threadid_t;/typedef u_int32_t db_threadid_t;/g' db.h  # workaround, see https://bitcointalk.org/index.php?topic=45507.0
make -j2
cd ../..
echo

echo  openssl...
cd $OPENSSL
export CC="gcc ${ADDITIONALCCFLAGS}"
./Configure mingw

make -j2
cd ..
echo

cd ../$EWBPATH
