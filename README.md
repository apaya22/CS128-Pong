# Two-Board Pong on FRDM-K64F and Arduino (VGA Output)

A hardware Pong game built for CS128. Game logic and all input handling run bare-metal on an NXP FRDM-K64F (ARM Cortex-M4), while an Elegoo Arduino Uno (ATmega328P) acts as a dedicated display driver and renders the game to a VGA monitor. The two boards talk over a custom SPI protocol, with the K64F as master and the Arduino as slave.

Players can control their paddles two ways: physical push buttons wired to GPIO interrupts, or hand gestures detected by a MediaPipe hand-tracking script running on a host computer and streamed to the K64F over UART.

**Authors:** Andy Payan, Edward Almaraz
**Course:** CS128 Final Project

## Features

- Full game of Pong (ball physics, paddle collision with angle zones, scoring, first-to-7 win condition) computed entirely on the K64F.
- Fully interrupt-driven firmware: GPIO edge interrupts for buttons, UART RX interrupt for gesture data, and an FTM timer overflow interrupt driving frame updates at roughly 15 Hz.
- Two independent control modes per player: physical buttons or MediaPipe hand gestures.
- Custom byte-framed SPI protocol that transmits only the critical sprite coordinates each frame rather than a full framebuffer.
- Bare-metal peripheral setup on the K64F (SPI0, UART0, FTM3, GPIO) done through direct register writes, without Processor Expert.
- VGA rendering on the Arduino using the VGAX library, including paddles, ball, a 3x5 bitmap score font, and winner text.

## System Architecture

The design splits responsibilities cleanly across two microcontrollers so that each one does the work it is best suited for.

**FRDM-K64F (the brain):**
- Runs all game logic: ball movement, paddle updates, collision detection, scoring, and win state.
- Handles all input through interrupts (buttons and gesture data).
- Packages the current game state and transmits it to the Arduino as SPI master.

**Arduino Uno (the display driver):**
- Receives sprite coordinates over SPI as a slave.
- Draws the current game state to a VGA monitor.
- Is solely responsible for rendering and refreshing visuals; it holds no game logic.

The main loop on the K64F is effectively empty. Everything happens inside interrupt handlers, which keeps timing deterministic and the design easy to reason about.

## Hardware

- NXP FRDM-K64F development board (ARM Cortex-M4)
- Elegoo Arduino Uno R3 (ATmega328P)
- VGA monitor plus a VGA connector broken out to the Arduino through a resistor ladder
- Four push buttons (two per player) on breadboards
- Host computer running the MediaPipe gesture script (only needed for gesture mode)
- A webcam (only needed for gesture mode)

### K64F pin usage

| Function | Pins | Notes |
|---|---|---|
| SPI0 MOSI / MISO / SCK | PTD2 / PTD3 / PTC5 | ALT2, PCS0 chip select |
| UART0 RX / TX | PTB16 / PTB17 | ALT3, 115200 baud |
| Player 1 buttons | PA1, PA2 | GPIO interrupt on both edges |
| Player 2 buttons | PB2, PB3 | GPIO interrupt on both edges |
| Frame timer | FTM3 | Overflow interrupt at roughly 15 Hz |

## Control Modes

**Button mode.** Each player gets an up and a down button. Because the GPIO interrupts fire on both edges, holding a button down keeps the paddle moving continuously and releasing it stops the paddle. The button ISRs only set a small direction flag (`player1_direction`, `player2_direction`); the actual paddle movement is applied later inside the frame update, which keeps the ISRs short.

**Gesture mode.** A Python script on the host computer uses MediaPipe Hands and OpenCV to track both hands from a webcam. Each hand maps to a player based on which side of the frame it is on. Thumb raised moves that player's paddle up, thumb lowered moves it down, and a neutral hand stops it. The script encodes both players' directions into a single byte and streams it over UART to the K64F at 115200 baud, where the UART RX interrupt decodes the bits and updates the same direction flags used by button mode.

The gesture packet uses one bit per action:

| Bit | Meaning |
|---|---|
| 0 | Player 1 up |
| 1 | Player 1 down |
| 2 | Player 2 up |
| 3 | Player 2 down |

## SPI Frame Protocol

Rather than shipping an entire framebuffer across SPI (which the Arduino could not keep up with at higher baud rates), the K64F sends only the handful of values needed to reconstruct the frame. Each frame is 8 data bytes wrapped in sync markers so the Arduino can reliably find frame boundaries.

```
0xAB   start-of-frame marker
[0]    FLAGS          (game state: win flag, which player won)
[1]    player1_top    (left paddle top y)
[2]    player1_bottom (left paddle bottom y)
[3]    player2_top    (right paddle top y)
[4]    player2_bottom (right paddle bottom y)
[5]    ball_x
[6]    ball_y
[7]    scores         (player 1 in low nibble, player 2 in high nibble)
0xBA   end-of-frame marker
```

On the Arduino side, an SPI poll routine watches for the `0xAB` start marker, buffers the next 8 bytes, and only marks a frame ready when it sees the `0xBA` end marker at the expected position. If the byte count is wrong, the partial frame is discarded and the reader resyncs on the next start marker. SPI interrupts are not used on the Arduino because they conflict with the VGAX library, so the frame reader polls instead.

Scores are packed into a single byte to save bandwidth: player 1's score sits in the low nibble and player 2's in the high nibble, unpacked with a mask and a shift on the Arduino.

## How a Frame Comes Together

1. The FTM3 timer overflows roughly 15 times per second and fires its interrupt.
2. Inside that ISR, the K64F updates paddle positions (from the current direction flags), advances the ball, checks wall and paddle collisions, and updates the score and win state.
3. The K64F transmits the new state to the Arduino using the SPI frame format above.
4. The Arduino reads the completed frame, clears the screen, and redraws the paddles, ball, and score, or a winner message if the win flag is set.

Ball collisions add a bit of character: the bounce angle depends on where the ball hits the paddle (five vertical zones), speed ramps up slightly on each paddle hit up to a cap, and a small amount of randomness is injected so rallies do not become perfectly repetitive. First player to 7 points wins.

## Technical Challenges

**SPI, button interrupts, and the camera stream all fighting over Processor Expert.** Configuring SPI alongside the GPIO interrupts and UART through Processor Expert produced redefinition conflicts. The fix was to drop Processor Expert entirely and configure the pins, clocks, and registers by hand. Once the SPI pins and control registers were set manually, the K64F and Arduino communicated cleanly.

**Bandwidth between the boards.** The original plan was to send full display-resolution frames over SPI, but the Arduino could not receive full frames reliably as the baud rate climbed. The solution was to send only the critical sprite coordinates each frame instead of the whole framebuffer. Given the Arduino's receive bottleneck, roughly 15 FPS turned out to be the stable sweet spot for smooth play.

## Repository Layout

```
.
├── k64f/          # Bare-metal C firmware: game logic, SPI0, UART0, FTM3, GPIO interrupts
├── arduino/       # Arduino display driver (VGAX rendering, SPI slave frame reader)
├── gesture/       # Python MediaPipe hand-tracking script (UART sender)
└── docs/          # Report, flowchart, circuit schematic, demo
```

Adjust these paths to match how you commit the files.

## Building and Running

**K64F firmware.** Build and flash with your usual FRDM-K64F toolchain (for example MCUXpresso). The firmware sets up all peripherals in `main` and then runs everything from interrupts.

**Arduino display.** Open the Arduino sketch, install the VGAX library, and upload to the Uno. Wire the VGA connector through the resistor ladder as shown in the circuit schematic in `docs/`.

**Gesture mode (optional).** On the host computer:

```
pip install opencv-python mediapipe pyserial
```

Set the serial `port` in the script to your K64F's device path, connect a webcam, and run the script. Button mode works with no host computer attached.

## Notes

- The internal game coordinate space is 120 by 60, matching the VGAX output resolution.
- Frame rate is intentionally held around 15 FPS to stay within the Arduino's SPI receive limit.
