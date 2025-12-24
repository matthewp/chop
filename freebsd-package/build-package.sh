#!/bin/sh

set -e

VERSION=${VERSION:-"0.0.0"}

echo "Building FreeBSD package for version $VERSION"

# Create staging directory
STAGEDIR=$(pwd)/stage
rm -rf $STAGEDIR
mkdir -p $STAGEDIR/usr/local/bin
mkdir -p $STAGEDIR/usr/local/share/man/man1

# Copy binary to staging
cp release/chop-freebsd-amd64 $STAGEDIR/usr/local/bin/chop
chmod +x $STAGEDIR/usr/local/bin/chop

# Copy and compress man page
gzip -c chop.1 > $STAGEDIR/usr/local/share/man/man1/chop.1.gz

# Create manifest with version
sed "s/%%VERSION%%/$VERSION/g" freebsd-package/pkg-manifest.ucl > pkg-manifest.ucl

# Create repository structure
rm -rf repo
mkdir -p repo/All

# Create the package
pkg create -M pkg-manifest.ucl -p freebsd-package/pkg-plist -r $STAGEDIR -o repo/All/

# Generate repository metadata
cd repo
pkg repo .
cd ..

echo "Package created in repo/All/"
echo "Repository metadata created in repo/"
