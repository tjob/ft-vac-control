# Serial signal traces

These traces (.sr files) capture the output signal of the Festool CT-F I Bluetooth module when not connected to a CT vacuum extractor.  They can be viewed using [Pulseview](https://sigrok.org/wiki/PulseView) from the sigrok project.


## Power on reset [(Startup.sr)](Startup.sr)
Approximately 590 ms after power on, the Bluetooth module sends a single 1 byte message of `0x1d` to the extractor.  It's function is unknown.

## Manual remote on [(On.sr)](On.sr)
When the Bluetooth module receives the first (and all other odd numbered) press of the manual remote button the module sends 3 separate messages to the extractor. Each is two bytes long.

```
Message 1:
0x0117 (2 bytes)  // Turn extractor on command
[Delay of ~284 ms]

Message 2:
0xff23 (2 bytes)  // Extractor at full speed?
[Delay of ~387 ms]

Message 3:
0x0117 (2 bytes)  // Repeated trun on command
```

Assumptions: The first and last message is the ON command, not clear why it's repeated (maybe because the extractor did not reply?).  The middle message sets the extractor speed.

## Manual remote off [(Off.sr)](Off.sr)

For the second press of the manual remote, and all subsequent even numbered presses, the Bluetooth module send 4 separate messages to the extractor.

```
Message 1:
0xac17   (2 bytes)  // Turn extractor off command
[Delay of ~11.7 ms]

Message 2:
0x000123 (3 bytes)  // Reduce speed, with rate?
[Delay of ~277 ms]

Message 3:
0x00a0   (2 bytes)  // Unknown?
[Delay of ~689 ms]

Message 4:
0x0023   (2 bytes)  // Reduce speed to zero?
```

Assumptions: The first message is the off command. The second and 4th messages are to reduce the speed to zero. The function of the third message is not obvious.



