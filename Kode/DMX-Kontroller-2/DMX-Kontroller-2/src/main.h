/*
 * main.h
 *
 * Created: 02.10.2013 12:28:51
 *  Author: oystein
 */ 


#ifndef MAIN_H_
#define MAIN_H_

void init(void);
void sendDMXPacket(void);

void uart_rx_notify(uint8_t port);
uint8_t toUint8_t(char ch);

#endif /* MAIN_H_ */