
/*	FINALPROJECT_AUTOMATICAL_DREAMGARDEN
 * 
 *
 * Created: 8/9/2020
 * Author :
 Instructor: PhD. Vo Minh Thanh
 */

#define F_CPU 8000000UL	
#include <stdio.h>
#include "lcd.h"	
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>	
#include <util/delay.h>		
#include "adc.h"	
#include "stdlib.h"
#include "keypad.h"
#include "rtc.h"
#define SS 4		// Slave Select        is Bit No.4
#define MOSI 5 		// Master Out Slave In is Bit No.5
#define MISO 6		// Master In Slave Out is Bit No.6
#define SCK 7 		// Shift Clock         is Bit No.7
	
char Shownhietdo[2],Showdoam[2],Shownguoi[3],Showkey[1]; //declare buffer for displaying temperature(temperature sensor), moisture(from SPI TC72 sensor), people (IR sensor)
int cout2=0,cout1=0,stt=1,count=0,cout3=0; //declare counting variables
uint8_t key,minute,hour,alarmode=0; //declare variables for RTC
double Alarm=0,Time=0;              //declare variables for Autonomous Pumping Mode
static volatile uint8_t LCDtt=1;    //declare variables to keep track of LCD screen
static volatile int doamsetup=50,tempsetup=50; //declare variables to input temperature and moisture from PC through UART
volatile char ch,rx_flag=0;
rtc_t rtc;
bool Day;

void main()
{	
	DDRC &= ~(1<<PC2); //Config PC2 input 
	DDRC &= ~(1<<PC3); //Config PC3 input
	DDRC &= ~(1<<PC4); //Config PC4 input
	PORTC |= (1<<PC2)|(1<<PC3)|(1<<PC4); //Use pull-up resistor for PC2, PC3, PC4

	DDRA |= (1<<PA4)|(1<<PA5); //Config PA4, PA5 output
	
	LCDInit(LS_NONE);  //Initialize LCD, no cursor
	initADC();		   //Initialize ADC
	SPI_MasterInit();  //Initialize SPI, master mode
	TC72_init();	   //Initialize TC72
	RTC_Init();		   //Initialize RTC
	LCDClear();		   //Clear LCD display
	KEYPAD_Init(PA_6,PA_3,PC_5,PC_6,PB_0,PB_1,PB_3,PA_7); //Initialize keypad at pin PA6, PA3, PC5, PC6, PB0, PB1, PB3, PA7
	TCNT0=178; //100ms
	TCCR0= (1<<CS00) | (1<<CS02); // prescaler 1024
	TIMSK = (1<<TOIE0); //Enable Timer0 Overflow Interrupt on Timer Interrupt Mask resistor
	MCUCSR = (1<<ISC2); //Config external hardware interrupt of INT2 is raising edge triggered.
	GICR  = (1<<INT2);  //Enable external hardware interrupt 2
	UCSRB= 0x98;   // Enable Receiver,Transmitter,Receiver interrupt(RXIE)
	UCSRC= 0x86;   // Asynchronous mode 8-bit data and 1-stop bit
	UCSRA= 0x00;   // Normal Baud rate(no doubling), Single processor commn
	UBRRL= 51;	   // Set baud rate 9600
	UBRRH= 0;      //
	sei (); 			// enable global interrupts

	while (1)
	{	
		if (ReadADC(1)/10>50) //check daytime/ nighttime
		{
			Day=true; //on daytime
			GICR  &= ~(1<<INT2); //Disable External Hardware Interrupt INT2, deactivate IR sensor, do not count people 
			cout1=0; // reset counter 
		}
		else
		{
			Day=false; //on nighttime
			GICR  = (1<<INT2); //Enable External Hardware Interrupt INT2, activate IR sensor, count people 
			cout2=cout1; // save the visitor value of the previous night (the initial people counter = 0 when it turn to daytime)  
		}
		
		minute = (uint16_t)rtc.min; //get current minute from real time clock
		minute = minute - checknumber(minute); //correct minute
		hour= (uint16_t)rtc.hour;   ////get current hour from real time clock
		hour= hour - checknumber(hour); //correct hour
		Time = (minute)+(hour)*60; //compute time for comparison (alarm mode)
		
		if ((alarmode==1)&&(Time==Alarm)) //if in alarm mode is turned on and setup time = alarm.
		{
			PORTA |= (1<<PA4); //activate water pump  
		}
		if (count == 10) // 
		{
			count=0; //Interrupt overflow each 1s
			if (ReadADC(0)/10<=doamsetup) //compare setup moisture with measured moisture
			{
				PORTA |= (1<<PA4); //activate moisture 
			}
			else {PORTA &= ~(1<<PA4);} ////deactivate moisture 
			if (ReadADC(2)/2-1>=tempsetup) //compare setup temperature with measured temperature
			{
				PORTA |= (1<<PA5); //activate temperature
			}
			else {PORTA &= ~(1<<PA5);} ////deactivate temperature 
		}
		switch (LCDtt)
		{
			case 1:
			LCDchinh(); //LCD display main screen
			break;
			case 2:
			LCDsntc(); //LCD display people monitor 
			break;
			case 3:
			LCDsetup(); //LCD display set up temperature & moisture form UART
			break;
			case 4:
			LCDhengio(); //LCD display timer-pump set up from keyboard
			break;
		}
		if (PINC==(PINC&~(1<<PC2))) //check PC2 status - people-monitoring button pressed
		{
			LCDtt=2; //change the screen tracker to 2
			_delay_ms(200);
		}
		
		if (PINC==(PINC&~(1<<PC3))) //check PC3 status - main screen button pressed 
		{
			LCDtt=1; //change the screen tracker to 1
			_delay_ms(200);
		}
		
		if (PINC==(PINC&~(1<<PC4))) //check PC4 status - setup button  
		{	
			cout3++; //change state of setup button
			if (cout3==1)//press 1 time
			{
				LCDtt=3;//go to temperature & moisture screen
			}
			if (cout3==2)//press 2 time
			{
				LCDtt=4;//go to timer-pump set up screen
				cout3=0;//
			}
			_delay_ms(300);//
		}
		
		
		
	}
}
void LCDsntc(void)
{
	LCDClear(); 
	LCDGotoXY(0,0);
	LCDWriteString("Dem qua co:");
	itoa(cout2,Shownguoi,10); //convert number of people (from type int to string) to display
	LCDGotoXY(0,1);
	LCDWriteString(Shownguoi);
	LCDWriteString(" truy cap");
	_delay_ms(100);	
}
void LCDchinh(void){
	LCDClear();
	LCDGotoXY(0,0);
	LCDWriteString("Nhietdo:");
	itoa(gettemperature(),Shownhietdo,10); //convert setup temperature (from type int to string) to display
	//itoa(tempsetup,Shownhietdo,10); 
	LCDWriteString(Shownhietdo);
	LCDGotoXY(0,1);
	LCDWriteString("Soil:");
	//itoa(ReadADC(0)/(ReadADC(2)/2-1),Showdoam,10);
	itoa(doamsetup,Showdoam,10); //convert setup moisture (from type int to string) to display
	LCDWriteString(Showdoam);
	LCDWriteString(" %");
	_delay_ms(100);
}
void SPI_MasterInit(void){
	
	DDRB |= (1<<MOSI) | (1<<SCK) | (1<<SS); // config MOSI, SCK, SS output
	DDRB &= ~(1<<MISO); //config MISO input
	
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0); // enable SPI, master mode, Shift Clock = CLK /16
	PORTB &= ~(1<<SS); //// Enable Slave Select Pin
}

void TC72_init(void){
	PORTB |= (1<<SS); // initializing the packet by pulling SS low
	SPI_write(0x80); //write address to control register 
	SPI_write(0x10); //write data to control register: continuous temperature conversion
	PORTB &= ~(1<<SS);  // terminate the packet by pulling SS high
}
float toFloat(signed int tempr) //get temperature in binary type and convert to float type
{
	float result = (float)(tempr>>8);//
	char count = tempr & 0x00C0; //
	
	if(count)
	{
		count = count >> 6; //
		result = result + (count * 0.25);//
	}
	
	return  (result); 
}
double TC72_ReadTempr() //Read temperature
{
	int temprMSB,temprLSB; 
	
	PORTB |= (1<<SS); // initializing the packet by pulling SS low
	
	SPI_write(0x02); //read address command to MSB temperature resister 
	temprMSB = SPI_read();
	temprLSB = SPI_read();
	
	PORTB &= ~(1<<SS); // terminate the packet by pulling SS high
	
	return ( (temprMSB<<8) + temprLSB ); //return in 16 bits datatype
}
void SPI_write(unsigned char cData){
	SPDR = cData;             // Start transmission
	while(!(SPSR & (1<<SPIF))); // Wait for transmission complete
	char clear = SPDR;            // return the received data
}
int SPI_read(){
	SPDR = 0;			// Start transmission of nothing
	while(!(SPSR & (1<<SPIF))); // Wait for transmission complete
	return SPDR;                   //// return the received data
}
int gettemperature(){
	int temp=toFloat(TC72_ReadTempr())*2;
	PORTB &= ~(1<<SS);  //terminate the packet by pulling SS high
	return temp;
}
ISR (INT2_vect){ 		// ISR for external interrupt 2
	cout1++; 	// toggle PORTA.2
}
void tx_char()
{
	while((UCSRA & 0x60)==0); // Wait till Transmitter(UDR) register becomes Empty
	UDR =ch;               // Load the data to be transmitted
}
ISR (USART_RXC_vect)
{
	ch=UDR;         // copy the received data into ch
	rx_flag=1;      //flag is set to indicate a char is received
}
void LCDsetup(){
	_delay_ms(200);
	if (stt==1)
	{
		LCDClear();
		LCDGotoXY(0,0);
		LCDWriteString("Temperature?");
		if(rx_flag==1)
		{
			tx_char();
			tempsetup=ch;
			rx_flag=0;
			stt++;
		}
	}
	if (stt==2)
	{
		LCDClear();
		LCDGotoXY(0,0);
		LCDWriteString("Do am?");
		if(rx_flag==1)
		{
			tx_char();
			doamsetup=ch;
			rx_flag=0;
			stt=1;
			LCDtt=1;
		}
	}
}
ISR(TIMER0_OVF_vect) // Interrupt Overflow each 1s
{
count++;
TCNT0=178;               //100ms
RTC_GetDateTime(&rtc);
}
void LCDhengio(){
	while (1)
	{
		LCDClear();
		LCDGotoXY(0,0);
		LCDWriteString("ALARM:");
		if (alarmode==1)
		{
			LCDWriteString(" ON");
			_delay_ms(50);
		}
		if (alarmode==0)
		{
			LCDWriteString(" OFF");
			_delay_ms(50);
		}
		if (PINC==(PINC&~(1<<PC3)))
		{
			alarmode=1;
			_delay_ms(200);
			Alarm=0;
			break;
		}
		if (PINC==(PINC&~(1<<PC2)))
		{
			alarmode=0;
			_delay_ms(200);
			goto Here;
		}
		if (PINC==(PINC&~(1<<PC4)))
		{	
			_delay_ms(200);
			Alarm=0;
			goto Here;
		}
	}
	LCDClear();
	LCDGotoXY(0,0);
	LCDWriteString("Time:ss/mm/hh");
	LCDGotoXY(2,1);
	LCDWriteString(":");
	LCDGotoXY(5,1);
	LCDWriteString(":");
	uint8_t cout4=0;
	while (1)
	{
		if (cout4==0)
		{
			LCDGotoXY(0,1);
			if (checkkey()==1)
			{	
				key = KEYPAD_GetKey();
				itoa(key-48,Showkey,10);
				LCDWriteString(Showkey);
				_delay_ms(100);
				cout4++;
			}
		}
		if (cout4==1)
		{
			LCDGotoXY(1,1);
			if (checkkey()==1)
			{
				key = KEYPAD_GetKey();
				itoa(key-48,Showkey,10);
				LCDWriteString(Showkey);
				_delay_ms(100);
				cout4++;
			}
		}
		if (cout4==2)
		{
			LCDGotoXY(3,1);
			if (checkkey()==1)
			{
				key = KEYPAD_GetKey();
				itoa(key-48,Showkey,10);
				Alarm=Alarm+(key-48)*10;
				LCDWriteString(Showkey);
				_delay_ms(100);
				cout4++;
			}
		}
		if (cout4==3)
		{
			LCDGotoXY(4,1);
			if (checkkey()==1)
			{
				key = KEYPAD_GetKey();
				itoa(key-48,Showkey,10);
				Alarm=Alarm+(key-48);
				LCDWriteString(Showkey);
				_delay_ms(100);
				cout4++;
			}
		}
		if (cout4==4)
		{
			LCDGotoXY(6,1);
			if (checkkey()==1)
			{
				key = KEYPAD_GetKey();
				itoa(key-48,Showkey,10);
				Alarm=Alarm+(key-48)*60*10;
				LCDWriteString(Showkey);
				_delay_ms(100);
				cout4++;
			}
		}
		if (cout4==5)
		{
			LCDGotoXY(7,1);
			if (checkkey()==1)
			{
				key = KEYPAD_GetKey();
				itoa(key-48,Showkey,10);
				Alarm=Alarm+(key-48)*60;
				LCDWriteString(Showkey);
				_delay_ms(100);
				cout4++;
			}
		}
		if (cout4==6)
		{	
			_delay_ms(3000);
			LCDtt=1;
			break;
		}
		
	}
Here:LCDtt=1;
}
int checkkey()
{	
	key= KEYPAD_GetKey();
	if ((key-48>=0) && (key-48<10))
	{
		return 1;
	}
	else {return 0;}
}
int checknumber(int ndata)
{
	if ((ndata>=16)&&(ndata<=25))
	{
		return 6;
	}
	if ((ndata>=32)&&(ndata<=41))
	{
		return 12;
	}
	if ((ndata>=48)&&(ndata<=57))
	{
		return 18;
	}
	if ((ndata>=64)&&(ndata<=73))
	{
		return 24;
	}
	if ((ndata>=80)&&(ndata<=89))
	{
		return 30;
	}
	if ((ndata>=0)&&(ndata<=9))
	{
		return 0;
	}
}