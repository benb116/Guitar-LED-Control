#include "m_general.h"
#include "m_bus.h"
#include "m_rf.h"
#include "m_usb.h"

void initTimer1();
void initADC();
void initPins();
float calcValAvg();
float calcIAvg();
void filterCalc(float avg);
int calcDiff(float freq, float tru);
void lightLED(int LEDindex);

#define RES 3000 // freq of measurements (Hz)
#define AMPTHRESH 2.5
#define AVGTOL 10
#define FRACTOL 0.05

int val[RES] = {0};
int filterval[RES] = {0};

int peaks[8] = {0};
int peakdif[7] = {0};

int i;
int LED;

int main(void)
{
	m_red(ON);
	m_clockdivide(4); // System clock 1 MHz
	m_bus_init();
	m_usb_init();
	while(!m_usb_isconnected()); // wait for a connection

	sei(); // Enable interrupts
	m_green(ON);
	while (1) {
		filterCalc(calcValAvg()); // Remove ADC values that are not above the threshold
		int peakcount = 0;
		for (int j = 0; j < RES-1; ++j) {
			// Count how many separate peaks there are
			// i.e. where one value is 0 and the next is non-zero
			if ((filterval[j] == 0) && (filterval[j+1] > 0)) {
				peaks[peakcount] = j;
				peakcount++;
				if (peakcount == 8) { // Limit the number of peaks to 8
					i = RES - 1;
				}
			}
		}
		for (int k = 0; k < (peakcount-1); ++k) {
			peakdif[k] = peaks[k+1]-peaks[k];
		}
		float avgdiff = calcIAvg(peakdif);
		float freq = (float) RES / avgdiff;
		m_usb_tx_int((int) (freq * 10));

		if (freq < 70) {LED = 0;} 
		else if (freq < 96.2) {LED = calcDiff(freq, 82.4);} 
		else if (freq < 128.4) {LED = calcDiff(freq, 110.0);} 
		else if (freq < 171.4) {LED = calcDiff(freq, 146.8);} 
		else if (freq < 221.4) {LED = calcDiff(freq, 196.0);} 
		else if (freq < 288.3) {LED = calcDiff(freq, 246.9);} 
		else if (freq < 350) {LED = calcDiff(freq, 329.6);} 
		else {LED = 0;}
		lightLED(LED);
	}
	return 0;
}

ISR(TIMER1_OVF_vect) { // When OCR1A is reached
	if (i >= RES) {
		i = 0;
	}
	val[i] = ADC;
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

	set(DIDR0, ADC0D); // Set as analog input F0

	set(ADCSRA, ADIE); // Enable interrupting

	set(ADCSRA, ADATE); // Set triggering

	clear(ADCSRB, MUX5); // Look at F0
	clear(ADMUX, MUX2);
	clear(ADMUX, MUX1);
	clear(ADMUX, MUX0);

	set(ADCSRA, ADEN); // Conversion
	set(ADCSRA, ADSC);

	set(ADCSRA, ADIF); // Reading result
}

void initPins() {
	DDRB |= (1 << 2) | (1 << 3) | (1 << 7);
	DDRD |= (1 << 0) | (1 << 1) | (1 << 2);
	DDRF |= (1 << 1) | (1 << 4) | (1 << 5);
				//^ Pin num to set
}

float calcValAvg() {
	int sum = 0;
	for (i=0; i<RES; i++) {
		sum = sum + val[i];
	}
	return (float)sum / RES;
}

float calcIAvg() {
	int sum = 0;
	for (i=0; i<8-1; i++) {
		sum = sum + val[i];
	}
	return (float)sum / (8-1);
}

void filterCalc(float avg) {
	for (int i = 0; i < RES; ++i) {
		filterval[i] = val[i] * (val[i] > avg * AMPTHRESH);
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

void lightLED(int LEDindex) {
	if (LEDindex == 1) {set(PORTB, 2);} else {clear(PORTB, 2);}
	if (LEDindex == 2) {set(PORTB, 3);} else {clear(PORTB, 3);}
	if (LEDindex == 3) {set(PORTB, 7);} else {clear(PORTB, 7);}
	if (LEDindex == 4) {set(PORTD, 0);} else {clear(PORTD, 0);}
	if (LEDindex == 5) {set(PORTD, 1);} else {clear(PORTD, 1);}
	if (LEDindex == 6) {set(PORTD, 2);} else {clear(PORTD, 2);}
	if (LEDindex == 7) {set(PORTF, 1);} else {clear(PORTF, 1);}
	if (LEDindex == 8) {set(PORTF, 4);} else {clear(PORTF, 4);}
	if (LEDindex == 9) {set(PORTF, 5);} else {clear(PORTF, 5);}
}