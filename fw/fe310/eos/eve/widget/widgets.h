#include "font.h"
#include "label.h"
#include "widget.h"

#include "freew.h"
#include "spacerw.h"
#include "pagew.h"
#include "strw.h"
#include "textw.h"
#include "selectw.h"

typedef union EVEWidgetSpec {
    EVEFreeSpec free;
    EVESpacerSpec spacer;
    EVEPageSpec page;
    EVEStrSpec str;
    EVETextSpec text;
    EVESelectSpec select;
} EVEWidgetSpec;

typedef int (*eve_widget_create_t) (EVEWidget *, EVERect *g, EVEWidgetSpec *);
typedef void (*eve_widget_destroy_t) (EVEWidget *);

int eve_widget_create(EVEWidget *widget, uint8_t type, EVERect *g, EVEWidgetSpec *spec);
void eve_widget_destroy(EVEWidget *widget);
