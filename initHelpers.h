/* 
 * File:   timerSetup.h
 * Author: remis2
 *
 * Created on November 15, 2014, 2:58 PM
 */

#include <p33Fxxxx.h>
//do not change the order of the following 3 definitions
#define FCY 12800000UL
#include <stdio.h>
#include <libpic30.h>

#include "lcd.h"
#include "led.h"

#ifndef TIMERSETUP_H
#define	TIMERSETUP_H

#ifdef	__cplusplus
extern "C" {
#endif

    void timer1Init(){

        //Timer1 inicialization
        __builtin_write_OSCCONL(OSCCONL | 2);
        T1CONbits.TON = 0;      //Disable Timer
        T1CONbits.TCS = 1;      //Enable external clock
        T1CONbits.TSYNC = 0;    //Dysable sync
        T1CONbits.TCKPS = 0b00; //Set 1:1 prescaler
        TMR1 = 0x00;            //Clear timer reg
        PR1 = 32767;
        IPC0bits.T1IP = 0x01;   //Set Timer2 Interrupt priority
        IFS0bits.T1IF = 0;      //Clear Timer1 Interrupt flag
        IEC0bits.T1IE = 1;      //Enable Timer1 interrupt
        T1CONbits.TON = 1;      //Start Timer

    }

    void timer2Init(){

        //Timer2 inicialization
        T2CONbits.TON = 0;      //Disable Timer
        T2CONbits.TCS = 0;      //Disable external clock
        T2CONbits.TGATE = 0;
        T2CONbits.TCKPS = 0b11; //Set 1:256 prescaler
        TMR2 = 0x00;
        PR2 = 50;
        IPC1bits.T2IP = 0x01;   //Set Timer2 Interrupt priority
        IFS0bits.T2IF = 0;      //Clear Timer1 Interrupt flag
        IEC0bits.T2IE = 1;      //Enable Timer1 interrupt
        T2CONbits.TON = 1;
    }

    void timer3Init(){

        //Timer3 inicialization
        T3CONbits.TON = 0;      //Disable Timer
        T3CONbits.TCS = 0;      //Disable external clock
        T3CONbits.TGATE = 0;
        T3CONbits.TCKPS = 0b11 ; //Set 1:256 prescaler
        TMR3 = 0x00;
        PR3 = 65535;
        IPC2bits.T3IP = 0x01;   //Set Timer2 Interrupt priority
        IFS0bits.T3IF = 0;      //Clear Timer1 Interrupt flag
        IEC0bits.T3IE = 0;      //Disable Timer1 interrupt
        T3CONbits.TON = 0;      // Timer not enabled
    }

    void lcdInit(){

        //Init LCD
        __C30_UART=1;
        lcd_initialize();
        lcd_clear();
    }

    void joystickInit(){

        // Joystick
        SETBIT (TRISEbits.TRISE8 );   //Set pin to input
        SETBIT (AD1PCFGHbits.PCFG20); //Set to digital
        SETBIT(IEC1bits.INT1IE);    //Interrupt Enable Control Register 1
                                    //External Interrupt 1 Enable bit
        SETBIT(IPC5bits.INT1IP);    //Interrupt Priority Control Register 5
                                    //External Interrupt 1 Priority bits
        CLEARBIT(IFS1bits.INT1IF);  //Interrupt Flag Status Register 1
        SETBIT(INTCON2bits.INT1EP); /*Interrupt Control Register 2 External
                                    Interrupt 1 Edge Detect Polarity Select bit*/
    }


#ifdef	__cplusplus
}
#endif

#endif	/* TIMERSETUP_H */

