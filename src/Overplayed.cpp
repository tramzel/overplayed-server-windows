/*
Overplayed Server by Steven T. Ramzel

This software is open-source with no warranty, it has no licence as most the code is altered from other sources
Based on:
	Server using Virtual Connection over UDP
	From "Networking for Game Programmers" - http://www.gaffer.org/networking-for-game-programmers
	Author: Glenn Fiedler <gaffer@gaffer.org>

	Parallel Port Joystick sample program
	From: http://ppjoy.bossstation.dnsalias.org/
	Package: http://ppjoy.bossstation.dnsalias.org/Docs/Diagrams/Virtual/IOCTLSample.zip
	Author: Deon van der Westhuysen
*/

#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>

#include "Net.h"

#include <conio.h>
#include <windows.h>
#include <winioctl.h>

#include "PPJIoctl.h"

#define	NUM_ANALOG	8		/* Number of analog values which we will provide */
#define	NUM_DIGITAL	20		/* Number of digital values which we will provide */

#define	JOYSTICK_STATE_V1	0x53544143

#pragma pack(push,1)		/* All fields in structure must be byte aligned. */
typedef struct
{
 unsigned long	Signature;				/* Signature to identify packet to PPJoy IOCTL */
 char			NumAnalog;				/* Num of analog values we pass */
 long			Analog[NUM_ANALOG];		/* Analog values */
 char			NumDigital;				/* Num of digital values we pass */
 char			Digital[NUM_DIGITAL];	/* Digital values */
}	JOYSTICK_STATE;
#pragma pack(pop)

using namespace std;
using namespace net;

int ServerPort = 30000;
const int ProtocolId = 0x99887766;
const float DeltaTime = .005f;
const float SendRate = 1.0f;
const float TimeOut = 5.0f;
unsigned short remoteSequence = 0;
unsigned short localSequence = 0;
unsigned char inPacket[38];
unsigned char outPacket[2];
long lastSendTime = 0;
long sendInterval = 1000;

int main( int argc, char * argv[] )
{
	 HANDLE				h;
	 char				ch;
	 JOYSTICK_STATE		JoyState;

	 DWORD				RetSize;
	 DWORD				rc;

	 long				*Analog;
	 char				*Digital;

	 wchar_t*				DevName;

	 DevName= L"\\\\.\\PPJoyIOCTL1";

	 /*Set port as argument passed*/
	 if (argc == 2) ServerPort = atoi(argv[1]);

	 /* Open a handle to the control device for the first virtual joystick. */
	 /* Virtual joystick devices are names PPJoyIOCTL1 to PPJoyIOCTL16. */
	 h= CreateFile(DevName,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);

	 /* Make sure we could open the device! */
	 if (h==INVALID_HANDLE_VALUE)
	 {
	  printf ("CreateFile failed with error code %d trying to open %s device\n",GetLastError(),DevName);
	  return 1;
	 }

	 /* Initialise the IOCTL data structure */
	 JoyState.Signature= JOYSTICK_STATE_V1;
	 JoyState.NumAnalog= NUM_ANALOG;	/* Number of analog values */
	 Analog= JoyState.Analog;			/* Keep a pointer to the analog array for easy updating */
	 JoyState.NumDigital= NUM_DIGITAL;	/* Number of digital values */
	 Digital= JoyState.Digital;			/* Digital array */

	if ( !InitializeSockets() )
	{
		printf( "failed to initialize sockets\n" );
		return 1;
	}

	Connection connection( ProtocolId, TimeOut );
	
	if ( !connection.Start( ServerPort ) )
	{
		printf( "could not start connection on port %d\n", ServerPort );
		return 1;
	}
	
	while ( true )
	{			
		while ( true )
		{
			int bytes_read = connection.ReceivePacket( inPacket, sizeof(inPacket));
			if ( bytes_read == 0 ){
				connection.Update( 10.f );
				break;
			}
			remoteSequence = (inPacket[0] << 8) | inPacket[1];
			if (remoteSequence == 0) {
				localSequence = 0;
			}
			//localSequence = 0;
			if (remoteSequence > localSequence || remoteSequence < localSequence - 0x7FFF){
				localSequence = remoteSequence;
				if ( connection.IsConnected() ){
					outPacket[0] = inPacket[0];
					outPacket[1] = inPacket[1];
					connection.SendPacket( outPacket, sizeof( outPacket ) );
				}
			}
			/* On each iteration clear position buffer: Analog in centre, buttons not pressed */
			Analog[0] = Analog[1] = Analog[2]= Analog[3]= Analog[4]= Analog[5]= Analog[6]= Analog[7]= (PPJOY_AXIS_MIN+PPJOY_AXIS_MAX)/2;
			memset (Digital,0,sizeof(JoyState.Digital));
			

			Analog[0] = (inPacket[2] << 8) | inPacket[3];
			//printf( "Analog[0]:%hu\n", Analog[0] );
			Analog[1] = (inPacket[4] << 8) | inPacket[5];
			//printf( "Analog[1]:%hu\n", Analog[1] );
			Analog[2] = (inPacket[6] << 8) | inPacket[7];
			//printf( "Analog[2]:%hu\n", Analog[2] );
			Analog[5] = (inPacket[8] << 8) | inPacket[9];
			//printf( "Analog[3]:%hu\n", Analog[3] );
			
			int i = 0;
			int mask = 128;
			while (i < 8) 
			{
				Digital[i++] = (mask & inPacket[10]) != 0;
				mask >>= 1;
			}
			int j = 0;
			mask = 128;
			while (j++ < 4) 
			{
				Digital[i++] = (mask & inPacket[11]) != 0;
				mask >>= 1;
			}
			i = 16;
			while (j++ < 9) 
			{
				Digital[i++] = (mask & inPacket[11]) != 0;
				mask >>= 1;
			}
		  /* Send request to PPJoy for processing. */
		  /* Currently there is no Return Code from PPJoy, this may be added at a */
		  /* later stage. So we pass a 0 byte output buffer.                      */
		  if (!DeviceIoControl(h,IOCTL_PPORTJOY_SET_STATE,&JoyState,sizeof(JoyState),NULL,0,&RetSize,NULL))
		  {
		   rc= GetLastError();
		   if (rc==2)
		   {
			printf ("Underlying joystick device deleted. Exiting read loop\n");
			break;
		   }
		   printf ("DeviceIoControl error %d\n",rc);
		  }
		}
		if (!connection.IsConnected()){
			localSequence = 0;
		}
		//wait( DeltaTime );
	}
	
	ShutdownSockets();
	 CloseHandle(h);
	return 0;
}
