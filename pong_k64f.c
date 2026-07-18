#include "fsl_device_registers.h" 
#define FREQ_21_MHz 20971520u
#include "stdint.h"
#include <math.h>
#include <time.h>
#define MIN(a,b) ((a)<(b)?(a):(b))


/*------------RAND WITH PARAMETER----------*/
static inline float frand(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}
///////////////////////////////////////////////////////////////////////////////////
/* ---------- UART Init ---------- */
void UART0_Init(void)
{
    SIM_SCGC4 |= SIM_SCGC4_UART0_MASK;
    SIM_SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB_PCR16 = PORT_PCR_MUX(3);
    PORTB_PCR17 = PORT_PCR_MUX(3);
    UART0_C2 &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK);
    const uint32_t uart_clk = FREQ_21_MHz;
    uint16_t sbr = (uart_clk / (16u * 115200u));
    UART0_BDH = (sbr >> 8) & 0x1F;
    UART0_BDL = (sbr & 0xFF);
    UART0_C1 = 0x00;
    UART0_C2 |= (UART_C2_RIE_MASK | UART_C2_RE_MASK | UART_C2_TE_MASK);
    NVIC_EnableIRQ(UART0_RX_TX_IRQn);
}

/* ---------- SPI0 INIT ---------- */
static void SPI0_Init(void)
{
    // SPI CLOCK enable
    SIM_SCGC6 |= SIM_SCGC6_SPI0_MASK;          // SPI0

    // Configure the SPI PINS

    PORTD_PCR2 = PORT_PCR_MUX(2);             // ALT2 = SPI0 MOSI
    PORTD_PCR3 = PORT_PCR_MUX(2);             // ALT2 = SPI0 MISO
    PORTC_PCR5 = PORT_PCR_MUX(2);             // ALT2 = SPI0 SCK

    // SPI MASTER
    SPI0_MCR = SPI_MCR_MSTR_MASK   |          // master
               SPI_MCR_PCSIS(1 << 0) |        // PCS0 inactive high
               SPI_MCR_HALT_MASK;             // halt while we configure CTAR

    SPI0_CTAR0 = SPI_CTAR_FMSZ(7) |           // 8 bits
                 SPI_CTAR_CPOL(0) |           // Clock polarity (0)
                 SPI_CTAR_CPHA(0) |           // Leading edge
                 SPI_CTAR_PBR(0)  |           // presacle (2)
                 SPI_CTAR_BR(7);              // Scaler (16)
    SPI0_MCR &= ~SPI_MCR_HALT_MASK;           // SPI tran. Init
}
/* ---------- SPI0 TX ---------- */
static void SPI0_SendByte(uint8_t data)
{
    while (!(SPI0_SR & SPI_SR_TFFF_MASK)) {}	// Wait TX
    SPI0_PUSHR = SPI_PUSHR_PCS(1 << 0) | data;
    SPI0_SR |= SPI_SR_TFFF_MASK; // Clear TFFF

    // Wait for a received byte (This is needed as it's the way that SPI WORKS.; uses a handshake technique)
    while (!(SPI0_SR & SPI_SR_RFDF_MASK)) {}
    (void)SPI0_POPR;                          // just read; we don't care about what we read since we only need to send
    SPI0_SR |= SPI_SR_RFDF_MASK;              // clear RFDF flag
}
/* ---------- SPI0 FRAME TRANSMIT ---------- */
///////////////////////////////////////////////////////////////////////////////////
/* ---------- Game Init ---------- */
#define SCREEN_W 120
#define SCREEN_H 60
// BALL SPEED
#define BASE_SPEED 4.0f
#define SPEED_INCR 0.1f
#define MAX_SPEED  2.5f
typedef struct
{
  //game flags
  uint8_t FLAGS;
  //ball
  float bx, by;
  float dx, dy;
  float speed;
  // paddles positions
  uint8_t player_1_TOP, player_1_BOTTOM;
  uint8_t player_2_TOP, player_2_BOTTOM;
  // scores
  uint8_t scores;
  uint8_t score1;
  uint8_t score2;
} gameInstance;
// Player direction (1 == UP) (0 == STOP) (-1 == DOWN)
volatile int8_t player1_direction = 0;
volatile int8_t player2_direction = 0;
// start game instance (global)
volatile static gameInstance game;

///////////////////////////////////////////////////////////////////////////////////


static void paddleCollision(uint8_t leftPaddle)
{
    float paddleTop, paddleBottom, paddleHeight;

    if (leftPaddle) {
        paddleTop = game.player_1_TOP;
        paddleBottom = game.player_1_BOTTOM;
        game.bx = 4; // correction

    } else {
        paddleTop = game.player_2_TOP;
        paddleBottom = game.player_2_BOTTOM;
        game.bx = SCREEN_W - 6;
    }

    // compute the paddle center
    paddleHeight = (paddleBottom - paddleTop);

    float hitPos = (game.by - paddleTop) / paddleHeight;
    float angleMult;

    if (hitPos < 0.2) {
        angleMult = -1.5;
    }
    else if (hitPos < 0.4)
    {
        angleMult = -1.0;
    }
    else if (hitPos < 0.6)
    {
        angleMult = frand(-0.3,0.3);
    }
    else if (hitPos < 0.8)
    {
        angleMult = 1.0;
    }
    else {
        angleMult = 1.5;
    }

    if(leftPaddle) {
        game.dx = 1.0; // move right
    } else {
        game.dx = -1.0; // move left
    }

    game.dy = angleMult; // update the ball direction

    // small randomness
    if (frand(0,100) < 10) game.dy += (frand(0,100) < 50)? 0.5 : -0.5;

    //limit horizontal movement
    if (abs(game.dy) < 0.3) game.dy = (frand(0,100) < 50) ? 0.7 : -0.7;

    // limit angles
    if (game.dy > 1.5)  game.dy = 1.5;
    if (game.dy < -1.5) game.dy = -1.5;

    //speed incr
    game.speed = MIN(game.speed + SPEED_INCR, MAX_SPEED);
}

///////////////////////////////////////////////////////////////////////////////////

static void updateScore(uint8_t score1, uint8_t score2)
{
	game.scores = (score2 << 4) | (score1);
}
///////////////////////////////////////////////////////////////////////////////////
/* ---------- Game Functions ---------- */
/*RESET STATES*/
static void gameInit()
{
  // game state flags
  game.FLAGS = 0;
  // ball position and velocity
  game.bx = SCREEN_W / 2;
  game.by = SCREEN_H / 2;
  game.dx = 1.0f;
  game.dy = 1.0f;
  game.speed = BASE_SPEED;
  // player paddle locations these are the y locations we need to keep track to avoid moving the paddle outside frame
  game.player_1_BOTTOM = 40;
  game.player_1_TOP    = 20;
  game.player_2_BOTTOM = 40;
  game.player_2_TOP    = 20;
  // scores
  game.scores = 0;
}
static void ballReset(float direction)
{
    game.bx = SCREEN_W / 2;
    game.by = SCREEN_H / 2;
    game.dx = direction; // move to the player who lost point last round
    game.dy = 1.0;
}

/*UPDATE LOGIC*/
/////////////////////////////////////////////////////////////////////////////////////////
#define PADDLE_HEIGHT 20 // paddle dimensions needed for working with the game bounds
#define PADDLE_SPEED 2

static void updatePaddles()
{
    // Player 1 paddle move
    if (player1_direction == 1) {  // Moving UP
        // Move paddle up (decrease Y values)
        if (game.player_1_TOP > PADDLE_SPEED) {
            game.player_1_TOP -= PADDLE_SPEED;
            game.player_1_BOTTOM -= PADDLE_SPEED;
        } else {
            // Lock to top edge
            game.player_1_TOP = 0;
            game.player_1_BOTTOM = PADDLE_HEIGHT;
        }
    }
    else if (player1_direction == -1) {  // Moving DOWN
        // Move paddle down
        if (game.player_1_BOTTOM < (SCREEN_H - PADDLE_SPEED)) {
            game.player_1_TOP += PADDLE_SPEED;
            game.player_1_BOTTOM += PADDLE_SPEED;
        } else {
            // Lock to bottom edge
            game.player_1_BOTTOM = SCREEN_H - 1;
            game.player_1_TOP = SCREEN_H - 1 - PADDLE_HEIGHT;
        }
    }

    // Player 2 paddle move
    if (player2_direction == 1) {  // Moving UP
        if (game.player_2_TOP > PADDLE_SPEED) {
            game.player_2_TOP -= PADDLE_SPEED;
            game.player_2_BOTTOM -= PADDLE_SPEED;
        } else {
            game.player_2_TOP = 0;
            game.player_2_BOTTOM = PADDLE_HEIGHT;
        }
    }
    else if (player2_direction == -1) {  // Moving DOWN
        if (game.player_2_BOTTOM < (SCREEN_H - PADDLE_SPEED)) {
            game.player_2_TOP += PADDLE_SPEED;
            game.player_2_BOTTOM += PADDLE_SPEED;
        } else {
            game.player_2_BOTTOM = SCREEN_H - 1;
            game.player_2_TOP = SCREEN_H - 1 - PADDLE_HEIGHT;
        }
    }
}
/////////////////////////////////////////////////////////////////////////////////////////
static void updateBall()
{

    game.bx += game.dx * game.speed;
    game.by += game.dy * game.speed;

    // Top and bottom wall collision
    if (game.by <= 1) {
        game.by = 1;
        game.dy = fabsf(game.dy);
    }
    if (game.by >= (SCREEN_H - 3)) {
        game.by = SCREEN_H - 3;
        game.dy = -fabsf(game.dy);
    }

    // Left paddle collision
    if (game.bx <= 4 && game.dx < 0) {
        if(game.by + 1 >= game.player_1_TOP && game.by <= game.player_1_BOTTOM) {
            paddleCollision(1);
        } else if (game.bx <= 0) {
            // Player 2 scores
            game.score2++;
            game.speed = BASE_SPEED;
            updateScore(game.score1, game.score2);
            if (game.score2 == 7) {
            	game.FLAGS |= 0x01;
                game.FLAGS |= 0x04; // Player 2 wins
                return;
            }
            ballReset(1.0f);
        }
    }
    // Right paddle collision
    if(game.bx >= (SCREEN_W - 6) && game.dx > 0) {
        if(game.by + 1 >= game.player_2_TOP && game.by <= game.player_2_BOTTOM) {
            paddleCollision(0);
        } else if (game.bx >= (SCREEN_W - 2)) {
            // Player 1 scores
            game.score1++;
            game.speed = BASE_SPEED;
            updateScore(game.score1, game.score2);
            if (game.score1 == 7) {
            	game.FLAGS |= 0x01;
                game.FLAGS |= 0x02; // Player 1 wins
                return;
            }
            ballReset(-1.0f);
        }
    }


}

/////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////
// FRAME FLAG
volatile uint8_t frame = 0;
void main(void)
{
    // srand seed
    srand(123456);

    // Enable Port A, B, C, D clock gating
    SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK |
                 SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK;
    // Configure PORTA & PORTB as before (buttons + interrupts)
    PORTA_PCR2 = 0xB0100; //changed to interrupt on either edge
    PORTA_PCR1 = 0xB0100;
    PORTB_PCR2 = 0xB0100;
    PORTB_PCR3 = 0xB0100;
    GPIOA_PDDR &= ~((1 << 2) | (1 << 1));
    GPIOB_PDDR &= ~((1 << 2) | (1 << 3));
    PORTA_ISFR = (1 << 2) | (1 << 1);
    PORTB_ISFR = (1 << 2) | (1 << 3);
    NVIC_EnableIRQ(PORTA_IRQn);
    NVIC_EnableIRQ(PORTB_IRQn);
    // ------ TIMER INIT ------ //
    SIM_SCGC3 |= SIM_SCGC3_FTM3_MASK;
    PORTC_PCR10 = 0x300; 			// PORTC PIN 10
    FTM3_MODE = 0xD; 	 			// ENABLE FTM3
    FTM3_MOD = 21844 * 2;				// Used to get close to 15Hz
    FTM3_SC = 0x0E;      			// System Clock / 32
    NVIC_EnableIRQ(FTM3_IRQn);		// Enable FTM3 interrupts
    FTM3_SC |= FTM_SC_TOIE_MASK;	// ENABLE TOF interrupt (bit 6)
    FTM3_CNT = 0;					// Reset counter
    // int SPI0 on PTD2/PTD3/PTC5
    SPI0_Init();
    UART0_Init();
    gameInit();

    for (;;)
    {
    	// all updates through interrupts ( :) )
    }
}
///////////////////////////////////////////////////////////////////////////////////
// TIMER (Frame Updates)
/*
 * 15 Frames per second.
 * Interrupt every 1/15 sec to update the game frame
 * Transmit the updated frame
 */
	/*
	 * UPDATES
	 * Update Paddles
	 * Update Ball
	 * Update Score
	 * Update Game State
	 */

void FTM3_IRQHandler(void)
{
    if (FTM3_SC & FTM_SC_TOF_MASK) {
        FTM3_SC &= ~FTM_SC_TOF_MASK;


        if (!(game.FLAGS && 0x01)) {
			updatePaddles();
			updateBall();

`
			SPI0_SendByte(0xAB);
			for(volatile int i=0; i<20000; i++);  // Delays transmit to ensure that Arduino (Bottleneck) properly receives SPI frames

			SPI0_SendByte(game.FLAGS);
			for(volatile int i=0; i<20000; i++);

			SPI0_SendByte(game.player_1_TOP);
			for(volatile int i=0; i<20000; i++);

			SPI0_SendByte(game.player_1_BOTTOM);
			for(volatile int i=0; i<20000; i++);

			SPI0_SendByte(game.player_2_TOP);
			for(volatile int i=0; i<20000; i++);

			SPI0_SendByte(game.player_2_BOTTOM);
			for(volatile int i=0; i<20000; i++);

			uint8_t send_bx = (uint8_t) game.bx;
			uint8_t send_by = (uint8_t) game.by;

			SPI0_SendByte(send_bx);
			for(volatile int i=0; i<20000; i++);

			SPI0_SendByte(send_by);
			for(volatile int i=0; i<20000; i++);

			SPI0_SendByte(game.scores);
			for(volatile int i=0; i<20000; i++);

			SPI0_SendByte(0xBA);
        }
    }
}

// Handles the position of player one paddle
void PORTA_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(PORTA_IRQn);

    // Player 1 UP (PA2)
    if (PORTA_ISFR & (1 << 2)) {
        if (GPIOA_PDIR & (1 << 2)) {
            // Released
            player1_direction = 1;
        } else {
            // Pressed
            player1_direction = 0; // UP
        }
    }

    // Player 1 DOWN (PA1)
    if (PORTA_ISFR & (1 << 1)) {
        if (GPIOA_PDIR & (1 << 1)) {
            // Released
            player1_direction = -1;
        } else {
            // Pressed
            player1_direction = 0; // DOWN
        }
    }

    PORTA_ISFR = (1 << 2) | (1 << 1);
}

// Handles the position of player two paddle
void PORTB_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(PORTB_IRQn);

    // Player 2 UP (PB2)
    if (PORTB_ISFR & (1 << 2)) {
        if (GPIOB_PDIR & (1 << 2)) {
            player2_direction = 1;   // Released
        } else {
            player2_direction = 0;   // Pressed = UP
        }
    }

    // Player 2 DOWN (PB3)
    if (PORTB_ISFR & (1 << 3)) {
        if (GPIOB_PDIR & (1 << 3)) {
            player2_direction = -1;   // Released
        } else {
            player2_direction = 0;  // Pressed = DOWN
        }
    }

    PORTB_ISFR = (1 << 2) | (1 << 3);
}

// BUILD UPON - Hand PADDLE CONTROL
void UART0_RX_TX_IRQHandler(void)
{
    if (UART0_S1 & UART_S1_RDRF_MASK)
    {
      // update player paddle direction based on hand coordination
        uint8_t c = UART0_D;
        if (c & (1 << 0)) player1_direction =  1;
        if (c & (1 << 1)) player1_direction = -1;
        if (c & (1 << 2)) player2_direction =  1;
        if (c & (1 << 3)) player2_direction = -1;
    }
}
