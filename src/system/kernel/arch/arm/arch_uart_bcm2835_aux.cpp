/*
 * Copyright 2023 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <arch/arm/arch_uart_bcm2835_aux.h>
#include <new>


ArchUARTBCM2835Aux::ArchUARTBCM2835Aux(addr_t base, int64 clock)
	:
	DebugUART8250(base, clock)
{
}


ArchUARTBCM2835Aux::~ArchUARTBCM2835Aux()
{
}


void
ArchUARTBCM2835Aux::InitPort(uint32 baud)
{
}


ArchUARTBCM2835Aux*
arch_get_uart_bcm2835_aux(addr_t base, int64 clock)
{
	static char buffer[sizeof(ArchUARTBCM2835Aux)];
	ArchUARTBCM2835Aux *uart = new(buffer) ArchUARTBCM2835Aux(base, clock);
	return uart;
}
