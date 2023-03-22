#include "pico/stdlib.h"
#include <stdio.h>
#include "decoder.h"

int main() {
    stdio_init_all();

    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Setup the input pin to read the linecode from the bluetooth adaptor
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

    initDecoder(&decoder);
    startDecoder(&decoder);

    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(150);
        gpio_put(LED_PIN, 0);
        sleep_ms(150);

        uint32_t message;

        if (queue_try_remove(&decoder.messageFIFO, &message))
        {
            // print the message
            printf("%08x\n", message);

            if (message == 0x0117) {
                // On command seen
                gpio_put(SSR_PIN, 1);
            }

            if (message == 0xac17) {
                // Off Command seen
                gpio_put(SSR_PIN, 0);
            }
        }
    }

}
