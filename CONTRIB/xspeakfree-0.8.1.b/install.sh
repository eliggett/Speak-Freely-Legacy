#!/bin/sh

INSTALL=$1
if [ "$INSTALL" = "" ]; then
	echo "syntax: install.sh <install path>"
	echo "	Where install path should be any directory"
	echo "	I recommend /usr/local/"
	exit
fi
set -x

mkdir $INSTALL/bin
mkdir $INSTALL/lib
mkdir $INSTALL/lib/xspeakfree

cp bin/* $INSTALL/bin
cp lib/xspeakfree/* $INSTALL/lib/xspeakfree
