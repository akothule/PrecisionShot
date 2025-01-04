# CS/EE 120B Custom Laboratory Project Report

# Precision Shot

## Ayush Kothule

## Dec 10, 2024

### **Introduction**

Precision Shot is a two-dimensional target shooting game where the player controls a cursor and has to aim it and shoot at a randomly placed target within 3 seconds. Once the player takes the shot or the time runs out, the target is randomly placed in a new location. The player earns a maximum of 10 points if they hit the target perfectly, but can earn partial points proportional to how far off center they are. The player won’t gain or lose points if they don’t shoot at all. The goal is to hit as many targets as accurately as possible within 20 seconds. The game features a second memory-focused game mode called Ghost Shot, where the target is briefly flashed for a fraction of a second, and has to shoot close to the target or they risk losing points proportional to how far off they are, even if they didn’t shoot. To compensate, the player is given 5 seconds per shot, and scoring is slightly more rewarding, being able to earn a maximum of 12 points. The original game mode is called Classic Shot and represents the basic functionality of the game. The entire project was completed successfully, and there are no major bugs/errors that limit usability.

### **Build-upons**

* Using the ST7796 480 x 320 LCD Screen  
  * Also contains a touch feature, but was not used  
* Custom Sprites for LCD Screen  
  * The AWP gun sprite  
* Ghost Shot  
  * Second game mode explained in Introduction

All 3 were successfully implemented.

### **User Guide**

* Inputs  
  * Joystick  
    * Used for controlling the game  
  * Button  
    * Used for going back to title screen at any moment  
* Outputs  
  * ST7796 480 x 320 LCD Screen  
    * Originally planned on using 1D7S and 4D7S for displaying shot clock and score but decided to display everything on LCD screen.

### **Hardware Components Used**

* Joystick  
* Button  
* ST7796 480 x 320 LCD Screen  
* Wires and resistors

### **Software Libraries Used**

* \#include "math.h"  
  * Used sqrt() and pow() for calculating distance from target and crosshair to calculate score and atan(), cos(), sin(), and abs() for crosshair movement.  
* \#include \<stdio.h\>  
  * Used sprintf() for converting numbers to character buffers to be displayed on LCD Screen  
* \#include \<stdlib.h\>  
  * Used rand() for random target placement

### **Wiring Diagram**

![Wiring Diagram](https://raw.githubusercontent.com/akothule/PrecisionShot/main/img/wiring_diagram.png)

![Wiring Image](https://raw.githubusercontent.com/akothule/PrecisionShot/main/img/wiring_image.png)

### **Task Diagram**

![Task Diagram](https://raw.githubusercontent.com/akothule/PrecisionShot/main/img/task_diagram.png)

### **SynchSM Diagrams**

Draw a SynchSM diagrams for **all tasks** in the system.

* Show the period  
* Declare the types of all local variables

![Joystick](https://raw.githubusercontent.com/akothule/PrecisionShot/main/img/joystick_synchSM.png)

![Game State](https://raw.githubusercontent.com/akothule/PrecisionShot/main/img/game_state_synchSM.png)

<div style="background-color: white;">
    <img src="https://raw.githubusercontent.com/akothule/PrecisionShot/main/img/game_state_synchSM.png" alt="Game State">
</div>

![Display Game State](https://raw.githubusercontent.com/akothule/PrecisionShot/main/img/display_game_state_synchSM.png)
