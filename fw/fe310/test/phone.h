#define VOICE_STATE_IDLE    0
#define VOICE_STATE_DIAL    1
#define VOICE_STATE_RING    2
#define VOICE_STATE_CIP     3

void app_phone(EVEWindow *window, EVEViewStack *stack);
void app_phone_action(EVEForm *form);
void app_phone_init(void);
unsigned char app_phone_state_get(void);
