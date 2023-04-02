#include "pico/stdlib.h"
#include <stdio.h>
#include <time.h>
#include "decoder.h"

#define AUTO_OFF_AFTER 60*60*1000 // For safety, automaticaly turn of the vacuum if left on for more than 1 Hour.

int main() {
    stdio_init_all();

    // Onboard LED on the Pi Pico 
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    const uint LED_PIN_MAKS = 1 << LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Setup the input pin to read the linecode from the bluetooth adaptor.
    const uint FTBT_PIN = 26;
    gpio_init(FTBT_PIN);
    gpio_set_dir(FTBT_PIN, GPIO_IN);

    // Setup the output pin to drive the Solid State Realay controling the power to the vacuum. 
    const uint SSR_PIN = 4;
    gpio_init(SSR_PIN);
    gpio_set_dir(SSR_PIN, GPIO_OUT);
    gpio_put(SSR_PIN, 0);

    // One and only decoder
    struct decoder decoder = {0};

    initDecoder(&decoder, FTBT_PIN);
    startDecoder(&decoder);

    absolute_time_t AutoOffTime = 0;

    while (true) {
        gpio_xor_mask(LED_PIN_MAKS);
        sleep_ms(150);

        uint32_t message;

        if (queue_try_remove(&decoder.messageFIFO, &message))
        {
            // print the message
            printf("%08x\n", message);

            if (message == 0x0117) {
                // On command seen
                gpio_put(SSR_PIN, 1);

#ifdef AUTO_OFF_AFTER
                AutoOffTime = make_timeout_time_ms(AUTO_OFF_AFTER);
#endif
            }

            if (message == 0xac17) {
                // Off command seen
                gpio_put(SSR_PIN, 0);
            }
        }

#ifdef AUTO_OFF_AFTER
        // Safety feature: Auto turn off the vacumme after 
        if (time_reached(AutoOffTime)) {
            gpio_put(SSR_PIN, 0);
        }
#endif

    }

}
