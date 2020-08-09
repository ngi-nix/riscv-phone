#define GSM_OK                      0
#define GSM_ERR                     -1
#define GSM_ERR_SIZE                -10
#define GSM_ERR_NOT_SUPPORTED       -11

#define GSM_TS_SIZE                 25
#define GSM_UD_SIZE                 160
#define GSM_UDH_SIZE                140
#define GSM_MSG_SIZE                GSM_UD_SIZE
#define GSM_ADDR_SIZE               20

/* Message-Type-Indicator */
#define GSM_MTI                     0x03
#define GSM_MTI_DELIVER             0x00
#define GSM_MTI_DELIVER_REPORT      0x00
#define GSM_MTI_SUBMIT              0x01
#define GSM_MTI_SUBMIT_REPORT       0x01
#define GSM_MTI_COMMAND             0x02
#define GSM_MTI_COMMAND_REPORT      0x02

#define GSM_MMS                     0x04    /* More-Messages-to-Send */
#define GSM_RD                      0x04    /* Reject-Duplicates */
#define GSM_LP                      0x08    /* Loop-Prevention */

/* Validity-Period-Format */
#define GSM_VPF                     0x18
#define GSM_VPF_NONE                0x00
#define GSM_VPF_ENHANCED            0x08
#define GSM_VPF_RELATIVE            0x10
#define GSM_VPF_ABSOLUTE            0x18

#define GSM_SRI                     0x20    /* Status-Report-Indication */
#define GSM_SRR                     0x20    /* Status-Report-Request */
#define GSM_SRQ                     0x20    /* Status-Report-Qualifier */
#define GSM_UDHI                    0x40    /* User-Data-Header-Indicator  */
#define GSM_RP                      0x80    /* Reply-Path */

/* Type-of-Number */
#define GSM_TON                     0x70
#define GSM_TON_UNKNOWN             0x00
#define GSM_TON_INTERNATIONAL       0x10
#define GSM_TON_NATIONAL            0x20
#define GSM_TON_NETWORK             0x30
#define GSM_TON_SUBSCRIBER          0x40
#define GSM_TON_ALPHANUMERIC        0x50
#define GSM_TON_ABBRREVIATED        0x60

/* Numbering-Plan-Identification */
#define GSM_NPI                     0x0f
#define GSM_NPI_UNKNOWN             0x00
#define GSM_NPI_TELEPHONE           0x01
#define GSM_NPI_DATA                0x03
#define GSM_NPI_TELEX               0x04
#define GSM_NPI_SCS1                0x05
#define GSM_NPI_SCS2                0x06
#define GSM_NPI_NATIONAL            0x08
#define GSM_NPI_PRIVATE             0x09
#define GSM_NPI_ERMES               0x0a

#define GSM_EXT                     0x80

/* Protocol-Identifier */
#define GSM_PID_DEFAULT             0
#define GSM_PID_TYPE0               64

/* Data-Coding-Scheme */
#define GSM_DCS_CLASS               0x03
#define GSM_DCS_ENC                 0x0c

#define GSM_DCS_CLASS_IND           0x10
#define GSM_DCS_COMPRESS_IND        0x20
#define GSM_DCS_DELETE_IND          0x40
#define GSM_DCS_GENERAL_IND         0x80
#define GSM_DCS_GROUP               0xf0

#define GSM_DCS_MWI_DISCARD         0xc0
#define GSM_DCS_MWI_STORE_GSM7      0xd0
#define GSM_DCS_MWI_STORE_UCS2      0xe0
#define GSM_DCS_MWI_SENSE           0x08
#define GSM_DCS_MWI_TYPE            0x03

#define GSM_DCS_ENCLASS             0xf0
#define GSM_DCS_ENCLASS_ENC         0x04

/* Parameter-Indicator */
#define GSM_PI_PID                  0x01
#define GSM_PI_DCS                  0x02
#define GSM_PI_UD                   0x04
#define GSM_PI_EXT                  0x08

/* character set */
#define GSM_ENC_7BIT                0x00
#define GSM_ENC_8BIT                0x04
#define GSM_ENC_UCS2                0x08

/* message waiting indication */
#define GSM_MWI_TYPE_VOICEMAIL      0x00
#define GSM_MWI_TYPE_FAX            0x01
#define GSM_MWI_TYPE_EMAIL          0x02
#define GSM_MWI_TYPE_OTHER          0x03

/* flags */
#define GSM_FLAG_COMPRESS           0x0001
#define GSM_FLAG_DELETE             0x0002
#define GSM_FLAG_DISCARD            0x0004
#define GSM_FLAG_TYPE0              0x0008
#define GSM_FLAG_STATUS_REPORT      0x0010
#define GSM_FLAG_REPLY_PATH         0x0020

/* message class */
#define GSM_FLAG_CLASS              0x0400
#define GSM_FLAG_CLASS0             0x0000  /* Flash */
#define GSM_FLAG_CLASS1             0x0100  /* ME-specific */
#define GSM_FLAG_CLASS2             0x0200  /* (U)SIM-specific */
#define GSM_FLAG_CLASS4             0x0300  /* TE-specific */
#define GSM_FLAG_CLASS_MASK         0x0f00

/* message waiting indication */
#define GSM_FLAG_MWI                0x4000
#define GSM_FLAG_MWI_SENSE          0x8000
#define GSM_FLAG_MWI_VOICEMAIL      0x0000
#define GSM_FLAG_MWI_FAX            0x1000
#define GSM_FLAG_MWI_EMAIL          0x2000
#define GSM_FLAG_MWI_OTHER          0x3000
#define GSM_FLAG_MWI_MASK           0xf000

uint8_t pdu_getc(char *pdu);
void pdu_putc(uint8_t ch, char *pdu);
void pdu_gets(char *pdu, uint8_t *s, int s_len);
void pdu_puts(uint8_t *s, int s_len, char *pdu);
int gsm_ucs2_to_7bit(uint16_t ucs2, char *gsm7, int gsm7_size);
int gsm_7bit_to_ucs2(char *gsm7, int gsm7_len, uint16_t *ucs2);

int gsm_7bit_enc(char *text, int text_len, char *pdu, int padb);
int gsm_7bit_dec(char *pdu, char *text, int text_len, int padb);
int gsm_addr_enc(char *addr, int addr_len, uint8_t addr_type, char *pdu, int pdu_size);
int gsm_addr_dec(char *pdu, int pdu_len, char *addr, int *addr_len, uint8_t *addr_type);
int gsm_sms_enc(char *addr, int addr_len, uint8_t addr_type, uint8_t *udh, int udh_len, uint8_t *msg, int msg_len, uint8_t enc, uint16_t flags, char *pdu, int pdu_size);
int gsm_sms_dec(char *pdu, int pdu_len, char *addr, int *addr_len, uint8_t *addr_type, uint8_t *udh, int *udh_len, uint8_t *msg, int *msg_len, char *ts, uint8_t *enc, uint16_t *flags);
