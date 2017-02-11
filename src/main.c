#include "m_general.h"
#include "m_bus.h"
#include "m_rf.h"
#include "m_usb.h"

#define RES 5000
#define NUMPERS 30
#define PERDIFF 10
#define FREQTOL 0.05

volatile int ledstatus[9] = {0};
int pers[NUMPERS] = {0};
int persShift[NUMPERS];
int pers2nd[NUMPERS/2];
int pers3rd[NUMPERS/3];


int enable = 1;
int h = 0;

int i = 0;
bool ishi = 0;
int perInd = 0;

int dripIndex = 0;

void initTimer1();
void initPins();

void tuneCheck();
void recordPer(int tick);
float calcFreq();
int calcDiff(float freq, float tru);
void tuneLED();

int min_ind(int array[], int size);
int max_ind(int array[], int size);
int avg(int array[], int size);

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

	sei(); // Enable interrupts
	m_red(ON);

	while(1) {
		// allOn();
		// dripLED();
		tuneLED();
		lightLEDs();
		m_usb_tx_int(perInd);
		m_usb_tx_string("\n");
	}
	return 0;
}

ISR(TIMER1_OVF_vect) { // When OCR1A is reached (4 kHz)
	// dripIncrement();
	tuneCheck();
}

void tuneCheck() {
	if (check(PINB, 4)) {
		if (ishi) {
			++i;
		} else {
			recordPer(i);
			i = 0;
			ishi = true;
		}
	} else {
		if (ishi) {
			ishi = false;
		}
		++i;
	}
}

void recordPer(int tick) {
	++perInd;
	if (perInd >= NUMPERS) {perInd = 0;}
	pers[perInd] = tick;
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
	clear(DDRB, 4); // B4 is input

	clear(PORTB, 2);
	clear(PORTB, 3);
	clear(PORTB, 7);
	clear(PORTD, 3);
	clear(PORTD, 4);
	clear(PORTD, 5);
	clear(PORTD, 6);
	clear(PORTF, 0);
	clear(PORTF, 1);
	clear(PORTB, 4); // Disable pull-up resistor
}

void tuneLED() {
	float freq = calcFreq();
	int LED;

	if 		(freq < 70) 	{LED = 0;} 
	else if (freq < 96.2) 	{LED = calcDiff(freq, 82.40);} 
	else if (freq < 128.4) 	{LED = calcDiff(freq, 110.0);} 
	else if (freq < 171.4) 	{LED = calcDiff(freq, 146.8);} 
	else if (freq < 221.4) 	{LED = calcDiff(freq, 196.0);} 
	else if (freq < 288.3) 	{LED = calcDiff(freq, 246.9);} 
	else if (freq < 350) 	{LED = calcDiff(freq, 329.6);} 
	else 					{LED = 0;}

	int j;
	for (j = 0; j < 9; ++j) {
		ledstatus[j] = (j == (LED - 1));
	}
}

float calcFreq() {
	int j;
	int k = (perInd+1);
	for (j = k; j < NUMPERS; ++j) {
		persShift[(j-k)] = pers[j];
	}
	for (j = 0; j < k; ++j) {
		persShift[(j+k)] = pers[j];
	}
	int maxPer = persShift[max_ind(persShift, NUMPERS)];
	int minPer = persShift[min_ind(persShift, NUMPERS)];

	if (maxPer - minPer < PERDIFF) {

	}


	for (j = 0; j < (NUMPERS/2); ++j) {
		pers2nd[j] = persShift[(2*j)] + persShift[(2*j-1)] ;
	}
	for (j = 0; j < (NUMPERS/3); ++j) {
		pers3rd[j] = persShift[(3*j)] + persShift[(3*j-1)] + persShift[(3*j-2)];
	}
}

// Find the index of the minimum value of an array
int min_ind(int array[], int size) {
	int min = array[0];
	int ind = 0;
	int j;
	for (j = 1; j < size; ++j) {
		if (array[j] < min) {
			min = array[j];
			ind = j;
		}
	}
	return ind;
}

// Find the index of the maximum value of an array
int max_ind(int array[], int size) {
	int max = array[0];
	int ind = 0;
	int j;
	for (j = 1; j < size; ++j) {
		if (array[j] > max) {
			max = array[j];
			ind = j;
		}
	}
	return ind;
}

int avg(int array[], int size) {
	int sum = 0;
	int j;
	for (j = 0; j < size; ++j) {
		sum += array[j];
	}
	int avgval = sum / size;
	return avgval;
}

int calcDiff(float freq, float tru) {
	float notehi = tru * 1.059463094;
	float notelo = tru / 1.059463094;
	float frac;
	if (freq > tru) {
		frac = (freq - tru) / (notehi - tru);
	} else {
		frac = (freq - tru) / (tru - notelo);
	}
	float LEDind = 5 + (frac / FREQTOL);
	if (LEDind < 1) {LEDind = 1;}
	if (LEDind > 9) {LEDind = 9;}
	int LE = LEDind + 0.5;
	// if(usbDebug) {
	// 	m_usb_tx_string(" Compto = ");
	// 	m_usb_tx_int(tru * 10);
	// 	m_usb_tx_string(" LED = ");
	// 	m_usb_tx_int(LE);
	// }

	return LE;
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