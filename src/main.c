/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "hw.h"
#include "radio.h"
#include "timeServer.h"
#include "delay.h"
#include "low_power.h"
#include "vcom.h"

#if defined( USE_BAND_868 )

#define RF_FREQUENCY                                868000000 // Hz

#elif defined( USE_BAND_915 )

#define RF_FREQUENCY                                915000000 // Hz

#else
#error "Please define a frequency band in the compiler options."
#endif

#define TX_OUTPUT_POWER                             14        // dBm

#if defined( USE_MODEM_LORA )

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
//  1: 250 kHz,
//  2: 500 kHz,
//  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
//  2: 4/6,
//  3: 4/7,
//  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         5         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#elif defined( USE_MODEM_FSK )

#define FSK_FDEV                                    25e3      // Hz
#define FSK_DATARATE                                50e3      // bps
#define FSK_BANDWIDTH                               50e3      // Hz
#define FSK_AFC_BANDWIDTH                           83.333e3  // Hz
#define FSK_PREAMBLE_LENGTH                         5         // Same for Tx and Rx
#define FSK_FIX_LENGTH_PAYLOAD_ON                   false

#else
#error "Please define a modem in the compiler options."
#endif

typedef enum {
	LOWPOWER, RX, RX_TIMEOUT, RX_ERROR, TX, TX_TIMEOUT,
} States_t;

#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 64 // Define the payload size here
#define LED_PERIOD_MS               200

#define LEDS_OFF   do{ \
                   LED_Off( LED_BLUE ) ;   \
                   LED_Off( LED_RED ) ;    \
                   LED_Off( LED_GREEN1 ) ; \
                   LED_Off( LED_GREEN2 ) ; \
                   } while(0) ;

const uint8_t PingMsg[] = "PING";
const uint8_t PongMsg[] = "PONG";
const uint8_t Lar[] = "LARRAITZ";

uint16_t BufferSize = BUFFER_SIZE;
uint8_t Buffer[BUFFER_SIZE];

States_t State = LOWPOWER;

int8_t RssiValue = 0;
int8_t SnrValue = 0;

/* Led Timers objects*/
static TimerEvent_t timerLed;

/* Private function prototypes -----------------------------------------------*/
/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone(void);

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout(void);

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout(void);

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError(void);

/*!
 * \brief Function executed on when led timer elapses
 */
static void OnledEvent(void);
/**
 * Main application entry point.
 */

uint8_t ReadyMsg[] = "PREST";
int esclavo = 0;
int prest = 0;

int main(void) {
	uint8_t i;

	HAL_Init();

	SystemClock_Config();

	DBG_Init();

	vcom_Init();

	HW_Init();

	/* Led Timers*/
	TimerInit(&timerLed, OnledEvent);
	TimerSetValue(&timerLed, LED_PERIOD_MS);

	TimerStart(&timerLed);

//   Radio initialization
	RadioEvents.TxDone = OnTxDone;
	RadioEvents.RxDone = OnRxDone;
	RadioEvents.TxTimeout = OnTxTimeout;
	RadioEvents.RxTimeout = OnRxTimeout;
	RadioEvents.RxError = OnRxError;

	Radio.Init(&RadioEvents);

	Radio.SetChannel( RF_FREQUENCY);

#if defined( USE_MODEM_LORA )

	Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
	LORA_SPREADING_FACTOR, LORA_CODINGRATE,
	LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
	true, 0, 0, LORA_IQ_INVERSION_ON, 3000000);

	Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
	LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
	LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
			LORA_IQ_INVERSION_ON, true);

#elif defined( USE_MODEM_FSK )

	Radio.SetTxConfig( MODEM_FSK, TX_OUTPUT_POWER, FSK_FDEV, 0,
			FSK_DATARATE, 0,
			FSK_PREAMBLE_LENGTH, FSK_FIX_LENGTH_PAYLOAD_ON,
			true, 0, 0, 0, 3000000 );

	Radio.SetRxConfig( MODEM_FSK, FSK_BANDWIDTH, FSK_DATARATE,
			0, FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH,
			0, FSK_FIX_LENGTH_PAYLOAD_ON, 0, true,
			0, 0,false, true );

#else
#error "Please define a frequency band in the compiler options."
#endif

	Radio.Rx( RX_TIMEOUT_VALUE);
	bool isMaster = true;

	while (1) {
		switch (State) {
		case RX:
			PRINTF("Buff: %s\n", Buffer);
			if (isMaster == true) {
				PRINTF("SOY EL MASTER\r\n");
				if (strncmp((const char*) Buffer, (const char*) "PREST", 5)	== 0) {
					PRINTF("Recibido: %s\n", Buffer);
					isMaster = false;
					PRINTF("ME CONVIERTO A ESCLAVO\r\n");
					esclavo = 1;
					Delay(1);
					Radio.Send( "OK", 2);
					Radio.Rx( RX_TIMEOUT_VALUE);
				} else {
					//isMaster = true;
					PRINTF("START AGAIN\r\n");
					Radio.Rx( RX_TIMEOUT_VALUE);
				}
//				Radio.Send("OK", BufferSize);
//				PRINTF("MASTER Y NI PING NI PONG\r\n");
//				isMaster = true;
//				Radio.Rx( RX_TIMEOUT_VALUE);
//				if (BufferSize > 0) {
//					if (strncmp((const char*) Buffer, (const char*) PongMsg, 4)
//							== 0) {
//						TimerStop(&timerLed);
//						LED_Off(LED_BLUE);
//						LED_Off(LED_GREEN);
//						LED_Off(LED_RED1);;
//						// Indicates on a LED that the received frame is a PONG
//						LED_Toggle(LED_RED2);
//
//						// Send the next PING frame
//						Buffer[0] = 'P';
//						Buffer[1] = 'I';
//						Buffer[2] = 'N';
//						Buffer[3] = 'G';
//						// We fill the buffer with numbers for the payload
//						for (i = 4; i < BufferSize; i++) {
//							Buffer[i] = i - 4;
//						}
//						PRINTF("...PING\n");
//
//						DelayMs(1);
//						Radio.Send(Buffer, BufferSize);
//					} else if (strncmp((const char*) Buffer,
//							(const char*) PingMsg, 4) == 0) { // A master already exists then become a slave
//						isMaster = false;
//						PRINTF("ME CONVIERTO A ESCLAVO\r\n");
//						//GpioWrite( &Led2, 1 ); // Set LED off
//						Radio.Rx( RX_TIMEOUT_VALUE);
//					} else // valid reception but neither a PING or a PONG message
//					{    // Set device as master ans start again
//						isMaster = true;
//						PRINTF("MASTER Y NI PING NI PONG\r\n");
//						Radio.Rx( RX_TIMEOUT_VALUE);
//					}
//				}
			} else {
				PRINTF("SOY ESCLAVO\r\n");
				if (BufferSize > 0) {
					PRINTF("Slave buff: %s\n", Buffer);
					if (strncmp((const char*) Buffer, (const char*) "PREST", 5)	== 0) {
						PRINTF("ESCLAVO PREST: %s\r\n", Buffer);
						Radio.Send("OK", BufferSize);
						prest = 1;
						DelayMs(1);
					} else 	{
						//isMaster = true;
						PRINTF("ESCLAVO OK");
						Radio.Send("OK", BufferSize);
						DelayMs(1);

					}
					Radio.Rx( RX_TIMEOUT_VALUE);
				}
			}
			Radio.Rx( RX_TIMEOUT_VALUE);
			State = LOWPOWER;
			break;
		case TX:
			// Indicates on a LED that we have sent a PING [Master]
			// Indicates on a LED that we have sent a PONG [Slave]
			//GpioWrite( &Led2, GpioRead( &Led2 ) ^ 1 );
			Radio.Rx( RX_TIMEOUT_VALUE);
			State = LOWPOWER;
			break;
		case RX_TIMEOUT:
		case RX_ERROR:
			if (isMaster == true) {
				PRINTF("RX_ERROR_MASTER\n");
				DelayMs(1);
				Radio.Send("READY", BufferSize);
			} else if (esclavo == 1 && prest == 0){
				PRINTF("RX_ERROR_ESCLAVO\n");
				Radio.Send("READY", BufferSize);
			} else if (esclavo == 1 && prest == 1){
				PRINTF("RX_ERROR_ESCLAVO\n");
				Radio.Send("OK", BufferSize);
			}
			else {
				Radio.Rx( RX_TIMEOUT_VALUE);
			}
			Radio.Rx( RX_TIMEOUT_VALUE);
			State = LOWPOWER;
			break;
		case TX_TIMEOUT:
			Radio.Rx( RX_TIMEOUT_VALUE);
			State = LOWPOWER;
			break;
		case LOWPOWER:
		default:
//			Radio.Rx( RX_TIMEOUT_VALUE);
			break;
		}

		DISABLE_IRQ( );
		/* if an interupt has occured after __disable_irq, it is kept pending
		 * and cortex will not enter low power anyway  */
		if (State == LOWPOWER) {
#ifndef LOW_POWER_DISABLE
			LowPower_Handler();
#endif
		}
		ENABLE_IRQ( );

	}
}

void OnTxDone(void) {
	Radio.Sleep();
	State = TX;
//	PRINTF("OnTxDone\n");
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
	Radio.Sleep();
	BufferSize = size;
	memcpy(Buffer, payload, BufferSize);
	RssiValue = rssi;
	SnrValue = snr;
	State = RX;

//	PRINTF("OnRxDone\n");
//	PRINTF("RssiValue=%d dBm, SnrValue=%d\n", rssi, snr);
}

void OnTxTimeout(void) {
	Radio.Sleep();
	State = TX_TIMEOUT;

//	PRINTF("OnTxTimeout\n");
}

void OnRxTimeout(void) {
	Radio.Sleep();
	State = RX_TIMEOUT;
//	PRINTF("OnRxTimeout\n");
}

void OnRxError(void) {
	Radio.Sleep();
	State = RX_ERROR;
//	PRINTF("OnRxError\n");
}

static void OnledEvent(void) {
	LED_Toggle(LED_BLUE);
	LED_Toggle(LED_RED1);
	LED_Toggle(LED_RED2);
	LED_Toggle(LED_GREEN);

	TimerStart(&timerLed);
}

