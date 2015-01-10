/*
             LUFA Library
     Copyright (C) Dean Camera, 2013.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2013  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the VirtualSerial demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "VirtualSerial.h"
#include "serial.h"

#define HAS_BOOTLOADER (defined(FUSE_BOOTSZ0) && defined(FUSE_BOOTSZ1))

#if HAS_BOOTLOADER
/* Calculate the size of the bootloader section in words based on
 * BOOTSZ[1..0]
 * See Table 23-11 of the ATmega32u2 datasheet for reference
 */
static inline uint16_t bootloader_size(void)
{
	uint8_t hfuse = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
	uint8_t bootsz = ((hfuse & FUSE_BOOTSZ0) == 0) | ((hfuse & FUSE_BOOTSZ1) == 0) << 1;
	return 256 << (3-bootsz);
}

/* Calculate the bootloader address. The bootloader is located at the end of the
 * flash. So we have to substract the bootloader size from the flash size
 */
static inline uint16_t bootloader_address(void)
{
	return ((FLASHEND >> 1) - bootloader_size()) + 1;
}

/* Jumps to the beginning of bootloader. Address is computed in runtime */
static inline void run_bootloader(void)
{
	USB_Disable();
	cli();

	uint16_t bladdr = bootloader_address();
	((void (*)(void))bladdr)();
}
#endif

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber   = 0,
				.DataINEndpoint           =
					{
						.Address          = CDC_TX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.DataOUTEndpoint =
					{
						.Address          = CDC_RX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.NotificationEndpoint =
					{
						.Address          = CDC_NOTIFICATION_EPADDR,
						.Size             = CDC_NOTIFICATION_EPSIZE,
						.Banks            = 1,
					},
			},
	};

/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs
 */
static FILE USBSerialStream;

void Menu_Task(void)
{
	char buf[10];

	uint8_t ret = SerialReadLine(buf, 10);
	if ( ret > 0 )
	{
		switch ( buf[0] )
		{
			case 'b':
#if (defined(FUSE_BOOTSZ0) && defined(FUSE_BOOTSZ1))
				run_bootloader();
#else
				SerialPutString("Unknown chip. Don't know where to find the bootloader." NEWLINESTR);
#endif
				break;
			case 'S':
				SerialPutString("Bootloader address: ");
#if (defined(FUSE_BOOTSZ0) && defined(FUSE_BOOTSZ1))
				{
					uint8_t n = 9;
					char buf[n];
					uint8_t m = snprintf(buf, n, "0x%4x" NEWLINESTR, bootloader_address());
					if (m < 0)
						SerialPutString("Format error!" NEWLINESTR);
					else if (m > n)
						SerialPutString("Output error!" NEWLINESTR);
					else
						SerialPutString(buf);
				}
#else
				SerialPutString("unknown" NEWLINESTR);
#endif
				cli();
				char lfuse = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS);
				char hfuse = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
				char efuse = boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS);
				char lock = boot_lock_fuse_bits_get(GET_LOCK_BITS);
				sei();
				{
					uint8_t n = 55;
					char buf[n];
					uint8_t m = snprintf(buf, n, "Fuses: lfuse:0x%2x, hfuse:0x%2x, efuse:0x%2x, lock:0x%2x" NEWLINESTR, lfuse, hfuse, efuse, lock);
					if (m < 0)
						SerialPutString("Format error!" NEWLINESTR);
					else if (m > n)
						SerialPutString("Output error!" NEWLINESTR);
					else
						SerialPutString(buf);
				}

				{
					uint8_t n = 19;
					char buf[n];
					uint8_t m = snprintf(buf, n, "FLASHEND: 0x%4x" NEWLINESTR, FLASHEND);
					if (m < 0)
						SerialPutString("Format error!" NEWLINESTR);
					else if (m > n)
						SerialPutString("Output error!" NEWLINESTR);
					else
						SerialPutString(buf);
				}

				{
					uint8_t n = 17;
					char buf[n];
					uint8_t m = snprintf(buf, n, "blsize: 0x%4x" NEWLINESTR, bootloader_size());
					if (m < 0)
						SerialPutString("Format error!" NEWLINESTR);
					else if (m > n)
						SerialPutString("Output error!" NEWLINESTR);
					else
						SerialPutString(buf);
				}

				break;
			case 'h':
			default:
				SerialPutString("OpenHR2 master menu" NEWLINESTR );
				SerialPutString(" h: print this message" NEWLINESTR);
				SerialPutString(" b: enter Bootloader mode" NEWLINESTR);
				SerialPutString(" S: print current settings" NEWLINESTR);
				break;
		}
	}
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	GlobalInterruptEnable();

	for (;;)
	{
		/* Must throw away unused bytes from the host, or it will lock up while waiting for the device */
		CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
		Menu_Task();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

	/* Hardware Initialization */
	USB_Init();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

