/**
 * @file decoder.c
 * @author tjob
 * @brief Decoder serial stream coming from Festool No. 202097 CT-F I/M
 * @version 0.1
 * @date 2023-04-02
 * 
 * @copyright Copyright (c) 2023
 * 
 * The Festool 202097 CT-F I/M Bluetooth add-on for CT26/36/48 vacuum extractors 
 * has 3 electrical connections to the host vacuum; GND, +5V, and Data. The 
 * data line is used for serial communication and runs at approximately 1 kHz. 
 * The Bluetooth receiver encodes bits onto the data line using a form of 
 * Manchester encoding. Line transitions from high to low represent a 1, and 
 * transitions from low to high represent a 0. The line idles high when nothing 
 * is being sent. Messages start with a "break", pulling the line low for 
 * approximately 2 bit periods (2 ms), followed by a "mark" of 1 ms.  There is 
 * a start bit, always 0, followed by several bytes of data. Bytes are sent 
 * least significant bit first.
 * 
 * This decoder receives and decodes the incoming serial stream into messages
 * up to 32bits in length (4 bytes). We define a message to be the bit sequence
 * after the start bit until the line goes idle again. All messages seen so far
 * are an integer number of bytes long. A single press of the manual remote 
 * button results in multiple messages being sent, with a relatively long delay 
 * (100's of ms) between then.
 * 
 * Note: While this decoder works, it's based on observations of one receiver 
 * and one manual remote button. It has not been tested with any of the Festool 
 * cordless Bluetooth battery tools. It is suspected that the serial interface 
 * is bi-directional, allowing the vacuum extractor to respond to the commands. 
 * This implementation is receive only.
 * 
 * Theory of operation:
 * on_16x_timer() runs, ticks from a hardware timer, at approximately 16 times 
 * bit rate. Based on the current state, how long we have been in that state, and
 * how long it's been since the last edge was seen we can decode the signal.
 * The timer is free running but the state machine within synchronizes to the
 * edges seen on the serial input. This takes care of any drift. 
 * 
 * To decode the Manchester signal we sample the input twice per bit. Each 
 * sample gives us a "Chirp", and two of those are needed to decode a bit.
 * Chirps are sampled at 1/4 of a bit period after an edge and again at 3/4 of 
 * a bit period if the next edge is not seen yet. Pairs of chirps give us 
 * decoded bits: 1,0 = 1 and 0,1 = 0.  
 */
#include "pico/stdlib.h"
#include "decoder.h"

/**
 * @brief Decode Manchester chirp pairs.  10 = 0 and 01 = 1
 * 
 * @param upper The upper, first, chirp
 * @param lower The lower, second, chirp
 * @return int 1 if bit is true, 0 for false and -1 for an invalid chirp pair
 */
int manPairToBit(bool upper, bool lower)
{
    if (upper && !lower)
        return 0;

    if (!upper && lower)
        return 1;

    return -1;
}

/**
 * @brief Clear any previously read bits, resets counters ready for the start of the next message.
 * 
 * @param dec Pointer to a decoder structure.
 */
void clearBits(decoder_t *dec)
{
    dec->message.data=0;
    dec->rxMask = 1;
    dec->message.bitsRecieved = 0;
}

/**
 * @brief Add the complete message to the fifo for consumption in the main loop.
 * 
 * @param dec Pointer to a decoder structure.
 */
void processBits(decoder_t *dec)
{
    queue_try_add(&dec->messageFIFO, &dec->message);
    clearBits(dec);
}

/**
 * @brief Set the State of the decoder
 * 
 * @param dec Pointer to a decoder structure.
 * @param newState The new state the decoder is transitioning too.
 */
void setState(decoder_t *dec, uint newState)
{
    dec->state = newState;
    dec->ticksInState = 0;  // Count since last state change.
}

/**
 * @brief Runs on a timer at 16x the speed of bit (1 ms), so every 62.5 us (i.e. at 16 kHz)
 * 
 * @param rt Pointer to a repeating timer structure.
 * @return Always returns true so the timer continues to tick. 
 * 
 * This is the main decoding state machine.
 */
bool __not_in_flash_func(on_16x_timer)(repeating_timer_t *rt)
{
    decoder_t *dec = (decoder_t*)rt->user_data;
    const bool current = gpio_get(dec->inputPin);
    bool edgeDetected = false;

    // Has there been an edge since the last tick?
    if (dec->last != current) {
        dec->last = current;
        edgeDetected = true;
        dec->ticksSinceEdge = 0;
    }

    switch (dec->state) {
        // The serial line idles high between messages.
        // In this state we are waiting for the line to fall, signaling the 
        // start of the "break" before the message content.
        case IDLE:
            if (current == 0) {
                setState(dec, BREAK);
            }
            break;

        // In the break state, the serial line is driven low for 2 ms. i.e
        // the length of 2 bits.
        case BREAK:
            if (current == 1) {
                if ((dec->ticksInState < (TICKS_PER_BIT * 2 - TICKS_PER_QBIT)) ||
                    (dec->ticksInState > (TICKS_PER_BIT * 2 + TICKS_PER_QBIT))) {
                    setState(dec, ERROR);
                } else {
                    setState(dec, MAB);
                }
            }
            break;

        // The Mark After Break (MAB), the line is drive high before 
        // transmission of the start bit.
        case MAB:
            if (dec->ticksInState == TICKS_PER_3QBITS) {
                // Read the first half (upper chirp) of the start bit
                // and save it for the next half
                dec->upperChirp = current;
            }

            if (edgeDetected) {
               setState(dec, dec->ticksInState < TICKS_PER_3QBITS ? ERROR : STARTBIT);
            }
            break;

        // Receive the start bit, it's always 0.
        case STARTBIT:
            if (dec->ticksSinceEdge == TICKS_PER_QBIT) {
                // This is where we would sample second half (lower chirp) of start bit, it will be zero; no need to decode it.
                
                // Setup for reading the message 
                clearBits(dec);

                // Now wait to read the first half bit (chirp) of the message.
                setState(dec, UPPERCHIRP);
            } 
            break;

        // Bits are sent in two parts, chirps, this state reads the second 
        // chirp of a bit. As the lower chirp comes after the upper chirp, at 
        // the end of this state we have a new bit.
        // 
        // When receiving the message content the state machine toggles back
        // and forth between the LOWERCHIRP and UPPERCHIRP states.
        case LOWERCHIRP:
            if (dec->ticksSinceEdge == TICKS_PER_QBIT || dec->ticksSinceEdge == TICKS_PER_3QBITS) {
                // Can now sample the second chirp of the current bit to get a full bit.
                int FullBit = manPairToBit(dec->upperChirp, current);

                if (FullBit == -1) {
                    // Invalid half bit pair
                    setState(dec, ERROR);
                } else {
                    dec->message.bitsRecieved++;

                    // Add the new bit to the message in the correct place pointed to by the rxMask.
                    dec->message.data |= FullBit ? dec->rxMask : 0;
                    dec->rxMask <<= 1;

                    setState(dec, UPPERCHIRP);
                }
            }

            if (dec->ticksSinceEdge == TICKS_PER_5QBITS) {
                // Something has gone wrong, should have had an edge by now, probably end of packet.
                setState(dec, WAITIDLE);
            }     
            break;

        // Receive the upper chirp, the first half of a bit sent on the line.
        case UPPERCHIRP:
            if (dec->ticksSinceEdge == TICKS_PER_QBIT || dec->ticksSinceEdge == TICKS_PER_3QBITS) {
                // Sample the first chirp of a new bit, save it to pair up with the next lower chirp we get.
                dec->upperChirp = current;
                setState(dec, LOWERCHIRP);
            }

            if (dec->ticksSinceEdge == TICKS_PER_5QBITS) {
                // Something has gone wrong, should have had an edge by now, probably end of packet.
                setState(dec, WAITIDLE);
            }            
            break;
        
        // In this state we wait for the serial line to go back to the idle 
        // state.
        case WAITIDLE:
            if (dec->ticksInState > TICKS_PER_BIT * 2 && current == 1) {
                processBits(dec);
                setState(dec, IDLE);
            }
            break;

        // For completeness, an error state.  Should land here unless there was a decode error.
        case ERROR:
        default:
            dec->decodeErrors++;
            clearBits(dec);
            setState(dec, IDLE);
            break;
    }

    dec->ticksInState++;
    dec->ticksSinceEdge++;
    dec->totalTicks++;

    return true; // Keep the timer repeating
}

/**
 * @brief Start the decoder timer ticking: on_16x_timer() will be called periodically.
 * 
 * @param dec Pointer to the decoder to start.
 * @return true if the timer could be created.
 */
bool startDecoder(decoder_t *dec)
{
    // Setup timer.  Negative delay_us to make period independent of the tick duration.
    return add_repeating_timer_us(-1000000/TICKS_PER_SEC, on_16x_timer, dec, &(dec->timer));
}

/**
 * @brief Stop the decoder timer from ticking. When stopped no new messages will be received.
 * 
 * @param dec Pointer to the decoder to stop.
 * @return true if the decoder timer was stopped.
 */
bool stopDecoder(decoder_t *dec)
{
    return cancel_repeating_timer(&(dec->timer));
}

/**
 * @brief Initialize the decoder.
 * 
 * @param dec Pointer to a decoder structure to be initialized.
 * @param GPIO_Pin The pin to read the input from.
 */
void initDecoder(decoder_t *dec, uint GPIO_Pin)
{
    dec->inputPin = GPIO_Pin;
    clearBits(dec);
    queue_init(&dec->messageFIFO, sizeof(msg_t), 16);
}
