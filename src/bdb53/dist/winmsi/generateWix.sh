#
#
# Usage: generateWix.sh <path_to_queryFile>
#
# Bash/shell script to generate a WiX .wxs file suitable for
# compilation using candle.exe
#
# Requirements:
#  python -- version 2.5 or higher needs to be in PATH
#  xqilla -- command line tool passed as $2 (uses the just-built version)
#

tmp_outfile="wixComponents.xml"
tscriptDir=$1
stage=stage
scriptDir=scripts
XQILLA=$2
outfile=$scriptDir/db_components.wxs
queryFile=$scriptDir/generateGroups.xq
envQueryFile=$scriptDir/generateEnv.xq
pruneFile=$scriptDir/pruneComponents.xq
pyScript=$scriptDir/genWix.py
envFile=wixEnv.xml
compare=cmp

if [ ! -f $tscriptDir/generateGroups.xq  -o ! -f $tscriptDir/genWix.py ]; then
    echo "Usage: generateWix.sh <path_to_winmsi>"
    exit 1
fi

# copy necessary files to stage
local_here=`pwd`
mkdir $stage/$scriptDir 2>/dev/null
for i in generateGroups.xq generateEnv.xq pruneComponents.xq genWix.py wixEnv.xml; do
    cp $tscriptDir/$i $stage/$scriptDir
done

cd $stage

echo "Generating $tmp_outfile"
if [ -f $outfile ]; then
    mv $outfile $outfile.save
fi

echo "python $pyScript $tmp_outfile"

python $pyScript $tmp_outfile

if [ ! -f $tmp_outfile ]; then
    echo "Failed to build $tmp_outfile"
    exit 1
fi

# XQilla expects file in same directory
cp $tmp_outfile $scriptDir/$tmp_outfile


# Prune off empty Component elements
$XQILLA -u -v "inFile" $tmp_outfile $pruneFile

# Create group references
#echo "$XQILLA -u -v 'inFile' $tmp_outfile $queryFile"
$XQILLA -u -v "inFile" $tmp_outfile $queryFile

# Create environment for WiX
#echo "$XQILLA -u -v 'inFile' $tmp_outfile -v 'envFile' $envFile $envQueryFile"
$XQILLA -u -v "inFile" $tmp_outfile -v "envFile" $envFile $envQueryFile


# XQilla puts an XML decl in updated files, using version 1.1
# and the candle.exe WiX compiler doesn't handle 1.1.  Also
# WiX 3.0 requires the namespace -- it is easier to change it here
# than modify the various scripts to use a default namespace.
#echo "Changing XML version to 1.1 and adding required namespace for Wix element"
sed -e's!version="1.1"!version="1.0"!g' -e's!<Wix>!<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">!g' $scriptDir/$tmp_outfile > $outfile

rm $scriptDir/$tmp_outfile

echo "Generated $outfile"
cd $local_here


