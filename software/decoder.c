#include "pico/stdlib.h"
#include "decoder.h"


/**
 * @brief Decode Manchester pairs.  10 = 0 and 01 = 1
 * 
 * @param upper The upper chirp
 * @param lower The lower chirp
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
 * @brief Clear any previously read bits, resets counters ready for the start of the next message
 * 
 * @param dec Pointer to a decoder structure
 */
void clearBits(struct decoder *dec)
{
    dec->message=0;
    dec->rxMask = 1;
    dec->bitsRecieved = 0;
}

/**
 * @brief Added a complete message to the fifo for consumption in the main loop.
 * 
 * @param dec Pointer to a decoder structure
 */
void ProcessBits(struct decoder *dec)
{
    queue_try_add(&dec->messageFIFO, &dec->message);
    clearBits(dec);
}

/**
 * @brief Set the State decoder
 * 
 * @param dec 
 * @param newState 
 */
void setState(struct decoder *dec, uint newState)
{
    dec->state = newState;
    dec->tickCountInState = 0;  // count since last state change
}


/**
 * @brief Runs on a timer at 16x the speed of bit (1 ms), so every 62.5 us (i.e. at 16 kHz)
 * 
 * @param t Pointer to a repeating timer structure
 * @return Alwasy returns true so the timer continues to tick 
 */
bool on_16x_timer(struct repeating_timer *t)
{
    struct decoder *dec = (struct decoder*)t->user_data;
    const bool current = gpio_get(dec->inputPin);
    bool edgeDetected = false;

    // Has there been an edge since the last tick?
    if (dec->last != current) {
        dec->last = current;
        edgeDetected = true;
        dec->ticksSinceEdge = 0;
    }

    switch (dec->state) {
        case IDLE:
            if (current == 0) {
                setState(dec, BREAK);
            }
            break;

        case BREAK:
            if (current == 1) {
                if ((dec->tickCountInState < (TICKS_PER_BIT * 2 - TICKS_PER_QBIT)) ||
                    (dec->tickCountInState > (TICKS_PER_BIT * 2 + TICKS_PER_QBIT))) {
                    setState(dec, ERROR);
                } else {
                    setState(dec, MAB);
                }
            }
            break;

        case MAB:
            if (dec->tickCountInState == TICKS_PER_3QBITS) {
                // Read the first half (upper chirp) of the start bit
                // and save it for the next half
                dec->upperChirp = current;
            }

            if (edgeDetected) {
               setState(dec, dec->tickCountInState < TICKS_PER_3QBITS ? ERROR : STARTBIT);
            }
            break;

        case STARTBIT:
            if (dec->ticksSinceEdge == TICKS_PER_QBIT) {
                // This is where we would sample second half (lower chirp) of start bit, it will be zero no need to decode it.
                
                // Setup for reading the message 
                clearBits(dec);

                // Now wait to read the first half bit (chirp) of the message.
                setState(dec, UPPERCHIRP);
            } 
            break;

        case LOWERCHIRP:
            if (dec->ticksSinceEdge == TICKS_PER_QBIT || dec->ticksSinceEdge == TICKS_PER_3QBITS) {
                // Can now sample the second chirp to get a full bit.
                int FullBit = manPairToBit(dec->upperChirp, current);

                if (FullBit == -1) {
                    // Invalid half bit pair
                    setState(dec, ERROR);
                } else {
                    dec->bitsRecieved++;

                    // Add the bit to the message in the correct place pointed to by the rxMask.
                    dec->message |= dec->rxMask & FullBit;
                    dec->rxMask <<= 1;

                    setState(dec, UPPERCHIRP);
                }
            }

            if (dec->ticksSinceEdge == TICKS_PER_5QBITS) {
                // Something has gone wrong, should have had and edge by now, probaby end of packet.
                setState(dec, WAITIDLE);
            }     
            break;

        case UPPERCHIRP:
            if (dec->ticksSinceEdge == TICKS_PER_QBIT || dec->ticksSinceEdge == TICKS_PER_3QBITS) {
                // Sample the first, upper, chirp and save it for the next half
                dec->upperChirp = current;
                setState(dec, LOWERCHIRP);
            }

            if (dec->ticksSinceEdge == TICKS_PER_5QBITS) {
                // Something has gone wrong, should have had and edge by now, probaby end of packet.
                setState(dec, WAITIDLE);
            }            
            break;
        
        case WAITIDLE:
            if (dec->tickCountInState > TICKS_PER_BIT * 2 && current == 1) {
                ProcessBits(dec);
                setState(dec, IDLE);
            }
            break;

        case ERROR:
        default:
            clearBits(dec);
            setState(dec, IDLE);
            break;
    }

    dec->tickCountInState++;
    dec->ticksSinceEdge++;

    return true; // keep repeating
}

uint startDecoder(struct decoder *dec)
{
    // Setup timer
    add_repeating_timer_us(-62, on_16x_timer, dec, &(dec->timer));
    return 0;
}

uint stopDecoder(struct decoder *dec)
{
    cancel_repeating_timer(&(dec->timer));
    return 0;
}

/**
 * @brief Initialize the decoder
 * 
 * @param dec Pointer to a decoder structure to be initilized
 * @param GPIO_Pin The pin do read the input from
 */
void initDecoder(struct decoder *dec, uint GPIO_Pin)
{
    dec->inputPin = GPIO_Pin;
    clearBits(dec);
    queue_init(&dec->messageFIFO, sizeof(uint32_t), 16);
}
