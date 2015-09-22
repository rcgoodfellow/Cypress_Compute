#!/usr/bin/env bash

VERSION="0.1"
TGT="CySim-${VERSION}"

mkdir -p ${TGT}/lib
mkdir -p ${TGT}/bin
cp /usr/local/lib/libCypress* ${TGT}/lib/
cp /usr/local/lib/libboost* ${TGT}/lib/
cp /usr/local/lib/libsundials* ${TGT}/lib/
cp /usr/local/lib/libmpi* ${TGT}/lib/
cp /usr/local/lib/libmpl* ${TGT}/lib/
cp /usr/local/lib/libopa* ${TGT}/lib/
cp /usr/local/lib/libxml* ${TGT}/lib/
cp /usr/local/lib/libyaml* ${TGT}/lib/

cp /usr/local/bin/cyc ${TGT}/bin/
cp /usr/local/bin/CPacketLauncher ${TGT}/bin/
cp /usr/local/bin/mpi* ${TGT}/bin/
cp /usr/local/bin/hydra* ${TGT}/bin/
cp /usr/local/bin/parkill ${TGT}/bin/
cp /usr/local/bin/sundials-config ${TGT}/bin/
cp /usr/local/bin/xml* ${TGT}/bin/

tar czf "${TGT}.tar.gz" "${TGT}"
rm -rf "${TGT}"
