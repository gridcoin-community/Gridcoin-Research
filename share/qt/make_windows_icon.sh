#!/usr/bin/env bash
# create multiresolution windows icon
export LC_ALL=C
ICON_DST=../../src/qt/res/icons/novacoin.ico

convert ../../src/qt/res/icons/novacoin-16.png ../../src/qt/res/icons/novacoin-32.png ../../src/qt/res/icons/novacoin-48.png ${ICON_DST}
