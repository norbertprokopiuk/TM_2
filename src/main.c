/**
 *************************************************************************
 ******************************************************************************
 * @file    main.c
 * @author  Ac6
 * @version V1.0
 * @date    01-December-2013
 * @brief   Default main function.
 ******************************************************************************
 */

/**
 ******************************************************************************
 * @file    main.c
 * @author  Ac6
 * @version V1.0
 * @date    01-December-2013
 * @brief   Default main function.
 ******************************************************************************
 */

#include <string.h>
#include "stm32l0xx.h"
#include "stm32l0xx_nucleo.h"

#define TEMP130_CAL_ADDR ((int32_t) *((uint16_t *) ((uint32_t) 0x1FF8007E)))
#define TEMP30_CAL_ADDR  ((int32_t) *((uint16_t *) ((uint32_t) 0x1FF8007A)))
#define BUFF_SIZE 32

int8_t command_T = 'T'; // wartosci litery T w kodzie ASCII
int8_t command_N = 'N'; // analogicznie jak wyzej
int8_t command_space = ' ';
int8_t command_zero = '0';
uint8_t temp = 0;
uint8_t which_command = 0; // flaga zawierajaca informacje o tym jaki jest poczatek poprawnie przeslanej informacji 0 - zla informacja 1- N_ 2-T_

uint8_t T_values[3] = { 0, 0, 0 }; //tablica zawierajaca cyfry przeslanej wiadomosci
uint8_t N_values[2] = { 0, 0 };
uint8_t N_flag[2] = { 0, 0 }; //0-start, 1- cyfra 2-blad
uint8_t T_flag[3] = { 0, 0, 0 };
// RX
volatile int8_t g_rxData[5] = { 0, 0, 0, 0, 0 }; //bufor
volatile int8_t g_rxFlag = 0;
// UART TX
volatile int8_t g_txBuf[] = "00.00 degC\r\n";
volatile int32_t g_txSize = 0;
volatile int32_t g_txTransmitted = 0;

int32_t temperature = 0;

static int8_t N = 1;  // <1,32>s
static int8_t T = 10; // <0,100>
static int16_t t = 0; // czas
static uint8_t t2 = 0; // czas do sprawdzania czy wiadomosc sie wyslala

uint8_t recived = 0;
uint8_t msg_flag = 0; // flaga konca wiadomosci
uint8_t T_zero_flag = 0;
uint8_t time_check = 10; // * 1 ms

void USART2_IRQHandler(void) {
	if (0 != (USART_ISR_RXNE & USART2->ISR)) {

		for (int i = sizeof(g_rxData) / sizeof(g_rxData[0]) - 1; i > 0; i--) {
			g_rxData[i] = g_rxData[i - 1];
		}
		g_rxData[0] = USART2->RDR;
		g_rxFlag = 1;
		recived = 1;

	}
	if ((0 != (USART_CR1_TXEIE & USART2->CR1))
			&& (0 != (USART_ISR_TXE & USART2->ISR))) {
		if ((0 != g_txSize) && (g_txTransmitted < g_txSize)) {
			USART2->TDR = (uint32_t) g_txBuf[g_txTransmitted];

			g_txTransmitted += 1;
		}

		if (g_txTransmitted == g_txSize) {
			USART2->CR1 &= ~USART_CR1_TXEIE;
		}
	}

}
// ADC
volatile int32_t g_data[32];
volatile int8_t g_dataCount = 0;
volatile int8_t g_dataFlag = 0;
//bufor cykliczny
uint8_t head = 0;
uint8_t tail = 0;

uint32_t odczyt() {
	int32_t data = 0;
	uint8_t head_temp = head;
	for (uint8_t i = 0; i < N; i++) {
		data += g_data[(head_temp + BUFF_SIZE - i) % BUFF_SIZE];
	}
	return data;
}
void zapis(uint16_t input) {
	uint8_t head_temp = (head + 1) % BUFF_SIZE;
	g_data[head_temp] = input;
	head = head_temp;
}

void ADC1_COMP_IRQHandler(void) {

	if (0 != (ADC_ISR_EOC & ADC1->ISR)) {
		zapis(ADC1->DR * 100); // odczyt jest przeskalowany razy 100 aby móc odczytac czesci dzesietne

		if ((sizeof(g_data) / sizeof(g_data[0]) - 1) != g_dataCount) {
			g_dataCount += 1;
		} else {
			g_dataFlag = 1;
		}
	}

}
// przerwanie timer'a - liczenie czasu
void TIM2_IRQHandler(void) // periodic 1ms
{
	if (0 != (TIM_SR_UIF & TIM2->SR)) {
		TIM2->SR &= ~TIM_SR_UIF;
		if (T_zero_flag == 0) {
			t++;
		}
		if (recived == 1) {
			t2++;
		}
	}
}
//funkcje do wydobycia liczb z otrzymanych wiadomosci
void T_number() {
	if (g_rxData[0] - command_zero >= 0 && g_rxData[0] - command_zero <= 9) {

		T_values[0] = g_rxData[0] - command_zero;
		T_flag[0] = 1;
		if (g_rxData[1] - command_zero >= 0
				&& g_rxData[1] - command_zero <= 9) {
			T_values[1] = g_rxData[1] - command_zero;
			T_flag[1] = 1;

			if (g_rxData[2] - command_zero >= 0
					&& g_rxData[2] - command_zero <= 9) {

				T_values[2] = g_rxData[2] - command_zero;
				T_flag[2] = 1;
			}

			else if (g_rxData[2] == command_space)

			{
				T_flag[2] = 0;
			}

			else

			{
				T_flag[2] = 2;
			}
		}

		else if (g_rxData[1] == command_space && g_rxData[2] == command_T)

		{
			T_flag[1] = 0;
		}

		else

		{
			T_flag[1] = 2;
		}

	}

	else

	{
		T_flag[0] = 2;
	}

	if (1 == T_flag[2] && 1 == T_flag[1] && 1 == T_flag[0]) {
		temp = 100 * T_values[2] + 10 * T_values[1] + T_values[0];

		if (temp == 100) {
			T = temp;
		}
	}

	else if (0 == T_flag[2] && 1 == T_flag[1] && 1 == T_flag[0]) {
		T = 10 * T_values[1] + T_values[0];
	} else if (0 == T_flag[2] && 0 == T_flag[1] && 1 == T_flag[0]) {
		T = T_values[0];
	}
	if (T != 0) {
		T_zero_flag = 0;
	} else {
		T_zero_flag = 1;
	}

	T_flag[0] = 0;
	T_flag[1] = 0;
	T_flag[2] = 0;

}

void N_number() {
	if (g_rxData[0] - command_zero >= 0 && g_rxData[0] - command_zero <= 9) {

		N_values[0] = g_rxData[0] - command_zero;
		N_flag[0] = 1;
		if (g_rxData[1] - command_zero >= 0
				&& g_rxData[1] - command_zero <= 9) {
			N_values[1] = g_rxData[1] - command_zero;
			N_flag[1] = 1;
		} else if (g_rxData[1] == command_space) {
			N_flag[1] = 0;
		} else {
			N_flag[1] = 2;
		}

	} else {
		N_flag[0] = 2;
	}

	if (1 == N_flag[1] && 1 == N_flag[0]) {
		temp = 10 * N_values[1] + N_values[0];
		if (temp <= 32) {
			N = temp;
		}
	} else if (1 == N_flag[0] && 0 == N_flag[1]) {
		temp = N_values[0];

		if (temp > 0) {
			N = temp;
		}
	}

	N_flag[0] = 0;
	N_flag[1] = 0;

}

int main(void) {
	// ------------- RCC -------------
	RCC->CR |= RCC_CR_HSION; // enable HSI
	while (0 == (RCC->CR & RCC_CR_HSIRDY)) { // wait until RDY bit is set
		// empty
	}
	RCC->CFGR |= RCC_CFGR_SW_0; // set SYSCLK to HSI16
	RCC->IOPENR |= RCC_IOPENR_GPIOAEN; // enable GPIOA clock
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN | RCC_APB1ENR_TIM6EN; // enable USART2 and TIM6 clocks
	RCC->APB2ENR |= RCC_APB2ENR_ADCEN; // enable ADC clock

	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	// ------------- GPIOA (pin 2 - TX & 3 - RX) -------------
	GPIOA->AFR[0] &= ~0x0000FF00;
	GPIOA->AFR[0] |= 0x00004400;

	GPIOA->MODER &= ~(GPIO_MODER_MODE3_1 | GPIO_MODER_MODE3_0
			| GPIO_MODER_MODE2_1 | GPIO_MODER_MODE2_0);
	GPIOA->MODER |= (GPIO_MODER_MODE3_1 | GPIO_MODER_MODE2_1);

	GPIOA->OSPEEDR |= (GPIO_OSPEEDER_OSPEED3_1 | GPIO_OSPEEDER_OSPEED3_0
			| GPIO_OSPEEDER_OSPEED2_1 | GPIO_OSPEEDER_OSPEED2_0);

	// ------------- USART2 -------------
	USART2->BRR = 139 - 1; // 16 000 000 / 115 200 = 138.88 //predkosc transmisji
	USART2->CR3 = USART_CR3_OVRDIS;  //bit wykrycia nadpisywania danych
	USART2->CR1 = USART_CR1_RXNEIE | USART_CR1_RE | USART_CR1_TE | USART_CR1_UE; // Aktywacja Uartu i transmitera
	USART2->ISR = 0xFFFFFFFF; // Rejestr błędów -> Wykrywamy wszystkie możliwe

	//--------------TIMER2----------------//
	TIM2->CR1 |= TIM_CR1_ARPE; //Auto-reload preload register
	TIM2->CR1 &= ~TIM_CR1_DIR; //Zliczanie w góre
	TIM2->ARR = 160 - 1; // 1000Hz = 1ms
	TIM2->PSC = 100 - 1;
	TIM2->CR1 |= TIM_CR1_CEN; //aktywacja licznika
	TIM2->DIER |= TIM_DIER_UIE;
	NVIC_EnableIRQ(TIM2_IRQn);

	// ------------- ADC1 -------------
	ADC1->IER = ADC_IER_EOCIE; // end of conversion interrupt enable
	ADC1->CR = ADC_CR_ADVREGEN; // enable voltage regulator
	ADC1->CFGR1 = ADC_CFGR1_EXTEN_0; // enable hardware trigger, select rising edge, TIM6_TRGO selected by default, right alignment by default, 12bits by default
	ADC1->CFGR2 = ADC_CFGR2_CKMODE_1 | ADC_CFGR2_CKMODE_0; // synchronous clock (PCLK) divided by 1
	ADC1->SMPR = ADC_SMPR_SMPR_0 | ADC_SMPR_SMPR_1 | ADC_SMPR_SMPR_2; // highest sampling time (160.5 clock cycles)
	ADC1->CHSELR = ADC_CHANNEL_18; // temperature sensor channel
	ADC->CCR = ADC_CCR_TSEN; // temperature sensor enable

	ADC1->CR |= ADC_CR_ADCAL; // start calibration
	while (0 != (ADC_CR_ADCAL & ADC1->CR)) { // wait until calibration is completed
		// empty
	}

	ADC1->ISR = ADC_ISR_ADRDY; // clear ADREADY bit
	ADC1->CR |= ADC_CR_ADEN; // enable ADC
	while (0 == (ADC_ISR_ADRDY & ADC1->ISR)) { // wait until ADC is ready
		// empty
	}
	ADC1->CR |= ADC_CR_ADSTART; // start operation (wait for trigger)

	// ------------- TIM6 -------------
	TIM6->CR2 = TIM_CR2_MMS_1; // set update event as TRGO signal source
	TIM6->PSC = 1600 - 1;
	TIM6->ARR = 1000 - 1; // set period to 100ms

	// ------------- NVIC -------------
	NVIC_EnableIRQ(USART2_IRQn); // nvic interrupt enable (USART2 interrupt)
	NVIC_EnableIRQ(ADC1_COMP_IRQn); // nvic interrupt enable (ADC1 interrupt)
	// TIM6 enable
	TIM6->CR1 |= TIM_CR1_CEN; // count enable
	NVIC_EnableIRQ(TIM21_IRQn);

	while (1) {
		// Filter

		int32_t data = odczyt() / N;

		// Calculate temperature
		temperature = ((((data * 330 / 300) - TEMP30_CAL_ADDR * 100)
				* (int32_t) (130 - 30) / (TEMP130_CAL_ADDR - TEMP30_CAL_ADDR))
				+ 30 * 100);

		if (((0 != g_rxFlag) && (g_txSize == g_txTransmitted))) {

			__disable_irq();

			if (t2 >= time_check) {
				g_rxFlag = 0;
				t2 = 0;
				recived = 0;
				if ((command_N == g_rxData[2])
						&& (command_space == g_rxData[1])) {
					which_command = 1;

				}
				if ((command_N == g_rxData[3])
						&& (command_space == g_rxData[2])) {
					which_command = 1;

				}

				if ((command_T == g_rxData[2])
						&& (command_space == g_rxData[1])) {
					which_command = 2;

				}
				if ((command_T == g_rxData[3])
						&& (command_space == g_rxData[2])) {
					which_command = 2;

				}
				if ((command_T == g_rxData[4])
						&& (command_space == g_rxData[3])) {
					which_command = 2;

				}

				if (which_command == 1) {
					N_number();

				}

				if (which_command == 2) {
					T_number();

				}

				which_command = 0;
				for (int i = sizeof(g_rxData) / sizeof(g_rxData[0]) - 1; i >= 0;
						i--) {
					g_rxData[i] = 0;
				}
			}

			__enable_irq();

		}

		if ((0 != g_dataFlag) && (g_txSize == g_txTransmitted)) {
			g_dataFlag = 0;

			/*
			 * timer inkrementuje zmienna t co 1ms natomiast wedlug zalozen zadania wiadmosc ma byc wysylana co T*100ms
			 * nalezy zatem przemnozyc T przez 100
			 */
			if (t >= 100 * T) {
				if (T_zero_flag == 0) {
					t = 0;
				} else {
					t = -1;
				}
				// Transmit data
				g_txBuf[0] = '0' + (temperature % 10000) / 1000;
				g_txBuf[1] = '0' + (temperature % 1000) / 100;
				g_txBuf[2] = '.';
				g_txBuf[3] = '0' + (temperature % 100) / 10;
				g_txBuf[4] = '0' + (temperature % 10);
				g_txSize = strlen((const char *) g_txBuf);
				g_txTransmitted = 0;
				USART2->CR1 |= USART_CR1_TXEIE;

			}

		}
	}
}

