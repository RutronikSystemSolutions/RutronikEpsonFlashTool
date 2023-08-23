#pragma once
button_stop->Enabled = "True";
	unsigned short	usReceivedMessageID;

	int deviceIndexAtSCB0 = FindDeviceAtSCB0();
	int rStatus;

	//Open the device at index deviceIndexAtSCB0
	if (deviceIndexAtSCB0 >= 0)
	{
		TxtBx_debug->AppendText("Play ALL started...\r\n");
		int status;
		stop = 0;

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

		rStatus = CyOpen(deviceIndexAtSCB0, interfaceNum, &cyHandle);

		//sprintf(deviceNumber);

		if (rStatus != CY_SUCCESS)
		{
			TxtBx_debug->AppendText("SPI Device open failed.\r\n");
			return;
		}

		PlaybackCheck->Start();


		aucIscSequencerConfigReq[16] = i


		for (int i = 0; i <= SignalCount; i++)
		{ 
			if (stop==1)
			{
				
				break;
				printf("Play ALL was stopped by user\r\n");
			}
			else
			{
					aucIscSequencerConfigReq[16] = i;
					status = EpsonDemo(deviceIndexAtSCB0);
					TxtBx_debug->AppendText("Playing phrase:" + i + " of " + SignalCount+"\r\n");
					/*Hard-Reset Procedure*/
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
					TxtBx_debug->AppendText("\nRestarting the IC.\r\n");
					EpsonSendMessage(aucIscResetReq, &usReceivedMessageID);
					Sleep(10);
					/*Receive ISC_RESET_RESP*/
					CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
					if (value)
					{
						iError = EpsonReceiveMessage(&usReceivedMessageID);
						if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_RESET_RESP)
						{
							printf("\nRestart failed.\r\n");
							return;
						}
					}
					/*Key-code Registration*/
					TxtBx_debug->AppendText("\nRegistering key-code.\r\n");
					/*Send ISC_TEST_REQ*/
					EpsonSendMessage(aucIscTestReq, &usReceivedMessageID);
					/*Receive ISC_TEST_RESP*/
					CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
					if (value)
					{
						iError = EpsonReceiveMessage(&usReceivedMessageID);
						if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_TEST_RESP)
						{
							printf("\nKey-code registration failed.\r\n");
							return;
						}
					}
					/*Get version info*/
					TxtBx_debug->AppendText("\nReading version info.\r\n");
					/*Send ISC_VERSION_REQ*/
					EpsonSendMessage(aucIscVersionReq, &usReceivedMessageID);
					/*Receive ISC_VERSION_RESP*/
					CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
					if (value)
					{
						iError = EpsonReceiveMessage(&usReceivedMessageID);
						if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_VERSION_RESP)
						{
							printf("\nVersion info read failed.\r\n");
							return;
						}
					}
					/*Set volume & sampling freq.*/
					TxtBx_debug->AppendText("\nVolume & sampling frequency setup.\r\n");
					/*Send ISC_AUDIO_CONFIG_REQ*/
					EpsonSendMessage(aucIscAudioConfigReq, &usReceivedMessageID);
					/*Receive ISC_AUDIO_CONFIG_RESP*/
					CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
					if (value)
					{
						iError = EpsonReceiveMessage(&usReceivedMessageID);
						if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_AUDIO_CONFIG_RESP)
						{
							TxtBx_debug->AppendText("\nVolume & sampling frequency setup failed.\r\n");
							return;
						}
					}
					/*Send ISC_SEQUENCER_CONFIG_REQ*/
					EpsonSendMessage(aucIscSequencerConfigReq, &usReceivedMessageID);
					/*Receive ISC_SEQUENCER_CONFIG_RESP*/
					CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
					if (value)
					{
						iError = EpsonReceiveMessage(&usReceivedMessageID);
						if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_CONFIG_RESP)
						{
							printf("\nSequencer configuration failed.\r\n");
							return;
						}
					}
					/*Start sequencer playback*/
					TxtBx_debug->AppendText("\nSequencer playback.\r\n");
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
							printf("\nSequencer playback failed.\r\n");
							return;
						}
					}
					/*Wait for playback termination*/
					do
					{
						/* Receive ISC_SEQUENCER_STATUS_IND */
						iError = EpsonReceiveMessage(&usReceivedMessageID);
						if (iError < SPIERR_SUCCESS || usReceivedMessageID == ID_ISC_MSG_BLOCKED_RESP)
						{
							return;
						}
					} while (usReceivedMessageID != ID_ISC_SEQUENCER_STATUS_IND);
					printf("\nPlayback has ended.\r\n");
					/*Sequencer stop*/
					printf("\nTerminating sequencer.\r\n");
					/*Send ISC_SEQUENCER_STOP_REQ*/
					EpsonSendMessage(aucIscSequencerStopReq, &usReceivedMessageID);
					/*Receive ISC_SEQUENCER_STOP_RESP*/
					CyGetGpioValue(cyHandle, MSGRDY_GPIO, &value);
					if (value)
					{
						iError = EpsonReceiveMessage(&usReceivedMessageID);
						if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_STOP_RESP)
				/*		{
							printf("\nSequencer termination failed.\r\n");
							return;
						}
					}*/
					Sleep(100);
			}
		
						}
				
			TxtBx_debug->AppendText("Demo succeeded.\r\n");
			CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
	
	
		//{
		//	TxtBx_debug->AppendText("Demo failed.\r\n");
		//	CySetGpioValue(cyHandle, MUTE_GPIO, 0); /*Set mute signal(MUTE) to High(disable)*/
		//}
	}	//cyNumDevices > 0 && cyReturnStatus == CY_SUCCESS
	else
	{
	TxtBx_debug->AppendText("No device found. Please connect device first!\r\n");
	}	
}