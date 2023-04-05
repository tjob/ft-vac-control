/**
 * @file decoder.h
 * @author tjob
 * @brief Decoder for Festool 202097 CT-FI/M-Set
 * @version 0.1
 * @date 2023-04-02
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef _FTDECODER_H
#define _FTDECODER_H

#include "pico/stdlib.h"
#include "pico/util/queue.h"

#define BIT_DURATION_MS 1       ///< Length of a bit in milliseconds, the bit period.
#define TICKS_PER_SEC   16000   ///< The decoder tick frequency in Hz.

#define TICKS_PER_BIT    TICKS_PER_SEC * BIT_DURATION_MS / 1000    ///< Number of ticks in a bit period
#define TICKS_PER_QBIT   TICKS_PER_BIT / 4      ///< Number of ticks in 1/4 of a bit period
#define TICKS_PER_3QBITS TICKS_PER_QBIT * 3     ///< Number of ticks in 3/4 of a bit period
#define TICKS_PER_5QBITS TICKS_PER_QBIT * 5     ///< Number of ticks in 5/4 (1 and a 1/4) of a bit period

// Educated guess of the command meanings.
#define CMD_POWER       0x17
#define CMD_ON          0x0100
#define CMD_OFF         0xac00
#define CMD_SPEEDCTRL   0x23
#define CMD_FULLSPEED   0xff00
#define CMD_NOSPEED     0x0000
#define CMD_PWR_ON_REST 0x1d
#define CMD_UNKNOWN     0x0a


/**
 * @brief Decoder states.
 * 
 * The tick function is a state machine that lives on one of the following states. 
 * The current state, how long we have been in it, and level sampled of input GPIO 
 * pin determines the next state. 
 */
typedef enum decoderStates {
    IDLE = 0,    ///< Idle, the line is high
    BREAK,       ///< Break, line held low for two bit periods
    MAB,         ///< Mark After Break, line is high for approximately 1 bit period after the BREAK.
    STARTBIT,    ///< The first bit after the break, always zero
    LOWERCHIRP,  ///< The lower (second) chirp of an encoded bit pair
    UPPERCHIRP,  ///< The upper (first) chirp of an encoded bit pair
    WAITIDLE,    ///< Wait for the line to go idle (high) again
    ERROR        ///< Something unexpected seen in the line code, clean up and then go to WAITIDLE.
} decoderStates_t;

/**
 * @brief Message structure.
 * 
 * Holds the bits received along with a count.
 */
typedef struct {
    uint32_t data;      ///< The message bytes combined into a 32 bit value.
    uint bitsRecieved;  ///< Number of bits received in message.
} msg_t;

/**
 * @brief Decoder structure.
 * 
 * Holds the state information and message bytes received so far.
 */
typedef struct decoder {
    uint inputPin;          ///< GPIO pin used for reading the input.
    decoderStates_t state;  ///< Decoder state.
    bool last;              ///< Input level at start of previous tick.
    uint32_t ticksInState;      ///< Count of ticks since last state change.
    uint32_t ticksSinceEdge;    ///< Count of ticks since last edge in input, raising or falling.
    repeating_timer_t timer;    ///< Timer structure used to tick the main receiver state machine function.
    bool upperChirp;        ///< First half of a bit (a chirp) received.
    msg_t message;          ///< The current message being received.
    uint32_t rxMask;        ///< Bit mask with only the next bit to be received set.
    queue_t messageFIFO;    ///< fifo queue used to store received messages until the main loop can consume them.
    uint32_t decodeErrors;  ///< A count of how many times this decoder enters the error state.
    uint32_t totalTicks;    ///< A count of how many times this decoder has ticked.
} decoder_t;


void initDecoder(decoder_t *dec, uint GPIO_Pin);
bool stopDecoder(decoder_t *dec);
bool startDecoder(decoder_t *dec);

#endif
