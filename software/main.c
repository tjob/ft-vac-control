/**
 * @file main.c
 * @author tjob
 * @brief Use a Raspberry Pi Pico and a Festool 202097 CT-FI/M-Set to control any vacuum
 * @version 0.1
 * @date 2023-04-02
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include <stdio.h>
#include <time.h>
#include "decoder.h"

int main() {
    const uint32_t AUTO_OFF_AFTER = 60*60*1000; ///< Automatically turn off the vacuum if left on for more than 1 Hour.
    const uint LED_PIN_MAKS = 1 << PICO_DEFAULT_LED_PIN;
    const uint FTBT_PIN = 26;   ///< GPIO pin connected to the Festool CT-FI/M Bluetooth receiver module. 
    const uint SSR_PIN = 4;     ///< GPIO pin connected to the Solid State Relay

    struct decoder decoder = {0}; ///< The one and only decoder
    absolute_time_t AutoOffTime = {0}; ///< Used to track the absolute time we should automatically turn off if left on.
    uint32_t ticksObserved = 0; ///< Used to check that decoder is still ticking for the hardware watchdog.
    uint32_t message = 0; ///< Holds messages de-queued from the decoder's FIFO. 

    stdio_init_all();

    if (watchdog_caused_reboot()) {
        printf("Rebooted by watchdog!\n");
    }

    // Onboard LED on the Pi Pico 
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Setup the input pin to read the line-code from the bluetooth adaptor.
    gpio_init(FTBT_PIN);
    gpio_set_dir(FTBT_PIN, GPIO_IN);

    // Setup the output pin to drive the Solid State Relay controlling the power to the vacuum. 
    gpio_init(SSR_PIN);
    gpio_set_dir(SSR_PIN, GPIO_OUT);
    gpio_put(SSR_PIN, 0);

    // Setup the decoder
    initDecoder(&decoder, FTBT_PIN);
    startDecoder(&decoder);

    // Enable the HW watchdog timer to 300ms
    watchdog_enable(300, 1);

    // Main loop.  Never ends, only way out is power down or watchdog reset.
    while (true) {
        gpio_xor_mask(LED_PIN_MAKS);
        sleep_ms(150);

        // Only kick the watchdog if both the timer is running and this main loop.
        // If either stop for any reason we want to reset the hardware.
        if (ticksObserved != decoder.totalTicks) {
            watchdog_update();
            ticksObserved = decoder.totalTicks;
        }

        // Consume all messages from the decoder waiting in the fifo.
        while (queue_try_remove(&decoder.messageFIFO, &message))
        {
            // print the message
            printf("%08x\n", message);

            // Is it a start command?
            if (message == (CMD_POWER | CMD_ON)) {
                // Turn on the SSR
                gpio_put(SSR_PIN, 1);

                // Safety feature: Calculate the time we should turn off automatically if we need to.
                AutoOffTime = make_timeout_time_ms(AUTO_OFF_AFTER);
            }

            // Is it a stop command?
            if (message == (CMD_POWER | CMD_OFF)) {
                // Turn off the SSR
                gpio_put(SSR_PIN, 0);
            }

            // All other commands are ignored for now.
        }

        // Safety feature: Auto turn off the vacuum if it's been on too long. 
        if (AUTO_OFF_AFTER && time_reached(AutoOffTime)) {
            gpio_put(SSR_PIN, 0);
        }
    }

}
