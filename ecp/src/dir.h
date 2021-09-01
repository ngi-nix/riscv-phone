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

ssize_t ecp_dir_parse(ECPDirList *list, unsigned char *buf, size_t buf_size);
int ecp_dir_serialize(ECPDirList *list, unsigned char *buf, size_t buf_size);

void ecp_dir_item_parse(ECPDirItem *item, unsigned char *buf);
void ecp_dir_item_serialize(ECPDirItem *item, unsigned char *buf);
