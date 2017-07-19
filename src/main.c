#include "m_general.h"
#include "m_bus.h"
#include "m_usb.h"

#define RES 70000 // Freq of measurements in Hz
#define NUMPERS 6 // How many recordings to hold on to
#define FREQTOL 0.5 // How close to the true frequency is good.
#define	LEDNUM 20
#define TWELTHROOTTWO 1.059463094
#define CORRECTGAIN 0.98
#define CORRECTBUMP 0.5885

#define DEBUGLEVEL 1

static float freqs[6] = {82.4, 110.0, 146.8, 196.0, 246.9, 329.6}; // Hz
static int freqmin = 70; // Minimum frequency that will be conisdered
static int freqmax = 350; // Maximum frequency that will be conisdered
static float perticks[6] = {0}; // To be calculated
int j;

volatile int ledstatus[9] = {0}; // LED is on/off -> 1/0
int pers[NUMPERS] = {0}; // Most recent period data
int persShift[NUMPERS] = {0}; // Shifted to be newest to oldest
int LEDHist[LEDNUM] = {0};

int enable = 1;
int h = 0; // Keep track of progress through one second
int g = 0; // Buffer of LED history

int tick = 0; // Number of ticks at RES frequency for a period
bool ishi = 0; // Used to determine whether a new period starts
int perInd = 0; // Which index in the array should we use

int dripIndex = 0; // progress within the drip sequence
int startupState = 0;
int blinkcount = 0;

int mode = 0;

void initTimer1();
void initPins();

void tuneCheck();
void recordPer(int tick);
float calcFreq();
int closeto(int in, int comp, int thresh);
int FindSumtick(int a, int b, int c, int d, int e, int thresh);
int calcDiff(float freq, float tru);
void tuneLED();

int min_ind(int array[], int size);
int max_ind(int array[], int size);
int avg(int array[], int size);

void allLED(int onoff);

void dripIncrement();
void dripLED();
void startup();

void lightLEDs();

int main(void)
{
	m_disableJTAG();
	m_clockdivide(0); // System clock 16 MHz
	m_bus_init();
	m_usb_init();
	// while(!m_usb_isconnected()) {}

	initTimer1();
	initPins();

	sei(); // Enable interrupts

	for (j = 0; j < 6; ++j) {
		perticks[j] = RES / freqs[j]; // Calculate the period lengths to match each string
	}

	m_red(ON);
	while(1) {
		if (DEBUGLEVEL) {
			m_usb_tx_string("\n");
			m_usb_tx_int(mode);
			m_usb_tx_string(" ");
		}
		switch(mode) {
			case 0:
				startup(); break;
			case 1:
				tuneLED(); break;
			case 2:
				dripLED(); break;
			case 3:
				allLED(1); break;
			default:
				allLED(0); break;
		}
		lightLEDs();
	}
	return 0;
}

void initTimer1() {
	set(TCCR1B, WGM13); // Use single slope up to OCR1A (mode 15)
	set(TCCR1B, WGM12);
	set(TCCR1A, WGM11); 
	set(TCCR1A, WGM10);

	set(TCCR1B, CS11); // Clock scaling /8

	set(TIMSK1, TOIE1); // Interrupt when overflow

	// 1 MHz clock / timer presale / sample rate
	int count = (16000000 / 8 / RES);
	OCR1A = count;
}

void initPins() {
	set(DDRB, 2); // Output pins
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

ISR(TIMER1_OVF_vect) { // When OCR1A is reached (4 kHz)
	switch(mode) {
		case 0:
			dripIncrement(); break;
		case 1:
			tuneCheck(); break;
		case 2:
			dripIncrement(); break;
	}
}

void dripIncrement() {
	if (h >= RES) {
		h = 0;
	}
	if (h % (RES / 20) == 0) { // Increment drip state @ 20 Hz
		++dripIndex;
	}
	if (dripIndex >= 20) {
		dripIndex = 0;
	}
	++h;
}

void tuneCheck() {
	if (check(PINB, 4)) { // If signal is high
		if (ishi) { // If it was already high
			++tick; // increment
		} else {
			recordPer(tick); // It was low, is now hi, start a new period
			tick = 1;
			ishi = true;
		}
	} else { // Low
		ishi = false;
		++tick; // Part of the same period
	}
}

void recordPer(int ticks) {
	++perInd;
	if (perInd >= NUMPERS) {perInd = 0;} // Cycle the index of new measurements
	pers[perInd] = ticks;
}

void startup() {
	if (!startupState) {
		for (j = 0; j < 9; ++j) {
			ledstatus[j] = (j <= dripIndex);
		}
		if (dripIndex >= 10) {startupState = 1;}
	} else {
		if (blinkcount > 7) {
			mode = 1;
		} else {
			allLED(dripIndex % 10 >= 5);
			if (dripIndex >= 19) {++blinkcount;}
		}
	}
}

void tuneLED() {
	m_green(ON);
	float freq = calcFreq();
	int LED = 0;
	// Which note is likely being played
	if 		(freq < freqmin){LED = 0;} 
	else if (freq < 96.2) 	{LED = calcDiff(freq, freqs[0]);} // 242.7
	else if (freq < 128.4) 	{LED = calcDiff(freq, freqs[1]);} // 181.8
	else if (freq < 171.4) 	{LED = calcDiff(freq, freqs[2]);} // 136.2
	else if (freq < 221.4) 	{LED = calcDiff(freq, freqs[3]);} // 102.0
	else if (freq < 288.3) 	{LED = calcDiff(freq, freqs[4]);} // 81.00
	else if (freq < freqmax){LED = calcDiff(freq, freqs[5]);} // 60.68
	else 					{LED = 0;}

	LEDHist[g] = LED;
	++g;
	if (g >= LEDNUM) {
		g = 0;
	}
	int smoothLED = avg(LEDHist, LEDNUM);
	// Light up the correct LED
	for (j = 0; j < 9; ++j) {
		ledstatus[j] = (j == (smoothLED - 1));
	}
}

float calcFreq() {
	int k = 0;
	// Shift the period measurements to start with the most recent one
	for (j = perInd; j >= 0; --j) {
		persShift[k] = pers[j];
		++k;
	}
	for (j = (NUMPERS-1); j > perInd; --j) {
		persShift[k] = pers[j];
		++k;
	}
	int Pa = persShift[0];
	int Pb = persShift[1];
	int Pc = persShift[2];
	int Pd = persShift[3];
	int Pe = persShift[4];

	// Find a repeating pattern and determine the period of repetition
	int sumtick = 1;
	sumtick = FindSumtick(Pa, Pb, Pc, Pd, Pe, 5);
	if (sumtick == 1) {
		sumtick = FindSumtick(Pa, Pb, Pc, Pd, Pe, 15);
	}

	// Convert to a frequency
	float prefreq = RES / (float) sumtick;
	float freq = prefreq * CORRECTGAIN + CORRECTBUMP;
	
	if (DEBUGLEVEL == 2) {
		m_usb_tx_int(Pa);
		m_usb_tx_string(" ");
		m_usb_tx_int(Pb);
		m_usb_tx_string(" ");
		m_usb_tx_int(Pc);
		m_usb_tx_string(" ");
		m_usb_tx_int(Pd);
		m_usb_tx_string(" ");
		m_usb_tx_int(Pe);
		m_usb_tx_string(" ");
	}

	if (DEBUGLEVEL == 1) {
		m_usb_tx_int(sumtick);
		m_usb_tx_string(" ");

		// m_usb_tx_int((int)(prefreq*10));	
		// m_usb_tx_string(" ");

		m_usb_tx_int((int)(freq));	
		m_usb_tx_string(".");
		m_usb_tx_int(((int)(freq*10) % 10));
		m_usb_tx_string(" ");
	}

	return freq;
}

int FindSumtick(int a, int b, int c, int d, int e, int thresh) {
	int sumtick = 1;
	if (closeto(a, b, thresh) && (RES / a > freqmin) && (RES / a < freqmax)) {
			sumtick = a;
	} else if (closeto(a, c, thresh) && (RES / (a+b) > freqmin) && (RES / (a+b) < freqmax)) {
		sumtick = a + b;
	} else if (closeto(a, d, thresh) && (RES / (a+b+c) > freqmin) && (RES / (a+b+c) < freqmax)) {
		sumtick = a + b + c;
	} else if (closeto(a, e, thresh) && (RES / (a+b+c+d) > freqmin) && (RES / (a+b+c+d) < freqmax)) {
		sumtick = a + b + c + d;
	}
	return sumtick;
}

int closeto(int in, int comp, int thresh) { // Is one number close to another (within thresh)
	int diff = in - comp;
	if (diff < 0) {diff = diff * -1;}
	return (diff <= thresh);
}

// How close to a given note are we
int calcDiff(float freq, float tru) {
	float notehi = tru * TWELTHROOTTWO; // half step up
	float notelo = tru / TWELTHROOTTWO; // half step down
	float frac;
	if (freq > tru) {
		frac = (freq - tru) / (notehi - tru); // Fraction of the way between one note and another
	} else {
		frac = (freq - tru) / (tru - notelo);
	}
	float LEDind = 5 + (frac / FREQTOL);
	if (LEDind < 1) {LEDind = 1;}
	if (LEDind > 9) {LEDind = 9;}

	int LE = LEDind + 0.5; // Round
	if (DEBUGLEVEL) {
		m_usb_tx_int(LE);
		m_usb_tx_string(" ");
	}
	return LE;
}

void dripLED() {
	for (j = 0; j < 9; ++j) {
		if (dripIndex < 10) {	// First half, incrementally turn LED's on
			ledstatus[j] = (j <= dripIndex);
		} else { // Then incrementally turn them off
			ledstatus[j] = (j+10 >= dripIndex);
		}
	}
}

void allLED(int onoff) {
	for (j = 0; j < 9; ++j) {
		ledstatus[j] = onoff;
	}
}

void lightLEDs() {
	if (ledstatus[0] && enable) {set(PORTB, 2);} else {clear(PORTB, 2);}
	if (ledstatus[1] && enable) {set(PORTB, 3);} else {clear(PORTB, 3);}
	if (ledstatus[2] && enable) {set(PORTB, 7);} else {clear(PORTB, 7);}
	if (ledstatus[3] && enable) {set(PORTD, 3);} else {clear(PORTD, 3);}
	if (ledstatus[4] && enable) {set(PORTD, 4);} else {clear(PORTD, 4);}
	if (ledstatus[5] && enable) {set(PORTD, 5);} else {clear(PORTD, 5);}
	if (ledstatus[6] && enable) {set(PORTD, 6);} else {clear(PORTD, 6);}
	if (ledstatus[7] && enable) {set(PORTF, 0);} else {clear(PORTF, 0);}
	if (ledstatus[8] && enable) {set(PORTF, 1);} else {clear(PORTF, 1);}
}

// Find the index of the minimum value of an array
int avg(int array[], int size) {
	int sum = 0;
	int j;
	for (j = 1; j < size; ++j) {
		sum += array[j];
	}
	float aver = (float) sum / size;
	int avrint = aver + 0.5;
	return avrint;
}