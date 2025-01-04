#include "timerISR-Fixed.h"
#include "helper.h"
#include "periph.h"
#include "serialATmega.h"
#include "spiAVR.h"
#include "akoth013_sprites.h"
#include "math.h"
#include <stdio.h>
#include <stdlib.h>

#define NUM_TASKS 3 // TODO: Change to the number of tasks being used

// Task struct for concurrent synchSMs implmentations
typedef struct _task
{
    signed char state;         // Task's current state
    unsigned long period;      // Task period
    unsigned long elapsedTime; // Time elapsed since last task tick
    int (*TickFct)(int);       // Task tick function
} task;

// TODO: Define Periods for each task
//  e.g. const unsined long TASK1_PERIOD = <PERIOD>
const unsigned long GCD_PERIOD = 100; // TODO:Set the GCD Period

task tasks[NUM_TASKS]; // declared task array with 5 tasks

// TODO: Declare your tasks' function and their states here

// code template for enums and tick functions
/*
enum _States
{
    _INIT,
};
int TickFct_(int state)
{
    // state transitions
    switch (state)
    {
    case _INIT:
        state =
        break;
    default:
        break;
    }

    // state actions
    switch (state)
    {
    case _INIT:
        break;
    default:
        break;
    }

    return state;
}
*/

void TimerISR()
{
    for (unsigned int i = 0; i < NUM_TASKS; i++)
    { // Iterate through each task in the task array
        if (tasks[i].elapsedTime == tasks[i].period)
        {                                                      // Check if the task is ready to tick
            tasks[i].state = tasks[i].TickFct(tasks[i].state); // Tick and set the next state for this task
            tasks[i].elapsedTime = 0;                          // Reset the elapsed time for the next tick
        }
        tasks[i].elapsedTime += GCD_PERIOD; // Increment the elapsed time by GCD_PERIOD
    }
}

void Send_Command(unsigned char command)
{
    PORTB = SetBit(PORTB, 1, 0);
    SPI_SEND(command);
}

void Send_Data(unsigned char data)
{
    PORTB = SetBit(PORTB, 1, 1);
    SPI_SEND(data);
}

// declare game states up here for gameState variable
enum GS_States
{
    GS_INIT,
    START_MENU,
    CHOOSE_MODE,
    CLASSIC_SHOT,
    GHOST_SHOT,
    GAME_OVER
};

// global variables
// range for generating random coords
unsigned short targetMinX = 20, targetMaxX = 300, targetMinY = 100, targetMaxY = 380;
unsigned short targetX = 0, targetY = 0;
// starting position of crosshair in the middle of screen: (160, 240)
unsigned short startCrosshairX = 160, startCrosshairY = 240;
unsigned short crosshairX = startCrosshairX, crosshairY = startCrosshairY;
// store old coords of crosshair to clear old position
unsigned short oldCrosshairX = 160, oldCrosshairY = 240;
unsigned char targetRadius = 16;
// how fast the crosshair can move
unsigned char crosshairSpeed = 2;
// background color
unsigned char bgCol[3] = {70, 130, 180};
// x and y for joystick and its mapped values
unsigned short joystickX = 0, joystickY = 0;
signed short mappedX = 0, mappedY = 0;
GS_States gameState = GS_INIT;
// square border for game
unsigned short borderMinX = 0, borderMaxX = 319;
unsigned short borderMinY = 80, borderMaxY = 400;
// original gun dimensions
unsigned char awpWidth = 72, awpHeight = 18;
// vertical offset for logo on start menu
unsigned char logoOffset = 100;
// timers for game
unsigned short gameTimer = 0;
unsigned short gameTimerMax = 200;
unsigned char shotClock = 0;
unsigned char shotClockMaxClassic = 30, shotClockMaxGhost = 50;
unsigned char blinkCounter = 0;
unsigned char blinkCounterMax = 10;
// boolean for gamemode. true = classic, false = ghost
bool isClassic = true;
// boolean for tracking if shot has been taken and for making sure if shot button is held down, it shoots only once
bool isShot = false, isShotHeld = false;
// how long target is flashed for in ghost shot
unsigned char ghostFlash = 2;
// score trackers
unsigned short currScoreClassic, currScoreGhost, topScoreClassic, topScoreGhost = 0;
// bool for if there is a new record
bool isNewRecord = false;
// char array for printing score and timers
char buffer[4]; // Enough to hold an unsigned short as a string (max 3 digits + null terminator)

// helper functions
// generates random coords for target
void randomCoords()
{
    targetX = (rand() % (targetMaxX - targetMinY)) + targetMinX;
    targetY = (rand() % (targetMaxY - targetMinY)) + targetMinY;
}

// calculates distance between 2 coords to help calculate scoring
unsigned short distance(unsigned short x1, unsigned short y1, unsigned short x2, unsigned short y2)
{
    return sqrt(pow(abs(x2 - x1), 2) + pow(abs(y2 - y1), 2));
}

// helper function to set the color of 1 pixel
void setColor(unsigned char r, unsigned char g, unsigned char b)
{
    // 16 bit COLMOD
    // SPI_SEND(B B B B B G G G) (8 bits bottom)
    // SPI_SEND(G G G R R R R R) (8 bits top)
    // Send_Data((b << 3) | (g >> 3));
    // Send_Data((g << 3) | (r & 0x1F));

    // 18 bit COLMOD
    // SPI_SEND(B B B B B B 0 0) (6 bits blue)
    // SPI_SEND(G G G G G G 0 0) (6 bits green)
    // SPI_SEND(R R R R R R 0 0) (6 bits red)
    Send_Data(b & 0xFC);
    Send_Data(g & 0xFC);
    Send_Data(r & 0xFC);
}

// draw functions for static parts of screen
void setBackground()
{
    // CASET
    Send_Command(0x2A);
    // x start
    Send_Data(0x00);
    Send_Data(0x00);
    // x end
    Send_Data(0x01);
    Send_Data(0x40);

    // RASET
    Send_Command(0x2B);
    // y start
    Send_Data(0x00);
    Send_Data(0x00);
    // y end
    Send_Data(0x01);
    Send_Data(0xE0);

    // color
    Send_Command(0x2C);
    for (unsigned short i = 0; i < 320; i++)
    {
        for (unsigned short j = 0; j < 480; j++)
        {
            setColor(bgCol[0], bgCol[1], bgCol[2]);
        }
    }
}

// color = 0: background color, color = 1: black, color = 2: white
void drawBorder(unsigned short minX, unsigned short maxX, unsigned short minY, unsigned short maxY, unsigned char color)
{
    // draw left border
    // CASET
    Send_Command(0x2A);
    // x start
    Send_Data(minX >> 8);
    Send_Data(minX);
    // x end
    Send_Data(minX >> 8);
    Send_Data(minX);

    // RASET
    Send_Command(0x2B);
    // y start
    Send_Data(minY >> 8);
    Send_Data(minY);
    // y end
    Send_Data(maxY >> 8);
    Send_Data(maxY);

    // color
    Send_Command(0x2C);
    switch (color)
    {
    case 0:
        for (unsigned short i = 0; i <= maxY; i++)
        {
            setColor(bgCol[0], bgCol[1], bgCol[2]);
        }
        break;
    case 1:
        for (unsigned short i = 0; i <= maxY; i++)
        {
            setColor(0, 0, 0);
        }
        break;
    case 2:
        for (unsigned short i = 0; i <= maxY; i++)
        {
            setColor(255, 255, 255);
        }
        break;
    default:
        break;
    }

    // draw right border
    // CASET
    Send_Command(0x2A);
    // x start
    Send_Data(maxX >> 8);
    Send_Data(maxX);
    // x end
    Send_Data(maxX >> 8);
    Send_Data(maxX);

    // RASET
    Send_Command(0x2B);
    // y start
    Send_Data(minY >> 8);
    Send_Data(minY);
    // y end
    Send_Data(maxY >> 8);
    Send_Data(maxY);

    // color
    Send_Command(0x2C);
    switch (color)
    {
    case 0:
        for (unsigned short i = 0; i <= maxY; i++)
        {
            setColor(bgCol[0], bgCol[1], bgCol[2]);
        }
        break;
    case 1:
        for (unsigned short i = 0; i <= maxY; i++)
        {
            setColor(0, 0, 0);
        }
        break;
    case 2:
        for (unsigned short i = 0; i <= maxY; i++)
        {
            setColor(255, 255, 255);
        }
        break;
    default:
        break;
    }

    // draw bottom border
    // CASET
    Send_Command(0x2A);
    // x start
    Send_Data(minX >> 8);
    Send_Data(minX);
    // x end
    Send_Data(maxX >> 8);
    Send_Data(maxX);

    // RASET
    Send_Command(0x2B);
    // y start
    Send_Data(minY >> 8);
    Send_Data(minY);
    // y end
    Send_Data(minY >> 8);
    Send_Data(minY);

    // color
    Send_Command(0x2C);
    switch (color)
    {
    case 0:
        for (unsigned short i = 0; i <= maxX; i++)
        {
            setColor(bgCol[0], bgCol[1], bgCol[2]);
        }
        break;
    case 1:
        for (unsigned short i = 0; i <= maxX; i++)
        {
            setColor(0, 0, 0);
        }
        break;
    case 2:
        for (unsigned short i = 0; i <= maxX; i++)
        {
            setColor(255, 255, 255);
        }
        break;
    default:
        break;
    }

    // draw top border
    // CASET
    Send_Command(0x2A);
    // x start
    Send_Data(minX >> 8);
    Send_Data(minX);
    // x end
    Send_Data(maxX >> 8);
    Send_Data(maxX);

    // RASET
    Send_Command(0x2B);
    // y start
    Send_Data((maxY + 1) >> 8);
    Send_Data(maxY + 1);
    // y end
    Send_Data((maxY + 1) >> 8);
    Send_Data(maxY + 1);

    // color
    Send_Command(0x2C);
    switch (color)
    {
    case 0:
        for (unsigned short i = 0; i <= maxX; i++)
        {
            setColor(bgCol[0], bgCol[1], bgCol[2]);
        }
        break;
    case 1:
        for (unsigned short i = 0; i <= maxX; i++)
        {
            setColor(0, 0, 0);
        }
        break;
    case 2:
        for (unsigned short i = 0; i <= maxX; i++)
        {
            setColor(255, 255, 255);
        }
        break;
    default:
        break;
    }
}

// for updating crosshair position
void clearCrosshair(unsigned short x, unsigned short y)
{
    // CASET
    Send_Command(0x2A);
    // x start
    unsigned short xStart = x - targetRadius + 1;
    Send_Data(xStart >> 8);
    Send_Data(xStart);
    // x end
    unsigned short xEnd = x + targetRadius;
    Send_Data(xEnd >> 8);
    Send_Data(xEnd);

    // RASET
    Send_Command(0x2B);
    // y start
    unsigned short yStart = y - targetRadius + 1;
    Send_Data(yStart >> 8);
    Send_Data(yStart);
    // y end
    unsigned short yEnd = y + targetRadius;
    Send_Data(yEnd >> 8);
    Send_Data(yEnd);

    // color
    Send_Command(0x2C);
    for (unsigned short i = 0; i < 2 * targetRadius; i++)
    {
        for (unsigned short j = 0; j < 2 * targetRadius; j++)
        {
            setColor(bgCol[0], bgCol[1], bgCol[2]);
        }
    }
}

void drawCrosshair(unsigned short x, unsigned short y)
{
    // CASET
    Send_Command(0x2A);
    // x start
    unsigned short xStart = x - targetRadius + 1;
    Send_Data(xStart >> 8);
    Send_Data(xStart);
    // x end
    unsigned short xEnd = x + targetRadius;
    Send_Data(xEnd >> 8);
    Send_Data(xEnd);

    // RASET
    Send_Command(0x2B);
    // y start
    unsigned short yStart = y - targetRadius + 1;
    Send_Data(yStart >> 8);
    Send_Data(yStart);
    // y end
    unsigned short yEnd = y + targetRadius;
    Send_Data(yEnd >> 8);
    Send_Data(yEnd);

    // color
    Send_Command(0x2C);
    for (unsigned short i = 0; i < 2 * targetRadius; i++)
    {
        for (unsigned short j = 0; j < 2 * targetRadius; j++)
        {
            // draw the circle with the crosshair and create the plus manually to keep it centered
            if (pgm_read_byte(&(crosshair[i / 2][j / 2])) || j == targetRadius - 1 || j == targetRadius || i == targetRadius - 1 || i == targetRadius)
            {
                setColor(0, 0, 0);
            }
            else
            {
                setColor(bgCol[0], bgCol[1], bgCol[2]);
            }
        }
    }
}

void drawTarget(unsigned short x, unsigned short y)
{
    // CASET
    Send_Command(0x2A);
    // x start
    unsigned short xStart = x - targetRadius + 1;
    Send_Data(xStart >> 8);
    Send_Data(xStart);
    // x end
    unsigned short xEnd = x + targetRadius;
    Send_Data(xEnd >> 8);
    Send_Data(xEnd);

    // RASET
    Send_Command(0x2B);
    // y start
    unsigned short yStart = y - targetRadius + 1;
    Send_Data(yStart >> 8);
    Send_Data(yStart);
    // y end
    unsigned short yEnd = y + targetRadius;
    Send_Data(yEnd >> 8);
    Send_Data(yEnd);

    // color
    Send_Command(0x2C);
    for (unsigned short i = 0; i < 2 * targetRadius; i++)
    {
        for (unsigned short j = 0; j < 2 * targetRadius; j++)
        {
            unsigned char pixel = pgm_read_byte(&(target[i / 2][j / 2]));
            switch (pixel)
            {
            case 0:
                setColor(bgCol[0], bgCol[1], bgCol[2]);
                break;
            case 1:
                setColor(255, 0, 0);
                break;
            case 2:
                setColor(255, 255, 255);
                break;
            default:
                break;
            }
        }
    }
}

void drawGhost(unsigned short x, unsigned short y)
{
    // CASET
    Send_Command(0x2A);
    // x start
    unsigned short xStart = x - targetRadius + 1;
    Send_Data(xStart >> 8);
    Send_Data(xStart);
    // x end
    unsigned short xEnd = x + targetRadius;
    Send_Data(xEnd >> 8);
    Send_Data(xEnd);

    // RASET
    Send_Command(0x2B);
    // y start
    unsigned short yStart = y - targetRadius + 1;
    Send_Data(yStart >> 8);
    Send_Data(yStart);
    // y end
    unsigned short yEnd = y + targetRadius;
    Send_Data(yEnd >> 8);
    Send_Data(yEnd);

    // color
    Send_Command(0x2C);
    for (unsigned short i = 0; i < 2 * targetRadius; i++)
    {
        for (unsigned short j = 0; j < 2 * targetRadius; j++)
        {
            if (i == j || i == targetRadius * 2 - j || i == j - 1 || i == targetRadius * 2 - j - 1)
            {
                setColor(0, 0, 0);
            }
            else
            {
                unsigned char pixel = pgm_read_byte(&(target[i / 2][j / 2]));
                switch (pixel)
                {
                case 0:
                    setColor(bgCol[0], bgCol[1], bgCol[2]);
                    break;
                case 1:
                    setColor(255, 0, 0);
                    break;
                case 2:
                    setColor(255, 255, 255);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

void drawAWP(unsigned short x, unsigned short y, unsigned char scale, bool isReversed)
{
    // CASET
    Send_Command(0x2A);
    // x start
    unsigned short xStart = x;
    Send_Data(xStart >> 8);
    Send_Data(xStart);
    // x end
    unsigned short xEnd = x + awpWidth * scale - 1;
    Send_Data(xEnd >> 8);
    Send_Data(xEnd);

    // RASET
    Send_Command(0x2B);
    // y start
    unsigned short yStart = y;
    Send_Data(yStart >> 8);
    Send_Data(yStart);
    // y end
    unsigned short yEnd = y + awpHeight * scale - 1;
    Send_Data(yEnd >> 8);
    Send_Data(yEnd);

    // color
    Send_Command(0x2C);
    // if isReversed is false, display as normal, else flip image horizontally
    if (!isReversed)
    {
        for (unsigned short i = 0; i < awpHeight * scale; i++)
        {
            for (unsigned short j = 0; j < awpWidth * scale; j++) // 72 since that's the original width
            {
                // Calculate which compressed column we're in and which of the 4 pixels we need
                unsigned short compressedCol = j / (4 * scale); // Get the compressed column
                unsigned char pixelOffset = (j / scale) % 4;    // Which of the 4 pixels in the compressed byte

                unsigned char pixels = pgm_read_word(&(awp_compressed[i / scale][compressedCol]));
                unsigned char pixel = (pixels >> (6 - (pixelOffset * 2))) & 0x03;

                switch (pixel)
                {
                case 0:
                    setColor(bgCol[0], bgCol[1], bgCol[2]);
                    break;
                case 1:
                    setColor(0, 0, 0);
                    break;
                case 2:
                    setColor(75, 83, 32);
                    break;
                case 3:
                    setColor(104, 105, 105);
                    break;
                default:
                    break;
                }
            }
        }
    }
    else
    {
        // make the image reversible by traversing X backwards
        for (unsigned short i = 0; i < awpHeight * scale; i++)
        {
            for (unsigned short j = 0; j < awpWidth * scale; j++)
            {
                // Calculate which compressed column we're in, starting from the right side
                unsigned short compressedCol = 17 - (j / (4 * scale)); // Changed to read from right to left
                unsigned char pixelOffset = 3 - ((j / scale) % 4);     // Reverse pixel order within each byte

                unsigned char pixels = pgm_read_word(&(awp_compressed[i / scale][compressedCol]));
                unsigned char pixel = (pixels >> (6 - (pixelOffset * 2))) & 0x03;

                switch (pixel)
                {
                case 0:
                    setColor(bgCol[0], bgCol[1], bgCol[2]);
                    break;
                case 1:
                    setColor(0, 0, 0);
                    break;
                case 2:
                    setColor(75, 83, 32);
                    break;
                case 3:
                    setColor(104, 105, 105);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

void drawCharacter(unsigned short x, unsigned short y, unsigned char scale, bool isLetter, unsigned char index)
{
    // CASET
    Send_Command(0x2A);
    // x start
    unsigned short xStart = x;
    Send_Data(xStart >> 8);
    Send_Data(xStart);
    // x end
    unsigned short xEnd = x + 8 * scale - 1;
    Send_Data(xEnd >> 8);
    Send_Data(xEnd);

    // RASET
    Send_Command(0x2B);
    // y start
    unsigned short yStart = y;
    Send_Data(yStart >> 8);
    Send_Data(yStart);
    // y end
    unsigned short yEnd = y + 8 * scale - 1;
    Send_Data(yEnd >> 8);
    Send_Data(yEnd);

    // color
    Send_Command(0x2C);
    if (isLetter)
    {
        for (unsigned short i = 0; i < 8 * scale; i++)
        {
            unsigned char row = pgm_read_byte(&(letters[index][7 - (i / scale)])); // Get the row, indexing from the top
            for (unsigned short j = 0; j < 8 * scale; j++)
            {
                unsigned char pixel = (row >> (7 - (j / scale))) & 0x01; // Extract the pixel value

                // Set the color based on the pixel value and send it
                if (pixel)
                {
                    setColor(0, 0, 0); // Set to white
                }
                else
                {
                    setColor(bgCol[0], bgCol[1], bgCol[2]); // Set to black
                }
            }
        }
    }
    else
    {
        for (unsigned short i = 0; i < 8 * scale; i++)
        {
            unsigned char row = pgm_read_byte(&(numbers[index][7 - (i / scale)])); // Get the row, indexing from the top
            for (unsigned short j = 0; j < 8 * scale; j++)
            {
                unsigned char pixel = (row >> (7 - (j / scale))) & 0x01; // Extract the pixel value

                // Set the color based on the pixel value and send it
                if (pixel)
                {
                    setColor(0, 0, 0); // Set to white
                }
                else
                {
                    setColor(bgCol[0], bgCol[1], bgCol[2]); // Set to black
                }
            }
        }
    }
}

void drawString(unsigned short x, unsigned short y, unsigned char scale, const char *str)
{
    unsigned short currentX = x;

    // Iterate through each character in the string
    for (unsigned int i = 0; str[i] != '\0'; i++)
    {
        char c = str[i];

        // Handle numbers (0-9)
        if (c >= '0' && c <= '9')
        {
            drawCharacter(currentX, y, scale, false, c - '0');
        }
        // Handle uppercase letters (A-Z)
        else if (c >= 'A' && c <= 'W')
        {
            drawCharacter(currentX, y, scale, true, c - 'A');
        }
        else if (c == 'Y')
        {
            // draw Y separately since I skipped X sprite
            drawCharacter(currentX, y, scale, true, 23);
        }
        else if (c == ':')
        {
            drawCharacter(currentX, y, scale, true, 24);
        }
        else if (c == ' ')
        {
            drawCharacter(currentX, y, scale, true, 25);
        }

        // Move x position to next character
        currentX += 8 * scale;
    }
}

void drawStartMenu()
{
    blinkCounter = 0;
    setBackground();
    drawAWP(16, 240 + logoOffset, 4, false);
    drawAWP(16, 240 - awpHeight * 4 - logoOffset, 4, true);
    drawTarget(startCrosshairX, startCrosshairY);
    drawString(52, 240 + 16 + 10 + 4, 3, "PRECISION");
    drawString(112, 240 - 24 - 16 - 10, 3, "SHOT");
}

// draw a border indicating which game mode will be selected
void drawSelection()
{
    if (isClassic)
    {
        drawBorder(startCrosshairX - 75 - 12 * 4 - 5, startCrosshairX - 75 - 12 * 4 + 12 * 8 + 5, startCrosshairY - 32 - 5, startCrosshairY + 21, 1);
        drawBorder(startCrosshairX + 75 - 10 * 4 - 5, startCrosshairX + 75 - 10 * 4 + 10 * 8 + 5, startCrosshairY - 32 - 5, startCrosshairY + 21, 0);
    }
    else
    {
        drawBorder(startCrosshairX - 75 - 12 * 4 - 5, startCrosshairX - 75 - 12 * 4 + 12 * 8 + 5, startCrosshairY - 32 - 5, startCrosshairY + 21, 0);
        drawBorder(startCrosshairX + 75 - 10 * 4 - 5, startCrosshairX + 75 - 10 * 4 + 10 * 8 + 5, startCrosshairY - 32 - 5, startCrosshairY + 21, 1);
    }
}

void drawChooseMode()
{
    setBackground();
    drawAWP(16, 240 + logoOffset + 65, 4, false);
    drawAWP(16, 240 - awpHeight * 4 - logoOffset - 65, 4, true);
    drawBorder(borderMinX, borderMaxX, borderMinY, borderMaxY, 2);
    drawString(32, 360, 2, "CHOOSE GAME MODE");
    drawString(160 - 4 * 20, 340, 1, "SELECT WITH JOYSTICK");
    drawTarget(startCrosshairX - 75, startCrosshairY);
    drawGhost(startCrosshairX + 75, startCrosshairY);
    drawString(startCrosshairX - 75 - 12 * 4, startCrosshairY - 32, 1, "CLASSIC SHOT");
    drawString(startCrosshairX + 75 - 10 * 4, startCrosshairY - 32, 1, "GHOST SHOT");
    drawSelection();
    drawString(160 - 6 * 8, 200 - 40, 2, "SCORES");
    drawBorder(160 - 6 * 8 - 5, 160 + 6 * 8 + 5, 195 - 40, 219 - 40, 1);
    drawString(160 - 11 * 8, 200 - 32 - 40, 2, "   TOP: ");
    drawString(160 - 11 * 8, 200 - 32 * 2 - 40, 2, "LATEST: ");
    drawBorder(160 - 11 * 8 - 5, 160 + 11 * 8 + 5, 200 - 32 * 2 - 5 - 40, 223 - 40, 1);
}

// background for the game
void drawGameBackground()
{
    // reset crosshair to starting position
    crosshairX = startCrosshairX, crosshairY = startCrosshairY;
    // reset scores
    if (isClassic)
    {
        currScoreClassic = 0;
    }
    else
    {
        currScoreGhost = 0;
    }
    setBackground();
    drawBorder(borderMinX, borderMaxX, borderMinY, borderMaxY, 2);
    drawAWP(166, 240 + logoOffset + 65 + 18, 2, false);
    drawAWP(10, 240 - awpHeight * 3 - logoOffset - 65, 2, true);
    if (isClassic)
    {
        drawString(8, 240 + logoOffset + 65 + 31, 2, "CLASSIC");
        drawTarget(startCrosshairX - 20, 240 + logoOffset + 65 + 38);
        sprintf(buffer, "%u", topScoreClassic);
    }
    else
    {
        drawString(30, 240 + logoOffset + 65 + 31, 2, "GHOST");
        drawGhost(startCrosshairX - 20, 240 + logoOffset + 65 + 38);
        sprintf(buffer, "%u", topScoreGhost);
    }
    drawString(165 + 24, 240 - awpHeight * 3 - logoOffset - 65 + 40, 1, "SCORES");
    drawBorder(165 + 21, 165 + 6 + 21 + 8 * 6, 240 - awpHeight * 3 - logoOffset - 69 + 41, 240 - awpHeight * 3 - logoOffset - 69 + 52, 1);
    drawString(165, 240 - awpHeight * 3 - logoOffset - 65 + 24, 1, "    TOP: ");
    drawString(165, 240 - awpHeight * 3 - logoOffset - 65 + 8, 1, "CURRENT: ");
    // display top score
    drawString(165 + 72, 240 - awpHeight * 3 - logoOffset - 65 + 24, 1, buffer);
    drawString(8, 240 + logoOffset + 65, 1, "GAME TIME LEFT: ");
    drawString(184, 240 + logoOffset + 65, 1, "SHOT CLOCK: ");
}

void drawGameOver()
{
    blinkCounter = 0;
    setBackground();
    drawAWP(16, 240 + logoOffset + 65, 4, false);
    drawAWP(16, 240 - awpHeight * 4 - logoOffset - 65, 4, true);
    drawBorder(borderMinX, borderMaxX, borderMinY, borderMaxY, 2);
    drawString(52, 360, 3, "GAME OVER");
    drawBorder(52 - 5, 52 + (9 * 8 * 3) + 5, 360 - 5, 360 + 8 * 3 + 2, 1);
    if (isClassic)
    {
        drawTarget(startCrosshairX, 360 - 28);
        drawString(160 - 12 * 8, 360 - 65, 2, "CLASSIC SHOT");
        // checks if current score is new record
        if (topScoreClassic < currScoreClassic)
        {
            topScoreClassic = currScoreClassic;
            isNewRecord = true;
        }
        else
        {
            isNewRecord = false;
        }
        sprintf(buffer, "%u", topScoreClassic);
    }
    else
    {
        drawGhost(startCrosshairX, 360 - 28);
        drawString(160 - 10 * 8, 360 - 65, 2, "GHOST SHOT");
        // checks if current score is new record
        if (topScoreGhost < currScoreGhost)
        {
            topScoreGhost = currScoreGhost;
            isNewRecord = true;
        }
        else
        {
            isNewRecord = false;
        }
        sprintf(buffer, "%u", topScoreGhost);
    }
    drawString(160 - 6 * 8, 200, 2, "SCORES");
    drawBorder(160 - 6 * 8 - 5, 160 + 6 * 8 + 5, 195, 219, 1);
    drawString(160 - 6 * 16, 200 - 32, 2, "    TOP: ");
    // display top score
    drawString(160 + 3 * 16, 200 - 32, 2, buffer);
    drawString(160 - 6 * 16, 200 - 32 * 2, 2, "CURRENT: ");
    // display current score
    if (isClassic)
    {
        sprintf(buffer, "%u", currScoreClassic);
    }
    else
    {
        sprintf(buffer, "%u", currScoreGhost);
    }
    drawString(160 + 3 * 16, 200 - 32 * 2, 2, buffer);
    drawBorder(160 - 6 * 16 - 5, 160 + 6 * 16 + 5, 200 - 32 * 2 - 5, 223, 1);
    drawString(24, 200 - 32 * 2 - 25, 1, "PRESS JOYSTICK TO CHOOSE GAME MODE");
    drawString(24, 200 - 32 * 2 - 45, 1, "PRESS BUTTON TO GO TO TITLE SCREEN");
}

// TODO: Create your tick functions for each task
// Joystick
enum J_States
{
    J_INIT,
    J_READ
};
int TickFct_Joystick(int state)
{
    // state transitions
    switch (state)
    {
    case J_INIT:
        state = J_READ;
        break;
    case J_READ:
        break;
    default:
        break;
    }

    // state actions
    switch (state)
    {
    case J_INIT:
        break;
    case J_READ:
        // get the x, y values
        joystickX = ADC_read(1);
        joystickY = ADC_read(0);
        mappedX = map(joystickX, 22, 1023, -5, 6) * crosshairSpeed;
        mappedY = map(joystickY, 22, 1023, -5, 6) * crosshairSpeed;

        signed char movementX = 0, movementY = 0;
        double angle = atan(abs(mappedY / mappedX));
        movementX = mappedX * cos(angle) * crosshairSpeed;
        movementY = mappedY * sin(angle) * crosshairSpeed;

        // move crosshair X
        if (crosshairX + movementX > borderMaxX - targetRadius - 1)
        {
            oldCrosshairX = crosshairX;
            crosshairX = borderMaxX - targetRadius - 1;
        }
        else if (crosshairX + movementX < targetRadius)
        {
            oldCrosshairX = crosshairX;
            crosshairX = targetRadius;
        }
        else
        {
            oldCrosshairX = crosshairX;
            crosshairX += movementX;
        }

        // move crosshair Y
        if (crosshairY + movementY > borderMaxY - targetRadius)
        {
            oldCrosshairY = crosshairY;
            crosshairY = borderMaxY - targetRadius;
        }
        else if (crosshairY + movementY < borderMinY + targetRadius)
        {
            oldCrosshairY = crosshairY;
            crosshairY = borderMinY + targetRadius;
        }
        else
        {
            oldCrosshairY = crosshairY;
            crosshairY += movementY;
        }

        // signals that shot has been taken
        if ((gameState == CLASSIC_SHOT || gameState == GHOST_SHOT) && !GetBit(PINC, 2) && !isShotHeld)
        {
            isShot = true;
            isShotHeld = true;
        }

        if (isShotHeld && GetBit(PINC, 2))
        {
            isShotHeld = false;
        }

        break;
    default:
        break;
    }

    return state;
}

// enum declared above global variables so gameState variable can be accessed by other tasks
// Game State Logic
int TickFct_GameState(int state)
{
    // state transitions
    switch (state)
    {
    case GS_INIT:
        state = START_MENU;
        break;
    case START_MENU:
        if (!GetBit(PINC, 2))
        {
            state = CHOOSE_MODE;
            gameState = CHOOSE_MODE;
        }
        break;
    case CHOOSE_MODE:
        if (isClassic && !GetBit(PINC, 2))
        {
            state = CLASSIC_SHOT;
            gameState = CLASSIC_SHOT;
            randomCoords();
            shotClock = 0;
            gameTimer = 0;
        }
        else if (!isClassic && !GetBit(PINC, 2))
        {
            state = GHOST_SHOT;
            gameState = GHOST_SHOT;
            randomCoords();
            shotClock = 0;
            gameTimer = 0;
        }
        else if (GetBit(PINC, 3))
        {
            state = START_MENU;
            gameState = START_MENU;
        }
        break;
    case CLASSIC_SHOT:
        if (gameTimer >= gameTimerMax)
        {
            state = GAME_OVER;
            gameState = GAME_OVER;
        }
        else if (GetBit(PINC, 3))
        {
            state = START_MENU;
            gameState = START_MENU;
        }
        break;
    case GHOST_SHOT:
        if (gameTimer >= gameTimerMax)
        {
            state = GAME_OVER;
            gameState = GAME_OVER;
        }
        else if (GetBit(PINC, 3))
        {
            state = START_MENU;
            gameState = START_MENU;
        }
        break;
    case GAME_OVER:
        if (!GetBit(PINC, 2))
        {
            state = CHOOSE_MODE;
            gameState = CHOOSE_MODE;
        }
        else if (GetBit(PINC, 3))
        {
            state = START_MENU;
            gameState = START_MENU;
        }
        break;
    default:
        break;
    }

    // state actions
    switch (state)
    {
    case GS_INIT:
        break;
    case START_MENU:
        break;
    case CHOOSE_MODE:
        if (mappedX < -2)
        {
            isClassic = true;
        }
        else if (mappedX > 2)
        {
            isClassic = false;
        }
        break;
    case CLASSIC_SHOT:
        // check if shot has been taken or shot clock ran out
        if (isShot || shotClock >= shotClockMaxClassic)
        {
            // reset shotClock
            shotClock = 0;
            // add score if player shot
            if (isShot)
            {
                // deal with scoring
                unsigned short d = distance(targetX, targetY, crosshairX, crosshairY);
                if (d < 20)
                {
                    currScoreClassic += 10 - d / 2;
                }
            }
            // reset isShot to false
            isShot = false;
            // generate new target placement
            clearCrosshair(targetX, targetY);
            randomCoords();
        }

        shotClock++;
        gameTimer++;
        break;
    case GHOST_SHOT:
        // check if shot has been taken or shot clock ran out
        if (isShot || shotClock >= shotClockMaxGhost)
        {
            // reset shotClock
            shotClock = 0;
            // deal with scoring
            unsigned short d = distance(targetX, targetY, crosshairX, crosshairY);
            // scoring is more forgiving and rewarding but will punish you if too off
            if (d < 25)
            {
                // only add the score if player shot
                if (isShot)
                {
                    currScoreGhost += 12 - d / 2;
                }
            }
            else
            {
                // subtract score even if player didnt shoot
                // score won't go below 0
                if ((d - 25) / 2 > currScoreGhost)
                {
                    currScoreGhost = 0;
                }
                else
                {
                    currScoreGhost -= (d - 25) / 2;
                }
            }
            // reset isShot to false
            isShot = false;
            // generate new target placement
            clearCrosshair(targetX, targetY);
            randomCoords();
        }

        shotClock++;
        gameTimer++;
        break;
    case GAME_OVER:
        break;
    default:
        break;
    }

    return state;
}

// Display on LCD
enum D_States
{
    D_INIT,
    D_START_MENU,
    D_CHOOSE_MODE,
    D_CLASSIC_SHOT,
    D_GHOST_SHOT,
    D_GAME_OVER
};
int TickFct_DisplayLCD(int state)
{
    // state transitions
    switch (state)
    {
        // draw all static images in state transition
    case D_INIT:
        state = D_START_MENU;
        drawStartMenu();
        break;
    case D_START_MENU:
        if (gameState == CHOOSE_MODE)
        {
            state = D_CHOOSE_MODE;
            drawChooseMode();
        }
        break;
    case D_CHOOSE_MODE:
        if (gameState == CLASSIC_SHOT)
        {
            state = D_CLASSIC_SHOT;
            drawGameBackground();
        }
        else if (gameState == GHOST_SHOT)
        {
            state = D_GHOST_SHOT;
            drawGameBackground();
        }
        else if (gameState == START_MENU)
        {
            state = D_START_MENU;
            drawStartMenu();
        }
        break;
    case D_CLASSIC_SHOT:
        if (gameState == GAME_OVER)
        {
            state = D_GAME_OVER;
            drawGameOver();
        }
        else if (gameState == START_MENU)
        {
            state = D_START_MENU;
            drawStartMenu();
        }
        break;
    case D_GHOST_SHOT:
        if (gameState == GAME_OVER)
        {
            state = D_GAME_OVER;
            drawGameOver();
        }
        else if (gameState == START_MENU)
        {
            state = D_START_MENU;
            drawStartMenu();
        }
        break;
    case D_GAME_OVER:
        if (gameState == START_MENU)
        {
            state = D_START_MENU;
            drawStartMenu();
        }
        else if (gameState == CHOOSE_MODE)
        {
            state = D_CHOOSE_MODE;
            drawChooseMode();
        }
    default:
        break;
    }

    // state actions
    switch (state)
    {
    case D_INIT:
        break;
    case D_START_MENU:
        if (blinkCounter <= blinkCounterMax / 2)
        {
            drawString(68, 160, 1, "PRESS JOYSTICK TO START");
        }
        else
        {
            drawString(68, 160, 1, "                       ");
        }

        if (blinkCounter > blinkCounterMax)
        {
            blinkCounter = 0;
        }
        else
        {
            blinkCounter++;
        }
        break;
    case D_CHOOSE_MODE:
        drawSelection();
        // display top score
        if (isClassic)
        {
            // clear previous digits
            if (topScoreClassic < 10)
            {
                drawString(200 + 8 * 2, 200 - 32 - 40, 2, "  ");
            }
            else if (topScoreClassic < 100)
            {
                drawString(200 + 8 * 2 * 2, 200 - 32 - 40, 2, " ");
            }
            sprintf(buffer, "%u", topScoreClassic);
        }
        else
        {
            // clear previous digits
            if (topScoreGhost < 10)
            {
                drawString(200 + 8 * 2, 200 - 32 - 40, 2, "  ");
            }
            else if (topScoreGhost < 100)
            {
                drawString(200 + 8 * 2 * 2, 200 - 32 - 40, 2, " ");
            }
            sprintf(buffer, "%u", topScoreGhost);
        }
        drawString(200, 200 - 32 - 40, 2, buffer);
        // display current score
        if (isClassic)
        {
            // clear previous digits
            if (currScoreClassic < 10)
            {
                drawString(200 + 8 * 2, 200 - 32 * 2 - 40, 2, "  ");
            }
            else if (currScoreClassic < 100)
            {
                drawString(200 + 8 * 2 * 2, 200 - 32 * 2 - 40, 2, " ");
            }
            sprintf(buffer, "%u", currScoreClassic);
        }
        else
        {
            // clear previous digits
            if (currScoreGhost < 10)
            {
                drawString(200 + 8 * 2, 200 - 32 * 2 - 40, 2, "  ");
            }
            else if (currScoreGhost < 100)
            {
                drawString(200 + 8 * 2 * 2, 200 - 32 * 2 - 40, 2, " ");
            }
            sprintf(buffer, "%u", currScoreGhost);
        }
        drawString(200, 200 - 32 * 2 - 40, 2, buffer);
        break;
    case D_CLASSIC_SHOT:
        // display crosshair
        if (oldCrosshairX != crosshairX || oldCrosshairY != crosshairY)
        {
            clearCrosshair(oldCrosshairX, oldCrosshairY);
        }
        drawCrosshair(crosshairX, crosshairY);

        // display timers
        // display game timer
        sprintf(buffer, "%u", (gameTimerMax / 10) - (gameTimer / 10));
        drawString(8 * 17, 240 + logoOffset + 65, 1, buffer);
        // clear digit if in single digit
        if (gameTimer == gameTimerMax - 90)
        {
            drawString(8 * 18, 240 + logoOffset + 65, 1, " ");
        }
        // display shot clock
        sprintf(buffer, "%u", (shotClockMaxClassic / 10) - (shotClock / 10));
        drawString(184 + 12 * 8, 240 + logoOffset + 65, 1, buffer);

        // display target
        drawTarget(targetX, targetY);

        // display current score
        // Convert unsigned short to string
        sprintf(buffer, "%u", currScoreClassic);

        drawString(165 + 72, 240 - awpHeight * 3 - logoOffset - 65 + 8, 1, buffer);
        break;
    case D_GHOST_SHOT:
        // display crosshair
        if (oldCrosshairX != crosshairX || oldCrosshairY != crosshairY)
        {
            clearCrosshair(oldCrosshairX, oldCrosshairY);
        }
        drawCrosshair(crosshairX, crosshairY);

        // display timers
        // display game timer
        sprintf(buffer, "%u", (gameTimerMax / 10) - (gameTimer / 10));
        drawString(8 * 17, 240 + logoOffset + 65, 1, buffer);
        // clear digit if in single digit
        if (gameTimer == gameTimerMax - 90)
        {
            drawString(8 * 18, 240 + logoOffset + 65, 1, " ");
        }
        // display shot clock
        sprintf(buffer, "%u", (shotClockMaxGhost / 10) - (shotClock / 10));
        drawString(184 + 12 * 8, 240 + logoOffset + 65, 1, buffer);

        // display target for split second
        if (shotClock < ghostFlash)
        {
            drawTarget(targetX, targetY);
        }
        else if (shotClock == ghostFlash)
        {
            // only clear target once
            clearCrosshair(targetX, targetY);
        }

        // display current score
        // clear previous digits
        if (currScoreGhost < 10)
        {
            drawString(165 + 72 + 8, 240 - awpHeight * 3 - logoOffset - 65 + 8, 1, "  ");
        }
        else if (currScoreGhost < 100)
        {
            drawString(165 + 72 + 16, 240 - awpHeight * 3 - logoOffset - 65 + 8, 1, " ");
        }
        // Convert unsigned short to string
        sprintf(buffer, "%u", currScoreGhost);

        drawString(165 + 72, 240 - awpHeight * 3 - logoOffset - 65 + 8, 1, buffer);
        break;
    case D_GAME_OVER:
        // blink new record repeatedly
        if (isNewRecord)
        {
            if (blinkCounter <= blinkCounterMax / 2)
            {
                drawString(160 - 10 * 8, 275 - 25, 2, "NEW RECORD");
            }
            else
            {
                drawString(160 - 10 * 8, 275 - 25, 2, "          ");
            }

            if (blinkCounter > blinkCounterMax)
            {
                blinkCounter = 0;
            }
            else
            {
                blinkCounter++;
            }
        }
        break;
    default:
        break;
    }

    return state;
}

// helper function for adding tasks in main function
void addTask(unsigned char i, signed char state, unsigned long period, int (*TickFct)(int))
{
    tasks[i].state = state;
    tasks[i].period = period;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = TickFct;
}

void HardwareReset()
{
    // setResetPinToLow;
    PORTB = SetBit(PORTB, 0, 0);
    _delay_ms(200);
    // setResetPinToHigh;
    PORTB = SetBit(PORTB, 0, 1);
    _delay_ms(200);
}

void ST7735_init()
{
    HardwareReset();
    // SWRESET
    Send_Command(0x01);
    _delay_ms(150);
    // SLPOUT
    Send_Command(0x11);
    _delay_ms(200);
    // COLMOD
    Send_Command(0x3A);
    Send_Data(0x06); // for 16 bit color mode. You can pick any color mode you want
    _delay_ms(10);
    // DISPON
    Send_Command(0x29);
    _delay_ms(200);
}

int main(void)
{
    // TODO: initialize all your inputs and ouputs
    // PORTB is output
    DDRB = 0xEF;
    PORTB = 0x10;
    // PORTC is input
    DDRC = 0x00;
    PORTC = 0xFF;
    // PORTD is output
    DDRD = 0xFF;
    PORTD = 0x00;

    ADC_init(); // initializes ADC
    serial_init(9600);
    SPI_INIT(); // initializes Display
    ST7735_init();

    // Seed the random number generator with the current time
    srand(ADC_read(4));

    // TODO: Initialize tasks here
    // Task Template
    /*
    tasks[i].state = _INIT;
    tasks[i].period = ;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_;
    */

    unsigned char i = 0;
    addTask(i, J_INIT, 100, &TickFct_Joystick);
    i++;
    addTask(i, GS_INIT, 100, &TickFct_GameState);
    i++;
    addTask(i, D_INIT, 100, &TickFct_DisplayLCD);

    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1)
    {
    }

    return 0;
}