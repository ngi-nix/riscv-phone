FILESEXTRAPATHS_prepend := "${THISDIR}/rvphone/cl-imx8:"

include rvphone/cl-imx8.inc

do_configure_append () {
    oe_runmake ${MACHINE}_defconfig
}

# KERNEL_MODULE_AUTOLOAD += "goodix"

COMPATIBLE_MACHINE = "(cl-imx8)"
