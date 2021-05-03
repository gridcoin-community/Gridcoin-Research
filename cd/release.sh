set -e

./ci/test_run_all.sh
ls ci/scratch
#for f in ci/scratch/releases/*.*; do
#    sha256sum $f > $f.SHA256
#done
