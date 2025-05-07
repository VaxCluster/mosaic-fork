#!/bin/sh

echo "Installing dependencies..."
dnf install -y epel-release
#dnf config-manager --set-enabled ol8_codeready_builder
dnf groupinstall -y 'Development Tools'
dnf install libXt-devel libX11-devel libXmu-devel libXpm-devel motif-devel clang

echo "Building Mosaic..."
echo "Using compiler: $CC"

cd /share
CC=$CC make linux
