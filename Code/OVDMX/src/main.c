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
 * -# "Insert system clock initialization code here" comment
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */
#include <asf.h>
#include "ovdmx.h"

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

/*
Package format:
Byte 0: 'O' (magic)
Byte 1: 'V' (magic)
Byte 2: 'D' (for DMX) or 'R' (for RDM, not supported yet)
Byte 3: MSB of data length
Byte 4: LSB of data length
Byte 5 to (package_length+5): DATA
Byte package_length+5: MSB of CRC16
Byte package_length+6: LSB of CRC16

CRC is calculated from byte 0 to the end of the data. 
The CRC bytes can either be written as 0,0 and the device will ignore the CRC check.
(CRC is currently not supported, so screw that)
*/


void usb_rx_notify(uint8_t port) 
{
	static uint8_t frame_data[600];
	static uint16_t frame_write_idx = 0;
	static int16_t frame_start_idx = -1;
	static uint16_t frame_length = 0;
	iram_size_t available = 0;
	iram_size_t read = 0;
	
	while(udi_cdc_is_rx_ready()) {
		read = available = udi_cdc_get_nb_received_data();
		if(available > 32) {
			read = 32;
		}
		// Read into the frame data buffer
		if(frame_write_idx > 550) {
			// Should never be this high
			frame_start_idx = -1;
			frame_length = 0;
			frame_write_idx = 0;
		}
		int remaining = udi_cdc_read_buf(&frame_data[frame_write_idx], read);
		frame_write_idx += (read-remaining);
		
		// If we have not yet found the header of the frame lets search for it
		if(frame_start_idx < 0)
		{
			// We need at least 5 bytes to read the headers
			if(frame_write_idx >= 5) 
			{
				// Search for the header
				uint8_t offset = 0;
				for(uint16_t idx = 0; idx < frame_write_idx; idx++)
				{
					if(offset == 0 && frame_data[idx] == (uint8_t)'O')
					{
						offset++; // Found the 'O' byte
						continue;
					}
					else if(offset == 1 && frame_data[idx] == (uint8_t)'V')
					{
						offset++; // Found the 'V' byte
						continue;
					}
					else if(offset == 2 && frame_data[idx] == (uint8_t)'D')
					{
						// Found the 'D' byte
						offset++;
						continue;
					}
					
					if(offset == 3) {
						// Header is found
						frame_start_idx = idx - offset; // Set the start of frame to the correct offset
						frame_length = 0;
						break;
					}
				}
				
				// If we have not found the correct start of frame
				if(frame_start_idx < 0)
				{
					dmx_packet.buffer[1] = 0xff;
					// reset write index to 0, and wait for more data
					frame_write_idx = 0;
					continue;
				}
			}
		}
		
		// Check if we have found the start of the frame
		if(frame_start_idx >= 0) {
			if(frame_length == 0) {
				// Lets read the frame length
				frame_length = (((uint16_t)frame_data[frame_start_idx+3])<<8)|((uint16_t)frame_data[frame_start_idx+4]);
				
				if(frame_length > 512) {
					// We do not allow a frame length of more than 512 bytes. Abort!
					dmx_packet.buffer[2] = 0xff;
					frame_start_idx = -1;
					frame_write_idx = 0;
					continue;
				}
				
			}
			
			// Check if we have received enough data for the full frame
			if(frame_write_idx >= (frame_start_idx+frame_length+5+2)) {
				// We have enough data for the full frame
				// Check if the last bytes is 0, 0
				if((frame_data[frame_start_idx+frame_length+5+0] == 0) 
					&& (frame_data[frame_start_idx+frame_length+5+1] == 0) )
				{
					// Valid frame. Copy the data to the dmx frame buffer
					memcpy(dmx_packet.buffer, &frame_data[frame_start_idx+5], frame_length);
				}
				else {
					dmx_packet.buffer[3] = 0xff;
				}
				
				// Copy the unprocessed part of the frame_buffer to the front of the buffer
				int16_t data_left = frame_write_idx - (frame_start_idx+frame_length+5+2);
				memcpy(frame_data, &frame_data[frame_start_idx+frame_length+5+2], data_left);
				frame_start_idx = -1;
				frame_length = 0;
				frame_write_idx = data_left;
				
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
	// Enable the clock for the USART module on PORTC
	sysclk_enable_module(SYSCLK_PORT_C, SYSCLK_USART1);
	
	//USART initialization should use the following sequence:
	//1. Set the TxD pin value high, and optionally set the XCK pin low.
	PORTC.OUT |= (1 << 7);
	//2. Set the TxD and optionally the XCK pin as output.
	PORTC.DIR |= (1<<4)|(1 << 5)|(1 << 7);
	//3. Set the baud rate and frame format. BAUD = 250k (7)
	USARTC1.BAUDCTRLA = 5;
	USARTC1.BAUDCTRLB = 0;
	//4. Set the mode of operation (enables XCK pin output in synchronous mode)., no parity, 2 stop bits, 8-bit length
	USARTC1.CTRLC |= (1 << 0)|(1 << 1)|(1 << 3);
	//5. Enable the transmitter
	USARTC1.CTRLB |= (1 << 3);
	
	PORTC.OUT |= (1 << 5);   //enable transmit
	PORTC.OUT |= (1 << 4);
	
	// Configure the DMA controller for transferring the DMA data
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
	USARTC1.DATA = 0x00; // Send the Break+Mark
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
