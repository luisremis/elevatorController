#include <p33Fxxxx.h>
//do not change the order of the following 3 definitions
#define FCY 12800000UL 
#include <stdio.h>
#include <libpic30.h>

#include "lcd.h"
#include "led.h"
#include "initHelpers.h"
#include "flexserial.h"
#include "crc16.h"
/* Initial configuration by EE */
// Primary (XT, HS, EC) Oscillator with PLL
_FOSCSEL(FNOSC_PRIPLL);

// OSC2 Pin Function: OSC2 is Clock Output - Primary Oscillator Mode: XT Crystal
_FOSC(OSCIOFNC_OFF & POSCMD_XT); 

// Watchdog Timer Enabled/disabled by user software
_FWDT(FWDTEN_OFF);

// Disable Code Protection
_FGS(GCP_OFF);

// --------------------- ELEVATOR CONTROLLER ---------------------

#define NUMBEROFFLOORS      10
#define NUMBEROFELEVATORS   2

#define BUTTON (pressedButtons[elevator])

#define SENSOR (sensorsAll[elevator])

#define ELEVATOR (eDirect[elevator])

#define OPCODE_BUTTONS_0   0
#define OPCODE_BUTTONS_1   1
#define OPCODE_SENSORS_0   4
#define OPCODE_SENSORS_1   5
#define OPCODE_OUTSIDE_UP  7
#define OPCODE_OUTSIDE_DW  8
#define OPCODE_SOFT_RESET  9

#define DIRECTION_UP  0
#define DIRECTION_DW  1
#define DIRECTION_ST  2


uint8_t message[5]={48,48,48,48,48};
int flag_newMessage = 0;
unsigned int elevator = 0;
int flagReset = 0;

uint8_t pressedButtons  [NUMBEROFELEVATORS][NUMBEROFFLOORS];
uint8_t sensorsAll      [NUMBEROFELEVATORS][NUMBEROFFLOORS];
uint8_t outsideUp       [NUMBEROFFLOORS];
uint8_t outsideDw       [NUMBEROFFLOORS];

struct elevatorDirection{

    uint8_t direction;
    uint8_t floor;
    uint8_t state;
    uint8_t prevState;
    uint8_t motorState;
};

void setState( struct elevatorDirection* elevator, uint8_t s){

    (*elevator).prevState = (*elevator).state;
    (*elevator).state = s;

    if ( ((*elevator).prevState >> 4) == 0x01 ){ // PARKED

        if ( ((*elevator).state >> 4) == 0x02 )
            (*elevator).direction = DIRECTION_UP;
        else
            (*elevator).direction = DIRECTION_DW;
    }

    if ( ((*elevator).prevState >> 4) == 0x02 ){ // UP_TO

            (*elevator).direction = DIRECTION_UP;
    }

    if ( ((*elevator).prevState >> 4) == 0x04 ){ // UP_TO

            (*elevator).direction = DIRECTION_DW;
    }
}

struct elevatorDirection eDirect[NUMBEROFELEVATORS];

enum elevator_state {
  PARKED_0  = 0x10,
  PARKED_1  = 0x11,
  PARKED_2  = 0x12,
  PARKED_3  = 0x13,
  PARKED_4  = 0x14,
  PARKED_5  = 0x15,
  PARKED_6  = 0x16,
  PARKED_7  = 0x17,
  PARKED_8  = 0x18,
  PARKED_9  = 0x19,

  UP_TO_1   = 0x21,
  UP_TO_2   = 0x22,
  UP_TO_3   = 0x23,
  UP_TO_4   = 0x24,
  UP_TO_5   = 0x25,
  UP_TO_6   = 0x26,
  UP_TO_7   = 0x27,
  UP_TO_8   = 0x28,
  UP_TO_9   = 0x29,

  DOWN_TO_8 = 0x48,
  DOWN_TO_7 = 0x47,
  DOWN_TO_6 = 0x46,
  DOWN_TO_5 = 0x45,
  DOWN_TO_4 = 0x44,
  DOWN_TO_3 = 0x43,
  DOWN_TO_2 = 0x42,
  DOWN_TO_1 = 0x41,
  DOWN_TO_0 = 0x40,

  UNKNOWN   = 0xff,
};

enum motor_state {
  STOP      = 0x00,
  MOVE_UP   = 0xc0,
  MOVE_DOWN = 0x80,
};

// global variables for state-machine state and motor control
//


char * getstrState(unsigned int state){

    if ( state == PARKED_0)
        return "PARKED_0";
    if ( state == PARKED_1)
        return "PARKED_1";
    if ( state == PARKED_2)
        return "PARKED_2";
    if ( state == PARKED_3)
        return "PARKED_3";
    if ( state == PARKED_4)
        return "PARKED_4";
    if ( state == PARKED_5)
        return "PARKED_5";
    if ( state == PARKED_6)
        return "PARKED_6";
    if ( state == PARKED_7)
        return "PARKED_7";
    if ( state == PARKED_8)
        return "PARKED_8";
    if ( state == PARKED_9)
        return "PARKED_9";

    if ( state == UP_TO_1)
        return "UP_TO_1";
    if ( state == UP_TO_2)
        return "UP_TO_2";
    if ( state == UP_TO_3)
        return "UP_TO_3";
    if ( state == UP_TO_4)
        return "UP_TO_4";
    if ( state == UP_TO_5)
        return "UP_TO_5";
    if ( state == UP_TO_6)
        return "UP_TO_6";
    if ( state == UP_TO_7)
        return "UP_TO_7";
    if ( state == UP_TO_8)
        return "UP_TO_8";
    if ( state == UP_TO_9)
        return "UP_TO_9";

    if ( state == DOWN_TO_8)
        return "DOWN_TO_8";
    if ( state == DOWN_TO_7)
        return "DOWN_TO_7";
    if ( state == DOWN_TO_6)
        return "DOWN_TO_6";
    if ( state == DOWN_TO_5)
        return "DOWN_TO_5";
    if ( state == DOWN_TO_4)
        return "DOWN_TO_4";
    if ( state == DOWN_TO_3)
        return "DOWN_TO_3";
    if ( state == DOWN_TO_2)
        return "DOWN_TO_2";
    if ( state == DOWN_TO_1)
        return "DOWN_TO_1";
    if ( state == DOWN_TO_0)
        return "DOWN_TO_0";

    return "UNKNOWN";
}

char * getstrMotor(unsigned int motor_state){

    if ( motor_state == STOP)
        return "STOP";
    if ( motor_state == MOVE_UP)
        return "UP  ";
    if ( motor_state == MOVE_DOWN)
        return "DOWN";

    return "UNKN";
}

uint8_t ctoui(char a){

    return a - 48;
}

void softReset(){

    int counter, aux;

    while (U2STAbits.URXDA) uart2_getc(); //flush

    for(counter = 0; counter < NUMBEROFELEVATORS; ++counter){
        eDirect[counter].state     = PARKED_0;
        eDirect[counter].prevState = PARKED_0;

        elevator = counter;
        motor(STOP);

        for(aux = 0; aux< NUMBEROFFLOORS; ++aux ){
            pressedButtons[counter][aux] = 0;
            sensorsAll    [counter][aux] = 0;
            outsideDw     [aux] = 0;
            outsideUp     [aux] = 0;
        }
    }
}

void error(void){
  motor(STOP);
  lcd_locate(0,6);
  lcd_printf("ERROR[%d] - %s", elevator, getstrState(ELEVATOR.state));
  flagReset = 1;
  while (flagReset);
}

void openDoors(){

    //routine to open doors

    lcd_locate(0,7);
    lcd_printf("Opening[%d].... ", elevator);

    TMR3 = 0x00;
    T3CONbits.TON = 1;      // Enable time
    while (TMR3 <= 65000){
        //wating timer
    }

    lcd_locate(0,7);
    lcd_printf("              \n", elevator);
    T3CONbits.TON = 0;      // Disable timer

    // Sensoring if elevator alligned with floor
}

void closeDoors(){

    lcd_locate(0,7);
    lcd_printf("Closing[%d].... ", elevator);

    TMR3 = 0x00;
    T3CONbits.TON = 1;      // Enable time
    while (TMR3 <= 65000){
        //wating timer
    }

    lcd_locate(0,7);
    lcd_printf("              \n");
    T3CONbits.TON = 0;      // Disable timer

    // Sensoring if someone is crossing the door
}

void motor( uint8_t action){

    if ( action == STOP ) {

        ELEVATOR.motorState = STOP;
        motor_set_duty(elevator,1500);
        //Stop actions
    }

    if ( action == MOVE_UP ) {

        ELEVATOR.motorState = MOVE_UP;
        motor_set_duty(elevator,2100);
        //Stop actions
    }

    if ( action == MOVE_DOWN ) {

        ELEVATOR.motorState = MOVE_DOWN;
        motor_set_duty(elevator,900);
        //Stop actions
    }
}

int abs (int a){

    if ( a < 0) {
        return -1*a;
    }
    return a;
}

void assignElevator(){

    int counter, aux;
    int elevatorAux, diff = 999;
    int flag_close = 0;
    int flag_stop  = 0;

    for(counter = 0; counter< NUMBEROFFLOORS; ++counter){

        if(outsideUp[counter] == 1){

            for (aux = 0; aux < NUMBEROFELEVATORS; ++aux){

                if(eDirect[aux].direction == DIRECTION_UP && 
                   eDirect[aux].floor < counter)
                {
                    pressedButtons[aux][counter] = 1;
                    outsideUp[counter] = 0;
                    flag_close = 1;
                }
                if (eDirect[aux].direction == DIRECTION_ST  &&
                    diff > abs(eDirect[aux].floor - counter))
                {
                    diff = abs(eDirect[aux].floor - counter);
                    elevatorAux = aux;
                    flag_stop = 1;
                }
            }

            if (!flag_close && flag_stop){
                pressedButtons[elevatorAux][counter] = 1;
                outsideUp[counter] = 0;
            }
        }
    }

    diff = 999;
    flag_close = 0;
    flag_stop  = 0;

    for(counter = 0; counter< NUMBEROFFLOORS; ++counter){

        if(outsideDw[counter] == 1){

            for (aux = 0; aux < NUMBEROFELEVATORS; ++aux){

                if(eDirect[aux].direction == DIRECTION_DW &&
                   eDirect[aux].floor > counter)
                {
                    pressedButtons[aux][counter] = 1;
                    outsideDw[counter] = 0;
                    flag_close = 1;
                }
                if (eDirect[aux].direction == DIRECTION_ST  &&
                    diff > abs(eDirect[aux].floor - counter))
                {
                    diff = abs(eDirect[aux].floor - counter);
                    elevatorAux = aux;
                    flag_stop = 1;
                }
            }

            if (!flag_close && flag_stop){
                pressedButtons[elevatorAux][counter] = 1;
                outsideDw[counter] = 0;
            }
        }
    }
}

void buttonRoutine(){

    uint16_t crc = 0;
    uint16_t crcReceived;
    uint8_t opcode, code;

    crc = crc_update( crc_update(crc, message[3]), message[4]);
    crcReceived = (message[1] << 8) + message[2];

    if (crc == crcReceived )
    {
        opcode = ctoui(message[3]);
        code   = ctoui(message[4]);
        CLEARBIT(LED4_PORT);
        CLEARBIT(LED1_PORT);
        uart2_putc(1);
        if      (opcode == OPCODE_BUTTONS_0){
            pressedButtons[0][code] = 1;
        }
        else if (opcode == OPCODE_BUTTONS_1){
            pressedButtons[1][code] = 1;
        }
        else if (opcode == OPCODE_SENSORS_0){
            sensorsAll[0][code] = 1;
        }
        else if (opcode == OPCODE_SENSORS_1){
            sensorsAll[1][code] = 1;
        }
        else if (opcode == OPCODE_OUTSIDE_UP){
            outsideUp[code] = 1;
        }
        else if (opcode == OPCODE_OUTSIDE_DW){
            outsideDw[code] = 1;
        }
        else if (opcode == OPCODE_SOFT_RESET){
            softReset();
        }
        else{
            SETBIT(LED1_PORT);  // Invalid Opcode
        }
    }
    else{
        uart2_putc(0);
        SETBIT(LED4_PORT);
    }

    flag_newMessage = 0;
}

void changeElevator(){

    ++elevator;

    if (elevator >= NUMBEROFELEVATORS)
        elevator = 0;
}

uint8_t buttomsValues(int elevator){

    int aux;
    int sum=0;

    for(aux = 0; aux< NUMBEROFFLOORS; ++aux ){
        sum += pressedButtons[elevator][aux];
    }

    return sum;
}

uint8_t sensorsValues(int elevator){

    int aux;
    int sum=0;

    for(aux = 0; aux< NUMBEROFFLOORS; ++aux ){
        sum += sensorsAll[elevator][aux];
    }

    return sum;
}

int main(void)
{
    unsigned int counter = 0;
    int i;
    int flagAction=0;
    lcdInit();
    uart2_init(9600);

    CLEARLED (LED1_TRIS); //Set pin to output
    CLEARLED (LED2_TRIS); //Set pin to output
    CLEARLED (LED3_TRIS); //Set pin to output
    CLEARLED (LED4_TRIS); //Set pin to output
    CLEARLED (LED5_TRIS); //Set pin to output

    softReset();
    joystickInit();
    motor_init(0);
    motor_init(1);
    timer1Init();
    timer3Init();

    while(1) {
  
      ++counter;
      assignElevator();
      changeElevator();

      if (counter % 10000 == 0){
          
          lcd_locate(0,0);
          lcd_printf("State[0,%d]: %s \n",eDirect[0].direction, getstrState(eDirect[0].state) );
          lcd_locate(0,1);
          lcd_printf("State[1,%d]: %s \n",eDirect[1].direction, getstrState(eDirect[1].state) );
          lcd_locate(0,2);
          lcd_printf("M0: %s - M1: %s \n",getstrMotor(eDirect[0].motorState),
                                          getstrMotor(eDirect[1].motorState) ) ;
          lcd_locate(0,3);
          lcd_printf("Opcode/code: %d/%d", ctoui(message[3]),ctoui(message[4]) ) ;
          //lcd_locate(0,4);
          //lcd_printf("counter: %d", counter ) ;
          /*lcd_locate(0,4);
          lcd_printf("%d / %d - %d / %d", sensorsValues(0),buttomsValues(0),
                                          sensorsValues(1),buttomsValues(1) ) ;*/
      }

      if (flag_newMessage){
          buttonRoutine();
      }

      flagAction = 0;

      switch( ELEVATOR.state & 0xF0 ) {

          /*----------------- PARKED STATES ---------------------- */
          case 0x10: //Some PARKED State

            ELEVATOR.floor = ELEVATOR.state & 0x0F;
            flagAction = 0;

            if (ELEVATOR.direction == DIRECTION_ST || ELEVATOR.direction == DIRECTION_UP ){

                for (i = ELEVATOR.floor + 1; i < NUMBEROFFLOORS; ++i ){
                    if (BUTTON[i]){
                      setState(&ELEVATOR, ELEVATOR.state + 0x11); //UP_TO_floor+1
                      closeDoors();
                      motor(MOVE_UP);
                      flagAction = 1;
                      break;
                    }
                }

                if (!flagAction) //// ELEVATOR.direction == DIRECTION_ST
                {
                    for (i = ELEVATOR.floor - 1; i >= 0; --i ){
                        if (BUTTON[i]){
                          setState(&ELEVATOR, ELEVATOR.state + 0x30 - 0x01); //DOWN_TO_floor-1
                          //closeDoors();
                          motor(MOVE_DOWN);
                          flagAction = 1;
                          break;
                        }
                    }
                }
            }
            else{ // ELEVATOR.direction == DIRECTION_DW

                for (i = ELEVATOR.floor - 1; i >= 0; --i ){ //DOWN_TO_floor-1
                    if (BUTTON[i]){
                      setState(&ELEVATOR, ELEVATOR.state + 0x30 - 0x01);
                      closeDoors();
                      motor(MOVE_DOWN);
                      flagAction = 1;
                      break;
                    }
                }          
            }

            if (!flagAction)
            {
              ELEVATOR.direction = DIRECTION_ST;
              motor(STOP);
            }
            break;

          /*----------------- UP_TO STATES ---------------------- */

          case 0x20:

            ELEVATOR.floor = (ELEVATOR.state & 0x0F) - 1 ;

            for (i = 0; i < NUMBEROFFLOORS; ++i)
            {
              if (SENSOR[i] && i != (ELEVATOR.floor + 1) ){ //All other sensors
                error();
              }
            }

            if (SENSOR[ELEVATOR.floor + 1])
            {
              SENSOR[ELEVATOR.floor + 1] = 0;
              if (BUTTON[ELEVATOR.floor + 1])
              {
                  motor(STOP);
                  setState(&ELEVATOR, ELEVATOR.state - 0x10); // To PARKED state
                  BUTTON[ELEVATOR.floor + 1] = 0;
                  openDoors();
                  flagAction = 1;
              }
              else{
                  for ( i = ELEVATOR.floor + 2; i < NUMBEROFFLOORS; ++i)
                  {
                    if (BUTTON[i])
                    {
                      setState(&ELEVATOR, ELEVATOR.state + 0x01); // To UP_TO_floor+1 state
                      flagAction =1;
                      break;
                    }
                  }
              }
              if (!flagAction)
              {
                  motor(STOP);
                  setState(&ELEVATOR, ELEVATOR.state - 0x10); // To PARKED state
                  openDoors();
              }
            }
            break;
          /*----------------- DOWN_TO STATES ---------------------- */

          case 0x40:

            ELEVATOR.floor = (ELEVATOR.state & 0x0F) + 1 ;
            for (i = 0; i < NUMBEROFFLOORS; ++i)
            {
              if (SENSOR[i] && i != (ELEVATOR.floor - 1) ){ //All other sensors
                error();
              }
            }

            if (SENSOR[ELEVATOR.floor - 1])
            {
              SENSOR[ELEVATOR.floor - 1] = 0;
              if (BUTTON[ELEVATOR.floor - 1])
              {
                  motor(STOP);
                  setState(&ELEVATOR, ELEVATOR.state - 0x30); // To PARKED state
                  BUTTON[ELEVATOR.floor - 1] = 0;
                  openDoors();
                  flagAction = 1;
              }
              else{
                  for ( i = (int)ELEVATOR.floor - 2; i >= 0; --i)
                  {
                    if (BUTTON[i])
                    {
                      setState(&ELEVATOR, ELEVATOR.state - 0x01); // To DOWN_TO_floor-1 state
                      flagAction =1;
                      break;
                    }
                  }
              }
              if (!flagAction)
              {
                  motor(STOP);
                  setState(&ELEVATOR, ELEVATOR.state - 0x30); // To PARKED state
                  openDoors();
              }
            }
            break;

          default:
            error();

  	} // end switch(state)
  }

  return 0;
}

void __attribute__((__interrupt__)) _T1Interrupt(void)
{
    TOGGLELED(LED2_PORT);
    IFS0bits.T1IF = 0;    // clear the interrupt flag
}
/*
void __attribute__((__interrupt__)) _T2Interrupt(void)
{
    IFS0bits.T2IF = 0;    // clear the interrupt flag
}
*/
void __attribute__((__interrupt__)) _INT1Interrupt(void)
{
    //INTERRUPT FOR STOP BUTTON

    motor(STOP);

    IFS1bits.INT1IF = 0;    // clear the interrupt flag

    while (PORTEbits.RE8 == 0 ) //button considered off but being pushed
    {
       SETLED(LED3_PORT);
    }
     CLEARLED(LED3_PORT);
     lcd_clear();
     flagReset = 0;
     softReset();
}

void __attribute__((__interrupt__)) _U2RXInterrupt(void)
{
    static unsigned int counter = 0;
    uint8_t aux;

    IFS1bits.U2RXIF = 0;    // clear TX interrupt flag

    while (U2STAbits.URXDA) {

       aux = uart2_getc();
       if (counter > 4) break; // 
       message[counter] = aux;
       counter++;
    }

    if (counter > 4 ){

        counter = 0;
        flag_newMessage = 1;      //Enable half bottom
        //TOGGLELED(LED5_PORT);
        while (U2STAbits.URXDA) uart2_getc(); //flush
    }
}
