do_compile_preppend () {
    make_file=${S}/iMX8M/soc.mak
    if [ -e ${make_file} ]; then
        sed -i "s/^dtbs = .*dtb/dtbs = ${UBOOT_DTB_NAME}/g" ${make_file}
        if [ ! -z ${DDR_FIRMWARE_VERSION} ]; then
            sed -i "/^lpddr4_.mem_.d = / i LPDDR_FW_VERSION = _${DDR_FIRMWARE_VERSION}" ${make_file}
        fi
    fi
}

addtask compile_preppend before do_compile after do_configure

COMPATIBLE_MACHINE = "(cl-imx8)"
