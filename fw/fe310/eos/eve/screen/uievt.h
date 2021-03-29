#define EVE_UIEVT_WIN_UPDATE_G          1
#define EVE_UIEVT_PAGE_UPDATE_G         2
#define EVE_UIEVT_PAGE_TOUCH            3
#define EVE_UIEVT_PAGE_SCROLL_START     4
#define EVE_UIEVT_PAGE_SCROLL_STOP      5
#define EVE_UIEVT_PAGE_TRACK_START      6
#define EVE_UIEVT_PAGE_TRACK_STOP       7
#define EVE_UIEVT_WIDGET_UPDATE_G       8

typedef struct EVEUIEvtTouch {
    EVETouch *touch;
    uint16_t evt;
    uint8_t tag0;
} EVEUIEvtTouch;
