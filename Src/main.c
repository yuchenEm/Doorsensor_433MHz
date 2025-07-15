/**
  * Doorsensor_433MHz/main.c
  *
  * Copyright (c) 2025 yuchenEm
  * Licensed under the MIT License
  */

#include "STC15Wxx.h"
#include "intrins.h"

// define pulse width(TH0, TL0 value): 1 LCK = 8 OSC clock; Freq = 5.5296MHz ; 12T mode
// 12.4ms
#define OOK_124LCK_H 0xE9
#define OOK_124LCK_L 0x79

// 0.4ms
#define OOK_4LCK_H 0xFF
#define OOK_4LCK_L 0x46

// 1.2ms
#define OOK_12LCK_H 0xFD
#define OOK_12LCK_L 0xD2

// define port:
//sfr P3 = 0xB0;
sbit LED1 = P3^1;
sbit LED2 = P3^0;
sbit KAI = P3^5;
sbit GUAN = P3^4;
sbit OOK_Data = P3^2;
sbit BAT_volt = P3^3;

/*************************** global variables: ***********************************************/
unsigned char door_flag; // =0 no action; =2 closed; =1 opened;
unsigned char volt_low_flag; // =0 battery ok; =2 battery low;

unsigned char code *pID = 0x0FFE; // pID points to #ID in Flash
unsigned char data_frame_buffer[100]; // store dataframe


/*********************************************************************************************/

/******************************************************************************
Delay function: void delay_x10ms(unsigned char x)
	x10ms based on 5.5MHz

*******************************************************************************/
void delay_x10ms(unsigned char x){
	unsigned char i,j,k;
	for(k=0;k<x;k++){
		i = 54;
		j = 199;
		do{
			while(--j);
		}
		while(--i);
	}
}

/*************************************************************************
Function: void OOK_Data_Sent(unsigned char sentdata)

	OOK/ASK software simulate EV1527 dataframe:
	Addr: from Flash ID, address from 0x0F9 to 0x0FF
				only use last two byte 0x0FE, 0x0FF
	Data: 25 section: 1(syn)+16(addr)+8(function)
				4 byte(2H, 2L)each section
				stored in arr data_frame_buffer[100]
**************************************************************************/
void OOK_Data_Sent(unsigned char sentdata){
	unsigned char i,j,k;
	unsigned char addr_H, addr_L, Data, buff;
	
	addr_H = *pID;
	addr_L = *pID++;
	Data = sentdata;
	
	// syn frame:
	data_frame_buffer[0] = OOK_4LCK_H;
	data_frame_buffer[1] = OOK_4LCK_L;
	data_frame_buffer[2] = OOK_124LCK_H;
	data_frame_buffer[3] = OOK_124LCK_L;
	
	for(i=4;i<100;){
		switch(i){
			case 4: 
				buff = addr_H; 
				break;
			case 36:
				buff = addr_L;
				break;
			case 68:
				buff = Data;
				break;
		}
		for(j=0;j<8;){
			if(buff & 0x80){ //send "1"
				data_frame_buffer[i] = OOK_12LCK_H;
				i++;
				data_frame_buffer[i] = OOK_12LCK_L;
				i++;
				data_frame_buffer[i] = OOK_4LCK_H;
				i++;
				data_frame_buffer[i] = OOK_4LCK_L;
				i++;
			}
			else{ //send "0"
				data_frame_buffer[i] = OOK_4LCK_H;
				i++;
				data_frame_buffer[i] = OOK_4LCK_L;
				i++;
				data_frame_buffer[i] = OOK_12LCK_H;
				i++;
				data_frame_buffer[i] = OOK_12LCK_L;
				i++;
			}
			j++;
			buff <<=1;
		}
	}
	
	TR0 = 1; //start timer0
	TMOD = 0x01;
	ET0 =0;
	
	P3M0 |= 0x04; // pp mode for P3.2
	OOK_Data = 0;
	
	for(k=0;k<20;k++){ //send dataframe 20 times
		for(i=0;i<100;){
			TF0 = 0; //clear timerINT flag
			OOK_Data = !OOK_Data; // flip current state
			TH0 = data_frame_buffer[i];
			i++;
			TL0 = data_frame_buffer[i];
			i++;
			
			while(!TF0); //waiting timer0INT flag = 1
		}
	}
	
	TF0 = 0;
	TR0 = 0;
	OOK_Data = 0;
	P3M0 &= ~0x04;
	
}

/*************************************************************************
Function: void System_init(void)

	system parameters initial
**************************************************************************/
void System_init(void){
	// initialize port:
	P0M0 = 0x00;
	P0M1 = 0x00;
	P1M0 = 0x00;
	P1M1 = 0x00;
	P2M0 = 0x00;
	P2M1 = 0x00;
	P3M0 = 0x00; 
	P3M1 = 0x00;
	P4M0 = 0x00;
	P4M1 = 0x00;
	P5M0 = 0x00;
	P5M1 = 0x00;
	P6M0 = 0x00;
	P6M1 = 0x00;
	P7M0 = 0x00;
	P7M1 = 0x00;
	//P3M0 |= (1<<1); //0000 0010
	//P3M0 |= 0x04; // pp mode for P3.2
	
	//P3M0 |= (1<<5);
	P3M1 |= (1<<5); // floating input mode for P3.5 for low-battery consumption 
	
	LED1 = 1;
	LED2 = 1;
	delay_x10ms(200);
	LED1 = 0;
	LED2 = 0;
	
	OOK_Data = 0;
	KAI = 1;
	GUAN = 1;
	BAT_volt = 1;
	
	door_flag = 0;
	volt_low_flag = 0;
	
	INT_CLKO |= (1<<4); //INT2 down-edge trigger only
	INT_CLKO |= (1<<5); //INT3 down-edge trigger only
	IT1 = 1; //down-edge trigger INT1
	EX1 = 1; //INT1
	AUXR &= 0x7f; //12T mode
	TMOD = 0x01;
	TR0 = 0; // timer0
	ET0 = 0;
	EA = 1;
}

/************************************************************************
Function: void door_status_detect(void)

	according door status change the door_flag value
*************************************************************************/
void door_status_detect(void){
	
	if(door_flag == 2){
		door_flag = 0;
		delay_x10ms(1);
		
		//P3M0 |= (1<<4);
		P3M1 |= (1<<4); // floating input mode for P3.4 for low-battery consumption 
		P3M0 &= (0<<5);
		P3M1 &= (0<<5); // normal mode for P3.5 for low-battery consumption 
		
		if(GUAN == 0){
			LED1 = 1;
			OOK_Data_Sent(0x0E);
			LED1 = 0;
		}
	}
		
	if(door_flag == 1){
		door_flag = 0;
		delay_x10ms(1);
		
		//P3M0 |= (1<<5);
		P3M1 |= (1<<5); // floating input mode for P3.5 for low-battery consumption 
		P3M0 &= (0<<4);
		P3M1 &= (0<<4); // normal mode for P3.4 for low-battery consumption 
		
		if(KAI == 0){
			LED1 = 1;
			OOK_Data_Sent(0x0A);
			LED1 = 0;
		}
	}
}

/**************************************************************************
Function: void battery_lowvolt_detect(void)

	according Bat_volt status change the volt_low_flag value
***************************************************************************/
void battery_lowvolt_detect(void){
	if(volt_low_flag == 1){
		delay_x10ms(1);
		if(BAT_volt == 0){
			OOK_Data_Sent(0x06);
			volt_low_flag = 2;
		}
		else{
			volt_low_flag = 0;
		}
	}
}

/*************************************************************************
Function: void battery_lowvolt(void)

	battery enter low volt status
**************************************************************************/
void battery_lowvolt(void){
	if(volt_low_flag == 2){
		LED2 = 1;
		delay_x10ms(100);
		LED2 = 0;
		delay_x10ms(100);
	}
}

/***********************************************************************
Function: void System_sleep(void)
	
	enter powder down mode
************************************************************************/
void System_sleep(void){
	
	if(door_flag == 0){
		battery_lowvolt();
		PCON = 0x02;
		_nop_();
		_nop_();
		_nop_();
		_nop_();
	}
}

/*************************************************************************
Function: main(void)

**************************************************************************/
void main(void){
	
	System_init();
	
	while(1){
		
		door_status_detect();
		battery_lowvolt_detect();
		
		System_sleep();
	}
}


/**************************************************************************
Interrupt Rountine:
	interrupt 2;
	interrupt 10;
	interrupt 11;
***************************************************************************/

void Int2_Routine(void) interrupt 10{
	door_flag = 2;
}

void Int3_Routine(void) interrupt 11{
	door_flag = 1;
}

void Int1_Routine(void) interrupt 2{
	volt_low_flag =1;
}
