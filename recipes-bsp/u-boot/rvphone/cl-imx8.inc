SRC_URI_append_cl-imx8 = " \
	file://0000-add-board-support.patch \
	file://dts/ \
	file://board/ \
	file://configs/ \
	file://include/ \
"

do_patch_copy () {
    mkdir -p ${S}/board/rvphone
    cp -a ${WORKDIR}/dts/* ${S}/arch/arm/dts/
    cp -a ${WORKDIR}/board/rvphone/cl-imx8 ${S}/board/rvphone/
    cp -a ${WORKDIR}/configs/* ${S}/configs/
    cp -a ${WORKDIR}/include/configs/* ${S}/include/configs/
    if [ ! -z ${DDR_FIRMWARE_VERSION} ]; then
        cp ${S}/board/compulab/cl-som-imx8/ddr/lpddr4_timing_2g_${DDR_FIRMWARE_VERSION}.c ${S}/board/compulab/cl-som-imx8/ddr/lpddr4_timing_2g.c
    fi
}

addtask patch_copy after do_patch before do_configure
