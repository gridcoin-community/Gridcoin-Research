if [ ! -f $1 ]; then
    echo ERROR: File $1 not found.
    exit
fi

echo File hash sha256:
shasum -a 256 -b $1
echo
echo Hash sha256 of disassembly [may take a while]:
objdump -d $1 | shasum -a 256
echo
