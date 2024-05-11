/****************************************************************
 * this example demonstrates streaming synthensized audio
 ****************************************************************/
 
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <maxmod9.h>

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#define SCREEN_PADDING 10
#define SAMPLE_LENGTH (SCREEN_WIDTH - 2*SCREEN_PADDING + 2)
#define GAIN 50

#define SAMPLING_RATE 44150
#define BUFFER_SIZE 1600

// PianoKeys was copied from the addon.c example program for devkitPro
typedef struct {
	union {
		struct {
			u16 c : 1;
			u16 c_sharp : 1; //rules!
			u16 d : 1;
			u16 d_sharp : 1;
			u16 e : 1;
			u16 f : 1;
			u16 f_sharp : 1;
			u16 g : 1;
			u16 g_sharp : 1;
			u16 a : 1;
			u16 a_sharp : 1;
			u16 _unknown1 : 1;
			u16 _unknown2 : 1;
			u16 b : 1;
			u16 high_c : 1; //is yummy
			u16 _unknown3 : 1;
		};
		u16 VAL;
	};
} PianoKeys;

struct SoundInfo {
	bool playing; // is the note currently playing?
	int time; // measured in samples elapsed since start of tone
	s8 outputSample; // the sample to output
};

// For each key, this array holds both the sound id of whatever is being played and how long
// the key has been held for.
// The sound id will be used to access the sounds, so that the volume can be changed and the sound can be stopped.
// Time will keep track of how long a key has been pressed for, and is used as the input for attackDecaySustain()
struct SoundInfo sounds[13];

int pitches[] = {
	55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104,
	110, 117, 123, 131, 139, 147, 156, 165, 175, 185, 196, 208,
	220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415,
	440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831,
	880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661,
	1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322,
	3520, 3729, 3951, 4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645
};

int pitch = 3;
int octave = 3;

s16 sample[SAMPLE_LENGTH];

/**
 * give a SoundInfo and get the desired sample phase
 * also increments time for the SoundInfo
 */
int getPhase(struct SoundInfo * sound, int freq) {
	int phase = ((sound->time++ * freq * SAMPLE_LENGTH) / SAMPLING_RATE);
	if (phase > 8*SAMPLE_LENGTH) sound->time = 0;
	//if (sound->time > (freq * SAMPLING_RATE)) sound->time = 0;
	return phase % SAMPLE_LENGTH;
}

/**
 * given a SoundInfo, return
 */
s16 getOutputSample(struct SoundInfo * sound, int freq) {
	if (sound->playing) {
		//if (sound->time % 10000 == 0) fprintf(stderr, "freq: %d\n", freq);
		return GAIN * sample[getPhase(sound, freq)];
	} else {
		return 0;
	}
}

void startSound(struct SoundInfo * sound) {
	sound->playing = true;
}

void stopSound(struct SoundInfo * sound) {
	sound->playing = false;
	sound->time = 0;
}

/**
 * updates the sounds array according to which keys are pressed
 * 
 * returns the sum of the output samples
 */
s16 updatePianoKeysHeld() {

	PianoKeys down;
	PianoKeys held;
	PianoKeys up;
	
	s16 sample = 0;

	// for whatever reason, the piano stuff won't work without checking for the guitar grip first.
	// oh well. ¯\_(ツ)_/¯
	guitarGripIsInserted();

	if (pianoIsInserted()) {
		pianoScanKeys();
		down.VAL = pianoKeysDown();
		held.VAL = pianoKeysHeld();
		up.VAL = pianoKeysUp();

		// this loops through the sounds array and the PianoKeys bitfields to detect key presses, play sounds,
		// change sound volume, and kill sounds.
		int bitfieldShift;
		for (int i = 0; i < 13; i++) {
			bitfieldShift = i + ((i >= 11) ? 2 : 0); // because of the gap in the PianoKeys bitfield, skip 11 and 12
			// if the key was just pressed, start the sound
			if (down.VAL & 1<<bitfieldShift) {
				startSound(&sounds[i]);
			}
			else if (held.VAL & 1<<bitfieldShift) {
				//sample += getOutputSample()	
			}
			// if the key was just released kill the sound.
			else if (up.VAL & 1<<bitfieldShift) {
				stopSound(&sounds[i]);
			}
		}
	}
	return sample;
}

/**
 * set the pixel at the given position white, and everything above and below it black
 * update the sample array to match the screen 
 */
void setPixel(int x, int y) {
	// make sure the input is within bounds
	if (x < SCREEN_PADDING) x = SCREEN_PADDING;
	if (x >= SCREEN_WIDTH - SCREEN_PADDING) x = SCREEN_WIDTH - SCREEN_PADDING;
	if (y < SCREEN_PADDING) y = SCREEN_PADDING;
	if (y >= SCREEN_HEIGHT - SCREEN_PADDING) y = SCREEN_HEIGHT - SCREEN_PADDING;
	
	// make the pixel at position white, and everything above and below black
	for (int i = SCREEN_PADDING; i <= SCREEN_HEIGHT - SCREEN_PADDING; i++) {
		if (i == y)
			VRAM_A[256 * i + x] = RGB15(31, 31, 31);
		else
			VRAM_A[256 * i + x] = RGB15(0, 0, 0);
	}

	sample[x - SCREEN_PADDING] = y - SCREEN_PADDING;
}

bool isInBounds(int i, int j) {
	if (i < SCREEN_PADDING ||
		i >= SCREEN_WIDTH - SCREEN_PADDING + 1 ||
		j < SCREEN_PADDING ||
		j >= SCREEN_HEIGHT - SCREEN_PADDING + 1)
		return false;
	else
		return true;
}

/**
 * trashiest line drawing algorithm in existence.
 * but it has heart <3
 */
void drawLine(int x1, int y1, int x2, int y2) {
	if (x1 == x2) {
		setPixel(x1, y1);
		return;	
	}
	if (x2 < x1) {
		int temp = x1;
		x1 = x2;
		x2 = temp;
		temp = y1;
		y1 = y2;
		y2 = temp;
	}
	for (int i = x1; i <= x2; i++) {
		int currX = i;
		int currY = y1 + ((y2 - y1) / ((x2 - x1) / (i - x1)));
		setPixel(currX, currY);
	}
	
}

/***********************************************************************************
 * on_stream_request
 *
 * Audio stream data request handler.
 ***********************************************************************************/
mm_word on_stream_request( mm_word length, mm_addr dest, mm_stream_formats format ) {
//----------------------------------------------------------------------------------

	s16 *target = dest;

	int len = length;
	for( ; len; len-- )
	{
		//updatePianoKeysHeld();

		int sample = 0;
		for (int i = 0; i < 13; i++) {
			sample += getOutputSample(&sounds[i], pitches[pitch + (12 * octave) + i]);
		}

		*target++ = sample;

	}
	
	return length;
}



/**********************************************************************************
 * main
 *
 * Program Entry Point
 **********************************************************************************/
int main( void ) {
//---------------------------------------------------------------------------------
	consoleDebugInit(DebugDevice_NOCASH);
	
	videoSetMode(MODE_FB0);
	vramSetBankA(VRAM_A_LCD);

	lcdMainOnBottom();

	// initialize the screen
	for (int i = 0; i < 256; i++) {
		for (int j = 0; j < 192; j++) {
			if (i <= SCREEN_PADDING - 1 ||
				i >= SCREEN_WIDTH - SCREEN_PADDING + 1 ||
				j <= SCREEN_PADDING - 1||
				j >= SCREEN_HEIGHT - SCREEN_PADDING + 1)
				VRAM_A[256 * j + i] = RGB15(15, 15, 15);
			else if (j == SCREEN_PADDING)
				VRAM_A[256 * j + i] = RGB15(31, 31, 31); 
			else
				VRAM_A[256 * j + i] = RGB15(0, 0, 0);
		}
	}

	// initialize sample
	for (int i = 0; i < SAMPLE_LENGTH; i++)
		sample[i] = 0;

	touchPosition touch;
	// has the pen been lifted? (ie, does a line need to be drawn?)
	bool hasLifted = true;
	// the previous x and y positions of the pen
	int prevX = 0;
	int prevY = 0;

	// initialize SoundInfo's
	for (int i = 0; i < 13; i++) {
		sounds[i].playing = false;
		sounds[i].time = 0;
		sounds[i].outputSample = 0;
	}

	//----------------------------------------------------------------
	// initialize maxmod without any soundbank (unusual setup)
	//----------------------------------------------------------------
	mm_ds_system sys;
	sys.mod_count 			= 0;
	sys.samp_count			= 0;
	sys.mem_bank			= 0;
	sys.fifo_channel		= FIFO_MAXMOD;
	mmInit( &sys );
	
	//----------------------------------------------------------------
	// open stream
	//----------------------------------------------------------------
	mm_stream mystream;
	mystream.sampling_rate	= SAMPLING_RATE;			// sampling rate = 25khz
	mystream.buffer_length	= BUFFER_SIZE;						// buffer length = 1200 samples
	mystream.callback		= on_stream_request;		// set callback function
	mystream.format			= MM_STREAM_16BIT_MONO;		// format = mono 8-bit
	mystream.timer			= MM_TIMER0;				// use hardware timer 0
	mystream.manual			= true;						// use manual filling
	mmStreamOpen( &mystream );
		
	//----------------------------------------------------------------
	// when using 'automatic' filling, your callback will be triggered
	// every time half of the wave buffer is processed.
	//
	// so: 
	// 25000 (rate)
	// ----- = ~21 Hz for a full pass, and ~42hz for half pass
	// 1200  (length)
	//----------------------------------------------------------------
	// with 'manual' filling, you must call mmStreamUpdate
	// periodically (and often enough to avoid buffer underruns)
	//----------------------------------------------------------------

	while( 1 )
	{
		//if (sounds[0].playing) fprintf(stderr, "sounds[0].playing == true\n");
		//else fprintf(stderr, "sounds[0].playing == false\n");
		updatePianoKeysHeld();
		
		// update stream
		mmStreamUpdate();

		// wait until next frame
		swiWaitForVBlank();
		
		//-----------------------------------------------------
		// get new keypad input
		//-----------------------------------------------------
		scanKeys();
		int keysD = keysDown();
		if (keysD & KEY_UP)
			octave++;
		if (keysD & KEY_DOWN)
			octave--;
		if (keysD & KEY_RIGHT)
			pitch++;
		if (keysD & KEY_LEFT)
			pitch--;
		if (keysD & KEY_X) {
			mmStreamClose();
			mmStreamOpen( &mystream );
		}
		
		int keysH = keysHeld();
		touchRead(&touch);
		if (keysH & KEY_TOUCH) {
			if (hasLifted) {
				setPixel(touch.px, touch.py);
				prevX = touch.px;
				prevY = touch.py;
			} else {
				drawLine(prevX, prevY, touch.px, touch.py);
				prevX = touch.px;
				prevY = touch.py;
			}
			hasLifted = false;
		} else {
			hasLifted = true;
		}
		
	}
	
	return 0;
}
