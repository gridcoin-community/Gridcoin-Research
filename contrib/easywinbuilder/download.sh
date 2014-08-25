echo Creating lib directory...
echo Downloading tools...
mingw-get install msys-wget-bin # > /dev/null 2>&1
mingw-get install msys-unzip-bin #> /dev/null 2>&1
mingw-get install msys-perl # > /dev/null 2>&1
# There is a problem with MSYS bash and sh that stalls OpenSSL config on some systems. Using rxvt shell as a workaround.
mingw-get install msys-rxvt # > /dev/null 2>&1
echo

set -o errexit

cd $ROOTPATHSH
if [ ! -d $EWBLIBS ]; then
    mkdir $EWBLIBS
fi

# echo Downloading source...
# wget --no-check-certificate -N "https://github.com/phelixbtc/namecoin-qt/archive/easywinbuilder.tar.gz" -O "source.tar.gz"
# echo

echo Downloading dependencies...
cd libs
wget -N "http://www.openssl.org/source/$OPENSSL.tar.gz"
wget -N "http://download.oracle.com/berkeley-db/$BERKELEYDB.tar.gz"
wget -N "http://downloads.sourceforge.net/project/boost/boost/$BOOSTVERSION/$BOOST.tar.gz"
wget -N "http://miniupnp.tuxfamily.org/files/download.php?file=$MINIUPNPC.tar.gz"
echo
