#include <VGAX.h>
#include <avr/pgmspace.h>
#include <SPI.h>

#define FRAME_LEN 8

volatile uint8_t frameBuf[FRAME_LEN];
volatile uint8_t rxIndex = 0;
volatile bool    syncing = false;
volatile bool    frameReady = false;

const uint8_t DIGIT_WIDTH  = 3;
const uint8_t DIGIT_HEIGHT = 5;

// Digit library
const uint8_t digit3x5[10][DIGIT_HEIGHT] PROGMEM = {
  { 0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  { 0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  { 0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  { 0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  { 0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  { 0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  { 0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  { 0b111, 0b001, 0b010, 0b010, 0b010}, // 7
  { 0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  { 0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

//font generated from BITFONZI - by Sandro Maffiodo
#define FNT_NANOFONT_HEIGHT 6
#define FNT_NANOFONT_SYMBOLS_COUNT 95

const unsigned char fnt_nanofont_data[FNT_NANOFONT_SYMBOLS_COUNT][1+FNT_NANOFONT_HEIGHT] PROGMEM={
{ 1, 128, 128, 128, 0, 128, 0, }, //glyph '!' code=0
{ 3, 160, 160, 0, 0, 0, 0, }, //glyph '"' code=1
{ 5, 80, 248, 80, 248, 80, 0, },  //glyph '#' code=2
{ 5, 120, 160, 112, 40, 240, 0, },  //glyph '$' code=3
{ 5, 136, 16, 32, 64, 136, 0, },  //glyph '%' code=4
{ 5, 96, 144, 104, 144, 104, 0, },  //glyph '&' code=5
{ 2, 128, 64, 0, 0, 0, 0, },  //glyph ''' code=6
{ 2, 64, 128, 128, 128, 64, 0, }, //glyph '(' code=7
{ 2, 128, 64, 64, 64, 128, 0, },  //glyph ')' code=8
{ 3, 0, 160, 64, 160, 0, 0, },  //glyph '*' code=9
{ 3, 0, 64, 224, 64, 0, 0, }, //glyph '+' code=10
{ 2, 0, 0, 0, 0, 128, 64, },  //glyph ',' code=11
{ 3, 0, 0, 224, 0, 0, 0, }, //glyph '-' code=12
{ 1, 0, 0, 0, 0, 128, 0, }, //glyph '.' code=13
{ 5, 8, 16, 32, 64, 128, 0, },  //glyph '/' code=14
{ 4, 96, 144, 144, 144, 96, 0, }, //glyph '0' code=15
{ 3, 64, 192, 64, 64, 224, 0, },  //glyph '1' code=16
{ 4, 224, 16, 96, 128, 240, 0, }, //glyph '2' code=17
{ 4, 224, 16, 96, 16, 224, 0, },  //glyph '3' code=18
{ 4, 144, 144, 240, 16, 16, 0, }, //glyph '4' code=19
{ 4, 240, 128, 224, 16, 224, 0, },  //glyph '5' code=20
{ 4, 96, 128, 224, 144, 96, 0, }, //glyph '6' code=21
{ 4, 240, 16, 32, 64, 64, 0, }, //glyph '7' code=22
{ 4, 96, 144, 96, 144, 96, 0, },  //glyph '8' code=23
{ 4, 96, 144, 112, 16, 96, 0, },  //glyph '9' code=24
{ 1, 0, 128, 0, 128, 0, 0, }, //glyph ':' code=25
{ 2, 0, 128, 0, 0, 128, 64, },  //glyph ';' code=26
{ 3, 32, 64, 128, 64, 32, 0, }, //glyph '<' code=27
{ 3, 0, 224, 0, 224, 0, 0, }, //glyph '=' code=28
{ 3, 128, 64, 32, 64, 128, 0, },  //glyph '>' code=29
{ 4, 224, 16, 96, 0, 64, 0, },  //glyph '?' code=30
{ 4, 96, 144, 176, 128, 112, 0, },  //glyph '@' code=31
{ 4, 96, 144, 240, 144, 144, 0, },  //glyph 'A' code=32
{ 4, 224, 144, 224, 144, 224, 0, }, //glyph 'B' code=33
{ 4, 112, 128, 128, 128, 112, 0, }, //glyph 'C' code=34
{ 4, 224, 144, 144, 144, 224, 0, }, //glyph 'D' code=35
{ 4, 240, 128, 224, 128, 240, 0, }, //glyph 'E' code=36
{ 4, 240, 128, 224, 128, 128, 0, }, //glyph 'F' code=37
{ 4, 112, 128, 176, 144, 112, 0, }, //glyph 'G' code=38
{ 4, 144, 144, 240, 144, 144, 0, }, //glyph 'H' code=39
{ 3, 224, 64, 64, 64, 224, 0, },  //glyph 'I' code=40
{ 4, 240, 16, 16, 144, 96, 0, },  //glyph 'J' code=41
{ 4, 144, 160, 192, 160, 144, 0, }, //glyph 'K' code=42
{ 4, 128, 128, 128, 128, 240, 0, }, //glyph 'L' code=43
{ 5, 136, 216, 168, 136, 136, 0, }, //glyph 'M' code=44
{ 4, 144, 208, 176, 144, 144, 0, }, //glyph 'N' code=45
{ 4, 96, 144, 144, 144, 96, 0, }, //glyph 'O' code=46
{ 4, 224, 144, 224, 128, 128, 0, }, //glyph 'P' code=47
{ 4, 96, 144, 144, 144, 96, 16, },  //glyph 'Q' code=48
{ 4, 224, 144, 224, 160, 144, 0, }, //glyph 'R' code=49
{ 4, 112, 128, 96, 16, 224, 0, }, //glyph 'S' code=50
{ 3, 224, 64, 64, 64, 64, 0, }, //glyph 'T' code=51
{ 4, 144, 144, 144, 144, 96, 0, },  //glyph 'U' code=52
{ 3, 160, 160, 160, 160, 64, 0, },  //glyph 'V' code=53
{ 5, 136, 168, 168, 168, 80, 0, },  //glyph 'W' code=54
{ 4, 144, 144, 96, 144, 144, 0, },  //glyph 'X' code=55
{ 3, 160, 160, 64, 64, 64, 0, },  //glyph 'Y' code=56
{ 4, 240, 16, 96, 128, 240, 0, }, //glyph 'Z' code=57
{ 2, 192, 128, 128, 128, 192, 0, }, //glyph '[' code=58
{ 5, 128, 64, 32, 16, 8, 0, },  //glyph '\' code=59
{ 2, 192, 64, 64, 64, 192, 0, },  //glyph ']' code=60
{ 5, 32, 80, 136, 0, 0, 0, }, //glyph '^' code=61
{ 4, 0, 0, 0, 0, 240, 0, }, //glyph '_' code=62
{ 2, 128, 64, 0, 0, 0, 0, },  //glyph '`' code=63
{ 3, 0, 224, 32, 224, 224, 0, },  //glyph 'a' code=64
{ 3, 128, 224, 160, 160, 224, 0, }, //glyph 'b' code=65
{ 3, 0, 224, 128, 128, 224, 0, }, //glyph 'c' code=66
{ 3, 32, 224, 160, 160, 224, 0, },  //glyph 'd' code=67
{ 3, 0, 224, 224, 128, 224, 0, }, //glyph 'e' code=68
{ 2, 64, 128, 192, 128, 128, 0, },  //glyph 'f' code=69
{ 3, 0, 224, 160, 224, 32, 224, },  //glyph 'g' code=70
{ 3, 128, 224, 160, 160, 160, 0, }, //glyph 'h' code=71
{ 1, 128, 0, 128, 128, 128, 0, }, //glyph 'i' code=72
{ 2, 0, 192, 64, 64, 64, 128, },  //glyph 'j' code=73
{ 3, 128, 160, 192, 160, 160, 0, }, //glyph 'k' code=74
{ 1, 128, 128, 128, 128, 128, 0, }, //glyph 'l' code=75
{ 5, 0, 248, 168, 168, 168, 0, }, //glyph 'm' code=76
{ 3, 0, 224, 160, 160, 160, 0, }, //glyph 'n' code=77
{ 3, 0, 224, 160, 160, 224, 0, }, //glyph 'o' code=78
{ 3, 0, 224, 160, 160, 224, 128, }, //glyph 'p' code=79
{ 3, 0, 224, 160, 160, 224, 32, },  //glyph 'q' code=80
{ 3, 0, 224, 128, 128, 128, 0, }, //glyph 'r' code=81
{ 2, 0, 192, 128, 64, 192, 0, },  //glyph 's' code=82
{ 3, 64, 224, 64, 64, 64, 0, }, //glyph 't' code=83
{ 3, 0, 160, 160, 160, 224, 0, }, //glyph 'u' code=84
{ 3, 0, 160, 160, 160, 64, 0, },  //glyph 'v' code=85
{ 5, 0, 168, 168, 168, 80, 0, },  //glyph 'w' code=86
{ 3, 0, 160, 64, 160, 160, 0, },  //glyph 'x' code=87
{ 3, 0, 160, 160, 224, 32, 224, },  //glyph 'y' code=88
{ 2, 0, 192, 64, 128, 192, 0, },  //glyph 'z' code=89
{ 3, 96, 64, 192, 64, 96, 0, }, //glyph '{' code=90
{ 1, 128, 128, 128, 128, 128, 0, }, //glyph '|' code=91
{ 3, 192, 64, 96, 64, 192, 0, },  //glyph '}' code=92
{ 3, 96, 192, 0, 0, 0, 0, },  //glyph '~' code=93
{ 4, 48, 64, 224, 64, 240, 0, },  //glyph 'Â£' code=94
};

// Strings stored in PROGMEM to display winner messages
static const char str0[] PROGMEM = "PLAYER 1 WINS";
static const char str1[] PROGMEM = "PLAYER 2 WINS";

VGAX vga; // init vga

////////////////////////////////////////////////////////////////////////
// ---------- STATIC GRAPHICS ---------- //

void graphics_digit(int x, int y, uint8_t score, byte color) {
  if (score > 9) return;
  for (uint8_t row = 0; row < DIGIT_HEIGHT; row++) {
    uint8_t bits = pgm_read_byte(&digit3x5[score][row]);
    for (uint8_t col = 0; col < DIGIT_WIDTH; col++) {
      if (bits & (1 << (DIGIT_WIDTH - 1 - col))) {
        vga.putpixel(x + col, y + row, color);
      }
    }
  }
}

void graphics_score(int s1, int s2) {
  int totalWidth = DIGIT_WIDTH + 1 + 1 + 1 + DIGIT_WIDTH;
  int startX = (VGAX_WIDTH - totalWidth) / 2;
  int y = 5;
  int x1 = startX;
  int colonX = x1 + DIGIT_WIDTH + 1;
  int x2 = colonX + 1 + 1;
  graphics_digit(x1, y, s1, 3);
  vga.putpixel(colonX, y + 1, 3);
  vga.putpixel(colonX, y + 3, 3);
  graphics_digit(x2, y, s2, 3);
}

void graphics_winner(int flags) {
  if (flags & 0x02) {
    vga.printPROGMEM((byte*)fnt_nanofont_data, FNT_NANOFONT_SYMBOLS_COUNT,
                     FNT_NANOFONT_HEIGHT, 3, 1, str0, 30, VGAX_HEIGHT/2-3, 3);
  } else if (flags & 0x04) {
    vga.printPROGMEM((byte*)fnt_nanofont_data, FNT_NANOFONT_SYMBOLS_COUNT,
                     FNT_NANOFONT_HEIGHT, 3, 1, str1, 30, VGAX_HEIGHT/2-3, 3);
  }
}

// ---------- SPRITE GRAPHICS ---------- //

void graphics_paddles(uint8_t p1_top, uint8_t p1_bottom, uint8_t p2_top, uint8_t p2_bottom) {
  const uint8_t paddleWidth = 3;

  // Player 1 paddle (left side)
  uint8_t p1_x = 5;
  for (uint8_t y = p1_top; y <= p1_bottom; y++) {
    for (uint8_t x = 0; x < paddleWidth; x++) {
      vga.putpixel(p1_x + x, y, 3);
    }
  }

  // Player 2 paddle (right side)
  uint8_t p2_x = VGAX_WIDTH - 5 - paddleWidth;
  for (uint8_t y = p2_top; y <= p2_bottom; y++) {
    for (uint8_t x = 0; x < paddleWidth; x++) {
      vga.putpixel(p2_x + x, y, 3);
    }
  }
}

void graphics_ball(uint8_t x, uint8_t y) {
  // Draw a 2x2 ball
  vga.putpixel(x,     y,     3);
  vga.putpixel(x + 1, y,     3);
  vga.putpixel(x,     y + 1, 3);
  vga.putpixel(x + 1, y + 1, 3);
}

////////////////////////////////////////////////////////////////////////

void pollSPI() {
// read spi
  while (SPSR & _BV(SPIF)) {
    uint8_t b = SPDR; 

// uses a sync marker to make sure we read at the start
    if (!syncing) {
      if (b == 0xAB) {
        syncing = true;
        rxIndex = 0;
      }
    } else {
      // Read in the actual updated SPI Frame
      if (b == 0xBA) {
        // USe of End marker to indicate the spi packet is read in correctly
        if (rxIndex == FRAME_LEN) {
          frameReady = true;
        }
        rxIndex = 0;
        syncing = false;
        break; // Exit after completing frame
      } else {
        // Regular data byte
        if (rxIndex < FRAME_LEN) {
          frameBuf[rxIndex++] = b;
        } else {
          // The frame was not read in properly reset / potentially skip frame
          syncing = false;
          rxIndex = 0;
        }
      }
    }
  }
}

void setup() {
  pinMode(MISO, OUTPUT);       // Slave out
  SPCR |= _BV(SPE);            // SPI slave enable
  SPCR &= ~_BV(SPIE);          // No interrupt (Does not work with VGA Library)

  vga.begin();                 // Init VGA lib
}

void loop() {
  pollSPI();

  if (frameReady) {
    // break down frame
    uint8_t flags       = frameBuf[0];
    uint8_t p1_top_y    = frameBuf[1];
    uint8_t p1_bottom_y = frameBuf[2];
    uint8_t p2_top_y    = frameBuf[3];
    uint8_t p2_bottom_y = frameBuf[4];
    uint8_t ball_x      = frameBuf[5];
    uint8_t ball_y      = frameBuf[6];
    uint8_t scores      = frameBuf[7];

    frameReady = false;

    // break down the scores packet
    uint8_t score1 = scores & 0x0F;
    uint8_t score2 = (scores >> 4) & 0x0F;

    vga.clear(0);

	// if frame is winner screen
    if (flags & 0x01) {
      graphics_winner(flags);
    } else {
      // draw the current game state
      graphics_paddles(p1_top_y, p1_bottom_y, p2_top_y, p2_bottom_y);
      graphics_ball(ball_x, ball_y);
      graphics_score(score1, score2);
    }

    VGAX::delay(1);
  }
}
