/**

 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
#include <asf.h>
#include "main.h"

#include <avr/interrupt.h>

struct {
	uint8_t start_code;
	uint8_t buffer[512];
} dmx_packet;


struct dma_channel_config dmach_conf;


int main (void)
{
	irq_initialize_vectors();
	cpu_irq_enable();
	sysclk_init();
	board_init();
	
	sysclk_enable_module(SYSCLK_PORT_C, SYSCLK_USART1);
	
	init();
	dmx_packet.start_code = 0;
	
	// Start USB stack to authorize VBus monitoring
	udc_start();
	
	while ( !( USARTC1.STATUS & USART_DREIF_bm) );
	USARTC1.DATA = 0x00;
	
	// Zero the buffer before we start
	for (int i = 0; i < 512; i++){
		dmx_packet.buffer[i] = 0;
	}
	
	// Trigger the first packet send manually
	sendDMXPacket();
	while(1) {
		
		//udi_cdc_putc('a');
		// Test ramp
		/*
		for (int i = 0; i < 512; i+=3){
			dmx_packet.buffer[i]++;
		}
		*/
		
	}
}



void uart_rx_notify(uint8_t port) 
{	
	static uint8_t nibble = 1;
	static uint16_t buffer_idx = 0;
	static uint8_t temp_value = 0;
	char data[64];
	iram_size_t available = 0;
	iram_size_t read = 0;
	
	while(udi_cdc_is_rx_ready()) {
		read = available = udi_cdc_get_nb_received_data();
		if(available > 32) {
			read = 32;
		}
		udi_cdc_read_buf(data, read);
		for(int idx = 0; idx < read; idx++)
		{
			char ch = data[idx];
			if (ch == '\n' || ch == '\r'){
				buffer_idx = 0;
				nibble=1;
			}else{
				if (nibble == 0) {
					dmx_packet.buffer[buffer_idx] = temp_value | toUint8_t(ch);
					nibble = 1;
					buffer_idx++;
					} else {
					temp_value = toUint8_t(ch) << 4;
					nibble = 0;
				}
			}
		}
	}
}

uint8_t toUint8_t(char ch){
	switch(ch) {
	case '0': return 0x0;
	case '1': return 0x1;
	case '2': return 0x2;
	case '3': return 0x3;
	case '4': return 0x4;
	case '5': return 0x5;
	case '6': return 0x6;
	case '7': return 0x7;
	case '8': return 0x8;
	case '9': return 0x9;
	case 'A': return 0xA;
	case 'B': return 0xB;
	case 'C': return 0xC;
	case 'D': return 0xD;
	case 'E': return 0xE;
	case 'F': return 0xF;
	case 'a': return 0xA;
	case 'b': return 0xB;
	case 'c': return 0xC;
	case 'd': return 0xD;
	case 'e': return 0xE;
	case 'f': return 0xF;
	default: return 0x0;
	}
}

void init(void){
	//USART initialization should use the following sequence:
	//1. Set the TxD pin value high, and optionally set the XCK pin low.
	PORTC.OUT |= (1 << 7);
	//2. Set the TxD and optionally the XCK pin as output.
	PORTC.DIR |= (1 << 4)|(1 << 5)|(1 << 7);
	//3. Set the baud rate and frame format. BAUD = 250k (7)
	USARTC1.BAUDCTRLA = 5;
	USARTC1.BAUDCTRLB = 0;
	//4. Set the mode of operation (enables XCK pin output in synchronous mode)., no parity, 2 stop bits, 8-bit length
	USARTC1.CTRLC |= (1 << 0)|(1 << 1)|(1 << 3);
	//5. Enable the transmitter
	USARTC1.CTRLB |= (1 << 3);
	
	PORTC.OUT |= (1 << 5);   //enable transmit
	PORTC.OUT |= (1 << 4);
	
	memset(&dmach_conf, 0, sizeof(dmach_conf));
	
	dma_channel_set_burst_length(&dmach_conf, DMA_CH_BURSTLEN_1BYTE_gc);
	dma_channel_set_single_shot(&dmach_conf);
	dma_channel_set_transfer_count(&dmach_conf, 513);
	dma_channel_set_src_reload_mode(&dmach_conf,
	DMA_CH_SRCRELOAD_TRANSACTION_gc);
	dma_channel_set_dest_reload_mode(&dmach_conf,
	DMA_CH_DESTRELOAD_TRANSACTION_gc);
	dma_channel_set_src_dir_mode(&dmach_conf, DMA_CH_SRCDIR_INC_gc);
	dma_channel_set_source_address(&dmach_conf,
	(uint16_t)(uintptr_t)&dmx_packet);
	dma_channel_set_dest_dir_mode(&dmach_conf, DMA_CH_DESTDIR_FIXED_gc);
	dma_channel_set_destination_address(&dmach_conf,
	(uint16_t)(uintptr_t)&USARTC1.DATA);
	dma_channel_set_trigger_source(&dmach_conf, DMA_CH_TRIGSRC_USARTC1_DRE_gc);
	
	dma_channel_set_interrupt_level(&dmach_conf, DMA_INT_LVL_HI);
	dma_enable();
	dma_set_priority_mode(DMA_PRIMODE_CH0RR123_gc);
	
	PMIC.CTRL |= PMIC_HILVLEN_bm;
	sei();
}

#define STATE_IDLE 8
#define STATE_BREAK_AND_MARK 10
#define STATE_START_CODE 12
#define STATE_DMA_TRANSFER 14

static volatile uint8_t dmx_state = STATE_IDLE;
void sendDMXPacket(void){
	dmx_state = STATE_BREAK_AND_MARK;
	// Change clock speed to send break and mark
	while ( !( USARTC1.STATUS & USART_TXCIF_bm) );
	USARTC1.STATUS |= USART_TXCIF_bm;
	USARTC1.BAUDCTRLA = 25;
	USARTC1.BAUDCTRLB = 0;
	
	while ( !( USARTC1.STATUS & USART_DREIF_bm) );
	// Enable interrupt for it to complete
	USARTC1.CTRLA |= USART_TXCINTLVL_HI_gc;
	USARTC1.DATA = 0x00; // Break+Mark

	/*
	do 
	{
	} while (dmx_state != STATE_DMA_TRANSFER);
	
	do {} while (dma_get_channel_status(0) != DMA_CH_TRANSFER_COMPLETED);
	
	dmx_state = STATE_IDLE;
	*/
}

void dma_callback(enum dma_channel_status status);

ISR(USARTC1_TXC_vect)
{
	// Disable the interrupt handler
	USARTC1.CTRLA &= ~USART_TXCINTLVL_HI_gc;
	
	dmx_state = STATE_START_CODE;
	
	// Change clock of uart for data transfer.
	//USARTC1.STATUS |= USART_TXCIF_bm;
	USARTC1.BAUDCTRLB = 0;
	USARTC1.BAUDCTRLA = 5;
	
	
	// Let DMA do the rest of the work :)
	dma_channel_write_config(0, &dmach_conf);
	dma_set_callback(0, dma_callback);
	dma_channel_enable(0);
	dmx_state = STATE_DMA_TRANSFER;
}

void dma_callback(enum dma_channel_status status)
{
	dmx_state = STATE_IDLE;
	dma_set_callback(0, NULL);
	// Send a new dmx packet
	sendDMXPacket();
	
};
