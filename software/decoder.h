#ifndef _FTDECODER_H
#define _FTDECODER_H

#include "pico/stdlib.h"
#include "pico/util/queue.h"


#define TICKS_PER_BIT 16
#define TICKS_PER_QBIT 4
#define TICKS_PER_3QBITS 12
#define TICKS_PER_5QBITS 20

enum decoderStates {
    IDLE = 0,   ///< Idle, the line is high
    BREAK,      ///< Break, line held low for two bit pariods
    MAB,        ///< Mark After Break, line is high for aproximatly 1 bit pariod after the BREAK.
    STARTBIT,   ///< The first bit after the break, alwasy zero
    LOWERCHIP,  ///< The lower (second) chip of an encoded bit pair
    UPPERCHIP,  ///< The upper (first) chip of an encoded bit pair
    WAITIDLE,   ///< Wait for the line to go idel (high) again
    ERROR       ///< Something unexpected seen in the line code, clean up and then go to WAITIDLE.
};

/**
 * @brief The decoder structure
 * 
 * Holds the state information and message bytes received so far.
 * 
 */
struct decoder {
    uint state;
    bool last;  ///< Input at privious tick
    uint32_t tickCountInState;  ///< Ticks since last state change
    uint32_t ticksSinceEdge;    ///< Ticks since last edge in input, raising or falling.
    struct repeating_timer timer;
    uint32_t message;
    uint bitsRecieved;
    bool upperChip; ///< First half of a bit (a chip) received
    uint32_t rxMask;
    queue_t messageFIFO;
};

void initDecoder(struct decoder *dec);
uint stopDecoder(struct decoder *dec);
uint startDecoder(struct decoder *dec);

#endif