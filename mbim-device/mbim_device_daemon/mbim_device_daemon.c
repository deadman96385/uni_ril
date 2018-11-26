
#define LOG_TAG "MBIM-Device"

#include <stdbool.h>
#include <utils/Log.h>

void
mbim_device_start_main_loop (int        argc,
                             char**     argv);
void
mbim_device_vendor_init     (int        argc,
                             char**     argv);
void
mbim_device_apn_init     (int        argc,
                             char**     argv);

int main(int argc, char **argv) {
    char cmd[128] = {0};

    mbim_device_start_main_loop(argc, argv);

    mbim_device_vendor_init(argc, argv);

    mbim_device_apn_init(argc, argv);

done:
    RLOGD("mbim device main thread starting sleep loop");
    while (true) {
        sleep(UINT32_MAX);
    }
    return 0;
}
