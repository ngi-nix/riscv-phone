DESCRIPTION = "esp32 spi daemon"
SECTION = "base"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = file://${WORKDIR}/src/LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"
PR = "r0"

SRC_URI = " \
	file://src/ \
"

do_compile () {
	cd ${WORKDIR}/src
	${MAKE}
}

do_install () {
	install -d ${D}${bindir}
	install -m 0755 ${WORKDIR}/src/esp32spid ${D}${bindir}/
}

DEPENDS = "libgpiod-dev"
RDEPENDS_${PN} = "libgpiod"