SRC_URI_append_cl-imx8 = " \
	file://0000-add-board-support.patch \
	file://0001-imx-hdp-video-modes.patch \
	file://0002-ov5640_mipi_v2-gpio-can-sleep.patch \
	file://0003-sn65dsi83-bridge-driver.patch \
	file://cl-imx8_defconfig \
	file://defconfig \
"

do_patch_copy () {
    cp -a ${WORKDIR}/dts ${S}/arch/arm64/boot/dts/rvphone
    cp -a ${WORKDIR}/sn65dsi83 ${S}/drivers/gpu/drm/bridge/
}

addtask patch_copy after do_patch before do_copy_defconfig
