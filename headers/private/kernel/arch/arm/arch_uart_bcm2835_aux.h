/*
 * Copyright 2023 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef __DEV_UART_BCM2835_AUX_H
#define __DEV_UART_BCM2835_AUX_H

#include <arch/generic/debug_uart_8250.h>


#define UART_KIND_BCM2835_AUX "bcm2835-aux"


class ArchUARTBCM2835Aux : public DebugUART8250 {
public:
							ArchUARTBCM2835Aux(addr_t base, int64 clock);
							~ArchUARTBCM2835Aux();
			void			InitPort(uint32 baud);
};


ArchUARTBCM2835Aux *arch_get_uart_bcm2835_aux(addr_t base, int64 clock);


#endif
