#include "label.h"
#include "widget.h"

#include "freew.h"
#include "spacerw.h"
#include "pagew.h"
#include "strw.h"
#include "textw.h"
#include "selectw.h"

typedef union EVEWidgetSpecT {
    EVEFreeSpec free;
    EVESpacerSpec spacer;
    EVEPageSpec page;
    EVEStrSpec str;
    EVETextSpec text;
    EVESelectSpec select;
} EVEWidgetSpecT;

typedef struct EVELabelSpec {
    EVERect g;
    EVEFont *font;
    char *title;
} APPLabelSpec;

typedef struct EVEWidgetSpec {
    APPLabelSpec label;
    struct {
        EVERect g;
        EVEFont *font;
        EVEWidgetSpecT spec;
        uint8_t type;
    } widget;
} EVEWidgetSpec;

typedef int (*eve_widget_create_t) (EVEWidget *, EVERect *g, EVEFont *, EVEWidgetSpecT *);
typedef void (*eve_widget_destroy_t) (EVEWidget *);

int eve_widget_create(EVEWidget *widget, uint8_t type, EVERect *g, EVEFont *font, EVEWidgetSpecT *spec);
void eve_widget_destroy(EVEWidget *widget);
