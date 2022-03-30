#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <sdc_crypto.h>
#include <eve/eve.h>
#include <eve/eve_kbd.h>
#include <eve/eve_font.h>

#include <eve/screen/window.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <aes/aes.h>
#include <ff.h>

#include "fs.h"

FATFS fs;

#define TEXT_SIZE   128
#define TEXT_FN     "test.txt"
#define KEY         "passwordpassword"

PARTITION VolToPart[FF_VOLUMES] = {
    {0, 1}      /* "0:" ==> 1st partition on the pd#0 */
};

static EOSSDCCrypto sdcc;
static AESCtx ctx_crypt;
static AESCtx ctx_essiv;

void app_fs(EVEWindow *window, EVEViewStack *stack) {
    EVEWidgetSpec spec[] = {
        {
            .label.title = "Text",
            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.spec.str.str_size = TEXT_SIZE,
        },
    };
    EVEForm *form = eve_form_create(window, stack, spec, 1, NULL, NULL, app_fs_close);
    EVEStrWidget *text = (EVEStrWidget *)eve_page_widget(&form->p, 0);
    FIL f;
    FRESULT rv;

    eos_spi_select(EOS_SPI_DEV_SDC);
    rv = f_open(&f, TEXT_FN, FA_READ);
    printf("f_open:%d\n", rv);
    if (!rv) {
        UINT r;

        rv = f_read(&f, text->str, TEXT_SIZE-1, &r);
        printf("f_read:%d\n", rv);
        if (rv != FR_OK) r = 0;
        text->str[r] = '\0';
        f_close(&f);
    }
    eos_spi_select(EOS_SPI_DEV_EVE);
    eve_strw_update(text);
}

void app_fs_close(EVEForm *form) {
    EVEStrWidget *text = (EVEStrWidget *)eve_page_widget(&form->p, 0);
    FIL f;
    FRESULT rv;

    eos_spi_select(EOS_SPI_DEV_SDC);
    rv = f_open(&f, TEXT_FN, FA_WRITE | FA_CREATE_ALWAYS);
    printf("f_open:%d\n", rv);
    if (!rv) {
        UINT w;

        rv = f_write(&f, text->str, strlen(text->str), &w);
        printf("f_write:%d\n", rv);
        f_close(&f);
    }
    eos_spi_select(EOS_SPI_DEV_EVE);
    eve_form_destroy(form);
}

void app_fs_init(void) {
    FRESULT rv;

    eos_sdcc_init(&sdcc, KEY, &ctx_crypt, (eve_sdcc_init_t)aes_init, (eve_sdcc_crypt_t)aes_cbc_encrypt, (eve_sdcc_crypt_t)aes_cbc_decrypt, &ctx_essiv, (eve_sdcc_init_t)aes_init, (eve_sdcc_essiv_t)aes_ecb_encrypt);
    eos_spi_select(EOS_SPI_DEV_SDC);
    rv = f_mount(&fs, "", 1);
    printf("f_mount:%d\n", rv);
    if (rv == FR_NO_FILESYSTEM) {
        uint8_t w[FF_MAX_SS];
        LBA_t plist[] = {100, 0};

        rv = f_fdisk(0, plist, w);
        printf("f_fdisk:%d\n", rv);
        rv = f_mkfs("0:", 0, w, sizeof(w));
        printf("f_mkfs:%d\n", rv);
        rv = f_mount(&fs, "", 1);
        printf("f_mount:%d\n", rv);
    }
    eos_spi_deselect();
}