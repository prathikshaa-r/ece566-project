/*
#include <stdio.h>



/*
	encodeNoteOn- obtain a midi encoding for starting a note
	Passed: pitch (0-127), velocity(0-127), and channel (0-15)
	Return: NOTEON midi encoding for the above values, parsed into the int message
	Status byte : 1001 CCCC (message bits 16-23)
	Data byte 1 : 0PPP PPPP (message bits 8-15)
	Data byte 2 : 0VVV VVV (message bits 0-7)
*/
int encodeNoteOn(int velocity, int pitch, int channel)
{
	int message = 0;
	message = message | (velocity & 0x0000007F);//velocity bits (0-7)
	message = message | (pitch & 0x0000007F)<<8;//pitch bits (8-15)
	message = message | (channel & 0x0000000F)<<16;//Lower bits of status byte, channel bits (16-19)
	message = message | 0x00900000;//Upper bits of status byte, 1001 (20-23)
	return message;
}

/*
	encodeNoteOff- obtain a midi encoding for ending a note
	Passed: pitch (0-127), velocity(0-127), and channel (0-15)
	Return: NOTEOFF midi encoding for the above values, parsed into the int message
	Status byte : 1000 CCCC (message bits 16-23)
	Data byte 1 : 0PPP PPPP (message bits 8-15)
	Data byte 2 : 0VVV VVV (message bits 0-7)
*/
int encodeNoteOff(int velocity, int pitch, int channel)
{
	int message = 0;
	message = message | (velocity & 0x0000007F);//velocity bits (0-7)
	message = message | (pitch & 0x0000007F)<<8;//pitch bits (8-15)
	message = message | (channel & 0x0000000F)<<16;//Lower bits of status byte, channel bits (16-19)
	message = message | 0x00800000;//Upper bits of status byte, 1000 (20-23)
	return message;
}

/*
	encodeInstrumentSelect- obtain a midi encoding for setting instrument type for a channel
	Passed: General MIDI standard instrument number(0-127)[https://www.midi.org/specifications/item/gm-level-1-sound-set], and channel (0-15)
	Return: NOTEOFF midi encoding for the above values, parsed into the int message
	Status byte : 1100 CCCC (message bits 8-15)
	Data byte 1 : 0PPP PPPP (message bits 0-7)
*/
int encodeInstrumentSelect(int instrumentNum, int channel)
{
	int message = 0;
	message = message | (instrumentNum & 0x0000007F);//instrument number select bits (0-7)
	message = message | (channel & 0x0000000F)<<8;//Lower bits of status byte, channel bits (8-11)
	message = message | 0x0000c000;//Upper bits of status byte, 1100 (12-15)
	return message;
}

int main()
{
	int val1 = encodeNoteOn(12,69,8);
	int val2 = encodeNoteOn(88,127,15);
	printf("%d\n%d\n",val1, val2);
	return 0;
}