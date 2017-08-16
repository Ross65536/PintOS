#! /bin/bash

if [ $# -ne 1 -o $1 == "-h" -o $1 == "--help" ]; then
  echo "Usage: $0 DSTDIR"
  exit 1
fi

CWD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
if [ ! -f $CWD/bochs-2.6.2-jitter-plus-segv.patch -a -f $CWD/bochs-2.6.2-xrandr-pkgconfig.patch -a -f $CWD/bochs-2.6.2-banner-stderr.patch -a -f $CWD/bochs-2.6.2-block-device-check.patch ]; then
  echo "Could not find the patch files for Bochs in $CWD."
  exit 1
fi

mkdir -p $1
if [ ! -d $1 ]; then
  echo "Could not find or create the destination directory"
  exit 1
fi
DSTDIR=$(cd $1 && pwd)

cd /tmp
mkdir $$
cd $$
wget https://sourceforge.net/projects/bochs/files/bochs/2.6.2/bochs-2.6.2.tar.gz/download -O bochs-2.6.2.tar.gz 
tar xzf bochs-2.6.2.tar.gz
cd bochs-2.6.2
cat $CWD/bochs-2.6.2-jitter-plus-segv.patch | patch -p1
cat $CWD/bochs-2.6.2-xrandr-pkgconfig.patch | patch -p1
cat $CWD/bochs-2.6.2-banner-stderr.patch | patch -p1
cat $CWD/bochs-2.6.2-block-device-check.patch | patch -p1
CFGOPTS="--with-x --with-x11 --with-term --with-nogui --prefix=$DSTDIR"
WD=$(pwd)
mkdir plain && cd plain
../configure $CFGOPTS --enable-gdb-stub && 
make -j8 && make install
cd $WD
mkdir with-dbg && cd with-dbg 
../configure --enable-debugger --disable-debugger-gui $CFGOPTS &&
make -j8
cp bochs $DSTDIR/bin/bochs-dbg 
cd $WD
