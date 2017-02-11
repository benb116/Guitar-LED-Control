#include "m_general.h"
#include "m_bus.h"
#include "m_rf.h"
#include "m_usb.h"

#define RES 20000
#define NUMPERS 20
#define PERDIFF 1
#define FREQTOL 0.05

volatile int ledstatus[9] = {0};
int pers[NUMPERS] = {0};
int persShift[NUMPERS];
int pers2nd[NUMPERS/2];
int pers3rd[NUMPERS/3];

int harmonics[4] = {0};

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
void calcFreq();
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
	m_clockdivide(0); // System clock 1 MHz
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
		// m_usb_tx_int(pers2nd[0]);
		// m_usb_tx_string(" ");
		// m_usb_tx_int(pers2nd[1]);
		// m_usb_tx_string(" ");
		// m_usb_tx_int(pers2nd[2]);
		// m_usb_tx_string(" ");
		// m_usb_tx_int(pers2nd[3]);
		// m_usb_tx_int(avg(pers2nd, (NUMPERS/2)));
		// m_usb_tx_string("\n");
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
	int count = (16000000 / 8 / RES);
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

int closeto(int in, int comp) {
	int diff = in - comp;
	if (diff < 0) {diff = diff * -1;}
	return (diff < 10);
}

void tuneLED() {
	calcFreq();
	int LED;

	/*
		If 2 or 3 ~ 240 & 4 > 300 -> E
		If 2 or 3 ~ 180 -> A
		If 1/2 ~ 135 & 2/4 ~ 270 -> D
		If 2 or 3 ~ 100 -> G
		If 1 or 2 ~ 80 -> B
		If 1 is ~ 60 -> Hi E
	*/
	// m_usb_tx_string("\n");
	// m_usb_tx_int(harmonics[1]);	
	// m_usb_tx_string(" ");
	// m_usb_tx_int(closeto(harmonics[1], 240));

	if (closeto(harmonics[1], 243) || closeto(harmonics[2], 243)) { // E
		m_usb_tx_string(" E");
		// if (closeto(harmonics[1], 240)) {
		// 	m_usb_tx_int(harmonics[1]);			
		// } else {
		// 	m_usb_tx_int(harmonics[2]);
		// }
	} else if (closeto(harmonics[1], 100) || closeto(harmonics[2], 100)) { // G
		m_usb_tx_string(" G");
		// if (closeto(harmonics[1], 100)) {
		// 	m_usb_tx_int(harmonics[1]);			
		// } else {
		// 	m_usb_tx_int(harmonics[2]);
		// }
	} else if (closeto(harmonics[1], 80) || (closeto(harmonics[0], 80) && closeto(harmonics[2], 240)) || (closeto(harmonics[0], 160) && closeto(harmonics[2], 320))) { // B
		m_usb_tx_string(" B");
		// if (closeto(harmonics[1], 80)) {
		// 	m_usb_tx_int(harmonics[1]);			
		// } else {
		// 	m_usb_tx_int(harmonics[0]);
		// }
	} else if (closeto(harmonics[0], 60) && closeto(harmonics[1], 120) && closeto(harmonics[3], 240)) { // Hi E
		m_usb_tx_string(" Eh");
		// m_usb_tx_int(harmonics[0]);
	} else if ((closeto(harmonics[0], 136) && closeto(harmonics[3], 540))) { // D
		// || (closeto(harmonics[2], 180) && harmonics[0], 50)
		m_usb_tx_string(" D");
		// if (closeto(harmonics[0], 136)) {
		// 	m_usb_tx_int(harmonics[0]);			
		// } else {
		// 	m_usb_tx_int(harmonics[1]);
		// }
	} else { // A
		m_usb_tx_string(" A");
		// if (closeto(harmonics[1], 180)) {
		// 	m_usb_tx_int(harmonics[1]);			
		// } else {
		// 	m_usb_tx_int(harmonics[2]);
		// }
	}

	// if 		(freq < 70) 	{LED = 0;} 
	// else if (freq < 96.2) 	{LED = calcDiff(freq, 82.40);} // 242.7
	// else if (freq < 128.4) 	{LED = calcDiff(freq, 110.0);} // 181.8
	// else if (freq < 171.4) 	{LED = calcDiff(freq, 146.8);} // 136.2
	// else if (freq < 221.4) 	{LED = calcDiff(freq, 196.0);} // 102.0
	// else if (freq < 288.3) 	{LED = calcDiff(freq, 246.9);} // 81.00
	// else if (freq < 350) 	{LED = calcDiff(freq, 329.6);} // 60.68
	// else 					{LED = 0;}

	// int j;
	// for (j = 0; j < 9; ++j) {
	// 	ledstatus[j] = (j == (LED - 1));
	// }
}

void calcFreq() {
	int j;
	int k = (perInd+1);
	for (j = k; j < NUMPERS; ++j) {
		persShift[(j-k)] = pers[j];
	}
	for (j = 0; j < k; ++j) {
		persShift[(j+k)] = pers[j];
	}

	for (j = 0; j < (NUMPERS/2); ++j) {
		pers2nd[j] = persShift[(2*j)] + persShift[(2*j-1)] ;
	}

	// int maxPer = pers2nd[max_ind(pers2nd, NUMPERS)];
	// int minPer = pers2nd[min_ind(pers2nd, NUMPERS)];

	// if (maxPer - minPer > PERDIFF) {
		// m_usb_tx_int((maxPer + minPer)/2);
		// m_usb_tx_string("\n");
	// }
	for (j = 0; j < (NUMPERS/3); ++j) {
		pers3rd[j] = persShift[(3*j)] + persShift[(3*j-1)] + persShift[(3*j-2)];
	}
	m_usb_tx_string("\n");
	m_usb_tx_int(persShift[0]);
	m_usb_tx_string(" ");
	m_usb_tx_int(persShift[1]+persShift[0]);
	m_usb_tx_string(" ");
	m_usb_tx_int(persShift[2]+persShift[1]+persShift[0]);
	m_usb_tx_string(" ");
	m_usb_tx_int(persShift[3]+persShift[2]+persShift[1]+persShift[0]);
	harmonics[0] = persShift[0];
	harmonics[1] = harmonics[0] + persShift[1];
	harmonics[2] = harmonics[1] + persShift[2];
	harmonics[3] = harmonics[2] + persShift[3];
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