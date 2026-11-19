#include "core/app.h"
#include "hal/hal.h"

int main(void) {
    AppResult rc = app_run(0);
#if defined(__ORBIS__) || defined(__PS4__)
    hal_sys_exit_clean(rc == APP_OK ? 0 : 1);
#endif
    return (rc == APP_OK) ? 0 : 1;
}
