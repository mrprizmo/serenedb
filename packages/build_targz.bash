#!/bin/bash

source /serenedb/packages/find_version.bash

v="$SERENEDB_TGZ_UPSTREAM"
name=serenedb
arch=""
ARCH=$(uname -p)

case "$ARCH" in
  x86_64)
    arch="_$ARCH"
    ;;

  *)
    if [[ "$ARCH" =~ ^arm64$|^aarch64$ ]]; then
      arch="_arm64"
    else
      echo "fatal, unknown architecture $ARCH for TGZ"
      exit 1
    fi
    ;;
esac

suffix=""
pushd package_all
rm -rf targz
mkdir targz
pushd debian/$name
cp -a --parents usr/sbin/* ../../targz
popd
pushd debian/$name-client
cp -a --parents usr/bin/* ../../targz
popd
pushd install
cp -a --parents usr/share/* ../targz
popd
if [[ "$STRIP_TARBALL" = false ]]; then
  cp /serenedb/build_serenedb/bin/serened ../targz/usr/sbin/serened
fi
cd targz
cp -a /serenedb/packages/tarball/* .
find bin "(" -name "*.bak" -o -name "*~" ")" -delete
rm -rf "/serenedb/$name-$v$arch"
cp -a . "/serenedb/$name-$v$arch"
cd /serenedb
tar czvf "$name-linux-$v$arch.tar.gz" "$name-$v$arch"
rm -rf "$name-$v$arch"

#and prepareInstall $WORKDIR/work/targz
