/*
 * ovdmx.h
 *
 * Created: 14.09.2015 18:52:20
 *  Author: Jan Ove Saltvedt
 */ 


#ifndef OVDMX_H_
#define OVDMX_H_


void init(void);
void sendDMXPacket(void);

void usb_rx_notify(uint8_t port);
uint8_t toUint8_t(char ch);


#endif /* OVDMX_H_ */