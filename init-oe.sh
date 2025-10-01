#!/bin/sh


BUILDDIR=build

rm -rf ${BUILDDIR}/conf
if [ ! -d ${BUILDDIR}/conf ]
then
	mkdir -p ${BUILDDIR}/conf
fi

# Remove existing links, if any
rm -f ${BUILDDIR}/conf/bblayers.conf ${BUILDDIR}/conf/buildsetup.conf ${BUILDDIR}/profile ${BUILDDIR}/conf/local.conf ${BUILDDIR}/conf/machine.conf
# Make symlinks so they are under version control
ln -s $PWD/conf/bblayers.conf ${BUILDDIR}/conf/bblayers.conf
ln -s $PWD/conf/local.conf ${BUILDDIR}/conf/local.conf

# Update the submodules if needed
#git submodule update --recursive

cd poky
source ./oe-init-build-env ../${BUILDDIR}
