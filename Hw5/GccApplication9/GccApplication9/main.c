/*
 * Servo_Demo.c
 *
 * Created: 12/10/2021 5:26:14 PM
 * Author : Aaron Liiva
 
Manual Servo Signal: PINB5
LED Blinker: PINB0
*/


#ifndef F_CPU

#ifdef USING_BOOTLOADER
#define F_CPU 2000000UL
#else
#define F_CPU 16000000UL
#endif

#endif

#include <stdlib.h>
#include<inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
struct Lock
{
	uint8_t  m_angle;
	char m_number;
	struct  Lock * rightLock;
	struct Lock * leftLock;	
};


struct Lock* myLock; // pointer to the current lock

void SetupInterrupts(void);
void BootLoaderFixes(void);

//Messages that display up, down, left, right and center onto the LCD screen
void showUP();
void showDOWN();
void showRIGHT();
void showLEFT();
void showCENTER();

#define scrollDelay 			250
#define LCD_REGISTER_COUNT      20
#define LCD_INITIAL_CONTRAST    0x0F
// LCD Data Register 0    // DEVICE SPECIFIC!!! (ATmega169)
#define pLCDREG ((unsigned char *)(0xEC))
// DEVICE SPECIFIC!!! (ATmega169) First LCD segment register
#define LCD_CONTRAST_LEVEL(level) LCDCCR=((LCDCCR&0xF0)|(0x0F & level))
#define FALSE   0
#define TRUE    (!FALSE)

#define PORTB5_SPEAKER_MASK 0b00100000
#define NOTE_A 3520

#define NOTE_B 2959

#define NOTE_C 2349

#define NOTE_D 1975

uint8_t otherBoolFlag = 1;
int angle = 0;

unsigned int LCD_character_table[] = {
	0x0A51,     // '*' (?)
	0x2A80,     // '+'
	0x0000,     // ',' (Not defined)
	0x0A00,     // '-'
	0x0A51,     // '.' Degree sign
	0x0000,     // '/' (Not defined)
	0x5559,     // '0'
	0x0118,     // '1'
	0x1e11,     // '2
	0x1b11,     // '3
	0x0b50,     // '4
	0x1b41,     // '5
	0x1f41,     // '6
	0x0111,     // '7
	0x1f51,     // '8
	0x1b51,     // '9'
	0x0000,     // ':' (Not defined)
	0x0000,     // ';' (Not defined)
	0x0000,     // '<' (Not defined)
	0x0000,     // '=' (Not defined)
	0x0000,     // '>' (Not defined)
	0x0000,     // '?' (Not defined)
	0x0000,     // '@' (Not defined)
	0x0f51,     // 'A' (+ 'a')
	0x3991,     // 'B' (+ 'b')
	0x1441,     // 'C' (+ 'c')
	0x3191,     // 'D' (+ 'd')
	0x1e41,     // 'E' (+ 'e')
	0x0e41,     // 'F' (+ 'f')
	0x1d41,     // 'G' (+ 'g')
	0x0f50,     // 'H' (+ 'h')
	0x2080,     // 'I' (+ 'i')
	0x1510,     // 'J' (+ 'j')
	0x8648,     // 'K' (+ 'k')
	0x1440,     // 'L' (+ 'l')
	0x0578,     // 'M' (+ 'm')
	0x8570,     // 'N' (+ 'n')
	0x1551,     // 'O' (+ 'o')
	0x0e51,     // 'P' (+ 'p')
	0x9551,     // 'Q' (+ 'q')
	0x8e51,     // 'R' (+ 'r')
	0x9021,     // 'S' (+ 's')
	0x2081,     // 'T' (+ 't')
	0x1550,     // 'U' (+ 'u')
	0x4448,     // 'V' (+ 'v')
	0xc550,     // 'W' (+ 'w')
	0xc028,     // 'X' (+ 'x')
	0x2028,     // 'Y' (+ 'y')
	0x5009,     // 'Z' (+ 'z')
	0x0000,     // '[' (Not defined)
	0x0000,     // '\' (Not defined)
	0x0000,     // ']' (Not defined)
	0x0000,     // '^' (Not defined)
	0x0000      // '_'
};

void LCD_AllSegments(char show)
{
	unsigned char i;

	if (show)
	show = 0xFF;

	// Set/clear all bits in all LCD registers
	for (i=0; i < LCD_REGISTER_COUNT; i++)
	*(pLCDREG + i) = show;
}

void LCD_Init (void)
{
	LCD_AllSegments(FALSE);                     // Clear segment buffer.
	LCD_CONTRAST_LEVEL(LCD_INITIAL_CONTRAST);    //Set the LCD contrast level
	// Select asynchronous clock source, enable all COM pins and enable all
	// segment pins.
	LCDCRB = (1<<LCDCS) | (3<<LCDMUX0) | (7<<LCDPM0);
	// Set LCD prescaler to give a framerate of 32,0 Hz
	LCDFRR = (0<<LCDPS0) | (7<<LCDCD0);
	LCDCRA = (1<<LCDEN) | (1<<LCDAB);           // Enable LCD and set low power waveform
	//Enable LCD start of frame interrupt
	//LCDCRA |= (1<<LCDIE);  // fixed--don't need this
	//updated 2006-10-10, setting LCD drive time to 1150us in FW rev 07,
	//instead of previous 300us in FW rev 06. Due to some variations on the LCD
	//glass provided to the AVR Butterfly production.
	LCDCCR |= (1<<LCDDC2) | (1<<LCDDC1) | (1<<LCDDC0);
	//gLCD_Update_Required = FALSE;
}

void LCD_WriteDigit(char c, char digit)
{

	unsigned int seg = 0x0000;                  // Holds the segment pattern
	char mask, nibble;
	char *ptr;
	char i;


	if (digit > 5)                              // Skip if digit is illegal
	return;

	//Lookup character table for segmet data
	if ((c >= '*') && (c <= 'z'))
	{
		// c is a letter
		if (c >= 'a')                           // Convert to upper case
		c &= ~0x20;                         // if necessarry

		c -= '*';

		seg = LCD_character_table[c];
	}

	// Adjust mask according to LCD segment mapping
	if (digit & 0x01)
	mask = 0x0F;                // Digit 1, 3, 5
	else
	mask = 0xF0;                // Digit 0, 2, 4

	//ptr = LCD_Data + (digit >> 1);  // digit = {0,0,1,1,2,2}
	ptr = pLCDREG + (digit >> 1);  // digit = {0,0,1,1,2,2}

	for (i = 0; i < 4; i++)
	{
		nibble = seg & 0x000F;
		seg >>= 4;
		if (digit & 0x01)
		nibble <<= 4;
		*ptr = (*ptr & mask) | nibble;
		ptr += 5;
	}
}

void WelcomeMessage(){
	for (int i = 0; i != 1; i++){
		LCD_AllSegments(FALSE);
		LCD_WriteDigit(0, 0);
		LCD_WriteDigit(0, 1);
		LCD_WriteDigit(0, 2);
		LCD_WriteDigit(0, 3);
		LCD_WriteDigit('W', 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit(0, 0);
		LCD_WriteDigit(0, 1);
		LCD_WriteDigit(0, 2);
		LCD_WriteDigit('W', 3);
		LCD_WriteDigit('E', 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit(0, 0);
		LCD_WriteDigit(0, 1);
		LCD_WriteDigit('W', 2);
		LCD_WriteDigit('E', 3);
		LCD_WriteDigit('L', 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit(0, 0);
		LCD_WriteDigit('W', 1);
		LCD_WriteDigit('E', 2);
		LCD_WriteDigit('L', 3);
		LCD_WriteDigit('C', 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit('W', 0);
		LCD_WriteDigit('E', 1);
		LCD_WriteDigit('L', 2);
		LCD_WriteDigit('C', 3);
		LCD_WriteDigit('O', 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit('E', 0);
		LCD_WriteDigit('L', 1);
		LCD_WriteDigit('C', 2);
		LCD_WriteDigit('O', 3);
		LCD_WriteDigit('M', 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit('L', 0);
		LCD_WriteDigit('C', 1);
		LCD_WriteDigit('O', 2);
		LCD_WriteDigit('M', 3);
		LCD_WriteDigit('E', 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit('C', 0);
		LCD_WriteDigit('O', 1);
		LCD_WriteDigit('M', 2);
		LCD_WriteDigit('E', 3);
		LCD_WriteDigit(0, 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit('O', 0);
		LCD_WriteDigit('M', 1);
		LCD_WriteDigit('E', 2);
		LCD_WriteDigit(0, 3);
		LCD_WriteDigit(0, 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit('M', 0);
		LCD_WriteDigit('E', 1);
		LCD_WriteDigit(0, 2);
		LCD_WriteDigit(0, 3);
		LCD_WriteDigit(0, 4);
		_delay_ms(scrollDelay);
		LCD_WriteDigit('E', 0);
		LCD_WriteDigit(0, 1);
		LCD_WriteDigit(0, 2);
		LCD_WriteDigit(0, 3);
		LCD_WriteDigit(0, 4);
		_delay_ms(scrollDelay);
		}
}

void PressToStart(){
	sei();
	char myString[46]= "      LOCKPICKING GAME PRESS CENTER TO START        ";
	otherBoolFlag = 1;
	while(otherBoolFlag){
		for(int i = 0; i != 45; i++){
			LCD_AllSegments(FALSE);
			LCD_WriteDigit(myString[i], 0);
			LCD_WriteDigit(myString[i+1], 1);
			LCD_WriteDigit(myString[i+2], 2);
			LCD_WriteDigit(myString[i+3], 3);
			LCD_WriteDigit(myString[i+4], 4);
			LCD_WriteDigit(myString[i+5], 5);
			_delay_ms(scrollDelay);
		}
	}
	cli();
}

void PressToQuit(){
	sei();
	char myString[100]= "    PRESS UP-DOWN TO PICK LOCK LED WILL TURN ON WHEN CORRECT PRESS CENTER TO QUIT        ";
	otherBoolFlag = 1;
	while(otherBoolFlag){
		for(int i = 0; i != 83; i++){
			LCD_AllSegments(FALSE);
			LCD_WriteDigit(myString[i], 0);
			LCD_WriteDigit(myString[i+1], 1);
			LCD_WriteDigit(myString[i+2], 2);
			LCD_WriteDigit(myString[i+3], 3);
			LCD_WriteDigit(myString[i+4], 4);
			LCD_WriteDigit(myString[i+5], 5);
			_delay_ms(scrollDelay);
		}
	}
	cli();
}

//Fills lock structures in a circular linked list, allows lock selection using right and left switch toggles.
void LockMenu(){
	struct Lock *lockOne, *lockTwo, *lockThree;
	lockOne = (struct Lock*)malloc(sizeof(struct Lock));
	lockOne->m_angle = 7;
	lockOne->m_number = '1';

	lockTwo = (struct Lock*)malloc(sizeof(struct Lock));
	lockTwo->m_angle = 11;
	lockTwo->m_number = '2';

	lockThree = (struct Lock*)malloc(sizeof(struct Lock));
	lockThree->m_angle = 15;
	lockThree->m_number = '3';

	lockOne->rightLock = lockTwo;
	lockOne->leftLock = lockThree;

	lockTwo->rightLock = lockThree;
	lockTwo->leftLock = lockOne;
	
	lockThree->rightLock = lockOne;
	lockThree->leftLock = lockTwo;

	myLock = lockOne;
	
	static uint8_t p4Prev=1;
	static uint8_t p2Prev=1; //for storing previous value of PE2 to detect
	static uint8_t p3Prev=1; //for storing previous value of PE3 to detect

	//while center button is not 1
	uint8_t boolFlag = 1;
	while(boolFlag){
		LCD_AllSegments(FALSE);
		LCD_WriteDigit('L', 0);
		LCD_WriteDigit('O', 1);
		LCD_WriteDigit('C', 2);
		LCD_WriteDigit('K', 3);
		LCD_WriteDigit(myLock->m_number, 4);
		_delay_ms(scrollDelay);
		
		if(((PINB & (1<<4))  == 0) &&
		((p4Prev & (1<<4))  != 0))
		{	
			while((PINB & (1<<4)) == 0){
				showCENTER();
			}
			boolFlag = 0;
		}
		//when on LEFT button status being newly pressed, but not when it is released
		if(((PINE & (1<<2))  == 0) &&
		((p2Prev & (1<<2))  != 0))
		{
			while((PINE & (1<<2))  == 0) //while button is pressed
			{
				showLEFT();
			
			}
			myLock = myLock->leftLock;
		}
		else if(((PINE & (1<<3))  == 0) &&
		((p3Prev & (1<<3))  != 0)) //when on RIGHT button status being newly pressed, but not when it is released
		{
			while((PINE & (1<<3))  == 0) //while button is pressed
			{
				showRIGHT();
				//Acts as debounce
			}
			myLock = myLock->rightLock;
		}
		else
		{
			//do nothing since I still don't know how we got here
		}
		p4Prev = (PINB);

		p2Prev = (PINE); //save LEFT button status
		p3Prev = (PINE); //save DOWN button status
	}
}

//This while loop is controlled by the the otherBoolFlag variable, which is itself controlled by the center button interupt
void Lockpicking(){
	otherBoolFlag = 1;
	while(otherBoolFlag){
		PORTD |= (1<<1); //turns on led
		_delay_ms(500);
		PORTD &= ~(1<<1); //turns off led
		_delay_ms(500);

		//Display current angle
		LCD_AllSegments(FALSE);
		LCD_WriteDigit(0, 0);
		LCD_WriteDigit(0, 1);
		char a, b, c;
		a = ((angle * 8) / 100);
		b = (((angle * 8) % 100) / 10);
		c = ((angle * 8) % 10);
		LCD_WriteDigit(a+48, 2);
		LCD_WriteDigit(b+48, 3);
		LCD_WriteDigit(c+48, 4);
		_delay_ms(scrollDelay);

		//Check current angle against current lock goal angle, blink for 7 cycles if true
		if (abs(angle - myLock->m_angle)==0){
			DDRB |= (1<<0); //Putting a 1 in bit 1 of DDRB, because this will be our led, output
			for (int i = 0; i != 7; i++){
				PORTB |= (1<<0); //turns on led
				_delay_ms(50);
				PORTB &= ~(1<<0); //turns off led
				_delay_ms(50);
				
			}
		}
	}
	
}

//Cleans up the dynamically allocated locks, makes sure that the light is no longer on
void EndRoutine(){
	struct Lock * tempLock = myLock->rightLock;
	free(myLock);
	myLock = tempLock;
	tempLock = myLock->rightLock;
	free(myLock);
	myLock = tempLock;
	tempLock = myLock->rightLock;
	free(myLock);
	for (int i = 0; i != angle; i++){
		PORTB |= 0b00000010;
		_delay_ms(1.5);
		PORTB ^= 0b00000010; //XORs and ORs are a safe way to change bits in a PORT
		_delay_ms(7.5);
	}
	angle = 0;	
}

int main(void)
{
	
	//Allows interrupts in debug mode
	#ifdef USING_BOOTLOADER
	BootLoaderFixes();
	#endif

	//DDRB |= 1 << 5; //Set PINB5 as output
	//Setup All Pushbuttons
	DDRB |= 1 << PINB1;
	//DDRB |= 1 << PINB0; //This is commented out because it messes everything up
	//PORTB &= ~(1<<0); //turns off led
	
	DDRB  &= ~0b11010000;  //set B6,B7, B4 as inputs, B1 is an output
	DDRE  &= ~0b00001100;  //set E2, E3 to inputs
	PORTB |=  0b11010000;  //enable pull up resistors on B4,B6,B7
	PORTE |=  0b00001100;  //enable pull up resistor on pin E2,E3
		
	LCD_Init(); //Initialize LCD
	SetupInterrupts();	//setup the interrupts
	sei();				//enable global interrupts
	//cli(); //disables global interrupts

	
    while (1)
    {
		cli();
		WelcomeMessage();
		PressToStart();
		PressToQuit();
		LockMenu();
		sei();
		Lockpicking();
		EndRoutine();
    }
	return 0;
}

void SetupInterrupts(void)
{
	PCMSK1  |= (1<<PCINT12);
	PCMSK1  |= (1<<PCINT14); //Unmask bit for UP Button on Butterfly, PB6->PCINT14 to allow it to trigger interrupt flag PCIF1
	PCMSK1  |= (1<<PCINT15); //Unmask bit for DOWN Button on Butterfly, PB7->PCINT15 to allow it to trigger interrupt flag PCIF1
	EIMSK   = (1<<PCIE0) | (1<<PCIE1);    //Enable interrupt for flag PCIF1 and PCIF0
	//Enable interrupts from PCIE0
}

//This performs adjustments needed to undo actions of Butterfly boot loader
void BootLoaderFixes(void)
{
	//Boot loader Disables E2 and E3 digital input drivers, which are used for left and right
	//The following code re-enables them by clearing the appropriate disable bits
	DIDR1 &= ~((1<<AIN0D)|(1<<AIN1D));
}

ISR(PCINT1_vect) 		//remember this is called on pin change 0->1 and 1->0
{
	static uint8_t p4Prev=1;
	static uint8_t p6Prev=1; //for storing previous value of PB6 to detect
	static uint8_t p7Prev=1; //for storing previous value of PB7 to detect
	
	if(((PINB & (1<<4))  == 0) &&
	((p4Prev & (1<<4))  != 0)) //when on UP button status being newly pressed, but not when it is released
	{
		
		while(((PINB & (1<<4))  == 0)) //while button is pressed
		{
			showCENTER();
		}
		otherBoolFlag = 0;
	
	}

	
	if(((PINB & (1<<6))  == 0) &&
	((p6Prev & (1<<6))  != 0)) //when on UP button status being newly pressed, but not when it is released
	{
		showUP();
		if(angle > 0){
			while(((PINB & (1<<6))  == 0)) //while button is pressed
			{
				//On time of 1.5ms + Off time of 7.5ms, causes counterclockwise rotation
				PORTB |= 0b00000010;
				_delay_ms(1.5);
				PORTB ^= 0b00000010; //XORs and ORs are a safe way to change bits in a PORT
				_delay_ms(7.5);
				angle--;
			}	
		}
	}
	else if(((PINB & (1<<7))  == 0) &&
	((p7Prev & (1<<7))  != 0)) //when on DOWN button status being newly pressed, but not when it is released
	{
		showDOWN();
		if(angle < 23){
			while(((PINB & (1<<7))  == 0)) //while button is pressed
			{
				//On time of 2.5ms + Off time of 19ms, causes clockwise rotation
				PORTB |= 0b00000010;
				_delay_ms(2.5);
				PORTB ^= 0b00000010;
				_delay_ms(19);
				angle++;
			}
		}
	}
	else
	{
		//do nothing since I don't know how we got here
	}
	p4Prev = (PINB); //save UP button status
	p6Prev = (PINB); //save UP button status
	p7Prev = (PINB); //save DOWN button status
}


//LCD functions
void showUP()
{
	LCD_AllSegments(FALSE);
	LCD_WriteDigit('U', 0);
	LCD_WriteDigit('P', 1);
	_delay_ms(100);
	return;
}

void showDOWN()
{
	LCD_AllSegments(FALSE);
	LCD_WriteDigit('D', 0);
	LCD_WriteDigit('O', 1);
	LCD_WriteDigit('W', 2);
	LCD_WriteDigit('N', 3);
	_delay_ms(100);
	return;
}

void showRIGHT()
{
	LCD_AllSegments(FALSE);
	LCD_WriteDigit('R', 0);
	LCD_WriteDigit('I', 1);
	LCD_WriteDigit('G', 2);
	LCD_WriteDigit('H', 3);
	LCD_WriteDigit('T', 4);
	_delay_ms(100);
	return;
}

void showLEFT()
{
	LCD_AllSegments(FALSE);
	LCD_WriteDigit('L', 0);
	LCD_WriteDigit('E', 1);
	LCD_WriteDigit('F', 2);
	LCD_WriteDigit('T', 3);
	_delay_ms(100);
	return;
}

void showCENTER()
{
	LCD_AllSegments(FALSE);
	LCD_WriteDigit('C', 0);
	LCD_WriteDigit('E', 1);
	LCD_WriteDigit('N', 2);
	LCD_WriteDigit('T', 3);
	LCD_WriteDigit('E', 4);
	LCD_WriteDigit('R', 5);
	_delay_ms(100);
	return;
}