#include <nds.h>
#include <maxmod9.h>

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#define SCREEN_PADDING 10
#define TABLE_LENGTH (SCREEN_WIDTH - 2*SCREEN_PADDING + 1)
#define TABLE_MAX (SCREEN_HEIGHT - 2*SCREEN_PADDING + 1)

#define SAMPLING_RATE 44150
#define BUFFER_SIZE 1600

/**
 * What screens do we need?
 * 
 * 1. Wavetable No. 1 Editor <-- TableEditor
 * 2. Wavetable No. 2 Editor <-- TableEditor
 * 3. Transition Shape Editor <-- TableEditor
 * 4. Transition Time Slider <-- Slider
 * 5. Swipe/Lerp Toggle <-- Toggle
 */

struct SoundInfo {
	bool playing; // is the note currently playing?
	int framesElapsed; // frames elapsed since start of tone
    int phaseFramesElapsed; // frames elapsed used for calculating phase. will be modded and deviate from actual frames elapsed
    int morphTableIndex;
    int freq;
	s8 outputSample; // the sample to output
};

class Editor;
class TableEditor;
class Slider;
class Piano;
class Audio;

class App {
public:
    static s8 wave1Array[TABLE_LENGTH];
    static s8 wave2Array[TABLE_LENGTH];
    static s8 morphArray[TABLE_LENGTH];
    /**
     * how long it takes to iterate over the morphArray, measured in number of frames
     */
    static int morphTime;
    static struct SoundInfo sounds[13];

    App() : 
        waveTableOne(wave1Array),
        waveTableTwo(wave2Array),
        morphShapeTable(morphArray),
        morphTimeSlider(morphTime),
        audio(30),
        editorArray {&waveTableOne, &waveTableTwo, &morphShapeTable, &morphTimeSlider},
        editorArrayIndex {0},
        numberOfScreens {4}
    {
        for (int i = 0; i < TABLE_LENGTH; i++) {
            wave1Arr[i] = 0;
            wave2Arr[i] = 0;
            morphTable[i] = 0;
        }
        morphTime = 0;
        for (int i = 0; i < 13; i++) {
            sounds[i].playing = false;
            sounds[i].framesElapsed = 0;
            sounds[i].phaseFramesElapsed = 0;
            sounds[i].morphTableIndex = 0;
            sounds[i].freq = 0;
            sounds[i].outputSample = 0;
        }

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

        editorArray[editorArrayIndex]->draw();
    }
    
    /**
     * Executes one main loop of the program
     */
    void ExecuteOneMainLoop() {
        handleInputs();
        editorArray[editorArrayIndex]->handleTouch();
        piano.resamplePianoKeys();
    }

    /**
     * this handles each loop in the audio stream
     * 
     * it returns the sample to be outputed
     */
    s16 ExecuteOneStreamLoop() {
        return audio.frameOutput();
    }

private:
    Editor * editorArray[4];
    int editorArrayIndex;
    int numberOfScreens;
    TableEditor waveTableOne;
    TableEditor waveTableTwo;
    TableEditor morphShapeTable;
    Slider morphTimeSlider;
    Audio audio;
    Piano piano;
    
    void handleInputs() {
        scanKeys();
		int keysD = keysDown();
        if (keysD & KEY_RIGHT)
            decEditorArrayIndex();
        else if (keysD & KEY_LEFT)
            incEditorArrayIndex();
		if (keysD & KEY_UP)
			piano.incOctave();
		if (keysD & KEY_DOWN)
			piano.decOctave();
		if (keysD & KEY_RIGHT)
			piano.incPitch();
		if (keysD & KEY_LEFT)
			piano.decPitch();
		if (keysD & KEY_X) {
			mmStreamClose();
			mmStreamOpen( &mystream );
		}
    }

    void incEditorArrayIndex() {
        editorArrayIndex++;
        editorArray[editorArrayIndex % numberOfScreens]->draw();
    }

    void decEditorArrayIndex() {
        editorArrayIndex--;
        editorArray[editorArrayIndex % numberOfScreens]->draw();
    }
}

App app = App();

/**
 * morphTablePos = lerp(0, TABLE_LENGTH, <<frames elapsed>>, Slider.time());
 * sample += lerp(waveTableOne[<<phase>>], waveTableTwo[<<phase>>], morphShapeTable.get(morphTablePos), TABLE_MAX)
 */
class Lerp {
public:
    /**
     * @param v0 the first value for the lerp
     * @param v1 the second value for the lerp
     * @param dialCurrent how many steps have taken place
     * @param dialMax how many steps are there between v0 and v1
     */
    static int lerp(int v0, int v1, int dialCurrent, int dialMax) {
        if (dialCurrent <= 0)
            return v0;
        if (dialCurrent >= dialMax)
            return v1;
        return v0 + ((dialCurrent * (v1 - v0)) / dialMax);
    }
}

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

/**
 * the Piano class encapsulates all of the communication done between the 
 * piano keys and the rest of the program. It holds info about each key  
 */
class Piano {
public:
    Piano() : pitches {
        55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104,
        110, 117, 123, 131, 139, 147, 156, 165, 175, 185, 196, 208,
        220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415,
        440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831,
        880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661,
        1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322,
        3520, 3729, 3951, 4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645
    }, pitch{3}, octave{3} {}
    

    void resamplePianoKeys() {
        PianoKeys down;
        PianoKeys held;
        PianoKeys up;

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
                    App::sounds[i].playing = true;
                    App::sounds[i].freq = pitches[pitch + (12 * octave) + i];
                }
                else if (held.VAL & 1<<bitfieldShift) {
                    // I'll keep this here in case I ever need it
                }
                // if the key was just released kill the sound.
                else if (up.VAL & 1<<bitfieldShift) {
                    App::sounds[i].framesElapsed = 0;
                    App::sounds[i].phaseFramesElapsed = 0;
                    App::sounds[i].morphTableIndex = 0;
                    App::sounds[i].playing = false;
                }
            }
        }
    }

    void incPitch() { pitch++; }
    void decPitch() { pitch--; }
    void incOctave() { octave++; }
    void decOctave() { octave--; }
private:
    int pitch;
    int octave;
    int pitches[84];
}

class Editor {
public:
    /**
     * draw the state of the editor upon switching to it
     */
    virtual void draw() = 0;

    /**
     * what does the editor do when the touchpad is interacted with?
     */
    virtual void handleTouch() = 0;

    /**
     * clear the editor
     */
    void clear() {
        for (int i = SCREEN_PADDING; i < SCREEN_WIDTH - SCREEN_PADDING; i++) {
            for (int j = SCREEN_PADDING; j < SCREEN_HEIGHT - SCREEN_PADDING; j++) {
                VRAM_A[256 * j + i] = RGB15(0, 0, 0);
            }
        }
    }
}

class TableEditor : public Editor {
public:
    TableEditor(s16 (&table_)[TABLE_LENGTH]) : table(table_) {
        for (int i = 0; i < TABLE_LENGTH; i++)
            table[i] = 0;
        hasLifted = true;
        previousX = 0;
        previousY = 0;
    }

    void draw() {
        clear();
        for (int i = 0; i < TABLE_LENGTH; i++) {
            int x = i + SCREEN_PADDING;
            int y = table[i] + SCREEN_PADDING;
            VRAM_A[256 * y + x] = RGB15(31, 31, 31);
        }
    }

    /**
     * @PRECONDITION scanKeys() must be called before this function
     */
    void handleTouch() {
        int keysH = keysHeld();
		touchRead(&touch);
		if (keysH & KEY_TOUCH) {
			if (hasLifted) {
				setPixel(touch.px, touch.py);
			} else {
				drawLine(previousX, previousY, touch.px, touch.py);
			}
            previousX = touch.px;
			previousY = touch.py;
			hasLifted = false;
		} else {
			hasLifted = true;
		}
    }

private:
    s16 (&table)[TABLE_LENGTH];
    touchPosition touch;
    int previousX;
    int previousY;
    bool hasLifted;
    
    void setPixel(int x, int y) {
        // make sure the input is within bounds
        if (x < SCREEN_PADDING) x = SCREEN_PADDING;
        if (x > SCREEN_WIDTH - SCREEN_PADDING) x = SCREEN_WIDTH - SCREEN_PADDING;
        if (y < SCREEN_PADDING) y = SCREEN_PADDING;
        if (y > SCREEN_HEIGHT - SCREEN_PADDING) y = SCREEN_HEIGHT - SCREEN_PADDING;
        
        // make the pixel at position white, and everything above and below black
        for (int i = SCREEN_PADDING; i <= SCREEN_HEIGHT - SCREEN_PADDING; i++) {
            if (i == y)
                VRAM_A[256 * i + x] = RGB15(31, 31, 31);
            else
                VRAM_A[256 * i + x] = RGB15(0, 0, 0);
        }

        table[x - SCREEN_PADDING] = y - SCREEN_PADDING;
    }

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
}


class Slider : public Editor {
public:
    Slider(int &morphTimeInit) : time(morphTimeInit) {
        previousX = 0;
        maxVal = SAMPLE_RATE * 10; // set max time to ten seconds
        drawSliderLine(SCREEN_PADDING);
    }

    void draw() {
        clear();
        drawSliderLine(previousX);
    }

    void handleTouch() {
        int keysH = keysHeld();
		touchRead(&touch);
        
        // make sure x is within an acceptable range
        int x = touch->px;
        if (x < SCREEN_PADDING)
            x = SCREEN_PADDING;
        if (x > SCREEN_WIDTH - SCREEN_PADDING)
            x = SCREEN_WIDTH - SCREEN_PADDING;
        
        // draw the line on the UI
        drawSliderLine(x);

        // set the previous to that the line can be erased later
        previousX = x;

        // scale x properly and set the internal time variable
        time = ((x - SCREEN_PADDING) * maxVal) / TABLE_LENGTH;
    }

    /**
     * retrieve the value from the slider
     * @return the number of frames to lerp between wave table one and wave table two
     */
    int time() {
        return time;
    }

private:
    int &time;
    int previousX;
    int maxVal;
    touchPosition touch;
    void drawSliderLine(int x) {
        for (int i = SCREEN_PADDING; i < SCREEN_HEIGHT - SCREEN_PADDING) {
            VRAM_A[256 * i + previousX] = RGB15(0, 0, 0);
            VRAM_A[256 * i + x] = RGB15(31, 31, 31);
        }
    }
}

class Audio {
public:
    Audio(int gain_) gain {gain_} {}

    s16 frameOutput() {
        s16 output = 0;
        for (int i = 0; i < 13; i++) {
            output += getOutputSample(&(App::sounds[i]));
        }
        return output;
    }
private:
    int gain;

    int getWavePhase(struct SoundInfo * sound) {
	    int phase = ((sound->phaseFramesElapsed * sound->freq * TABLE_LENGTH) / SAMPLING_RATE);
        if (phase > 8 * TABLE_LENGTH)
            sound->phaseFramesElapsed = 0;
	    return phase % TABLE_LENGTH;
    }

    int getMorphIndex(struct SoundInfo * sound) {
        return Lerp::lerp(0, TABLE_LENGTH, sound->framesElapsed, App::morphTime);
    }

    void incrementFrameCount(struct SoundInfo * sound) {
        sound->phaseFramesElapsed++;
        sound->framesElapsed++:
    }

    s16 getOutputSample(struct SoundInfo * sound) {
        if (sound->playing) {
            int phase = getWavePhase(sound);
            s16 sample1 = App::wave1Arr[phase];
            s16 sample2 = App::wave2Arr[phase];
            
            int output = Lerp::lerp(sample1, sample2, App::morphArr[getMorphIndex(sound)], TABLE_MAX);

            incrementFrameCount(sound);
            return output;
        } else {
            return 0;
        }
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
		*target++ = app.ExecuteOneStreamLoop();
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
		
		// update stream
		mmStreamUpdate();

		// wait until next frame
		swiWaitForVBlank();
		
        app.ExecuteOneMainLoop();
		
	}
	
	return 0;
}