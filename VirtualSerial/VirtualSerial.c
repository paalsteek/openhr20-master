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

//uint32_t Boot_Key ATTR_NO_INIT;

#define MAGIC_BOOT_KEY 0xDEADBEEF
#define FLASH_SIZE_BYTES 0x3FFF
#define BOOTLOADER_SEC_SIZE_BYTES 0x07FF
//#define BOOTLOADER_START_ADDRESS  (FLASH_SIZE_BYTES - BOOTLOADER_SEC_SIZE_BYTES)
#define BOOTLOADER_START_ADDRESS 0x3800

//void Bootloader_Jump_Check(void) ATTR_INIT_SECTION(3);
//#void Bootloader_Jump_Check(void)
//{
	// If the reset source was the watchdog and the key is correct, clear it and jump to the bootloader
//	if ((MCUSR & (1<<WDRF)) && (Boot_Key == MAGIC_BOOT_KEY))
//	{
//		Boot_Key = 0;
//		((void (*)(void))BOOTLOADER_START_ADDRESS)();
//	}
//}

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

void Menu_Task()
{
	char buf[10];

	uint8_t ret = SerialReadLine(buf, 10);
	if ( ret > 0 )
	{
		switch ( buf[0] )
		{
			case 'b':
				cli();
				// Disable USB
				USB_Disable();
				// Wait for 2 seconds so the host can detach the usb device
				for ( uint8_t i = 0; i < 256; i++ )
					_delay_ms(16);
				__asm("jmp 0x7000");
				break;
			case 'd':
			case 'D':
			case 'i':
			case 'I':
			case 'p':
			case 'P':
			case 'S':
			case 't':
			case 'T':
				SerialPutString("This funktion is not implemented yet." NEWLINESTR );
				break;
			case 'h':
			default:
				SerialPutString("C8H10N4O2 menu" NEWLINESTR );
				SerialPutString(" h: print this message" NEWLINESTR);
				SerialPutString(" b: enter Bootloader mode" NEWLINESTR);
				SerialPutString(" S: print current settings" NEWLINESTR );
				SerialPutString("-----" NEWLINESTR );
				SerialPutString(" p/P: increase/decrease p gain" NEWLINESTR );
				SerialPutString(" i/I: increase/decrease i gain" NEWLINESTR );
				SerialPutString(" d/D: increase/decrease d gain" NEWLINESTR );
				SerialPutString(" t/T: increase/decrease setpoint" NEWLINESTR );
				SerialPutString(" r: reset PID configuration to default values" NEWLINESTR );
				SerialPutString(" s: save PID configuration to EEPROM" NEWLINESTR );
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

