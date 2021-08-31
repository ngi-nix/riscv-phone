#define ECP_MAX_DIR_ITEM    30
#define ECP_SIZE_DIR_ITEM   40

typedef struct ECPDirItem {
    ECPNode node;
    uint16_t capabilities;
} ECPDirItem;

typedef struct ECPDirList {
    ECPDirItem item[ECP_MAX_DIR_ITEM];
    uint16_t count;
} ECPDirList;

void ecp_dir_parse_item(unsigned char *buf, ECPDirItem *item);
void ecp_dir_serialize_item(unsigned char *buf, ECPDirItem *item);
