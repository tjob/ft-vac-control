#ifndef _FTDECODER_H
#define _FTDECODER_H

#include "pico/stdlib.h"
#include "pico/util/queue.h"

#define BIT_DURATION_MS 1   ///< Length of a bit in ms.
#define TICKS_PER_SEC 16000 ///< Tick frequany

#define TICKS_PER_BIT TICKS_PER_SEC * BIT_DURATION_MS / 1000    ///< Number of ticks in bit pariod
#define TICKS_PER_QBIT TICKS_PER_BIT / 4    ///< Number of ticks in 1/4 of a bit pariod
#define TICKS_PER_3QBITS TICKS_PER_QBIT * 3 ///< Number of ticks in 3/4 of a bit pariod
#define TICKS_PER_5QBITS TICKS_PER_QBIT * 5 ///< Number of ticks in 1+1/4 of a bit pariod

// Edugated guesses as to the command meanings.
#define CMD_POWER       0x17
#define CMD_ON          0x0100
#define CMD_OFF         0xac00
#define CMD_SPEEDCTRL   0x23
#define CMD_FULLSPEED   0xff00
#define CMD_NOSPEED     0x0000


enum decoderStates {
    IDLE = 0,   ///< Idle, the line is high
    BREAK,      ///< Break, line held low for two bit pariods
    MAB,        ///< Mark After Break, line is high for aproximatly 1 bit pariod after the BREAK.
    STARTBIT,   ///< The first bit after the break, alwasy zero
    LOWERCHIRP,  ///< The lower (second) chirp of an encoded bit pair
    UPPERCHIRP,  ///< The upper (first) chirp of an encoded bit pair
    WAITIDLE,   ///< Wait for the line to go idel (high) again
    ERROR       ///< Something unexpected seen in the line code, clean up and then go to WAITIDLE.
};

/**
 * @brief The decoder structure
 * 
 * Holds the state information and message bytes received so far.
 */
struct decoder {
    uint inputPin;  ///< GPIO pin used for reading the input.
    uint state; ///< Decoder state.
    bool last;  ///< Input level at start of privious tick.
    uint32_t tickCountInState;  ///< Ticks since last state change.
    uint32_t ticksSinceEdge;    ///< Ticks since last edge in input, raising or falling.
    struct repeating_timer timer;   ///< Timer strucutre used to tick the main reciver state machine function.
    uint32_t message;   ///< The current messge being recived.
    uint bitsRecieved;  ///< Number of bit received for current message.
    bool upperChirp; ///< First half of a bit (a chirp) received.
    uint32_t rxMask;    ///< Bit mask with only the next bit to be recived set.
    queue_t messageFIFO;  ///< fifo quite used to storying recived measaged untill the main loop can consume them.
    uint32_t decodeErrors; ///< A count of how many timers this decoder enters the error state.
    uint32_t totalTicks; ///< A count of how many times this decoder has ticked.
};

void initDecoder(struct decoder *dec, uint GPIO_Pin);
bool stopDecoder(struct decoder *dec);
bool startDecoder(struct decoder *dec);

#endif