/* --------------
 * DMX-Kontroller
 * --------------
 * main.c
 *
 * Created: 18.08.2013 12:12:32
 * Author: Øystein Smith
 */ 


#define F_CPU 32000000

#include <asf.h>
#include <stdio_usb.h>
#include <avr/io.h>
#include <util/delay.h>

void init();
void sendDMXPacket(uint8_t* dmxData);

int main (void)
{
	irq_initialize_vectors();
	cpu_irq_enable();
	init();
	//board_init();

	stdio_usb_init();
	udc_start();
	
	uint8_t ch;
	
	while (true) {

		scanf("%c",&ch); // get one input character

		if (ch) {
			printf("%c",'\n');
			printf("%c",ch); // echo to output
		}
	}
}

void init(){
	
	OSC.CTRL = OSC_RC32MEN_bm; //enable 32MHz oscillator
	while(!(OSC.STATUS & OSC_RC32MRDY_bm));   //wait for stability
	CCP = CCP_IOREG_gc; //secured access
	CLK.CTRL = 0x01; //choose this osc source as clk
	
	//USART initialization should use the following sequence:
	//1. Set the TxD pin value high, and optionally set the XCK pin low.
	PORTC.OUT |= (1 << 7);
	//2. Set the TxD and optionally the XCK pin as output.
	PORTC.DIR |= (1 << 5)|(1 << 7);
	//3. Set the baud rate and frame format. BAUD = 250k (7)
	USARTC1.BAUDCTRLA = 7;
	USARTC1.BAUDCTRLB = 0;
	//4. Set the mode of operation (enables XCK pin output in synchronous mode)., no parity, 2 stop bits, 8-bit length
	USARTC1.CTRLC |= (1 << 0)|(1 << 1)|(1 << 3);
	//5. Enable the transmitter
	USARTC1.CTRLB |= (1 << 3);
	
	PORTC.OUT |= (1 << 5);   //enable transmit
}

void sendDMXPacket(uint8_t* dmxData){
	while ( !USART_TXCIF_bm);
	_delay_us(500);
	USARTC1.BAUDCTRLA = 15;
	USARTC1.BAUDCTRLB = 0;
	_delay_us(500);
	
	while ( !( USARTC1.STATUS & USART_DREIF_bm) );
	USARTC1.DATA = 0x00; // Break+Mark

	while (!USART_TXCIF_bm);
	_delay_us(500);
	USARTC1.BAUDCTRLB = 0;
	USARTC1.BAUDCTRLA = 7;
	_delay_us(500);
	//USARTC1.BAUDCTRLB = 0;
	//_delay_us(1000); //Break
	
	while ( !( USARTC1.STATUS & USART_DREIF_bm) );
	USARTC1.DATA = 0xff; // start byte
	
	while ( !( USARTC1.STATUS & USART_DREIF_bm) );
	USARTC1.DATA = 0x00; // start byte
	
	for (int i=0; i<512; i++){
		while ( !( USARTC1.STATUS & USART_DREIF_bm) );
		USARTC1.DATA = dmxData[i];
	}
}