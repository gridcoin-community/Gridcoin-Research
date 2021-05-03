set -e

./ci/test_run_all.sh
echo $(ls ./depends/i686-w64-mingw32)
#for f in ci/scratch/releases/*.*; do
#    sha256sum $f > $f.SHA256
#done
