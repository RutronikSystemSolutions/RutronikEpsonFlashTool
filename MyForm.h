#include "Windows.h"
#include "isc_msgs.h" 
#include "CyUSBSerial.h"
#include "MyForm1.h"
#include <array>
#include <iostream>
#include <thread> 
#pragma once

#using <System.Drawing.dll>
#using <System.dll>
#using <System.Windows.Forms.dll>

namespace RutEpsonFlashTool
{

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace std;
	using namespace Threading;

	//for read_files
	using namespace System::IO;



	
	
	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------
	/*
## Cypress USB-Serial Windows Example source file (spimaster.cpp)
## ===========================
##
##  Copyright Cypress Semiconductor Corporation, 2013-2014,
##  All Rights Reserved
##  UNPUBLISHED, LICENSED SOFTWARE.
##
##  CONFIDENTIAL AND PROPRIETARY INFORMATION
##  WHICH IS THE PROPERTY OF CYPRESS.
##
##  Use of this file is governed
##  by the license agreement included in the file
##
##     <install>/license/license.txt
##
##  where <install> is the Cypress software
##  installation root directory path.
##
## ===========================
*/

// spimaster.cpp : Defines the entry point for the console application.
// 



/* Define VID & PID */
#define VID 0x04B4
#define PID 0x000A 


/*Epson SPI Error Definitions*/
#define SPIERR_TIMEOUT				1
#define SPIERR_SUCCESS				0
#define SPIERR_NULL_PTR				-1
#define SPIERR_GET_ERROR_CODE		-2
#define SPIERR_RESERVED_MESSAGE_ID	-3
#define SPIERR_ISC_VERSION_RESP		-4
#define	SPI_MSGRDY_TIMEOUT			10000

/* */
#define MAX_RECEIVED_DATA_LEN		20

#define RESET_GPIO					(UCHAR)10
#define MUTE_GPIO					(UCHAR)11
#define STBY_GPIO					(UCHAR)12
#define MSGRDY_GPIO					(UCHAR)13
#define version						v0.3
#define date						23/02/2021
	

	unsigned char	aucReceivedData[MAX_RECEIVED_DATA_LEN];
	unsigned short	usMessageErrorCode;
	unsigned short	usBlockedMessageID;
	unsigned short	usSequenceStatus;
	int SignalCount = 0;
	int cSignal = 1;
	unsigned int stop=0, acount;
	int epsonStatus = 0;
	UINT8 cnt = 1, nb = 1, once = 0, stBlock = 0;

	CY_DATA_BUFFER cyDatabufferWrite, cyDatabufferRead;
	CY_RETURN_STATUS rStatus;

	//Variable to store cyHandle of the selected device
	CY_HANDLE cyHandle;
	//Varible to capture return values from USB Serial API 
	CY_RETURN_STATUS cyReturnStatus;

	//CY_DEVICE_INFO provides additional details of each device such as product Name, serial number etc..
	CY_DEVICE_INFO cyDeviceInfo, cyDeviceInfoList[16];

	//Structure to store VID & PID defined in CyUSBSerial.h
	CY_VID_PID cyVidPid;

	//Variables used by application
	UINT8 cyNumDevices;
	unsigned char deviceID[16];



	/* Sequencer message table */
	unsigned char aucIscSequencerConfigReq[] =
	{
		0x00, //Padding word
		0xAA, //Message start command 
		0x10, //Message length LSB
		0x00, //Message length MSB
		0xC4, //Sequencer configure request LSB
		0x00, //Sequencer configure request MSB
		0x01, //Number of times sequence is played back LSB
		0x00, //Number of times sequence is played back MSB
		0x01, //Number of sequence playback files LSB
		0x00, //Number of sequence playback files MSB
			  //file event  PS_0005
		0x00, //Delay_ms LSB
		0x00, //Delay_ms MSB
		0x01, //Descramble LSB
		0x00, //Descramble MSB
		0x03, //File_type LSB
		0x00, //File_type MSB
		0x03, //filename LSB
		0x00, //filename MSB
	};
	// length of above request message

	int iIscSequencerConfigReqLen = sizeof(aucIscSequencerConfigReq);

	CY_RETURN_STATUS NORFlashWriteEnable(CY_HANDLE cyHandle)
	{
		unsigned char wr_data, rd_data;
		CY_RETURN_STATUS status = CY_SUCCESS;
		CY_DATA_BUFFER writeBuf;
		CY_DATA_BUFFER readBuf;

		printf("\nSending SPI Write Enable command to device...");
		writeBuf.buffer = &wr_data;
		writeBuf.length = 1;

		readBuf.buffer = &rd_data;
		readBuf.length = 1;

		wr_data = 0x06; /* Write enable command*/

		status = CySpiReadWrite(cyHandle, &readBuf, &writeBuf, 5000);
		if (status != CY_SUCCESS)
		{
			printf("\nFailed to send SPI Write Enable command to device.");
			return status;
		}
		printf("\nSPI Write Enable command sent successfully.");
		return status;
	}

	CY_RETURN_STATUS NORFlashWaitForIdle(CY_HANDLE cyHandle)
	{
		char rd_data[2], wr_data[2];
		CY_DATA_BUFFER writeBuf, readBuf;
		int timeout = 0xFFFF;
		CY_RETURN_STATUS status;


		printf("\nSending SPI Status query command to device...");
		writeBuf.length = 2;
		writeBuf.buffer = (unsigned char*)wr_data;

		readBuf.length = 2;
		readBuf.buffer = (unsigned char*)rd_data;

		// Loop here till read data indicates SPI status is not idle
		// Condition to be checked: rd_data[1] & 0x01

		do
		{
			wr_data[0] = 0x05; /* Get SPI status */
			status = CySpiReadWrite(cyHandle, &readBuf, &writeBuf, 5000);

			if (status != CY_SUCCESS)
			{
				//				printf("\nFailed to send SPI status query command to device.");
				break;
			}
			timeout--;
			if (timeout == 0)
			{
				//				printf("\nMaximum retries completed while checking SPI status, returning with error code.");
				status = CY_ERROR_IO_TIMEOUT;
				return status;
			}

		} while (rd_data[1] & 0x01); //Check SPI Status

//		printf("\nSPI is now in idle state and ready for receiving additional data commands.");
		return status;
	}

	CY_RETURN_STATUS NORFlashChipErase(CY_HANDLE cyHandle)
	{
		unsigned char wr_data, rd_data;
		CY_RETURN_STATUS status = CY_SUCCESS;
		CY_DATA_BUFFER writeBuf;
		CY_DATA_BUFFER readBuf;

		writeBuf.buffer = &wr_data;
		writeBuf.length = 1;

		readBuf.buffer = &rd_data;
		readBuf.length = 1;

		wr_data = 0x60; /* Chip Erase command*/

		status = NORFlashWriteEnable(cyHandle);
		if (status)
		{
			printf("Error in setting Write Enable:0x%X \n", status);
			return status;
		}

		status = CySpiReadWrite(cyHandle, &readBuf, &writeBuf, 5000);
		if (status != CY_SUCCESS)
		{
			printf("\nFailed to send SPI Write Enable command to device.");
			return status;
		}

		status = NORFlashWaitForIdle(cyHandle);
		if (status)
		{
			printf("Error in Waiting for NOR Flash active state:0x%X \n", status);
			return status;
		}

		return status;
	}

	CY_RETURN_STATUS NORFlashSectorErase(CY_HANDLE cyHandle, UINT addr)
	{
		CY_DATA_BUFFER cyDatabufferWrite, cyDatabufferRead;
		CY_RETURN_STATUS status = CY_SUCCESS;
		unsigned char wbuffer[4];
		unsigned char rbuffer[4];

		cyDatabufferRead.buffer = rbuffer;
		cyDatabufferRead.length = 4;

		cyDatabufferWrite.buffer = wbuffer;
		cyDatabufferWrite.length = 4;

		memset(rbuffer, 0x00, 4);
		memset(wbuffer, 0x00, 4);

		wbuffer[0] = 0x20;   // SPI Sector Erase command
		wbuffer[1] = (addr >> 16) & 0xFF; //Page address - MSB
		wbuffer[2] = (addr >> 8) & 0xFF;  //Page address - MSB
		wbuffer[3] = (addr & 0xFF);  // //Page address - LSB

		status = NORFlashWaitForIdle(cyHandle);
		if (status)
		{
			printf("Error in Waiting for NOR Flash active state:0x%X \n", status);
			return status;
		}

		status = NORFlashWriteEnable(cyHandle);
		if (status)
		{
			printf("Error in setting Write Enable:0x%X \n", status);
			return status;
		}

		status = CySpiReadWrite(cyHandle, &cyDatabufferRead, &cyDatabufferWrite, 100);
		if (status != CY_SUCCESS)
		{
			printf("Error in doing SPI data write :0x%X \n", cyDatabufferWrite.transferCount);
			return status;
		}

		status = NORFlashWaitForIdle(cyHandle);
		if (status)
		{
			printf("Error in Waiting for NOR Flash active state:0x%X \n", status);
			return status;
		}

		return status;
	}

	CY_RETURN_STATUS NORFlashBlockErase(CY_HANDLE cyHandle, UINT addr)
	{
		CY_DATA_BUFFER cyDatabufferWrite, cyDatabufferRead;
		CY_RETURN_STATUS status = CY_SUCCESS;
		unsigned char wbuffer[4];
		unsigned char rbuffer[4];

		cyDatabufferRead.buffer = rbuffer;
		cyDatabufferRead.length = 4;

		cyDatabufferWrite.buffer = wbuffer;
		cyDatabufferWrite.length = 4;

		memset(rbuffer, 0x00, 4);
		memset(wbuffer, 0x00, 4);

		wbuffer[0] = 0xD8;   // SPI Block Erase command
		wbuffer[1] = (addr >> 16) & 0xFF; //Page address - MSB
		wbuffer[2] = (addr >> 8) & 0xFF;  //Page address - MSB
		wbuffer[3] = (addr & 0xFF);  // //Page address - LSB

		status = NORFlashWaitForIdle(cyHandle);

		if (status)
		{
			printf("Error in Waiting for NOR Flash active state:0x%X \n", status);
			return status;
		
		}

		status = NORFlashWriteEnable(cyHandle);
		if (status)
		{
			printf("Error in setting Write Enable:0x%X \n", status);

			return status;
		}

		status = CySpiReadWrite(cyHandle, &cyDatabufferRead, &cyDatabufferWrite, 100);
		if (status != CY_SUCCESS)
		{
			printf("Error in doing SPI data write :0x%X \n", cyDatabufferWrite.transferCount);
			return status;
		}

		status = NORFlashWaitForIdle(cyHandle);
		if (status)
		{
			printf("Error in Waiting for NOR Flash active state:0x%X \n", status);
			return status;
		}

		return status;
	}

	CY_RETURN_STATUS NORFlashReadPage(CY_HANDLE cyHandle, CY_DATA_BUFFER* cyDatabufferRead, UINT addr)
	{
		CY_DATA_BUFFER cyDatabufferWrite;
		CY_RETURN_STATUS status = CY_SUCCESS;
		unsigned char wbuffer[260];

		status = NORFlashWaitForIdle(cyHandle);
		if (status)
		{
			printf("Error in Waiting for NOR Flash active state:0x%X \n", status);
			return rStatus;
		}

		memset(wbuffer, 0x00, 260);
		cyDatabufferWrite.buffer = wbuffer;
		cyDatabufferWrite.length = 260; //4 bytes command + 256 bytes page size

		// In this case the first 4 bytes of WriteBuffer has valid command data.
		wbuffer[0] = 0x03;				//SPI Read command
		wbuffer[1] = (addr >> 16) & 0xFF; //Page address - MSB
		wbuffer[2] = (addr >> 8) & 0xFF;  //Page address - MSB
		wbuffer[3] = (addr & 0xFF);      //Page address - LSB

		status = CySpiReadWrite(cyHandle, cyDatabufferRead, &cyDatabufferWrite, 100);
		if (status)
		{
			printf("CySpiReadWrite returned failure code during read operation.\n");
			return status;
		}

		return status;
	}

	CY_RETURN_STATUS  NORFlashWritePage(CY_HANDLE cyHandle, CY_DATA_BUFFER* buffer, UINT addr)
	{
		CY_DATA_BUFFER cyDatabufferRead, cyDatabufferWrite;
		CY_RETURN_STATUS status = CY_SUCCESS;
		unsigned char rbuffer[260];
		unsigned char wbuffer[260];

		status = NORFlashWaitForIdle(cyHandle);
		if (status)
		{
			printf("Error in Waiting for NOR Flash active state:0x%X \n", status);
			return rStatus;
		}

		status = NORFlashWriteEnable(cyHandle);
		if (status)
		{
			printf("Error in setting Write Enable:0x%X \n", status);
			return status;
		}

		memset(rbuffer, 0x00, 260);
		memset(wbuffer, 0x00, 260);
		cyDatabufferRead.buffer = rbuffer;
		cyDatabufferRead.length = 260; //4 bytes command + 256 bytes page size
		cyDatabufferWrite.buffer = wbuffer;
		cyDatabufferWrite.length = 260; //4 bytes command + 256 bytes page size

		// In this case the first 4 bytes of WriteBuffer has valid command data.
		wbuffer[0] = 0x02;				  //SPI Write command
		wbuffer[1] = (addr >> 16) & 0xFF; //Page address - MSB
		wbuffer[2] = (addr >> 8) & 0xFF;  //Page address - MSB
		wbuffer[3] = (addr & 0xFF);       //Page address - LSB

		/* Copy the rest of the data to the write buffer*/
		memcpy(&wbuffer[4], buffer->buffer, 256);

		status = CySpiReadWrite(cyHandle, &cyDatabufferRead, &cyDatabufferWrite, 100);
		if (status)
		{
			printf("CySpiReadWrite returned failure code during read operation.\n");
			return status;
		}

		return status;
	}

	UCHAR EpsonSendReceiveByte(CY_HANDLE cyHandle, UCHAR	ucSendData)
	{
		CY_DATA_BUFFER WriteBuff, ReadBuff;
		UCHAR	ucReceivedData = 0x0;

		/* Transfer and receive a single byte */
		WriteBuff.buffer = &ucSendData;
		WriteBuff.length = 1;
		ReadBuff.buffer = &ucReceivedData;
		ReadBuff.length = 1;
		CySpiReadWrite(cyHandle, &ReadBuff, &WriteBuff, 1000); // shorten to 100 for better application performance if only dipswitch 6  (SPI_MISO) is not connected 

		return ucReceivedData;
	}

	int EpsonSendMessage(unsigned char* pucSendMessage, unsigned short* pusReceivedMessageID)
	{
		unsigned char	ucReceivedData;				// temporary received data
		unsigned short	usSendLength;
		int				iReceivedCounts = 0;		// byte counts of received message (except prefix "0x00 0xAA")
		unsigned short	usReceiveLength = 0;		// length of received message

#ifdef CHECKSUM
		unsigned char	ucCheckSum = 0;				// volue of check sum
		int				iSentCounts = 0;			// byte counts of sent message (including prefix "0x00 0xAA")
#endif

		if (pucSendMessage == NULL || pusReceivedMessageID == NULL)
		{

			return SPIERR_NULL_PTR;

		}

		*pusReceivedMessageID = 0xFFFF;
		usSendLength = pucSendMessage[3];
		usSendLength = (usSendLength << 8) | pucSendMessage[2];

#ifdef CHECKSUM
		usSendLength += HEADER_LEN + 1;
#else
		usSendLength += HEADER_LEN;
#endif

		while (usSendLength > 0 || usReceiveLength > 0)
		{
			if (usSendLength == 0)
			{
				ucReceivedData = EpsonSendReceiveByte(cyHandle, (UCHAR)0);

#ifdef CHECKSUM
			}
			else if (usSendLength == 1)
			{
				ucReceivedData = EpsonSendReceiveByte(cyHandle, ucCheckSum);
				usSendLength--;
#endif
			}
			else
			{

#ifdef CHECKSUM
				if (iSentCounts >= HEADER_LEN)
				{
					ucCheckSum += *pucSendMessage;
				}
				iSentCounts++;
#endif
				ucReceivedData = EpsonSendReceiveByte(cyHandle, *pucSendMessage++);
				usSendLength--;

			}

			// check message prefix(0xAA)
			if (usReceiveLength == 0 && ucReceivedData == 0xAA)
			{
				usReceiveLength = 2; // set the length of message length field
			}
			else if (usReceiveLength > 0)
			{
				if (iReceivedCounts < MAX_RECEIVED_DATA_LEN)
				{
					aucReceivedData[iReceivedCounts] = ucReceivedData;
				}
				if (iReceivedCounts == 1)
				{
					usReceiveLength = ucReceivedData;
					usReceiveLength = (usReceiveLength << 8) | aucReceivedData[iReceivedCounts - 1];
					usReceiveLength -= 2; // set the length except message length field
				}
				iReceivedCounts++;
				usReceiveLength--;
			}

		}

		if (iReceivedCounts > 0)
		{
			unsigned short	usId;
			usId = aucReceivedData[3];
			usId = (usId << 8) | aucReceivedData[2];
			*pusReceivedMessageID = usId;
		}

		return SPIERR_SUCCESS;
	}

	// table used to establish transmit

	const unsigned char aucIscVersionResp[LEN_ISC_VERSION_RESP] =
	{
		0x14, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x00,
		0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};

	int EpsonReceiveMessage(unsigned short* pusReceivedMessageID)
	{
		unsigned short	i;
		unsigned char	aucHeader[2];
		unsigned char	usTmp;
		unsigned short	usLen = 0;
		unsigned short	usId = 0x0;
		int				iReceivedCounts = 0;
		int				iTimeOut = 0;

		if (pusReceivedMessageID == NULL)
		{
			return SPIERR_NULL_PTR;
		}

		*pusReceivedMessageID = 0xFFFF;
		usMessageErrorCode = 0;
		usBlockedMessageID = 0;

		aucHeader[0] = EpsonSendReceiveByte(cyHandle, 0);
		aucHeader[1] = EpsonSendReceiveByte(cyHandle, 0);

		while (1)
		{
			if (
				aucHeader[0] == 0x00 &&
				aucHeader[1] == 0xaa)
			{
				usTmp = aucReceivedData[iReceivedCounts++] = EpsonSendReceiveByte(cyHandle, 0);
				usLen = aucReceivedData[iReceivedCounts++] = EpsonSendReceiveByte(cyHandle, 0);
				usLen = (usLen << 8) + usTmp;
				usTmp = aucReceivedData[iReceivedCounts++] = EpsonSendReceiveByte(cyHandle, 0);
				usId = aucReceivedData[iReceivedCounts++] = EpsonSendReceiveByte(cyHandle, 0);
				usId = (usId << 8) + usTmp;

				for (i = 4; i < usLen; i++)
				{
					if (iReceivedCounts < MAX_RECEIVED_DATA_LEN)
					{
						aucReceivedData[iReceivedCounts++] = EpsonSendReceiveByte(cyHandle, 0);
					}
					else
					{
						EpsonSendReceiveByte(cyHandle, 0);
					}
				}
				break;
			}

			aucHeader[0] = aucHeader[1];
			aucHeader[1] = EpsonSendReceiveByte(cyHandle, 0);

			if (iTimeOut >= SPI_MSGRDY_TIMEOUT)
			{
				return SPIERR_TIMEOUT;
			}

			iTimeOut++;
		}
		*pusReceivedMessageID = usId;

		// check RESP or IND message.
		switch (usId) {
		case ID_ISC_RESET_RESP://4
		case ID_ISC_AUDIO_PAUSE_IND://4
		case ID_ISC_PMAN_STANDBY_EXIT_IND://4
		case ID_ISC_UART_CONFIG_RESP://4
		case ID_ISC_UART_RCVRDY_IND://4
		case ID_ISC_AUDIODEC_READY_IND://0x11->17
			break;

		case ID_ISC_TEST_RESP://6
		case ID_ISC_ERROR_IND://6
		case ID_ISC_AUDIO_CONFIG_RESP://
		case ID_ISC_AUDIO_VOLUME_RESP://6
		case ID_ISC_AUDIO_MUTE_RESP://6
		case ID_ISC_PMAN_STANDBY_ENTRY_RESP://6
		case ID_ISC_AUDIODEC_CONFIG_RESP://6
		case ID_ISC_AUDIODEC_DECODE_RESP://6
		case ID_ISC_AUDIODEC_PAUSE_RESP://6
		case ID_ISC_AUDIODEC_ERROR_IND://6
		case ID_ISC_SEQUENCER_CONFIG_RESP://6
		case ID_ISC_SEQUENCER_START_RESP://6
		case ID_ISC_SEQUENCER_STOP_RESP://6
		case ID_ISC_SEQUENCER_PAUSE_RESP://6
		case ID_ISC_SEQUENCER_ERROR_IND://6
		case ID_ISC_AUDIODEC_STOP_RESP://20
			usMessageErrorCode = aucReceivedData[5];
			usMessageErrorCode = (usMessageErrorCode << 8) + aucReceivedData[4];
			break;

		case ID_ISC_SEQUENCER_STATUS_IND://6
			usSequenceStatus = aucReceivedData[5];
			usSequenceStatus = (usSequenceStatus << 8) + aucReceivedData[4];
			break;

		case ID_ISC_VERSION_RESP://20
			for (i = 0; i < LEN_ISC_VERSION_RESP; i++)
			{
				if (aucReceivedData[i] != aucIscVersionResp[i])
				{
					return SPIERR_ISC_VERSION_RESP;
					break;
				}
			}
			break;

		case ID_ISC_MSG_BLOCKED_RESP://8
			usBlockedMessageID = aucReceivedData[5];
			usBlockedMessageID = (usBlockedMessageID << 8) + aucReceivedData[4];
			usMessageErrorCode = aucReceivedData[7];
			usMessageErrorCode = (usMessageErrorCode << 8) + aucReceivedData[6];
			break;

		default:
			return SPIERR_RESERVED_MESSAGE_ID;
			break;
		}

		if (usMessageErrorCode != 0)
		{
			return SPIERR_GET_ERROR_CODE;
		}

		return SPIERR_SUCCESS;
	}


	int EpsonDemo(int deviceNumber)
		/*
	Function Name: int SPIMaster(int deviceNumber)
	Purpose: Demonstates how to communicate with EEPROM connected on USB Serial DVK to SPI
	- Demonstrates how to set/get configuration of SPI device
	- Demonstrates how to perform read/write operations
	Arguments:
	deviceNumber - The device number identified during the enumeration process
	Retrun Code: returns falure code of USB-Serial API, -1 for local failures.
	*/
	{
		unsigned short	usReceivedMessageID;
		int interfaceNum = 0;
		int	iError = 0;
		unsigned char wbuffer[1024];
		unsigned char rbuffer[1024];
		UINT8 value = 0;

		cyDatabufferWrite.buffer = wbuffer;
		cyDatabufferRead.buffer = aucReceivedData;

		memset(aucReceivedData, 0x00, sizeof(aucReceivedData));
		memset(wbuffer, 0x00, sizeof(wbuffer));
		memset(rbuffer, 0x00, sizeof(wbuffer));

		rStatus = CyOpen(deviceNumber, interfaceNum, &cyHandle);

		//sprintf(deviceNumber);

		if (rStatus != CY_SUCCESS)
		{
			printf("SPI Device open failed.\n");
			return rStatus;
		}

		/*Hard-Reset Procedure*/
		//CySetGpioValue(cyHandle, RESET_GPIO, 1);
		//Sleep(100);
		//CySetGpioValue(cyHandle, RESET_GPIO, 0);
		//Sleep(100);
		//CySetGpioValue(cyHandle, RESET_GPIO, 1);
		//Sleep(500);

		/*Set mute signal(MUTE) to Low(enable)*/
		CySetGpioValue(cyHandle, MUTE_GPIO, 1);
		
		/*Set stanby signal(STBYEXIT) to Low(deassert)*/
		CySetGpioValue(cyHandle, STBY_GPIO, 0);
		Sleep(100);

		/*Send ISC_RESET_REQ*/

		//printf("\nRestarting the IC.\n");
		EpsonSendMessage(aucIscResetReq, &usReceivedMessageID);
		Sleep(10); //gain_fix_test (10)
		/*Receive ISC_RESET_RESP*/
		CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
		if (value)
		{
			iError = EpsonReceiveMessage(&usReceivedMessageID);
			if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_RESET_RESP)
			{
				printf("\nRestart failed.\n");
				return -1;
			}
			value = 0;

		}

		/*Key-code Registration*/
		printf("\nRegistering key-code.\n");
		/*Send ISC_TEST_REQ*/
		EpsonSendMessage(aucIscTestReq, &usReceivedMessageID);
		/*Receive ISC_TEST_RESP*/
		CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
		if (value)
		{
			iError = EpsonReceiveMessage(&usReceivedMessageID);
			if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_TEST_RESP)
			{
				printf("\nKey-code registration failed.\n");
				return -1;
			}
		}

		/*Get version info*/
		printf("\nReading version info.\n");
		/*Send ISC_VERSION_REQ*/
		EpsonSendMessage(aucIscVersionReq, &usReceivedMessageID);
		/*Receive ISC_VERSION_RESP*/
		CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
		if (value)
		{
			iError = EpsonReceiveMessage(&usReceivedMessageID);
			if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_VERSION_RESP)
			{
				printf("\nVersion info read failed.\n");
				return -1;
			}
		}
		

		/*Set volume & sampling freq.*/
		printf("\nVolume & sampling frequency setup.\n");
		/*Send ISC_AUDIO_CONFIG_REQ*/
		
		EpsonSendMessage(aucIscAudioConfigReq, &usReceivedMessageID);
		/*Receive ISC_AUDIO_CONFIG_RESP*/
		CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
		if (value)
		{
			iError = EpsonReceiveMessage(&usReceivedMessageID);
			if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_AUDIO_CONFIG_RESP)
			{
				printf("\nVolume & sampling frequency setup failed.\n");
				return -1;
			}
		}

		/*Sequencer configuration*/
		//printf("\nSequencer configuration.\n");
		//printf("\nSequencer configuration.\n");
	//	this->debug->AppendText("\nSequencer configuration.\n");

		/*Send ISC_SEQUENCER_CONFIG_REQ*/
		EpsonSendMessage(aucIscSequencerConfigReq, &usReceivedMessageID);
		/*Receive ISC_SEQUENCER_CONFIG_RESP*/
		CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
		if (value)
		{
			iError = EpsonReceiveMessage(&usReceivedMessageID);
			if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_CONFIG_RESP)
			{
				printf("\nSequencer configuration failed.\n");
				return -1;
			}
		}

		/*Start sequencer playback*/
		printf("\nSequencer playback.\n");
		/*Set notify status indication*/
		aucIscSequencerStartReq[6] = 0;
		/*Send ISC_SEQUENCER_START_REQ*/
		EpsonSendMessage(aucIscSequencerStartReq, &usReceivedMessageID);
		/*Receive ISC_SEQUENCER_START_RESP*/
		CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
		if (value)
		{
			iError = EpsonReceiveMessage(&usReceivedMessageID);
			if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_START_RESP)
			{
				printf("\nSequencer playback failed.\n");
				return -1;
			}
		}

		/*Wait for playback termination*/
		do
		{
			/* Receive ISC_SEQUENCER_STATUS_IND */
			iError = EpsonReceiveMessage(&usReceivedMessageID);
			if (iError < SPIERR_SUCCESS || usReceivedMessageID == ID_ISC_MSG_BLOCKED_RESP)
			{
				return -1;
			}
		} while (usReceivedMessageID != ID_ISC_SEQUENCER_STATUS_IND);
		printf("\nPlayback has ended.\n");

		/*Sequencer stop*/
		printf("\nTerminating sequencer.\n");
		/*Send ISC_SEQUENCER_STOP_REQ*/
		EpsonSendMessage(aucIscSequencerStopReq, &usReceivedMessageID);
		/*Receive ISC_SEQUENCER_STOP_RESP*/
		CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
		if (value)
		{
			iError = EpsonReceiveMessage(&usReceivedMessageID);
			if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_STOP_RESP)
			{
				printf("\nSequencer termination failed.\n");
				return -1;
			}
		}
		return CY_SUCCESS;
	}

	int EpsonChipErase(int deviceNumber)
	{
		unsigned short	usReceivedMessageID;
		int interfaceNum = 0;

		rStatus = CyOpen(deviceNumber, interfaceNum, &cyHandle);
		EpsonSendMessage(aucIscSpiSwitch, &usReceivedMessageID);
		Sleep(1);
		NORFlashChipErase(cyHandle);
		return CY_SUCCESS;
		CyClose(&cyHandle);

	}

	int EpsonUploadFile(int deviceNumber)
	{
		//unsigned short	usReceivedMessageID;
		int interfaceNum = 0;
		int	iError = 0;

		rStatus = CyOpen(deviceNumber, interfaceNum, &cyHandle);

		return CY_SUCCESS;
	}
	int FindDeviceAtSCB0()
	{
		CY_VID_PID cyVidPid;

		cyVidPid.vid = VID; //Defined as macro
		cyVidPid.pid = PID; //Defined as macro

		//Array size of cyDeviceInfoList is 16 
		cyReturnStatus = CyGetDeviceInfoVidPid(cyVidPid, deviceID, (PCY_DEVICE_INFO)&cyDeviceInfoList, &cyNumDevices, 16);

		int deviceIndexAtSCB0 = -1;
		for (int index = 0; index < cyNumDevices; index++) {
			printf("\nNumber of interfaces: %d\n \
                Vid: 0x%X \n\
                Pid: 0x%X \n\
                Serial name is: %s\n\
                Manufacturer name: %s\n\
                Product Name: %s\n\
                SCB Number: 0x%X \n\
                Device Type: %d \n\
                Device Class: %d\n\n\n",
				cyDeviceInfoList[index].numInterfaces,
				cyDeviceInfoList[index].vidPid.vid,
				cyDeviceInfoList[index].vidPid.pid,
				cyDeviceInfoList[index].serialNum,
				cyDeviceInfoList[index].manufacturerName,
				cyDeviceInfoList[index].productName,
				cyDeviceInfoList[index].deviceBlock,
				cyDeviceInfoList[index].deviceType[0],
				cyDeviceInfoList[index].deviceClass[0]);

			// Find the device at device index at SCB0
			if (cyDeviceInfoList[index].deviceBlock == SerialBlock_SCB0)
			{
				deviceIndexAtSCB0 = index;
			}
		}
		return deviceIndexAtSCB0;


	

	}




	public ref class MyForm : public System::Windows::Forms::Form

	{

		
	public:
		int numberToCompute;
	private: System::Windows::Forms::TrackBar^ volControl;
	private: System::Windows::Forms::Label^ label10;
	private: System::Windows::Forms::Label^ label9;
	private: System::Windows::Forms::GroupBox^ groupBox8;
	private: System::Windows::Forms::Label^ label11;
	private: System::Windows::Forms::GroupBox^ groupBox7;
	private: System::Windows::Forms::Label^ label12;
	private: System::Windows::Forms::Label^ actual_volume;
	private: System::Windows::Forms::Label^ board_check;
	private: System::Windows::Forms::Timer^ timer1;
	private: System::Windows::Forms::Label^ label13;
	private: System::Windows::Forms::Label^ label15;
	private: System::Windows::Forms::Label^ epson_status;
	private: System::Windows::Forms::Button^ button_HW_reset;
	private: System::Windows::Forms::Button^ button_SW_reset;

	public:
		int highestPercentageReached;
		//delegate void ProgressBarCallback(System::Object^ obj);
		MyForm(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
			numberToCompute = highestPercentageReached = 0;
			InitializeBackgoundWorker();
		}
		


	private:

		// Set up the BackgroundWorker object by 
		// attaching event handlers. 
		void InitializeBackgoundWorker()
		{
			//backgroundWorker1->DoWork += gcnew DoWorkEventHandler(this, &MyForm::backgroundWorker1_DoWork);
			backgroundWorker1->RunWorkerCompleted += gcnew RunWorkerCompletedEventHandler(this, &MyForm::backgroundWorker1_RunWorkerCompleted);
			backgroundWorker1->ProgressChanged += gcnew ProgressChangedEventHandler(this, &MyForm::backgroundWorker1_ProgressChanged);
			
		}

	
	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~MyForm()
		{
			if (components)
			{
				delete components;
			}
		}

	private: System::Windows::Forms::Button^ Buttn_SelectRomFile;
	private: System::Windows::Forms::Button^ button1;
	private: System::Windows::Forms::TextBox^ TxtBx_debug;
	private: System::Windows::Forms::Label^ label1;
	private: System::Windows::Forms::Button^ Buttn_WriteToFlash;
	private: System::Windows::Forms::Button^ Buttn_SelectListFile;

	private: System::Windows::Forms::TextBox^ textBox2;
	private: System::Windows::Forms::MenuStrip^ menuStrip1;
	private: System::Windows::Forms::ToolStripMenuItem^ fileToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^ helpToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^ exitToolStripMenuItem;
	private: System::Windows::Forms::TextBox^ textBox_selectedRomfile;

	private: System::Windows::Forms::Button^ button7;
	private: System::Diagnostics::EventLog^ eventLog1;
	private: System::Windows::Forms::OpenFileDialog^ openFileDialog1;
	private: System::Windows::Forms::Label^ label2;
	private: System::Windows::Forms::BindingSource^ bindingSource1;
	private: System::Windows::Forms::TextBox^ textBox_keycode;

	private: System::Windows::Forms::Button^ Buttn_KeyCode;
	private: System::Windows::Forms::Label^ label4;
	private: System::Windows::Forms::Label^ label3;
	private: System::Windows::Forms::Label^ label5;
	private: System::Windows::Forms::Button^ button2;

	private: System::Windows::Forms::Button^ button4_play;

	private: System::Windows::Forms::Label^ label7;
	private: System::Windows::Forms::GroupBox^ groupBox1;
	private: System::Windows::Forms::Label^ label6;
	private: System::Windows::Forms::ToolStripMenuItem^ aboutToolStripMenuItem;

	private: System::Windows::Forms::GroupBox^ groupBox2;
	private: System::Windows::Forms::GroupBox^ groupBox3;
	private: System::Windows::Forms::Button^ button3;
	private: System::Windows::Forms::Button^ button4;
	private: System::Windows::Forms::GroupBox^ groupBox4;
	private: System::Windows::Forms::Button^ button5;
	private: System::Windows::Forms::TextBox^ phrase_number;
	private: System::Windows::Forms::Button^ button6;
	private: System::Windows::Forms::GroupBox^ groupBox5;
	private: System::Windows::Forms::ToolStripMenuItem^ eraseALLFlashToolStripMenuItem;
	private: System::Windows::Forms::GroupBox^ groupBox6;
	private: System::Windows::Forms::Button^ button_stop;
	private: System::Windows::Forms::PictureBox^ pictureBox1;
	private: System::Windows::Forms::Label^ label8;
	private: System::Windows::Forms::PictureBox^ pictureBox3;
	private: System::Windows::Forms::PictureBox^ pictureBox2;

	private: System::Windows::Forms::Button^ button8;

	private: System::ComponentModel::BackgroundWorker^ backgroundWorker1;






	private: System::ComponentModel::IContainer^ components;

#pragma region Windows Form Designer generated code
		   /// <summary>
		   /// Required method for Designer support - do not modify
		   /// the contents of this method with the code editor.
		   /// </summary>
		   void InitializeComponent(void)
		   {
			   this->components = (gcnew System::ComponentModel::Container());
			   System::ComponentModel::ComponentResourceManager^ resources = (gcnew System::ComponentModel::ComponentResourceManager(MyForm::typeid));
			   this->Buttn_SelectRomFile = (gcnew System::Windows::Forms::Button());
			   this->button1 = (gcnew System::Windows::Forms::Button());
			   this->TxtBx_debug = (gcnew System::Windows::Forms::TextBox());
			   this->label1 = (gcnew System::Windows::Forms::Label());
			   this->Buttn_WriteToFlash = (gcnew System::Windows::Forms::Button());
			   this->Buttn_SelectListFile = (gcnew System::Windows::Forms::Button());
			   this->textBox2 = (gcnew System::Windows::Forms::TextBox());
			   this->menuStrip1 = (gcnew System::Windows::Forms::MenuStrip());
			   this->fileToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->eraseALLFlashToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->exitToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->helpToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->aboutToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->textBox_selectedRomfile = (gcnew System::Windows::Forms::TextBox());
			   this->button7 = (gcnew System::Windows::Forms::Button());
			   this->eventLog1 = (gcnew System::Diagnostics::EventLog());
			   this->openFileDialog1 = (gcnew System::Windows::Forms::OpenFileDialog());
			   this->bindingSource1 = (gcnew System::Windows::Forms::BindingSource(this->components));
			   this->label2 = (gcnew System::Windows::Forms::Label());
			   this->label3 = (gcnew System::Windows::Forms::Label());
			   this->label4 = (gcnew System::Windows::Forms::Label());
			   this->Buttn_KeyCode = (gcnew System::Windows::Forms::Button());
			   this->textBox_keycode = (gcnew System::Windows::Forms::TextBox());
			   this->button2 = (gcnew System::Windows::Forms::Button());
			   this->label5 = (gcnew System::Windows::Forms::Label());
			   this->button4_play = (gcnew System::Windows::Forms::Button());
			   this->groupBox1 = (gcnew System::Windows::Forms::GroupBox());
			   this->groupBox8 = (gcnew System::Windows::Forms::GroupBox());
			   this->actual_volume = (gcnew System::Windows::Forms::Label());
			   this->label9 = (gcnew System::Windows::Forms::Label());
			   this->label11 = (gcnew System::Windows::Forms::Label());
			   this->label10 = (gcnew System::Windows::Forms::Label());
			   this->volControl = (gcnew System::Windows::Forms::TrackBar());
			   this->label12 = (gcnew System::Windows::Forms::Label());
			   this->groupBox6 = (gcnew System::Windows::Forms::GroupBox());
			   this->button_stop = (gcnew System::Windows::Forms::Button());
			   this->groupBox5 = (gcnew System::Windows::Forms::GroupBox());
			   this->label6 = (gcnew System::Windows::Forms::Label());
			   this->groupBox4 = (gcnew System::Windows::Forms::GroupBox());
			   this->button4 = (gcnew System::Windows::Forms::Button());
			   this->button5 = (gcnew System::Windows::Forms::Button());
			   this->phrase_number = (gcnew System::Windows::Forms::TextBox());
			   this->groupBox3 = (gcnew System::Windows::Forms::GroupBox());
			   this->groupBox7 = (gcnew System::Windows::Forms::GroupBox());
			   this->label7 = (gcnew System::Windows::Forms::Label());
			   this->groupBox2 = (gcnew System::Windows::Forms::GroupBox());
			   this->button3 = (gcnew System::Windows::Forms::Button());
			   this->button6 = (gcnew System::Windows::Forms::Button());
			   this->pictureBox1 = (gcnew System::Windows::Forms::PictureBox());
			   this->label8 = (gcnew System::Windows::Forms::Label());
			   this->pictureBox3 = (gcnew System::Windows::Forms::PictureBox());
			   this->pictureBox2 = (gcnew System::Windows::Forms::PictureBox());
			   this->button8 = (gcnew System::Windows::Forms::Button());
			   this->backgroundWorker1 = (gcnew System::ComponentModel::BackgroundWorker());
			   this->board_check = (gcnew System::Windows::Forms::Label());
			   this->timer1 = (gcnew System::Windows::Forms::Timer(this->components));
			   this->label13 = (gcnew System::Windows::Forms::Label());
			   this->epson_status = (gcnew System::Windows::Forms::Label());
			   this->label15 = (gcnew System::Windows::Forms::Label());
			   this->button_SW_reset = (gcnew System::Windows::Forms::Button());
			   this->button_HW_reset = (gcnew System::Windows::Forms::Button());
			   this->menuStrip1->SuspendLayout();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->eventLog1))->BeginInit();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->bindingSource1))->BeginInit();
			   this->groupBox1->SuspendLayout();
			   this->groupBox8->SuspendLayout();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->volControl))->BeginInit();
			   this->groupBox6->SuspendLayout();
			   this->groupBox5->SuspendLayout();
			   this->groupBox4->SuspendLayout();
			   this->groupBox3->SuspendLayout();
			   this->groupBox2->SuspendLayout();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox1))->BeginInit();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox3))->BeginInit();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox2))->BeginInit();
			   this->SuspendLayout();
			   // 
			   // Buttn_SelectRomFile
			   // 
			   this->Buttn_SelectRomFile->DialogResult = System::Windows::Forms::DialogResult::Cancel;
			   this->Buttn_SelectRomFile->Location = System::Drawing::Point(591, 61);
			   this->Buttn_SelectRomFile->Margin = System::Windows::Forms::Padding(2);
			   this->Buttn_SelectRomFile->Name = L"Buttn_SelectRomFile";
			   this->Buttn_SelectRomFile->Size = System::Drawing::Size(101, 28);
			   this->Buttn_SelectRomFile->TabIndex = 6;
			   this->Buttn_SelectRomFile->Text = L"Select  ROM file";
			   this->Buttn_SelectRomFile->UseVisualStyleBackColor = true;
			   this->Buttn_SelectRomFile->Visible = false;
			   this->Buttn_SelectRomFile->Click += gcnew System::EventHandler(this, &MyForm::button2_Click);
			   // 
			   // button1
			   // 
			   this->button1->Location = System::Drawing::Point(840, 467);
			   this->button1->Margin = System::Windows::Forms::Padding(4);
			   this->button1->Name = L"button1";
			   this->button1->Size = System::Drawing::Size(83, 30);
			   this->button1->TabIndex = 8;
			   this->button1->Text = L"Exit";
			   this->button1->UseVisualStyleBackColor = true;
			   this->button1->Click += gcnew System::EventHandler(this, &MyForm::button1_Click);
			   // 
			   // TxtBx_debug
			   // 
			   this->TxtBx_debug->AcceptsReturn = true;
			   this->TxtBx_debug->Cursor = System::Windows::Forms::Cursors::IBeam;
			   this->TxtBx_debug->Location = System::Drawing::Point(471, 308);
			   this->TxtBx_debug->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->TxtBx_debug->Multiline = true;
			   this->TxtBx_debug->Name = L"TxtBx_debug";
			   this->TxtBx_debug->ReadOnly = true;
			   this->TxtBx_debug->ScrollBars = System::Windows::Forms::ScrollBars::Vertical;
			   this->TxtBx_debug->Size = System::Drawing::Size(451, 152);
			   this->TxtBx_debug->TabIndex = 10;
			   this->TxtBx_debug->Text = L"Powered by Rutronik\r\n";
			   // 
			   // label1
			   // 
			   this->label1->AllowDrop = true;
			   this->label1->AutoSize = true;
			   this->label1->Location = System::Drawing::Point(473, 290);
			   this->label1->Name = L"label1";
			   this->label1->Size = System::Drawing::Size(93, 13);
			   this->label1->TabIndex = 11;
			   this->label1->Text = L"Output information";
			   this->label1->Click += gcnew System::EventHandler(this, &MyForm::label1_Click);
			   // 
			   // Buttn_WriteToFlash
			   // 
			   this->Buttn_WriteToFlash->DialogResult = System::Windows::Forms::DialogResult::Cancel;
			   this->Buttn_WriteToFlash->Location = System::Drawing::Point(785, 53);
			   this->Buttn_WriteToFlash->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->Buttn_WriteToFlash->Name = L"Buttn_WriteToFlash";
			   this->Buttn_WriteToFlash->Size = System::Drawing::Size(132, 37);
			   this->Buttn_WriteToFlash->TabIndex = 12;
			   this->Buttn_WriteToFlash->Text = L"Write to Flash";
			   this->Buttn_WriteToFlash->UseVisualStyleBackColor = true;
			   this->Buttn_WriteToFlash->Visible = false;
			   this->Buttn_WriteToFlash->Click += gcnew System::EventHandler(this, &MyForm::Buttn_WriteToFlash_Click);
			   // 
			   // Buttn_SelectListFile
			   // 
			   this->Buttn_SelectListFile->DialogResult = System::Windows::Forms::DialogResult::Cancel;
			   this->Buttn_SelectListFile->Enabled = false;
			   this->Buttn_SelectListFile->Location = System::Drawing::Point(263, 16);
			   this->Buttn_SelectListFile->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->Buttn_SelectListFile->Name = L"Buttn_SelectListFile";
			   this->Buttn_SelectListFile->Size = System::Drawing::Size(132, 34);
			   this->Buttn_SelectListFile->TabIndex = 15;
			   this->Buttn_SelectListFile->Text = L"Select  LIST file";
			   this->Buttn_SelectListFile->UseVisualStyleBackColor = true;
			   this->Buttn_SelectListFile->Click += gcnew System::EventHandler(this, &MyForm::Buttn_SelectListFile_Click);
			   // 
			   // textBox2
			   // 
			   this->textBox2->Location = System::Drawing::Point(17, 22);
			   this->textBox2->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->textBox2->Name = L"textBox2";
			   this->textBox2->ReadOnly = true;
			   this->textBox2->Size = System::Drawing::Size(137, 20);
			   this->textBox2->TabIndex = 14;
			   this->textBox2->TextChanged += gcnew System::EventHandler(this, &MyForm::textBox2_TextChanged);
			   // 
			   // menuStrip1
			   // 
			   this->menuStrip1->ImageScalingSize = System::Drawing::Size(20, 20);
			   this->menuStrip1->Items->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(2) {
				   this->fileToolStripMenuItem,
					   this->helpToolStripMenuItem
			   });
			   this->menuStrip1->Location = System::Drawing::Point(0, 0);
			   this->menuStrip1->Name = L"menuStrip1";
			   this->menuStrip1->Padding = System::Windows::Forms::Padding(5, 2, 0, 2);
			   this->menuStrip1->Size = System::Drawing::Size(933, 24);
			   this->menuStrip1->TabIndex = 16;
			   this->menuStrip1->Text = L"menuStrip1";
			   this->menuStrip1->ItemClicked += gcnew System::Windows::Forms::ToolStripItemClickedEventHandler(this, &MyForm::menuStrip1_ItemClicked);
			   // 
			   // fileToolStripMenuItem
			   // 
			   this->fileToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(2) {
				   this->eraseALLFlashToolStripMenuItem,
					   this->exitToolStripMenuItem
			   });
			   this->fileToolStripMenuItem->Name = L"fileToolStripMenuItem";
			   this->fileToolStripMenuItem->Size = System::Drawing::Size(37, 20);
			   this->fileToolStripMenuItem->Text = L"File";
			   // 
			   // eraseALLFlashToolStripMenuItem
			   // 
			   this->eraseALLFlashToolStripMenuItem->Enabled = false;
			   this->eraseALLFlashToolStripMenuItem->Name = L"eraseALLFlashToolStripMenuItem";
			   this->eraseALLFlashToolStripMenuItem->Size = System::Drawing::Size(154, 22);
			   this->eraseALLFlashToolStripMenuItem->Text = L"Erase ALL Flash";
			   this->eraseALLFlashToolStripMenuItem->Click += gcnew System::EventHandler(this, &MyForm::button2_Click_1);
			   // 
			   // exitToolStripMenuItem
			   // 
			   this->exitToolStripMenuItem->Name = L"exitToolStripMenuItem";
			   this->exitToolStripMenuItem->Size = System::Drawing::Size(154, 22);
			   this->exitToolStripMenuItem->Text = L"Exit";
			   this->exitToolStripMenuItem->Click += gcnew System::EventHandler(this, &MyForm::exitToolStripMenuItem_Click_1);
			   // 
			   // helpToolStripMenuItem
			   // 
			   this->helpToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(1) { this->aboutToolStripMenuItem });
			   this->helpToolStripMenuItem->Name = L"helpToolStripMenuItem";
			   this->helpToolStripMenuItem->Size = System::Drawing::Size(44, 20);
			   this->helpToolStripMenuItem->Text = L"Help";
			   // 
			   // aboutToolStripMenuItem
			   // 
			   this->aboutToolStripMenuItem->Name = L"aboutToolStripMenuItem";
			   this->aboutToolStripMenuItem->Size = System::Drawing::Size(107, 22);
			   this->aboutToolStripMenuItem->Text = L"About";
			   this->aboutToolStripMenuItem->Click += gcnew System::EventHandler(this, &MyForm::aboutToolStripMenuItem_Click_1);
			   // 
			   // textBox_selectedRomfile
			   // 
			   this->textBox_selectedRomfile->Location = System::Drawing::Point(48, 63);
			   this->textBox_selectedRomfile->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->textBox_selectedRomfile->Name = L"textBox_selectedRomfile";
			   this->textBox_selectedRomfile->ReadOnly = true;
			   this->textBox_selectedRomfile->Size = System::Drawing::Size(237, 20);
			   this->textBox_selectedRomfile->TabIndex = 5;
			   // 
			   // button7
			   // 
			   this->button7->Enabled = false;
			   this->button7->Location = System::Drawing::Point(17, 21);
			   this->button7->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->button7->Name = L"button7";
			   this->button7->Size = System::Drawing::Size(139, 30);
			   this->button7->TabIndex = 17;
			   this->button7->Text = L"Play ALL";
			   this->button7->UseVisualStyleBackColor = true;
			   this->button7->Click += gcnew System::EventHandler(this, &MyForm::button_playall_Click);
			   // 
			   // eventLog1
			   // 
			   this->eventLog1->SynchronizingObject = this;
			   // 
			   // openFileDialog1
			   // 
			   this->openFileDialog1->FileName = L"openFileDialog1";
			   // 
			   // bindingSource1
			   // 
			   this->bindingSource1->CurrentChanged += gcnew System::EventHandler(this, &MyForm::bindingSource1_CurrentChanged);
			   // 
			   // label2
			   // 
			   this->label2->AllowDrop = true;
			   this->label2->AutoSize = true;
			   this->label2->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 6.6F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label2->Location = System::Drawing::Point(11, 68);
			   this->label2->Name = L"label2";
			   this->label2->Size = System::Drawing::Size(15, 12);
			   this->label2->TabIndex = 18;
			   this->label2->Text = L"1.";
			   this->label2->Click += gcnew System::EventHandler(this, &MyForm::label2_Click);
			   // 
			   // label3
			   // 
			   this->label3->AllowDrop = true;
			   this->label3->AutoSize = true;
			   this->label3->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 6.6F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label3->Location = System::Drawing::Point(11, 154);
			   this->label3->Name = L"label3";
			   this->label3->Size = System::Drawing::Size(15, 12);
			   this->label3->TabIndex = 19;
			   this->label3->Text = L"3.";
			   // 
			   // label4
			   // 
			   this->label4->AllowDrop = true;
			   this->label4->AutoSize = true;
			   this->label4->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 6.6F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label4->Location = System::Drawing::Point(11, 236);
			   this->label4->Name = L"label4";
			   this->label4->Size = System::Drawing::Size(15, 12);
			   this->label4->TabIndex = 20;
			   this->label4->Text = L"3.";
			   // 
			   // Buttn_KeyCode
			   // 
			   this->Buttn_KeyCode->DialogResult = System::Windows::Forms::DialogResult::Cancel;
			   this->Buttn_KeyCode->Enabled = false;
			   this->Buttn_KeyCode->Location = System::Drawing::Point(263, 20);
			   this->Buttn_KeyCode->Margin = System::Windows::Forms::Padding(4);
			   this->Buttn_KeyCode->Name = L"Buttn_KeyCode";
			   this->Buttn_KeyCode->Size = System::Drawing::Size(132, 30);
			   this->Buttn_KeyCode->TabIndex = 22;
			   this->Buttn_KeyCode->Text = L"Register keycode";
			   this->Buttn_KeyCode->UseVisualStyleBackColor = true;
			   this->Buttn_KeyCode->Click += gcnew System::EventHandler(this, &MyForm::Buttn_KeyCode_Click);
			   // 
			   // textBox_keycode
			   // 
			   this->textBox_keycode->AutoCompleteMode = System::Windows::Forms::AutoCompleteMode::Suggest;
			   this->textBox_keycode->AutoCompleteSource = System::Windows::Forms::AutoCompleteSource::HistoryList;
			   this->textBox_keycode->Enabled = false;
			   this->textBox_keycode->Location = System::Drawing::Point(17, 23);
			   this->textBox_keycode->Margin = System::Windows::Forms::Padding(4);
			   this->textBox_keycode->Name = L"textBox_keycode";
			   this->textBox_keycode->Size = System::Drawing::Size(76, 20);
			   this->textBox_keycode->TabIndex = 24;
			   this->textBox_keycode->Text = L"1AAA1F85";
			   this->textBox_keycode->TextChanged += gcnew System::EventHandler(this, &MyForm::textBox_keycode_TextChanged);
			   // 
			   // button2
			   // 
			   this->button2->Cursor = System::Windows::Forms::Cursors::AppStarting;
			   this->button2->Location = System::Drawing::Point(471, 470);
			   this->button2->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->button2->Name = L"button2";
			   this->button2->Size = System::Drawing::Size(135, 31);
			   this->button2->TabIndex = 25;
			   this->button2->Text = L"Erase ALL Flash";
			   this->button2->UseVisualStyleBackColor = true;
			   this->button2->Visible = false;
			   this->button2->Click += gcnew System::EventHandler(this, &MyForm::button2_Click_1);
			   // 
			   // label5
			   // 
			   this->label5->AllowDrop = true;
			   this->label5->AutoSize = true;
			   this->label5->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 6.6F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label5->Location = System::Drawing::Point(11, 156);
			   this->label5->Name = L"label5";
			   this->label5->Size = System::Drawing::Size(15, 12);
			   this->label5->TabIndex = 26;
			   this->label5->Text = L"2.";
			   this->label5->Click += gcnew System::EventHandler(this, &MyForm::label5_Click);
			   // 
			   // button4_play
			   // 
			   this->button4_play->Enabled = false;
			   this->button4_play->Location = System::Drawing::Point(263, 18);
			   this->button4_play->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->button4_play->Name = L"button4_play";
			   this->button4_play->Size = System::Drawing::Size(132, 28);
			   this->button4_play->TabIndex = 9;
			   this->button4_play->Text = L"Play selected";
			   this->button4_play->UseVisualStyleBackColor = true;
			   this->button4_play->Click += gcnew System::EventHandler(this, &MyForm::button3_Click);
			   // 
			   // groupBox1
			   // 
			   this->groupBox1->Controls->Add(this->groupBox8);
			   this->groupBox1->Controls->Add(this->label12);
			   this->groupBox1->Controls->Add(this->groupBox6);
			   this->groupBox1->Controls->Add(this->groupBox5);
			   this->groupBox1->Controls->Add(this->label6);
			   this->groupBox1->Controls->Add(this->groupBox4);
			   this->groupBox1->Controls->Add(this->groupBox3);
			   this->groupBox1->Controls->Add(this->label7);
			   this->groupBox1->Location = System::Drawing::Point(4, 117);
			   this->groupBox1->Margin = System::Windows::Forms::Padding(4);
			   this->groupBox1->Name = L"groupBox1";
			   this->groupBox1->Padding = System::Windows::Forms::Padding(4);
			   this->groupBox1->Size = System::Drawing::Size(441, 380);
			   this->groupBox1->TabIndex = 29;
			   this->groupBox1->TabStop = false;
			   this->groupBox1->Text = L"Voice playback control";
			   // 
			   // groupBox8
			   // 
			   this->groupBox8->Controls->Add(this->actual_volume);
			   this->groupBox8->Controls->Add(this->label9);
			   this->groupBox8->Controls->Add(this->label11);
			   this->groupBox8->Controls->Add(this->label10);
			   this->groupBox8->Controls->Add(this->volControl);
			   this->groupBox8->Location = System::Drawing::Point(27, 166);
			   this->groupBox8->Name = L"groupBox8";
			   this->groupBox8->Size = System::Drawing::Size(401, 68);
			   this->groupBox8->TabIndex = 40;
			   this->groupBox8->TabStop = false;
			   this->groupBox8->Text = L"Volume control";
			   // 
			   // actual_volume
			   // 
			   this->actual_volume->AllowDrop = true;
			   this->actual_volume->AutoSize = true;
			   this->actual_volume->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->actual_volume->Location = System::Drawing::Point(358, 19);
			   this->actual_volume->Name = L"actual_volume";
			   this->actual_volume->Size = System::Drawing::Size(32, 15);
			   this->actual_volume->TabIndex = 47;
			   this->actual_volume->Text = L"73%";
			   // 
			   // label9
			   // 
			   this->label9->AllowDrop = true;
			   this->label9->AutoSize = true;
			   this->label9->Location = System::Drawing::Point(17, 47);
			   this->label9->Name = L"label9";
			   this->label9->Size = System::Drawing::Size(31, 13);
			   this->label9->TabIndex = 46;
			   this->label9->Text = L"Mute";
			   // 
			   // label11
			   // 
			   this->label11->AllowDrop = true;
			   this->label11->AutoSize = true;
			   this->label11->Location = System::Drawing::Point(331, 47);
			   this->label11->Name = L"label11";
			   this->label11->Size = System::Drawing::Size(33, 13);
			   this->label11->TabIndex = 46;
			   this->label11->Text = L"100%";
			   // 
			   // label10
			   // 
			   this->label10->AllowDrop = true;
			   this->label10->AutoSize = true;
			   this->label10->Location = System::Drawing::Point(254, 47);
			   this->label10->Name = L"label10";
			   this->label10->Size = System::Drawing::Size(26, 13);
			   this->label10->TabIndex = 46;
			   this->label10->Text = L"0dB";
			   // 
			   // volControl
			   // 
			   this->volControl->AccessibleName = L"";
			   this->volControl->Location = System::Drawing::Point(20, 19);
			   this->volControl->Maximum = 67;
			   this->volControl->Name = L"volControl";
			   this->volControl->Size = System::Drawing::Size(339, 45);
			   this->volControl->TabIndex = 45;
			   this->volControl->Tag = L"";
			   this->volControl->TickFrequency = 10;
			   this->volControl->Value = 49;
			   this->volControl->ValueChanged += gcnew System::EventHandler(this, &MyForm::volControl_ValueChanged);
			   // 
			   // label12
			   // 
			   this->label12->AllowDrop = true;
			   this->label12->AutoSize = true;
			   this->label12->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 6.6F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label12->Location = System::Drawing::Point(2, 330);
			   this->label12->Name = L"label12";
			   this->label12->Size = System::Drawing::Size(15, 12);
			   this->label12->TabIndex = 41;
			   this->label12->Text = L"6.";
			   // 
			   // groupBox6
			   // 
			   this->groupBox6->Controls->Add(this->button_stop);
			   this->groupBox6->Controls->Add(this->button7);
			   this->groupBox6->Location = System::Drawing::Point(27, 237);
			   this->groupBox6->Margin = System::Windows::Forms::Padding(4);
			   this->groupBox6->Name = L"groupBox6";
			   this->groupBox6->Padding = System::Windows::Forms::Padding(4);
			   this->groupBox6->Size = System::Drawing::Size(401, 68);
			   this->groupBox6->TabIndex = 39;
			   this->groupBox6->TabStop = false;
			   this->groupBox6->Text = L"Play all phrases";
			   // 
			   // button_stop
			   // 
			   this->button_stop->AutoEllipsis = true;
			   this->button_stop->Enabled = false;
			   this->button_stop->Location = System::Drawing::Point(263, 21);
			   this->button_stop->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->button_stop->Name = L"button_stop";
			   this->button_stop->Size = System::Drawing::Size(132, 30);
			   this->button_stop->TabIndex = 18;
			   this->button_stop->Text = L"STOP";
			   this->button_stop->UseVisualStyleBackColor = true;
			   this->button_stop->Click += gcnew System::EventHandler(this, &MyForm::button_stop_Click);
			   // 
			   // groupBox5
			   // 
			   this->groupBox5->Controls->Add(this->textBox2);
			   this->groupBox5->Controls->Add(this->Buttn_SelectListFile);
			   this->groupBox5->Location = System::Drawing::Point(27, 23);
			   this->groupBox5->Margin = System::Windows::Forms::Padding(4);
			   this->groupBox5->Name = L"groupBox5";
			   this->groupBox5->Padding = System::Windows::Forms::Padding(4);
			   this->groupBox5->Size = System::Drawing::Size(401, 60);
			   this->groupBox5->TabIndex = 38;
			   this->groupBox5->TabStop = false;
			   this->groupBox5->Text = L"Select ListFile.txt first";
			   // 
			   // label6
			   // 
			   this->label6->AllowDrop = true;
			   this->label6->AutoSize = true;
			   this->label6->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 6.6F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label6->Location = System::Drawing::Point(2, 266);
			   this->label6->Name = L"label6";
			   this->label6->Size = System::Drawing::Size(15, 12);
			   this->label6->TabIndex = 30;
			   this->label6->Text = L"5.";
			   // 
			   // groupBox4
			   // 
			   this->groupBox4->Controls->Add(this->button4);
			   this->groupBox4->Controls->Add(this->button5);
			   this->groupBox4->Controls->Add(this->phrase_number);
			   this->groupBox4->Controls->Add(this->button4_play);
			   this->groupBox4->Location = System::Drawing::Point(27, 306);
			   this->groupBox4->Margin = System::Windows::Forms::Padding(4);
			   this->groupBox4->Name = L"groupBox4";
			   this->groupBox4->Padding = System::Windows::Forms::Padding(4);
			   this->groupBox4->Size = System::Drawing::Size(401, 63);
			   this->groupBox4->TabIndex = 36;
			   this->groupBox4->TabStop = false;
			   this->groupBox4->Text = L"Play selected phrase";
			   // 
			   // button4
			   // 
			   this->button4->Enabled = false;
			   this->button4->Location = System::Drawing::Point(20, 18);
			   this->button4->Margin = System::Windows::Forms::Padding(4);
			   this->button4->Name = L"button4";
			   this->button4->Size = System::Drawing::Size(75, 28);
			   this->button4->TabIndex = 34;
			   this->button4->Text = L"Previous";
			   this->button4->UseVisualStyleBackColor = true;
			   this->button4->Click += gcnew System::EventHandler(this, &MyForm::Previous_click);
			   // 
			   // button5
			   // 
			   this->button5->Enabled = false;
			   this->button5->Location = System::Drawing::Point(151, 18);
			   this->button5->Margin = System::Windows::Forms::Padding(4);
			   this->button5->Name = L"button5";
			   this->button5->Size = System::Drawing::Size(75, 28);
			   this->button5->TabIndex = 35;
			   this->button5->Text = L"Next";
			   this->button5->UseVisualStyleBackColor = true;
			   this->button5->Click += gcnew System::EventHandler(this, &MyForm::Next_Clik);
			   // 
			   // phrase_number
			   // 
			   this->phrase_number->AutoCompleteMode = System::Windows::Forms::AutoCompleteMode::Suggest;
			   this->phrase_number->AutoCompleteSource = System::Windows::Forms::AutoCompleteSource::HistoryList;
			   this->phrase_number->Enabled = false;
			   this->phrase_number->Location = System::Drawing::Point(103, 19);
			   this->phrase_number->Margin = System::Windows::Forms::Padding(4);
			   this->phrase_number->Name = L"phrase_number";
			   this->phrase_number->Size = System::Drawing::Size(39, 20);
			   this->phrase_number->TabIndex = 25;
			   this->phrase_number->TextChanged += gcnew System::EventHandler(this, &MyForm::phrase_number_TextChanged);
			   // 
			   // groupBox3
			   // 
			   this->groupBox3->Controls->Add(this->groupBox7);
			   this->groupBox3->Controls->Add(this->textBox_keycode);
			   this->groupBox3->Controls->Add(this->Buttn_KeyCode);
			   this->groupBox3->Location = System::Drawing::Point(27, 94);
			   this->groupBox3->Margin = System::Windows::Forms::Padding(4);
			   this->groupBox3->Name = L"groupBox3";
			   this->groupBox3->Padding = System::Windows::Forms::Padding(4);
			   this->groupBox3->Size = System::Drawing::Size(401, 65);
			   this->groupBox3->TabIndex = 33;
			   this->groupBox3->TabStop = false;
			   this->groupBox3->Text = L"Enter Keycode first";
			   // 
			   // groupBox7
			   // 
			   this->groupBox7->Location = System::Drawing::Point(4, 72);
			   this->groupBox7->Name = L"groupBox7";
			   this->groupBox7->Size = System::Drawing::Size(390, 44);
			   this->groupBox7->TabIndex = 25;
			   this->groupBox7->TabStop = false;
			   this->groupBox7->Text = L"groupBox7";
			   // 
			   // label7
			   // 
			   this->label7->AllowDrop = true;
			   this->label7->AutoSize = true;
			   this->label7->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 6.6F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label7->Location = System::Drawing::Point(7, 192);
			   this->label7->Name = L"label7";
			   this->label7->Size = System::Drawing::Size(15, 12);
			   this->label7->TabIndex = 26;
			   this->label7->Text = L"4.";
			   this->label7->Click += gcnew System::EventHandler(this, &MyForm::label7_Click);
			   // 
			   // groupBox2
			   // 
			   this->groupBox2->Controls->Add(this->button3);
			   this->groupBox2->Location = System::Drawing::Point(33, 36);
			   this->groupBox2->Margin = System::Windows::Forms::Padding(4);
			   this->groupBox2->Name = L"groupBox2";
			   this->groupBox2->Padding = System::Windows::Forms::Padding(4);
			   this->groupBox2->Size = System::Drawing::Size(412, 80);
			   this->groupBox2->TabIndex = 32;
			   this->groupBox2->TabStop = false;
			   this->groupBox2->Text = L"Voice data ROM erase and write";
			   this->groupBox2->Enter += gcnew System::EventHandler(this, &MyForm::groupBox2_Enter);
			   // 
			   // button3
			   // 
			   this->button3->Enabled = false;
			   this->button3->ImageAlign = System::Drawing::ContentAlignment::TopCenter;
			   this->button3->Location = System::Drawing::Point(260, 16);
			   this->button3->Margin = System::Windows::Forms::Padding(4);
			   this->button3->Name = L"button3";
			   this->button3->Size = System::Drawing::Size(132, 57);
			   this->button3->TabIndex = 34;
			   this->button3->Text = L"Select and Write to flash";
			   this->button3->UseVisualStyleBackColor = true;
			   this->button3->Click += gcnew System::EventHandler(this, &MyForm::button3_Click_1);
			   // 
			   // button6
			   // 
			   this->button6->Location = System::Drawing::Point(786, 136);
			   this->button6->Margin = System::Windows::Forms::Padding(4);
			   this->button6->Name = L"button6";
			   this->button6->Size = System::Drawing::Size(135, 28);
			   this->button6->TabIndex = 37;
			   this->button6->Text = L"button6";
			   this->button6->UseVisualStyleBackColor = true;
			   this->button6->Visible = false;
			   // 
			   // pictureBox1
			   // 
			   this->pictureBox1->Image = (cli::safe_cast<System::Drawing::Image^>(resources->GetObject(L"pictureBox1.Image")));
			   this->pictureBox1->Location = System::Drawing::Point(578, 52);
			   this->pictureBox1->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->pictureBox1->Name = L"pictureBox1";
			   this->pictureBox1->Size = System::Drawing::Size(345, 112);
			   this->pictureBox1->SizeMode = System::Windows::Forms::PictureBoxSizeMode::Zoom;
			   this->pictureBox1->TabIndex = 38;
			   this->pictureBox1->TabStop = false;
			   this->pictureBox1->Click += gcnew System::EventHandler(this, &MyForm::pictureBox1_Click);
			   // 
			   // label8
			   // 
			   this->label8->AllowDrop = true;
			   this->label8->AutoSize = true;
			   this->label8->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 10));
			   this->label8->Location = System::Drawing::Point(466, 53);
			   this->label8->Name = L"label8";
			   this->label8->Size = System::Drawing::Size(86, 17);
			   this->label8->TabIndex = 39;
			   this->label8->Text = L"Powered by:";
			   // 
			   // pictureBox3
			   // 
			   this->pictureBox3->Image = (cli::safe_cast<System::Drawing::Image^>(resources->GetObject(L"pictureBox3.Image")));
			   this->pictureBox3->Location = System::Drawing::Point(578, 170);
			   this->pictureBox3->Margin = System::Windows::Forms::Padding(4);
			   this->pictureBox3->Name = L"pictureBox3";
			   this->pictureBox3->Size = System::Drawing::Size(171, 70);
			   this->pictureBox3->SizeMode = System::Windows::Forms::PictureBoxSizeMode::Zoom;
			   this->pictureBox3->TabIndex = 41;
			   this->pictureBox3->TabStop = false;
			   // 
			   // pictureBox2
			   // 
			   this->pictureBox2->Image = (cli::safe_cast<System::Drawing::Image^>(resources->GetObject(L"pictureBox2.Image")));
			   this->pictureBox2->Location = System::Drawing::Point(750, 170);
			   this->pictureBox2->Margin = System::Windows::Forms::Padding(4);
			   this->pictureBox2->Name = L"pictureBox2";
			   this->pictureBox2->Size = System::Drawing::Size(171, 70);
			   this->pictureBox2->SizeMode = System::Windows::Forms::PictureBoxSizeMode::Zoom;
			   this->pictureBox2->TabIndex = 42;
			   this->pictureBox2->TabStop = false;
			   // 
			   // button8
			   // 
			   this->button8->Location = System::Drawing::Point(451, 83);
			   this->button8->Margin = System::Windows::Forms::Padding(4);
			   this->button8->Name = L"button8";
			   this->button8->Size = System::Drawing::Size(100, 28);
			   this->button8->TabIndex = 43;
			   this->button8->Text = L"Thread_start";
			   this->button8->UseVisualStyleBackColor = true;
			   this->button8->Visible = false;
			   // 
			   // backgroundWorker1
			   // 
			   this->backgroundWorker1->WorkerReportsProgress = true;
			   this->backgroundWorker1->WorkerSupportsCancellation = true;
			   this->backgroundWorker1->DoWork += gcnew System::ComponentModel::DoWorkEventHandler(this, &MyForm::backgroundWorker1_DoWork);
			   // 
			   // board_check
			   // 
			   this->board_check->AllowDrop = true;
			   this->board_check->AutoSize = true;
			   this->board_check->BackColor = System::Drawing::SystemColors::Control;
			   this->board_check->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->board_check->ForeColor = System::Drawing::Color::Red;
			   this->board_check->Location = System::Drawing::Point(85, 504);
			   this->board_check->Name = L"board_check";
			   this->board_check->Size = System::Drawing::Size(82, 15);
			   this->board_check->TabIndex = 44;
			   this->board_check->Text = L"Disconnected";
			   // 
			   // timer1
			   // 
			   this->timer1->Enabled = true;
			   this->timer1->Interval = 2000;
			   this->timer1->Tick += gcnew System::EventHandler(this, &MyForm::timer1_Tick);
			   // 
			   // label13
			   // 
			   this->label13->AllowDrop = true;
			   this->label13->AutoSize = true;
			   this->label13->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label13->Location = System::Drawing::Point(1, 504);
			   this->label13->Name = L"label13";
			   this->label13->Size = System::Drawing::Size(78, 15);
			   this->label13->TabIndex = 45;
			   this->label13->Text = L"Board status:";
			   // 
			   // epson_status
			   // 
			   this->epson_status->AllowDrop = true;
			   this->epson_status->AutoSize = true;
			   this->epson_status->BackColor = System::Drawing::SystemColors::Control;
			   this->epson_status->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->epson_status->ForeColor = System::Drawing::Color::Gray;
			   this->epson_status->Location = System::Drawing::Point(317, 504);
			   this->epson_status->Name = L"epson_status";
			   this->epson_status->Size = System::Drawing::Size(59, 15);
			   this->epson_status->TabIndex = 46;
			   this->epson_status->Text = L"Unknown";
			   // 
			   // label15
			   // 
			   this->label15->AllowDrop = true;
			   this->label15->AutoSize = true;
			   this->label15->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->label15->Location = System::Drawing::Point(197, 504);
			   this->label15->Name = L"label15";
			   this->label15->Size = System::Drawing::Size(114, 15);
			   this->label15->TabIndex = 47;
			   this->label15->Text = L"Mode switch status:";
			   // 
			   // button_SW_reset
			   // 
			   this->button_SW_reset->Location = System::Drawing::Point(453, 136);
			   this->button_SW_reset->Name = L"button_SW_reset";
			   this->button_SW_reset->Size = System::Drawing::Size(75, 23);
			   this->button_SW_reset->TabIndex = 48;
			   this->button_SW_reset->Text = L"SW_reset";
			   this->button_SW_reset->UseVisualStyleBackColor = true;
			   this->button_SW_reset->Visible = false;
			   this->button_SW_reset->Click += gcnew System::EventHandler(this, &MyForm::button_SW_reset_Click);
			   // 
			   // button_HW_reset
			   // 
			   this->button_HW_reset->Location = System::Drawing::Point(453, 177);
			   this->button_HW_reset->Name = L"button_HW_reset";
			   this->button_HW_reset->Size = System::Drawing::Size(75, 23);
			   this->button_HW_reset->TabIndex = 49;
			   this->button_HW_reset->Text = L"HW_reset";
			   this->button_HW_reset->UseVisualStyleBackColor = true;
			   this->button_HW_reset->Visible = false;
			   this->button_HW_reset->Click += gcnew System::EventHandler(this, &MyForm::button_HW_reset_Click);
			   // 
			   // MyForm
			   // 
			   this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::None;
			   this->AutoSizeMode = System::Windows::Forms::AutoSizeMode::GrowAndShrink;
			   this->ClientSize = System::Drawing::Size(933, 523);
			   this->Controls->Add(this->button_HW_reset);
			   this->Controls->Add(this->button_SW_reset);
			   this->Controls->Add(this->label15);
			   this->Controls->Add(this->epson_status);
			   this->Controls->Add(this->label13);
			   this->Controls->Add(this->board_check);
			   this->Controls->Add(this->button8);
			   this->Controls->Add(this->pictureBox2);
			   this->Controls->Add(this->pictureBox3);
			   this->Controls->Add(this->label8);
			   this->Controls->Add(this->pictureBox1);
			   this->Controls->Add(this->button2);
			   this->Controls->Add(this->button6);
			   this->Controls->Add(this->label5);
			   this->Controls->Add(this->label4);
			   this->Controls->Add(this->label3);
			   this->Controls->Add(this->label2);
			   this->Controls->Add(this->textBox_selectedRomfile);
			   this->Controls->Add(this->Buttn_WriteToFlash);
			   this->Controls->Add(this->label1);
			   this->Controls->Add(this->TxtBx_debug);
			   this->Controls->Add(this->button1);
			   this->Controls->Add(this->menuStrip1);
			   this->Controls->Add(this->groupBox2);
			   this->Controls->Add(this->groupBox1);
			   this->Icon = (cli::safe_cast<System::Drawing::Icon^>(resources->GetObject(L"$this.Icon")));
			   this->MainMenuStrip = this->menuStrip1;
			   this->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->MaximizeBox = false;
			   this->MaximumSize = System::Drawing::Size(949, 570);
			   this->MinimumSize = System::Drawing::Size(949, 516);
			   this->Name = L"MyForm";
			   this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			   this->Text = L"Rutronik Epson Tool";
			   this->Load += gcnew System::EventHandler(this, &MyForm::MyForm_Load);
			   this->menuStrip1->ResumeLayout(false);
			   this->menuStrip1->PerformLayout();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->eventLog1))->EndInit();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->bindingSource1))->EndInit();
			   this->groupBox1->ResumeLayout(false);
			   this->groupBox1->PerformLayout();
			   this->groupBox8->ResumeLayout(false);
			   this->groupBox8->PerformLayout();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->volControl))->EndInit();
			   this->groupBox6->ResumeLayout(false);
			   this->groupBox5->ResumeLayout(false);
			   this->groupBox5->PerformLayout();
			   this->groupBox4->ResumeLayout(false);
			   this->groupBox4->PerformLayout();
			   this->groupBox3->ResumeLayout(false);
			   this->groupBox3->PerformLayout();
			   this->groupBox2->ResumeLayout(false);
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox1))->EndInit();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox3))->EndInit();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox2))->EndInit();
			   this->ResumeLayout(false);
			   this->PerformLayout();

		   }
#pragma endregion

		//   System::ComponentModel::BackgroundWorker^ backgroundWorker1;
	
	private: System::Void button1_Click(System::Object^ sender, System::EventArgs^ e) {
		Application::Exit();
	}
	private: System::Void exitToolStripMenuItem_Click(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void aboutToolStripMenuItem_Click(System::Object^ sender, System::EventArgs^ e) {

	}

	public: System::Void button2_Click(System::Object^ sender, System::EventArgs^ e) {
		int deviceIndexAtSCB0 = FindDeviceAtSCB0();
		if (deviceIndexAtSCB0 >= 0)
		{
			OpenFileDialog^ openFileDialog1 = gcnew OpenFileDialog();
			openFileDialog1->Filter = "All Files (*.*)|*.*";
			openFileDialog1->FilterIndex = 1;
			openFileDialog1->Multiselect = false;
			if (openFileDialog1->ShowDialog() == System::Windows::Forms::DialogResult::OK)
			{
				textBox_selectedRomfile->AppendText(openFileDialog1->SafeFileName);
				TxtBx_debug->AppendText("\n Loaded ROM file:" + (openFileDialog1->SafeFileName));
				
			}
			else
			{
				TxtBx_debug->AppendText("\n ROM file not selected!");
				textBox_selectedRomfile->Clear();
			}
		}
		else
		{
			TxtBx_debug->AppendText("No device found!");
		}
	}


	private: System::Void button3_Click(System::Object^ sender, System::EventArgs^ e) {

		//-----play one phrase from Flash -------------
		int deviceIndexAtSCB0 = FindDeviceAtSCB0();
		timer1->Enabled = FALSE;  //disable board check during play 
		//Open the device at index deviceIndexAtSCB0
		if (deviceIndexAtSCB0 >= 0)
		{
			
			if (epsonStatus)
			{

				//---Play selected phrase--update aucIscSequencerConfigReq-----
				TxtBx_debug->AppendText("Play " + (cSignal)+" phrase\n" + "\r\n");
				aucIscSequencerConfigReq[16] = cSignal - 1;
				button3->Enabled = FALSE;
				Buttn_SelectListFile->Enabled = FALSE;
				Buttn_KeyCode->Enabled = FALSE;
				button_stop->Enabled = FALSE;
				button7->Enabled = FALSE;
				button4_play->Enabled = FALSE;
				button4->Enabled = FALSE;
				button5->Enabled = FALSE;
				phrase_number->Enabled = FALSE;
				volControl->Enabled = FALSE;
				Sleep(50);
				int status = EpsonDemo(deviceIndexAtSCB0);
				if (status == 0)
				{
					TxtBx_debug->AppendText("Play finnished.\r\n");
					CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
					timer1->Enabled = TRUE;  //resume board status check 
				}
				else
				{
					TxtBx_debug->AppendText("Play failed.\r\n");
					CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
					timer1->Enabled = TRUE;  //resume board status check 
				}
				
				button3->Enabled = TRUE;
				Buttn_SelectListFile->Enabled = TRUE;
				Buttn_KeyCode->Enabled = TRUE;
				button_stop->Enabled = TRUE;
				button7->Enabled = TRUE;
				button4_play->Enabled = TRUE;
				button4->Enabled = TRUE;
				button5->Enabled = TRUE;
				phrase_number->Enabled = TRUE;
				volControl->Enabled = TRUE;
				Sleep(50);

			} //cyNumDevices > 0 && cyReturnStatus == CY_SUCCESS
			else
			{
				TxtBx_debug->AppendText("No Epson IC found. Check mode switch config!\r\n");
			}
		}
			
		else {
			TxtBx_debug->AppendText("No device found. Please connect device first!" + "\r\n");
		}
		CyClose(&cyHandle);
	}


	private: System::Void button7_Click(System::Object^ sender, System::EventArgs^ e) {
		button_stop->Enabled = "True";
		int deviceIndexAtSCB0 = FindDeviceAtSCB0();

		//Open the device at index deviceIndexAtSCB0
		if (deviceIndexAtSCB0 >= 0)
		{
			TxtBx_debug->AppendText("Play ALL started...\r\n");
			int status;
			for (int i = 0; i <= SignalCount; i++) {
				aucIscSequencerConfigReq[16] = i;
				status = EpsonDemo(deviceIndexAtSCB0);
			}
			if (status == 0)
			{
				TxtBx_debug->AppendText("Demo succeeded." + "\r\n");
				CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
			}
			else
			{
				TxtBx_debug->AppendText("Demo failed." + "\r\n");
				CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
			}
		} //cyNumDevices > 0 && cyReturnStatus == CY_SUCCESS
		else 
		{
			TxtBx_debug->AppendText("No device found. Please connect device first!\r\n");
		}

	}
	private: System::Void label1_Click(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void menuStrip1_ItemClicked(System::Object^ sender, System::Windows::Forms::ToolStripItemClickedEventArgs^ e) {
	}

	private: System::Void Buttn_SelectListFile_Click(System::Object^ sender, System::EventArgs^ e) {


		OpenFileDialog^ openFileDialog1 = gcnew OpenFileDialog();
		openFileDialog1->Filter = "txt Files (*.txt*)|*.txt*";
		openFileDialog1->FileName = "ListFile.txt";
		openFileDialog1->FilterIndex = 1;
		openFileDialog1->Multiselect = false;
		SignalCount = 0;
		//cSignal = 1;
		if (openFileDialog1->ShowDialog() == System::Windows::Forms::DialogResult::OK)
		{
			String^ fileName = gcnew String(openFileDialog1->FileName);

			try
			{
				StreamReader^ din = File::OpenText(fileName);
				String^ str;
				while ((str = din->ReadLine()) != nullptr)
				{
					SignalCount++;
				}
				textBox2->Text = openFileDialog1->SafeFileName;
				cSignal = SignalCount;
				TxtBx_debug->AppendText(cSignal + " phrase loaded\r\n");
				TxtBx_debug->AppendText("Please enter the keycode\r\n");
				phrase_number->Text = (cSignal + "");

				din->Close();

				Buttn_KeyCode->Enabled = TRUE;
				textBox_keycode->Enabled = TRUE;
			}
			catch (Exception^ e)
			{
				if (dynamic_cast<FileNotFoundException^>(e))
					TxtBx_debug->AppendText("File '{0}' not found" + fileName+"\r\n");
				else
					TxtBx_debug->AppendText("Exception: ({0})" + e);
				SignalCount = 0;
				textBox2->Clear();
			}

		}
		else
		{
		TxtBx_debug->AppendText("List file not selected!\r\n");
		textBox_selectedRomfile->Clear();
		}
		
	}
	private: System::Void Buttn_KeyCode_Click(System::Object^ sender, System::EventArgs^ e) {


		String^ Key = textBox_keycode->Text;
		String^ key1;
		String^ key2;
		String^ key3;
		String^ key4;

		if (Key->Length != 8)
		{
			TxtBx_debug->AppendText("Incorrect Keycode. Please check." + "\r\n");
		}
		else
		{
			key1 = Key->Substring(0, 2);
			int num1 = Convert::ToInt32(key1, 16);
			key2 = Key->Substring(2, 2);
			int num2 = Convert::ToInt32(key2, 16);
			key3 = Key->Substring(4, 2);
			int num3 = Convert::ToInt32(key3, 16);
			key4 = Key->Substring(6, 2);
			int num4 = Convert::ToInt32(key4, 16);
			aucIscTestReq[10] = num4;
			aucIscTestReq[11] = num3;
			aucIscTestReq[12] = num2;
			aucIscTestReq[13] = num1;
			TxtBx_debug->AppendText("Registered Keycode:" + Key + "\r\n");
			button7->Enabled = "True";
			button4_play->Enabled = "True";
			button4->Enabled = "True";
			button5->Enabled = "True";
			phrase_number->Enabled = "True";
		}

	}
	private: System::Void button2_Click_1(System::Object^ sender, System::EventArgs^ e) {
		if (MessageBox::Show("ALL flash content will be DESTROYED.  Press 'YES' to continue, 'NO' to abort operation. ", "FLASH MEMORY ERASE WARNING!\r\n", MessageBoxButtons::YesNoCancel) == System::Windows::Forms::DialogResult::Yes)
		{
			TxtBx_debug->AppendText("Button 'Yes' selected\r\n");
			timer1->Enabled = FALSE;			// stop board status check			
			int deviceIndexAtSCB0 = FindDeviceAtSCB0();
			if (deviceIndexAtSCB0 >= 0)
			{
				if (epsonStatus)
				{
					TxtBx_debug->AppendText("Flash erase takes  1 minute, please wait....\r\n");
					int status = EpsonChipErase(deviceIndexAtSCB0); // Erase entire Flash chip
					TxtBx_debug->AppendText("Erase finished\r\n");
					if (status == 0)
					{
						TxtBx_debug->AppendText("Flash erased successfully \r\n");
						/*Hard reset after flashing*/
						CySetGpioValue(cyHandle, RESET_GPIO, 0);
						Sleep(100);
						CySetGpioValue(cyHandle, RESET_GPIO, 1);
						Sleep(500);
						timer1->Enabled = TRUE;			// resume board status check	
					}
					else
					{
						TxtBx_debug->AppendText("Erase failed\r\n");
						timer1->Enabled = TRUE;			// resume board status check	
					}
				}
				else
				{
					TxtBx_debug->AppendText("No Epson IC found. Check mode switch config!\r\n");
				}
			}
			else
			{
				TxtBx_debug->AppendText("No device found!\r\n");
			}
		}
		else
		{
			TxtBx_debug->AppendText("Operation canceled \r\n");
		}
		CyClose(&cyHandle);
	}
	private: System::Void MyForm_Load(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void button4_Click(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void Buttn_WriteToFlash_Click(System::Object^ sender, System::EventArgs^ e) {
		int deviceIndexAtSCB0 = FindDeviceAtSCB0();
		if (deviceIndexAtSCB0 >= 0)
		{
			CY_DATA_BUFFER cyDatabufferWrite;
			unsigned char zwbuffer[256];

			unsigned short	usReceivedMessageID;
			int interfaceNum = 0;
			int deviceIndexAtSCB0 = FindDeviceAtSCB0();
			rStatus = CyOpen(deviceIndexAtSCB0, interfaceNum, &cyHandle);
			EpsonSendMessage(aucIscSpiSwitch, &usReceivedMessageID);
			Sleep(1);
			try
			{
				String^ fileName = gcnew String(openFileDialog1->FileName);
				FileStream^ fs = gcnew FileStream(fileName, FileMode::Open, FileAccess::Read);
				BinaryReader^ br = gcnew BinaryReader(fs);
				TxtBx_debug->AppendText("\ncontent lenght: " + br->BaseStream->Length.ToString());
				UINT addr = br->BaseStream->Position;
				TxtBx_debug->AppendText("\nStarting write to flash.\r\n");
				int status = 0;
				while (addr < br->BaseStream->Length)
				{
					memset(zwbuffer, 0x00, 256);
					int j = 0;
					for (unsigned int i = addr; i < (addr + 255); i++)
					{
						zwbuffer[j] = br->BaseStream->ReadByte();
						j++;
					}

					memset(zwbuffer, 0x00, 256);
					cyDatabufferWrite.buffer = zwbuffer;
					cyDatabufferWrite.length = 256; //4 bytes command + 256 bytes page size

					status = NORFlashWritePage(cyHandle, &cyDatabufferWrite, addr);
					addr = addr + 256;
				}
				TxtBx_debug->AppendText("\n last read addr: " + addr + "\r\n");
				TxtBx_debug->AppendText("\n" + br->BaseStream->ReadByte().ToString("X2") + "\r\n");
				TxtBx_debug->AppendText("\n Write to flash completed.\r\n");
				fs->Close();
			}
			catch (Exception^ e)
			{
				if (dynamic_cast<FileNotFoundException^>(e))
					TxtBx_debug->AppendText("File '{0}' not found" + (openFileDialog1->FileName)+"\r\n");
				else
					TxtBx_debug->AppendText("Exception: ({0})" + e + "\r\n");
			}
			TxtBx_debug->AppendText("Select ROM file first\r\n");
		}
		else
		{
			TxtBx_debug->AppendText("No device found!\r\n");
		}
	}
	private: System::Void bindingSource1_CurrentChanged(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void button5_Click(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void label2_Click(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void label5_Click(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void exitToolStripMenuItem_Click_1(System::Object^ sender, System::EventArgs^ e) {
		Application::Exit();
	}
	private: System::Void textBox_keycode_TextChanged(System::Object^ sender, System::EventArgs^ e) {

	}
	private: System::Void label7_Click(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void aboutToolStripMenuItem_Click_1(System::Object^ sender, System::EventArgs^ e) {	
		
		MyForm1^ form = gcnew MyForm1;
		form->ShowDialog();
		form->BringToFront(); 
	}
	private: System::Void button3_Click_1(System::Object^ sender, System::EventArgs^ e) {

		if (MessageBox::Show("Further operations will erase current content of the flash memory  Press 'YES' to continue, 'NO' to abort operation. ", "FLASH MEMORY ERASE WARNING!\r\n", MessageBoxButtons::YesNoCancel) == System::Windows::Forms::DialogResult::Yes)
		{
		//	TxtBx_debug->AppendText("Button Yes selected\r\n");

			int deviceIndexAtSCB0 = FindDeviceAtSCB0();
			UINT8 value = 0;
			unsigned short	usReceivedMessageID;
		
			if (deviceIndexAtSCB0 >= 0)
			{ 
				timer1->Enabled = FALSE;	// stop board status check

				//TxtBx_debug->AppendText("stop timer1 \r\n");
				unsigned char zwbuffer[256];
				unsigned short	usReceivedMessageID;
				int interfacenum = 0;
			//	int deviceIndexAtSCB0 = FindDeviceAtSCB0();
				int eVal = 0;

				rStatus = CyOpen(deviceIndexAtSCB0, interfacenum, &cyHandle);
								
				if (epsonStatus)
				{				


					openFileDialog1->Filter = "Bin Files (*.bin*)|*.bin*";
					openFileDialog1->FilterIndex = 1;
					openFileDialog1->FileName = "ROMImage_xxxxxx_xxxxxx.bin";
					openFileDialog1->Multiselect = false;

					if (openFileDialog1->ShowDialog() == System::Windows::Forms::DialogResult::OK)
					{
											EpsonSendMessage(aucIscSpiSwitch, &usReceivedMessageID);
					Sleep(1);
						String^ fileName = gcnew String(openFileDialog1->FileName);
						textBox_selectedRomfile->Text = openFileDialog1->SafeFileName;
						TxtBx_debug->AppendText("Loaded ROM file:" + (openFileDialog1->SafeFileName) + "\r\n");

						try
						{
							FileStream^ fs = gcnew FileStream(fileName, FileMode::Open, FileAccess::Read);
							BinaryReader^ br = gcnew BinaryReader(fs);

							TxtBx_debug->AppendText("\r\ncontent lenght: " + br->BaseStream->Length.ToString() + "\r\n ");
							UINT addr = br->BaseStream->Position;
							int cblock = (((br->BaseStream->Length) / 1024) / 64 + 1);
							int status = 0;
							TxtBx_debug->AppendText("Erasing " + cblock + " block's" + "\r\n ");
							status = -1;
							for (int i = 0; i < cblock; i++)
							{
								status = NORFlashBlockErase(cyHandle, addr);
								Sleep(10);
								//	TxtBx_debug->AppendText("\r\nstatus"+status);
								TxtBx_debug->AppendText(".");
								addr = addr + 65535;

							}
							if (status == 0)
							{
								TxtBx_debug->AppendText("Flash erased succesfully\r\n");
							}
							else
							{
								TxtBx_debug->AppendText("Flash erasing failed\r\n");
							}

							TxtBx_debug->AppendText("Starting write to flash, please wait\r\n");
							status = 0;
							addr = 0;
							while (addr < br->BaseStream->Length)
							{
								int j = 0;
								int printcounter = 0;
								for (unsigned int i = addr; i <= (addr + 255); i++)
								{
									zwbuffer[j] = br->BaseStream->ReadByte();
									j++;
								}

								cyDatabufferWrite.buffer = zwbuffer;
								cyDatabufferWrite.length = 256; //4 bytes command + 256 bytes page size
								status = NORFlashWritePage(cyHandle, &cyDatabufferWrite, addr);
								addr = addr + 256;

							}
							TxtBx_debug->AppendText("\n last read addr: " + addr + "\r\n");
							if (status == 0)
							{
								TxtBx_debug->AppendText("write to flash completed succesfully\r\n");
								// Reset after write to flash before play procedure//
								TxtBx_debug->AppendText("To play please first select the List file \r\n");

								CySetGpioValue(cyHandle, RESET_GPIO, 1);
								Sleep(100);
								CySetGpioValue(cyHandle, RESET_GPIO, 0);
								Sleep(100);
								CySetGpioValue(cyHandle, RESET_GPIO, 1);
								Sleep(500);

							}
							else
							{
								TxtBx_debug->AppendText("\nNORFlashWritePage respond :" + status + "\r\n");
							}

							/*Hard-Reset Procedure*/


							fs->Close();
						}
						catch (Exception^ e)
						{
							if (dynamic_cast<FileNotFoundException^>(e))
								TxtBx_debug->AppendText("File '{0}' not found" + fileName + "\r\n");
							else
								TxtBx_debug->AppendText("Exception: ({0})" + e + "\r\n");

						}
					}

					else
					{
						TxtBx_debug->AppendText("ROM file not selected!\r\n");
						textBox_selectedRomfile->Clear();
					}

				}
				else
				{
				TxtBx_debug->AppendText("No Epson IC found. Check mode switch config!\r\n");
				}
			}
			else
			{
				TxtBx_debug->AppendText("No device found!\r\n");
			}
		}
		else
		{
			TxtBx_debug->AppendText("Operation canceled \r\n");

		}

		CyClose(&cyHandle);
		timer1->Enabled = TRUE;			// resume board status check

	}

	private: System::Void groupBox2_Enter(System::Object^ sender, System::EventArgs^ e) {
	}
	private: System::Void Previous_click(System::Object^ sender, System::EventArgs^ e) {
		if (SignalCount != 0) {
			if (cSignal == 1) {
				cSignal = SignalCount;
			}
			else {
				cSignal--;
			}
			phrase_number->Text = (cSignal + "");
		}
	}
	private: System::Void Next_Clik(System::Object^ sender, System::EventArgs^ e) {
		if (SignalCount != 0) {
			if (cSignal == SignalCount) {
				cSignal = 1;
			}
			else {
				cSignal++;
			}
			phrase_number->Text = (cSignal + "");
		}
	}
	private: System::Void phrase_number_TextChanged(System::Object^ sender, System::EventArgs^ e) {
		if (phrase_number->Text == "")
		{

		}
		else {
			if ((UINT32::Parse(phrase_number->Text) > SignalCount) || (UINT32::Parse(phrase_number->Text) < 1)) {
				cSignal = UINT32::Parse(phrase_number->Text);
			}
			else {
				cSignal = UINT32::Parse(phrase_number->Text);
			}
		}
	}
	private: System::Void textBox2_TextChanged(System::Object^ sender, System::EventArgs^ e) {
	}

		   int PlayOneSellected(int i, int deviceIndexAtSCB0, BackgroundWorker^ worker, DoWorkEventArgs^ e) {
			   int result = 0;
			   unsigned short	usReceivedMessageID;
			   int	iError = 0;
			   UINT8 value = 0;

			   if (worker->CancellationPending)
			   {
				   e->Cancel = true;
				   return 0;
				 /*  EpsonSendMessage(aucIscSequencerStopReq, &usReceivedMessageID);

				   CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
				   if (value)
				   {
					   iError = EpsonReceiveMessage(&usReceivedMessageID);
					   if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_STOP_RESP)
					   {
						   printf("\nSequencer termination failed.\n");
						   return -1;
						   TxtBx_debug->AppendText("usReceivedMessageID="+ usReceivedMessageID + "\r\n");
					   }
				   }
				   TxtBx_debug->AppendText("Stop command 1\r\n");

				   EpsonSendMessage(aucIscSequencerStopReq, &usReceivedMessageID);

				   CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
				   if (value)
				   {
					   iError = EpsonReceiveMessage(&usReceivedMessageID);
					   if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_STOP_RESP)
					   {
						   TxtBx_debug->AppendText("usReceivedMessageID_1=" + usReceivedMessageID + "\r\n");
						   printf("\nSequencer termination failed.\n");
						   return -1;
					   }
				   }
				   TxtBx_debug->AppendText("Stop command 2\r\n");
				   */
				   Sleep(100);
				   timer1->Enabled = TRUE;
			   }
			   else
			   {   
				   if (deviceIndexAtSCB0 >= 0){
						aucIscSequencerConfigReq[16] = i;
						result = EpsonDemo(deviceIndexAtSCB0);
					}
					if (result == 0){
					    CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
					 // TxtBx_debug->AppendText("epsondemo result="+result + "\n");
					}
					else
					   {
						   CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
						// TxtBx_debug->AppendText("epsondemo result=" + result + "\n");
					   }
				   }
			    return result;
		   }


		   int PlayAllSequence(BackgroundWorker^ worker, DoWorkEventArgs^ e) {
			   int result = 0;
			   if (worker->CancellationPending)
			   {
				   e->Cancel = true;
			   }
			   else
			   {
				   button_stop->Enabled = "True";
				   int deviceIndexAtSCB0 = FindDeviceAtSCB0();

				   //Open the device at index deviceIndexAtSCB0
				   if (deviceIndexAtSCB0 >= 0)
				   {
					   int status;
					   for (int i = 0; i < SignalCount; i++) {
						   aucIscSequencerConfigReq[16] = i;
						   status = EpsonDemo(deviceIndexAtSCB0);
						   result = i;
					   }
					   if (status == 0)
					   {
						   //TxtBx_debug->AppendText("Demo succeeded." + "\n");
						   CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
					   }
					   else
					   {
						   //TxtBx_debug->AppendText("Demo failed." + "\n");
						   CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
					   }
				   }
			   }
			   return result;
}
		   void backgroundWorker1_DoWork(Object^ sender, DoWorkEventArgs^ e)
		   {
			   BackgroundWorker^ worker = dynamic_cast<BackgroundWorker^>(sender);

			   button_stop->Enabled = TRUE;
			   button7->Enabled = FALSE;

			   int deviceIndexAtSCB0 = FindDeviceAtSCB0();
			   int status = 0;
		



			  // return CY_SUCCESS;

			   if (deviceIndexAtSCB0 >= 0)
			   {
				   for (int i = 0; i < SignalCount; i) {
					   status = PlayOneSellected(i, deviceIndexAtSCB0, worker, e);
					   i++;
				   }
				   if (status == 0)
				   {
					   CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
					  // button7->Text = "Play ALL";
					//   MessageBox::Show("Play all phrases completed! ");
				   }
				   else
				   {
					   CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
					 //  button7->Text = "Play ALL";
					   MessageBox::Show("Play all failed!", "Information", MessageBoxButtons::OK);
					   
				   }
			   }
			   else {
				   MessageBox::Show("No device found. Please connect device first!", "No device found.", MessageBoxButtons::OK);
				   //TxtBx_debug->AppendText("No device found");
			   }
		   }
//-------------------------------------------------------------//

	private: System::Void button_playall_Click(System::Object^ sender, System::EventArgs^ e)
	{
		timer1->Enabled = FALSE;
		TxtBx_debug->AppendText("Play all started" +"\r\n");
		button3->Enabled = FALSE;
		Buttn_SelectListFile->Enabled = FALSE;
		Buttn_KeyCode->Enabled = FALSE;
		button7->Enabled = FALSE;
		button4_play->Enabled = FALSE;
		button4->Enabled = FALSE;
		button5->Enabled = FALSE;
		phrase_number->Enabled = FALSE;
		volControl->Enabled = FALSE;
			
		if (backgroundWorker1->IsBusy != true)
			{
				button_stop->Enabled = "True";
				backgroundWorker1->RunWorkerAsync();
			}
	}

	private: System::Void pictureBox1_Click(System::Object^ sender, System::EventArgs^ e) {
		System::Diagnostics::Process::Start("www.rutronik.com");
	}
	
	   private: System::Void backgroundWorker1_ProgressChanged(System::Object^ sender, System::ComponentModel::ProgressChangedEventArgs^ e) {

	   }
	   private: System::Void backgroundWorker1_RunWorkerCompleted(System::Object^ sender, System::ComponentModel::RunWorkerCompletedEventArgs^ e) {
		 
		   timer1->Enabled = TRUE;
		   button7->Enabled = TRUE;
		   button3->Enabled = TRUE;
		   Buttn_SelectListFile->Enabled = TRUE;
		   Buttn_KeyCode->Enabled = TRUE;
		   button4->Enabled = TRUE;
		   button5->Enabled = TRUE;
		   phrase_number->Enabled = TRUE;
		   volControl->Enabled = TRUE;

		   if (e->Cancelled)    //Messages for the events
		   {
			   MessageBox::Show("Play sequence was interrupted by user");
			   button4_play->Enabled = TRUE;
			   button_stop->Enabled = FALSE;
			   CyClose(&cyHandle);

		   }
		   else
		   {
			   MessageBox::Show("Play all phrases completed! ");
			   button_stop->Enabled = FALSE;
			   button4_play->Enabled = TRUE;
			  CyClose(&cyHandle);
		   }
	   }
private: System::Void button_stop_Click(System::Object^ sender, System::EventArgs^ e) {
	button_stop->Enabled = FALSE;
	if (stBlock == 0)
	{
		stBlock = 1;
		unsigned short	usReceivedMessageID;
		UINT8 value = 0;
		this->backgroundWorker1->CancelAsync();
		button7->Enabled = TRUE;
		CySetGpioValue(cyHandle, MUTE_GPIO, 0);
		stBlock = 0;
		
	}
}
private: System::Void volControl_ValueChanged(System::Object^ sender, System::EventArgs^ e) {
	aucIscAudioConfigReq[7] = volControl->Value;
	
	UINT cVal= (volControl->Value);
	if (cVal == 0)
	{
		actual_volume->Text = "Mute";
	}
	else
	{
		cVal = cVal * 100 / volControl->Maximum ;

		actual_volume->Text = "" + (cVal)+"%";
	}
	
}

private: System::Void timer1_Tick(System::Object^ sender, System::EventArgs^ e) {
		int deviceIndexAtSCB0 = FindDeviceAtSCB0();

	if (deviceIndexAtSCB0 >= 0)
	{
		board_check->Text = "Connected!";
		board_check->ForeColor = Color::Green;
		EpsonCheck();
		Sleep(500);
		if (epsonStatus)
		{
			
			epson_status->Text = "USB";
			epson_status->ForeColor = Color::Green;
			
			/*enable menu items if mode switch is in usb position*/
			button3->Enabled = TRUE;
			Buttn_SelectListFile->Enabled = TRUE;
			//Buttn_KeyCode->Enabled = TRUE;
			eraseALLFlashToolStripMenuItem->Enabled = TRUE;
			volControl->Enabled = TRUE;
			cnt =1; 
			this->timer1->Interval = 2000;
			if (once==0)
			{
			
			TxtBx_debug->AppendText("RutAdaptBoard-TextToSpeech board is initializing");
			for (int i = 0; i <= 3; i++)
			{
				TxtBx_debug->AppendText(". ");
				Sleep(150);
			}
			Sleep(300);
			TxtBx_debug->AppendText("\r\n"+"The board is ready \r\n");
			once = 1;
			nb = 0; //
			}
		
		}

		else
		{
			epson_status->Text = "Arduino";
			epson_status->ForeColor = Color::Red;
			epsonStatus = 0; //disable 
			/*disable menu items if mode switch is in Arduino position*/
			button3->Enabled = FALSE;
			Buttn_SelectListFile->Enabled = FALSE;
			eraseALLFlashToolStripMenuItem->Enabled = FALSE;
			volControl->Enabled = FALSE;
		
			button_stop->Enabled = FALSE;
			Buttn_KeyCode->Enabled = FALSE;
			button7->Enabled = FALSE;
			button4_play->Enabled = FALSE;
			button4->Enabled = FALSE;
			button5->Enabled = FALSE;
			phrase_number->Enabled = FALSE;
			//volControl->Enabled = FALSE;

			//this->timer1->Interval = 3000;
			once = 0; //for initializing board only ocne after it has been connected
			
			if (cnt)
			{	
				TxtBx_debug->AppendText("Please disconnect the board and change mode switch to USB mode!\r\n");
				cnt = 0;
				
			
			}
			
		}
		
	}
	else
	{
		board_check->Text = "Disconected";
		board_check->ForeColor = Color::Red;
		epson_status->Text = "Unknown";
		epson_status->ForeColor = Color::Gray;
		button3->Enabled = FALSE;
		Buttn_SelectListFile->Enabled = FALSE;

		button_stop->Enabled = FALSE;
		Buttn_KeyCode->Enabled = FALSE;
		button7->Enabled = FALSE;
		button4_play->Enabled = FALSE;
		button4->Enabled = FALSE;
		button5->Enabled = FALSE;
		phrase_number->Enabled = FALSE;
		volControl->Enabled = FALSE;

		once = 0;
		if (nb == 1)
		{
			TxtBx_debug->AppendText("Please connect RutAdaptBoard-TextToSpeech board first!\r\n");
			nb = 0;
			Sleep(300);
		}
	}
	
}
private: System::Void button_SW_reset_Click(System::Object^ sender, System::EventArgs^ e) {
	
	
	
	TxtBx_debug->AppendText("Board SW reset " + "\r\n");
	int deviceIndexAtSCB0 = FindDeviceAtSCB0();
	int interfacenum =0;
	UINT8 gvalue = 0;
	unsigned short	usReceivedMessageID;
	
	CyOpen(deviceIndexAtSCB0, interfacenum, &cyHandle);
	EpsonSendMessage(aucIscTestReq, &usReceivedMessageID);
	//Sleep(10); 
		

	CyGetGpioValue(cyHandle, MSGRDY_GPIO, &gvalue);
	TxtBx_debug->AppendText("MSGRDY="+gvalue+"\r\n");
	EpsonReceiveMessage(&usReceivedMessageID);
	Sleep(500); 
	gvalue = 0;
	CyClose(&cyHandle);

}
private: System::Void button_HW_reset_Click(System::Object^ sender, System::EventArgs^ e) {
	int deviceIndexAtSCB0 = FindDeviceAtSCB0();
	UINT8 value = 0;
	unsigned short	usReceivedMessageID;


	if (deviceIndexAtSCB0 >= 0)
	{

		//unsigned char zwbuffer[256];
		unsigned short	usReceivedMessageID;
		int interfacenum = 0;
		int deviceIndexAtSCB0 = FindDeviceAtSCB0();
		int eVal = 0;
		int rdystatus = 10;
		int iError = 0;
		int iiError = 0;

		rStatus = CyOpen(deviceIndexAtSCB0, interfacenum, &cyHandle);
	
		CySetGpioValue(cyHandle, RESET_GPIO, 1);
		Sleep(100);
		CySetGpioValue(cyHandle, RESET_GPIO, 0);
		Sleep(100);
		CySetGpioValue(cyHandle, RESET_GPIO, 1);
		Sleep(500);

		/*Set mute signal(MUTE) to Low(enable)*/
		CySetGpioValue(cyHandle, MUTE_GPIO, 1);

		/*Set stanby signal(STBYEXIT) to Low(deassert)*/
		CySetGpioValue(cyHandle, STBY_GPIO, 0);
		Sleep(100);

		/*Send ISC_RESET_REQ*/

		//printf("\nRestarting the IC.\n");
	//	EpsonSendMessage(aucIscResetReq, &usReceivedMessageID);
		EpsonSendMessage(aucIscTestReq, &usReceivedMessageID);
		Sleep(10); //gain_fix_test (10)
		
		CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
		//if (value)
		//{
		//	TxtBx_debug->AppendText("MSGRDY=1\r\n");
			iError = EpsonReceiveMessage(&usReceivedMessageID);
		//	if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_RESET_RESP)
		//	{
			TxtBx_debug->AppendText("reset failed\r\n");
		//		
		//	}
		value = 0;
		CyClose(&cyHandle);

	}
	else
	{
		TxtBx_debug->AppendText("No USB device found!\r\n");
	}
}

int EpsonCheck()
{
	int deviceIndexAtSCB0 = FindDeviceAtSCB0();
	int interfacenum = 0;
	UINT8 value = 1;
	UINT8 a = 1;
	int	Error = 0;
	unsigned short	usReceivedMessageID;

	CyOpen(deviceIndexAtSCB0, interfacenum, &cyHandle);
	EpsonSendMessage(aucIscResetReq, &usReceivedMessageID);
	CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
	if (value)
	{
		Error = EpsonReceiveMessage(&usReceivedMessageID);

			if (Error < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_RESET_RESP)
			{	
				printf("\nRestart failed.\n");
				TxtBx_debug->AppendText("Error= " + Error + "\r\n");  //print error code in 
				TxtBx_debug->AppendText("Please disconnect the board and change mode switch to USB mode!\r\n");  //print error code in window
				this->timer1->Interval = 7000;
				a = 0;
				return -1;
			//	Sleep(1000);
				epsonStatus = 0;
				CyClose(&cyHandle);
				
			}
	}

	if (value)
	{	
		epsonStatus = 1;
		return 1;
		value = 0;
		a = 1;
	}
		
	else {
		epsonStatus = 0;
		return 0;
	}
}
};
}

