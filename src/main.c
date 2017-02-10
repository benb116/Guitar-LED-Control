#include "m_general.h"
#include "m_bus.h"
#include "m_rf.h"
#include "m_usb.h"

#define RES 4000
#define NUMVAL 600

volatile int ledstatus[9] = {0};
int vals[NUMVAL] = {0};

int enable = 1;
int h = 0;
int i = 0;
int curval = 0;
int dripIndex = 0;

void initTimer1();
void initADC();
void initPins();

void allOn();

void dripIncrement();
void dripLED();

void lightLEDs();

int main(void)
{
	m_disableJTAG();
	m_clockdivide(4); // System clock 1 MHz
	m_bus_init();
	m_usb_init();
	while(!m_usb_isconnected()) {}
	initTimer1();
	initPins();
	initADC();
	sei(); // Enable interrupts
	m_red(ON);
	while(1) {
		if (curval > 100) {
			// m_green(ON);
			ledstatus[1] = 1;
			// set(PORTB, 3);
		} else {
			// m_green(OFF);
			ledstatus[1] = 0;
			// clear(PORTB, 3);
		}
		// allOn();
		// dripLED();
		lightLEDs();
	}
	return 0;
}

ISR(TIMER1_OVF_vect) { // When OCR1A is reached (4 kHz)
	dripIncrement();

	curval = ADC;
	vals[i] = curval;
	
}

void initTimer1() {
	set(TCCR1B, WGM13); // Use single slope up to OCR1A (mode 15)
	set(TCCR1B, WGM12);
	set(TCCR1A, WGM11); 
	set(TCCR1A, WGM10);

	set(TCCR1B, CS11); // Clock scaling /8

	set(TIMSK1, TOIE1); // Interrupt when overflow

	// 1 MHz clock / timer presale / sample rate
	int count = (1000000 / 8 / RES);
	OCR1A = count;
}

void initADC() {
	clear(ADMUX, REFS1); // Set ref voltage to VCC
	set(ADMUX, REFS0);

	set(ADCSRA, ADPS1); // Set clock scale CHANGE IF clockdivide CHANGES!!!!!!
	set(ADCSRA, ADPS0); // Set clock scale CHANGE IF clockdivide CHANGES!!!!!!

    set(DIDR2, ADC11D); // Set as analog input B4

	set(ADCSRA, ADIE); // Enable interrupting

	set(ADCSRA, ADATE); // Set triggering

	set(ADCSRB, MUX5); // Look at B4
	clear(ADMUX, MUX2);
	set(ADMUX, MUX1);
	set(ADMUX, MUX0);

	set(ADCSRA, ADEN); // Conversion
	set(ADCSRA, ADSC);

	set(ADCSRA, ADIF); // Reading result
}
void initPins() {
	set(DDRB, 2);
	set(DDRB, 3);
	set(DDRB, 7);
	set(DDRD, 3);
	set(DDRD, 4);
	set(DDRD, 5);
	set(DDRD, 6);
	set(DDRF, 0);
	set(DDRF, 1);
	// clear(DDRB, 4); // B4 is ADC Input

	clear(PORTB, 2);
	clear(PORTB, 3);
	clear(PORTB, 7);
	clear(PORTD, 3);
	clear(PORTD, 4);
	clear(PORTD, 5);
	clear(PORTD, 6);
	clear(PORTF, 0);
	clear(PORTF, 1);
	// clear(PORTB, 4); // Disable pull-up resistor
}

void dripIncrement() {
	if (h >= RES) {
		h = 0;
	}
	if (h % (RES / 20) == 0) {
		++dripIndex;
	}
	if (dripIndex >= 20) {
		dripIndex = 0;
	}
	++h;

	++i;
	if (i >= NUMVAL) {
		i = 0;
	}
}

void dripLED() {
	int j;
	for (j = 0; j < 9; ++j) {
		if (dripIndex < 10) {
			ledstatus[j] = (j <= dripIndex);
		} else {
			ledstatus[j] = ((j+10) >= dripIndex);
		}
	}
}

void allOn() {
	ledstatus[0] = 1;
	ledstatus[1] = 1;
	ledstatus[2] = 1;
	ledstatus[3] = 1;
	ledstatus[4] = 1;
	ledstatus[5] = 1;
	ledstatus[6] = 1;
	ledstatus[7] = 1;
	ledstatus[8] = 1;
}

void lightLEDs() {
	if (enable){
		if (ledstatus[0]) {set(PORTB, 2);} else {clear(PORTB, 2);}
		if (ledstatus[1]) {set(PORTB, 3);} else {clear(PORTB, 3);}
		if (ledstatus[2]) {set(PORTB, 7);} else {clear(PORTB, 7);}
		if (ledstatus[3]) {set(PORTD, 3);} else {clear(PORTD, 3);}
		if (ledstatus[4]) {set(PORTD, 4);} else {clear(PORTD, 4);}
		if (ledstatus[5]) {set(PORTD, 5);} else {clear(PORTD, 5);}
		if (ledstatus[6]) {set(PORTD, 6);} else {clear(PORTD, 6);}
		if (ledstatus[7]) {set(PORTF, 0);} else {clear(PORTF, 0);}
		if (ledstatus[8]) {set(PORTF, 1);} else {clear(PORTF, 1);}
	}
}