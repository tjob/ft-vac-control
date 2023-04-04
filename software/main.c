/**
 * @file main.c
 * @author tjob
 * @brief Use a Raspberry Pi Pico and a "Festool 202097 CT-FI/M-Set" to control any vacuum extractor.
 * @version 0.1
 * @date 2023-04-02
 * 
 * @copyright Copyright (c) 2023
 * 
 * Uses one GPIO pin, as an input, to read the serial data line from the 
 * Festool Bluetooth receiver. Another GPIO pin, as an output, is used to turn 
 * on/off a relay controlling power to the vacuum  extractor.
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

    struct decoder decoder = {0};       ///< The one and only decoder
    absolute_time_t AutoOffTime = {0};  ///< Used to track the absolute time we should automatically turn off if left on.
    uint32_t ticksObserved = 0;         ///< Used to check the decoder is still ticking for the hardware watchdog.
    uint32_t message = 0;               ///< Holds a message de-queued from the decoder's FIFO. 

    stdio_init_all();

    if (watchdog_caused_reboot()) {
        printf("Rebooted by watchdog!\n");
    }

    // Setup onboard LED on the Pi Pico as an output. 
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Setup the input pin to read the serial line-code from the bluetooth adaptor.
    gpio_init(FTBT_PIN);
    gpio_set_dir(FTBT_PIN, GPIO_IN);

    // Setup the output pin to drive the Solid State Relay controlling the power to the vacuum. 
    gpio_init(SSR_PIN);
    gpio_set_dir(SSR_PIN, GPIO_OUT);
    gpio_put(SSR_PIN, false);   // Default relay output to off at startup.

    // Setup the decoder
    initDecoder(&decoder, FTBT_PIN);
    startDecoder(&decoder);

    // Enable the HW watchdog timer. Needs servicing at least ever 300ms to avoid a hardware reset.
    watchdog_enable(300, true);

    // Main loop.  Never ends, only way out is power down or watchdog reset.
    while (true) {
        // Toggle the onboard LED
        gpio_xor_mask(LED_PIN_MAKS);

        // Delay long enough to see the LED blink, short enough not to add too much latency
        // retrieving messages from the decoder. 
        sleep_ms(150);

        // Only service the watchdog if both the decoder timer and this main loop
        // are running.  If either stops, for any reason, watchdog will reset the 
        // hardware for us.
        if (ticksObserved != decoder.totalTicks) {
            watchdog_update();
            ticksObserved = decoder.totalTicks;
        }

        // Consume all messages from the decoder waiting in the fifo.
        while (queue_try_remove(&decoder.messageFIFO, &message))
        {
            // Print the message
            printf("0x%08lx\n", message);

            // Is it a start command?
            if (message == (CMD_POWER | CMD_ON)) {
                // Turn on the output relay
                gpio_put(SSR_PIN, true);

                // Safety feature: Calculate the time we should turn off automatically if we need to.
                AutoOffTime = make_timeout_time_ms(AUTO_OFF_AFTER);
            }

            // Is it a stop command?
            if (message == (CMD_POWER | CMD_OFF)) {
                // Turn off the output relay
                gpio_put(SSR_PIN, false);
            }

            // All other commands are ignored for now.
        }

        // Safety feature: Automatically turn off the vacuum if it's been on too long. 
        if (AUTO_OFF_AFTER && time_reached(AutoOffTime)) {
            gpio_put(SSR_PIN, 0);
        }
    }

    // Should never get here! Halt.
    stopDecoder(&decoder);
    while (true) {}
    return 0;
}
