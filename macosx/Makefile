################################################################################
#
# Simple makefile for building on Mac OS X with the
# Project Builder command line tool 'pbxbuild'
#
# RCS: @(#) $Id: Makefile,v 1.4 2002/09/26 17:06:14 das Exp $
#
################################################################################

INSTALL_ROOT	?=

BUILD_DIR	?= ${CURDIR}/../../build

TARGET		= Tcl

DEVBUILDSTYLE	= Development
DEPBUILDSTYLE	= Deployment

PBXBUILD	= /usr/bin/pbxbuild

BUILD		= ${PBXBUILD} SYMROOT="${BUILD_DIR}" -target "${TARGET}"

DEVBUILD	= ${BUILD} -buildstyle "${DEVBUILDSTYLE}"
DEPBUILD	= ${BUILD} -buildstyle "${DEPBUILDSTYLE}"

INSTALLOPTS	= INSTALL_ROOT="${INSTALL_ROOT}"

EMBEDDEDOPTS	= DYLIB_INSTALL_PATH="@executable_path/../Frameworks"

################################################################################

all: develop deploy

install: install-develop install-deploy

embedded: embedded-deploy

install-embedded: install-embedded-deploy

clean: clean-develop clean-deploy

################################################################################

develop:
	${DEVBUILD}

deploy:
	${DEPBUILD}

install-develop: 
	${DEVBUILD} install ${INSTALLOPTS}

install-deploy:
	${DEPBUILD} install ${INSTALLOPTS}

embedded-develop: forceRelink
	${DEVBUILD} ${EMBEDDEDOPTS}

embedded-deploy: forceRelink
	${DEPBUILD} ${EMBEDDEDOPTS}

install-embedded-develop: 
	${DEVBUILD} install ${INSTALLOPTS} ${EMBEDDEDOPTS}

install-embedded-deploy:
	${DEPBUILD} install ${INSTALLOPTS} ${EMBEDDEDOPTS}

clean-develop:
	${DEVBUILD} clean

clean-deploy:
	${DEPBUILD} clean

################################################################################

forceRelink:
	@-cd ${BUILD_DIR}; \
	rm -rf Tcl.framework tclsh8.4 \
	Development.build/Tcl.build/Tcl Deployment.build/Tcl.build/Tcl

################################################################################

.PHONY: all install embedded clean develop deploy install-develop install-deploy \
embedded-develop embedded-deploy install-embedded-develop install-embedded-deploy \
clean-develop clean-deploy forceRelink \

################################################################################
