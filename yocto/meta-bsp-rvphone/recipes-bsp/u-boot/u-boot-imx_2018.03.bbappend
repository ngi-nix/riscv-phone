FILESEXTRAPATHS_prepend := "${THISDIR}/rvphone/cl-imx8:"

include rvphone/cl-imx8.inc

PACKAGE_ARCH = "${MACHINE_ARCH}"
COMPATIBLE_MACHINE = "(cl-imx8)"
