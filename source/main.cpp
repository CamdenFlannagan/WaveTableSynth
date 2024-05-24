#include <nds.h>
#include <maxmod9.h>
#include <malloc.h>
#include <stdio.h>
#include "nds/ndstypes.h"

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#define SCREEN_PADDING 10
#define TABLE_LENGTH (SCREEN_WIDTH - 2*SCREEN_PADDING + 1)
#define TABLE_MAX (SCREEN_HEIGHT - 2*SCREEN_PADDING + 1)

#define SAMPLING_RATE 10000
#define BUFFER_SIZE 1200

#define DEPOP_FRAMES 50 // enforced attack and release to get rid of the pop

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
    bool stopping; // used for depopping
    bool pingPongDirection; // true for forward, false for backwards
	int framesElapsed; // frames elapsed since start of tone
    int phaseFramesElapsed; // frames elapsed used for calculating phase. will be modded and deviate from actual frames elapsed
    int morphTableIndex; // the current position in the morph table
    int freq; // the frequency of the sound to be played
    int depopFramesElapsed; // used for depopping
    int lastSampleOutputted; // used for depopping
};

s16 wave1Array[TABLE_LENGTH];
s16 wave2Array[TABLE_LENGTH];
s16 transition[TABLE_LENGTH];
int transitionTime;
struct SoundInfo sounds[13];

/**
 * 0 if using Morph algorithm, 1 if using Swipe algorithm, 2 if using Combination of both algorithms
 */
int algorithm;

/**
 * 0 for no transition cycle, 1 for looping through transition table, 2 for ping-pong
 */
int transitionCycle;

PrintConsole *pc;

class Editor {
public:
    const char *description;
    Editor(const char *description_) : description{description_} {}

    /**
     * draw the state of the editor upon switching to it
     */
    virtual void draw() = 0;

    /**
     * what does the editor do when the touchpad is interacted with?
     */
    virtual void handleTouch() = 0;

    /**
     * display the information about the current editor
     */
    void printInfo() {
        consoleClear();
        printf("%s", description);
    }

    /**
     * clear the editor
     */
    void clear() {
        for (int i = SCREEN_PADDING; i <= SCREEN_WIDTH - SCREEN_PADDING; i++) {
            for (int j = SCREEN_PADDING; j <= SCREEN_HEIGHT - SCREEN_PADDING; j++) {
                VRAM_A[256 * j + i] = RGB15(0, 0, 0);
            }
        }
    }
};

template <typename T>
class LinkedRing {
public:
    LinkedRing() : current{NULL}, size{0} {}
    void add(T datum_) {
        size++;
        if (current == NULL) {
            current = new Node(NULL, datum_, NULL);;
            current->prev = current;
            current->next = current;
        } else {
            Node* newNode = new Node(current->prev, datum_, current);
            current->prev->next = newNode;
            current->prev = newNode;
            current = current->prev;
        }
    }
    T prev() {
        current = current->prev;
        return current->datum;
    }
    T curr() {
        return current->datum;
    }
    T next() {
        current = current->next;
        return current->datum;
    }

private:
    class Node {
    public:
        Node* prev;
        T datum;
        Node* next;
        Node(Node* prev_, T datum_, Node* next_) : prev{prev_}, datum{datum_}, next{next_} {}
    };

    Node* current;
    int size;
};

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
};

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
    Piano() :
    noteNames {
        "A    ", "A#/Bb", "B    ", "C    ", "C#/Db", "D    ", "D#/Eb", "E    ", "F    ", "F#/Gb", "G    ", "G#/Ab"
    }, pitches {
        28, 29, 31, 33, 35, 37, 39, 41, 44, 46, 49, 52, 
        55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104, 
        110, 117, 123, 131, 139, 147, 156, 165, 175, 185, 196, 208, 
        220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415, 
        440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831, 
        880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 
        1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322
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
                    sounds[i].playing = true;
                    sounds[i].freq = pitches[pitch + (12 * octave) + i];
                }
                else if (held.VAL & 1<<bitfieldShift) {
                    // I'll keep this here in case I ever need it
                }
                // if the key was just released kill the sound.
                else if (up.VAL & 1<<bitfieldShift) {
                    sounds[i].framesElapsed = 0;
                    sounds[i].phaseFramesElapsed = 0;
                    sounds[i].morphTableIndex = 0;
                    sounds[i].playing = false;
                    sounds[i].stopping = true;
                    sounds[i].pingPongDirection = true;
                }
            }
        }
    }

    void printRoot() {
        pc->cursorX = 0;
        pc->cursorY = 8;
        printf("Root: %s", noteNames[pitch % 12]);
    }

    void incPitch() {
        pitch++;
        printRoot();
    }
    void decPitch() {
        pitch--;
        printRoot();
    }
    void incOctave() {
        octave++;
        printRoot();
    }
    void decOctave() {
        octave--;
        printRoot();
    }

private:
    const char *noteNames[12];
    int pitches[84];
    int pitch;
    int octave;
};

class TableEditor : public Editor {
public:
    TableEditor(const char * description, s16 (&table_)[TABLE_LENGTH]) : Editor(description), table(table_) {
        for (int i = 0; i < TABLE_LENGTH; i++)
            table[i] = 0;
        hasLifted = true;
        previousX = 0;
        previousY = 0;
    }
 
    void draw() {
        printInfo();
        for (int i = 0; i < TABLE_LENGTH; i++) {
            int x = i + SCREEN_PADDING;
            int y = table[i] + SCREEN_PADDING;
            setPixel(x, y);
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
        
        // display guiding lines that divide the screen equally into twelve parts
        int tableX = x - SCREEN_PADDING;
        bool guider = false;
        for (int k = 1; k < 12; k++) {
            if (tableX == k*TABLE_LENGTH/12) {
                guider = true;
                break;
            } 
        }

        // make the pixel at position white, and everything above and below black
        for (int i = SCREEN_PADDING; i <= SCREEN_HEIGHT - SCREEN_PADDING; i++) {
            if (i == y)
                VRAM_A[256 * i + x] = RGB15(31, 31, 31);
            else {
                if (guider)
                    VRAM_A[256 * i + x] = RGB15(10, 10, 10);
                else
                    VRAM_A[256 * i + x] = RGB15(0, 0, 0);
            }
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
};


class Slider : public Editor {
public:
    Slider(const char * description, int &val_, int maxVal_) : Editor(description), val(val_), maxVal{maxVal_} {
        previousX = SCREEN_PADDING;
        drawSliderLine(SCREEN_PADDING);
    }

    void draw() {
        printInfo();
        clear();
        drawSliderLine(previousX);
    }

    void handleTouch() {
        int keysH = keysHeld();
        if (keysH & KEY_TOUCH) {
            touchRead(&touch);
            
            int x = touch.px;
            if (x < SCREEN_PADDING)
                x = SCREEN_PADDING;
            if (x > SCREEN_WIDTH - SCREEN_PADDING)
                x = SCREEN_WIDTH - SCREEN_PADDING;

            // draw the line on the UI
            drawSliderLine(x);

            // set the previous to that the line can be erased later
            previousX = x;

            // scale x properly and set the internal time variable
            val = ((x - SCREEN_PADDING) * maxVal) / TABLE_LENGTH;
        }
    }

private:
    int &val;
    int previousX;
    int maxVal;
    touchPosition touch;
    void drawSliderLine(int x) {
        for (int i = SCREEN_PADDING; i <= SCREEN_HEIGHT - SCREEN_PADDING; i++) {
            VRAM_A[256 * i + previousX] = RGB15(0, 0, 0);
            VRAM_A[256 * i + x] = RGB15(31, 31, 31);
        }
    }
};

class Switch : public Editor {
public:
    Switch(const char * description, int &val_, int numOptions_) : Editor(description), val(val_), numOptions{numOptions_} {}

    void handleTouch() {
        int keysH = keysHeld();
        if (keysH & KEY_TOUCH) {
            touchRead(&touch);
            
            // asign the value
            val = touch.px >= SCREEN_WIDTH / 2;

            int x = touch.px - SCREEN_PADDING;
            for (int i = 0; i < numOptions; i++) {
                if ((i*TABLE_LENGTH)/numOptions <= x && x < ((i + 1)*TABLE_LENGTH)/numOptions) {
                    val = i;
                    break;
                }
            }

            // redisplay the switch
            drawSwitch();
        }
    }

    void draw() {
        printInfo();
        drawSwitch();
    }
private:
    int &val;
    int numOptions;
    touchPosition touch;

    void drawSwitch() {
        for (int i = SCREEN_PADDING; i <= SCREEN_WIDTH - SCREEN_PADDING; i++) {
            for (int j = SCREEN_PADDING; j <= SCREEN_HEIGHT - SCREEN_PADDING; j++) {
                int x = i - SCREEN_PADDING;
                if ((val*TABLE_LENGTH)/numOptions <= x && x < ((val + 1)*TABLE_LENGTH)/numOptions)
                    VRAM_A[256 * j + i] = RGB15(31, 31, 31);
                else
                    VRAM_A[256 * j + i] = RGB15(0, 0, 0);
            }
        }
    }
};

class Audio {
public:
    Audio(int gain_) : gain {gain_} {}

    s16 frameOutput() {
        s16 output = 0;
        for (int i = 0; i < 13; i++) {
            output += getOutputSample(&(sounds[i]));
        }
        return output - 32761; // add minimum negative plus a few (I don't want to think about edge cases anymore)
    }
private:
    int gain;

    /**
     * @param v0 the first value for the lerp
     * @param v1 the second value for the lerp
     * @param dialCurrent how many steps have taken place
     * @param dialMax how many steps are there between v0 and v1
     */
    int lerp(int v0, int v1, int dialCurrent, int dialMax) {
        if (dialCurrent <= 0)
            return v0;
        if (dialCurrent >= dialMax)
            return v1;
        return v0 + ((dialCurrent * (v1 - v0)) / dialMax);
    }

    int getWavePhase(struct SoundInfo * sound) {
	    int phase = div32((sound->phaseFramesElapsed * sound->freq * TABLE_LENGTH), SAMPLING_RATE);
        if (phase > 8 * TABLE_LENGTH) {
            sound->phaseFramesElapsed = 0;
        }
	    return phase % TABLE_LENGTH;
    }

    int getTransitionIndex(struct SoundInfo * sound) {
        return lerp(0, TABLE_LENGTH - 1, sound->framesElapsed, transitionTime);
    }

    void incrementFrameCount(struct SoundInfo * sound) {
        sound->phaseFramesElapsed++;
        switch (transitionCycle) {
            case 0:
                sound->framesElapsed++;
                break;
            case 1:
                sound->framesElapsed = (sound->framesElapsed + 1) % transitionTime;
                break;
            case 2: {
                if (sound->pingPongDirection == true) {
                    if (sound->framesElapsed++ >= transitionTime)
                        sound->pingPongDirection = false;
                } else {
                    if (sound->framesElapsed-- <= 0)
                        sound->pingPongDirection = true;
                }
                break;
            }
        }
    }

    int getOutputSample(struct SoundInfo * sound) {
        if (sound->playing) {
            int phase = getWavePhase(sound);
            s16 sample1 = wave1Array[phase];
            s16 sample2 = wave2Array[phase];
            
            int transitionValue = transition[getTransitionIndex(sound)];

            int output;
            switch (algorithm) {
                case 0: { // Morph algorithm
                    output = lerp(sample1, sample2, transitionValue, TABLE_MAX - 1);
                    break;
                }
                case 1: { // Swipe algorithm
                    int split = lerp(0, TABLE_LENGTH, transitionValue, TABLE_MAX - 1);
                    if (phase > split)
                        output = sample1;
                    else
                        output = sample2;
                    break;
                }
                case 2: { // using a combination of both algorithms
                    int morph = lerp(sample1, sample2, transitionValue, TABLE_MAX - 1);
                    int swipe;
                    int split = lerp(0, TABLE_LENGTH, transitionValue, TABLE_MAX - 1);
                    if (phase > split)
                        swipe = sample1;
                    else
                        swipe = sample2;
                    output = lerp(swipe, morph, transitionValue, TABLE_MAX - 1);
                    break;
                }
                default: // if the default is reached, something went wrong
                    output = 0;
            }

            // if the note just started, depop by lerping to initial output
            if (sound->depopFramesElapsed < DEPOP_FRAMES) {
                output = lerp(0, output, sound->depopFramesElapsed++, DEPOP_FRAMES);
            } else {
                incrementFrameCount(sound);
            }
            
            sound->lastSampleOutputted = output;

            return gain * output;
        } else {
            int output;
            if (sound->stopping) {
                output = lerp(0, sound->lastSampleOutputted, sound->depopFramesElapsed--, DEPOP_FRAMES);
                if (sound->depopFramesElapsed <= 0)
                    sound->stopping = false;
            } else {
                output = 0;
            }

            return output;
        }
    }
};

class App {
public:
    App() : 
        waveTableOne("Wavetable One", wave1Array),
        waveTableTwo("Wavetable Two", wave2Array),
        morphShapeTable("Transition Shape", transition),
        morphTimeSlider("Transition Time", transitionTime, SAMPLING_RATE * 10),
        transitionCycleSwitch("Transition Cycle Mode\n1. Forward\n2. Loop\n3. Ping Pong", transitionCycle, 3),
        algorithmSwitch("Transition Algorithm\n1. Morph\n2. Swipe\n3. Combos", algorithm, 3),
        audio(54),
        editorRing()
    {
        editorRing.add(&algorithmSwitch);
        editorRing.add(&transitionCycleSwitch);
        editorRing.add(&morphTimeSlider);
        editorRing.add(&morphShapeTable);
        editorRing.add(&waveTableTwo);
        editorRing.add(&waveTableOne);
    }
    
    void displayTitle() {
        printf("Wavetable Synthesizer\n for the Nintendo DS\n\n");
        printf("Press START to continue");
    }

    void initScreen() {
        // initialize the screen
        for (int i = 0; i < 256; i++) {
            for (int j = 0; j < 192; j++) {
                if (i < SCREEN_PADDING ||
                    i > SCREEN_WIDTH - SCREEN_PADDING ||
                    j < SCREEN_PADDING ||
                    j > SCREEN_HEIGHT - SCREEN_PADDING)
                    VRAM_A[256 * j + i] = RGB15(15, 15, 15);
                else if (j == SCREEN_PADDING)
                    VRAM_A[256 * j + i] = RGB15(31, 31, 31); 
                else
                    VRAM_A[256 * j + i] = RGB15(0, 0, 0);
            }
        }
        editorRing.curr()->draw();
        piano.printRoot();
    }

    /**
     * Executes one main loop of the program
     */
    void ExecuteOneMainLoop() {
        handleInputs();
        editorRing.curr()->handleTouch();
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
    TableEditor waveTableOne;
    TableEditor waveTableTwo;
    TableEditor morphShapeTable;
    Slider morphTimeSlider;
    Switch transitionCycleSwitch;
    Switch algorithmSwitch;
    Audio audio;
    Piano piano;
    LinkedRing<Editor *> editorRing;

    void handleInputs() {
        scanKeys();
		int keysD = keysDown();
        if (keysD & KEY_L) {
            editorRing.prev()->draw();
            piano.printRoot();
        }
        if (keysD & KEY_R) {
            editorRing.next()->draw();
            piano.printRoot();
        }
        if (keysD & KEY_UP)
			piano.incOctave();
		if (keysD & KEY_DOWN)
			piano.decOctave();
		if (keysD & KEY_RIGHT)
			piano.incPitch();
		if (keysD & KEY_LEFT)
			piano.decPitch();
        int keysH = keysHeld();
        if (keysH & KEY_X) {
            sounds[0].playing = true;
            sounds[0].freq = 220;
        }
        int keysU = keysUp();
        if (keysU & KEY_X) {
            sounds[0].framesElapsed = 0;
            sounds[0].phaseFramesElapsed = 0;
            sounds[0].morphTableIndex = 0;
            sounds[0].playing = false;
            sounds[0].stopping = true;
        }
		/*if (keysD & KEY_X) {
			mmStreamClose();
			mmStreamOpen( &mystream );
		}*/
    }
};

App app = App();

/***********************************************************************************
 * on_stream_request
 *
 * Audio stream data request handler.
 ***********************************************************************************/
mm_word on_stream_request( mm_word length, mm_addr dest, mm_stream_formats format ) {
//----------------------------------------------------------------------------------

	s16 *target = (s16*)dest;

	int len = length;
	for( ; len; len-- ) {
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
	pc = consoleDemoInit();
	
	videoSetMode(MODE_FB0);
	vramSetBankA(VRAM_A_LCD);

	lcdMainOnBottom();
    
    app.initScreen();

    // initialize all global variables
    for (int i = 0; i < TABLE_LENGTH; i++) {
        wave1Array[i] = 0;
        wave2Array[i] = 0;
        transition[i] = 0;
    }
    transitionTime = 0;
    for (int i = 0; i < 13; i++) {
        sounds[i].playing = false;
        sounds[i].stopping = false;
        sounds[i].pingPongDirection = true;
        sounds[i].framesElapsed = 0;
        sounds[i].phaseFramesElapsed = 0;
        sounds[i].morphTableIndex = 0;
        sounds[i].freq = 0;
        sounds[i].depopFramesElapsed = 0;
        sounds[i].lastSampleOutputted = 0;
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
		
		// update stream
		mmStreamUpdate();

		// wait until next frame
		swiWaitForVBlank();
		
        app.ExecuteOneMainLoop();
		
	}
	
	return 0;
}