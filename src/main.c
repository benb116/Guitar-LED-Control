#include "m_general.h"
#include "m_bus.h"
#include "m_rf.h"
#include "m_usb.h"

void initTimer1();
void initADC();
void initPins();

float calcValAvg();
float calcIAvg(int );
void filterCalc(float avg);
int calcDiff(float freq, float tru);
void tuneLED();
void prettyLED();
void prettyDrip();

void lightLEDs();

#define RES 600 // freq of measurements (Hz)
#define AMPTHRESH 2.5
#define AVGTOL 10
#define FRACTOL 0.05
#define PRETTYTHRESH 100
#define PRETTYMAX 500

volatile int ledstatus[9] = {0};

enum Mode {
	TUNE, METRO, SHOW
};

int currentMode = TUNE;

int i, j, k;
int val[RES] = {0};
int filterval[RES] = {0};
int peaks[8] = {0};
int peakdif[7] = {0};

int prettyMode = 0;

int main(void)
{
	m_clockdivide(4); // System clock 1 MHz
	m_red(ON);
	m_bus_init();
	m_usb_init();
	while(!m_usb_isconnected()); // wait for a connection
	initTimer1();
	initADC();
	initPins();
	sei(); // Enable interrupts
	while (1) {
		// switch(currentMode) {
		// 	case TUNE:
		// tuneLED();
		// 		break;
		// 	case METRO:
		// 		// BPMLED(currentBPM);
		// 		break;
		// 	case SHOW:
		// 		prettyLED();
		// 		break;
		// }
		// lightLEDs();
		if (ledstatus[1]) {m_red(ON);} else {m_red(OFF);}
	}
	return 0;
}

ISR(TIMER1_OVF_vect) { // When OCR1A is reached
	if (i >= RES) {
		i = 0;
	}
	val[i] = ADC;
	if (ADC > 100) {
		m_green(ON);
		ledstatus[1] = 1;
		// set(PORTB, 3);
	} else {
		m_green(OFF);
		ledstatus[1] = 0;
		// clear(PORTB, 3);
	}
	// if (prettyMode) {
	// 	if (i % (RES/5) == 0) {
	// 		prettyDrip();
	// 	}
	// }
	lightLEDs();	aq

	i++;
}

void initTimer1() {
	set(TCCR1B, WGM13); // Use single slope up to OCR1A (mode 15)
	set(TCCR1B, WGM12);
	set(TCCR1A, WGM11); 
	set(TCCR1A, WGM10);

	set(TCCR1B, CS11); // Clock scaling /8

	set(TIMSK1, TOIE1); // Interrupt when overflow

	// 1 MHz clock / timer presale / sample rate
	int count = 125000 / RES;
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
	set(DDRD, 0);
	set(DDRD, 1);
	set(DDRD, 2);
	set(DDRD, 3);
	set(DDRF, 0);
	set(DDRF, 1);
	clear(DDRB, 4);

	clear(PORTB, 2);
	clear(PORTB, 3);
	clear(PORTB, 7);
	clear(PORTD, 0);
	clear(PORTD, 1);
	clear(PORTD, 2);
	clear(PORTD, 3);
	clear(PORTF, 0);
	clear(PORTF, 1);
	clear(PORTB, 4);
}

void tuneLED() {
	prettyMode = 0;
	int LED = 0;

	filterCalc(calcValAvg()); // Remove ADC values that are not above the threshold
	int peakcount = 0;
	for (j = i; j < RES; ++j) {
		// Count how many separate peaks there are
		// i.e. where one value is 0 and the next is non-zero
		// Limit to 8 peaks
		if ((filterval[j] == 0) && (filterval[j+1] > 0 && peakcount < 8)) {
			peaks[peakcount] = j;
			peakcount++;
		}
	}
	for (j = 0; j < (i-1); ++j) {
		// Count how many separate peaks there are
		// i.e. where one value is 0 and the next is non-zero
		if ((filterval[j] == 0) && (filterval[j+1] > 0 && peakcount < 8)) {
			peaks[peakcount] = j + RES;
			peakcount++;
		}
	}
	for (k = 0; k < (peakcount-1); ++k) {
		peakdif[k] = peaks[k+1]-peaks[k];
	}
	float avgdiff = calcIAvg(peakcount);
	float freq = (float) RES / avgdiff;
	m_usb_tx_int((int) (freq * 10));
	m_usb_tx_string("\n");

	if 		(freq < 70) 	{LED = 0;} 
	else if (freq < 96.2) 	{LED = calcDiff(freq, 82.4);} 
	else if (freq < 128.4) 	{LED = calcDiff(freq, 110.0);} 
	else if (freq < 171.4) 	{LED = calcDiff(freq, 146.8);} 
	else if (freq < 221.4) 	{LED = calcDiff(freq, 196.0);} 
	else if (freq < 288.3) 	{LED = calcDiff(freq, 246.9);} 
	else if (freq < 350) 	{LED = calcDiff(freq, 329.6);} 
	else 					{LED = 0;}

	for (j = 0; j < 9; ++j) {
		ledstatus[j] = (j == LED - 1);
	}
}

float calcValAvg() {
	int sum = 0;
	for (j=0; j<RES; j++) {
		sum = sum + val[j];
	}
	return (float)sum / RES;
}

float calcIAvg(int peakcount) {
	int sum = 0;
	for (j=0; j<(peakcount-1); j++) {
		sum = sum + peakdif[j];
	}
	return (float)sum / (peakcount-1);
}

void filterCalc(float avg) {
	for (j = 0; j < RES; ++j) {
		filterval[j] = val[j] * (val[j] > avg * AMPTHRESH);
	}
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
	float LEDind = 5 + (frac / FRACTOL);
	if (LEDind < 1) {LEDind = 1;}
	if (LEDind > 9) {LEDind = 9;}
	int LE = LEDind + 0.5;
	return LE;
}

void prettyLED() {
	float sigAvg = calcValAvg();
	if (sigAvg < PRETTYTHRESH) {
		prettyDrip();
		prettyMode = 1;
	} else {
		float frac = sigAvg / PRETTYMAX;
		OCR1B = OCR1A * frac;
		prettyMode = 2;
	}
}
int dripIndex = 0;
void prettyDrip() {
	for (j = 0; j < 9; ++j) {
		ledstatus[j] = (j - dripIndex < 2);
	}
}

void lightLEDs() {
	if (ledstatus[0]) {set(PORTB, 2);} else {clear(PORTB, 2);}
	if (ledstatus[1]) {set(PORTB, 3);} else {clear(PORTB, 3);}
	if (ledstatus[2]) {set(PORTB, 7);} else {clear(PORTB, 7);}
	if (ledstatus[3]) {set(PORTD, 0);} else {clear(PORTD, 0);}
	if (ledstatus[4]) {set(PORTD, 1);} else {clear(PORTD, 1);}
	if (ledstatus[5]) {set(PORTD, 2);} else {clear(PORTD, 2);}
	if (ledstatus[6]) {set(PORTD, 3);} else {clear(PORTD, 3);}
	if (ledstatus[7]) {set(PORTF, 0);} else {clear(PORTF, 0);}
	if (ledstatus[8]) {set(PORTF, 1);} else {clear(PORTF, 1);}
}