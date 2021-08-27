DESCRIPTION = "esp32 spi daemon"
SECTION = "base"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${WORKDIR}/LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"
PR = "r0"

FILESEXTRAPATHS_prepend := "${THISDIR}:"

SRC_URI = " \
	file://src/ \
	file://LICENSE \
"

S = "${WORKDIR}/src"
TARGET_CC_ARCH += "${LDFLAGS}"

do_compile () {
	cd ${S}
	${MAKE}
}

do_install () {
	install -d ${D}${bindir}
	install -m 0755 ${S}/esp32spid ${D}${bindir}/
}

DEPENDS = "libgpiod"
RDEPENDS_${PN} = "libgpiod"