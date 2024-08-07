#include <nds.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <nf_lib.h>
#include <fat.h>
#include "nds/ndstypes.h"
#include <maxmod9.h>
#include <string.h>

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#define SCREEN_PADDING 10
#define TABLE_LENGTH (SCREEN_WIDTH - 2*SCREEN_PADDING + 1)
#define TABLE_MAX (SCREEN_HEIGHT - 2*SCREEN_PADDING + 1)

#define PRINT_WIDTH 32

#define SAMPLING_RATE 10000
#define BUFFER_SIZE 1200

#define DEPOP_FRAMES 50

class MidiInfo {
public:
    struct midi_info {
        char name[5];
        int midi_key_number;
        int pitch;
    };
    struct midi_info info[128];
    MidiInfo()
    : info {
        {"C-2", 0, 8},{"C#-2", 1, 8},{"D-2", 2, 9},{"D#-2", 3, 9},{"E-2", 4, 10},{"F-2", 5, 10},{"F#-2", 6, 11},{"G-2", 7, 12},{"G#-2", 8, 12},{"A-1", 9, 13},{"A#-1", 10, 14},{"B-1", 11, 15},{"C-1", 12, 16},{"C#-1", 13, 17},{"D-1", 14, 18},{"D#-1", 15, 19},{"E-1", 16, 20},{"F-1", 17, 21},{"F#-1", 18, 23},{"G-1", 19, 24},{"G#-1", 20, 25},{"A0", 21, 27},{"A#0", 22, 29},{"B0", 23, 30},{"C0", 24, 32},{"C#0", 25, 34},{"D0", 26, 36},{"D#0", 27, 38},{"E0", 28, 41},{"F0", 29, 43},{"F#0", 30, 46},{"G0", 31, 48},{"G#0", 32, 51},{"A1", 33, 55},{"A#1", 34, 58},{"B1", 35, 61},{"C1", 36, 65},{"C#1", 37, 69},{"D1", 38, 73},{"D#1", 39, 77},{"E1", 40, 82},{"F1", 41, 87},{"F#1", 42, 92},{"G1", 43, 97},{"G#1", 44, 103},{"A2", 45, 110},{"A#2", 46, 116},{"B2", 47, 123},{"C2", 48, 130},{"C#2", 49, 138},{"D2", 50, 146},{"D#2", 51, 155},{"E2", 52, 164},{"F2", 53, 174},{"F#2", 54, 184},{"G2", 55, 195},{"G#2", 56, 207},{"A3", 57, 220},{"A#3", 58, 233},{"B3", 59, 246},{"C3", 60, 261},{"C#3", 61, 277},{"D3", 62, 293},{"D#3", 63, 311},{"E3", 64, 329},{"F3", 65, 349},{"F#3", 66, 369},{"G3", 67, 391},{"G#3", 68, 415},{"A4", 69, 440},{"A#4", 70, 466},{"B4", 71, 493},{"C4", 72, 523},{"C#4", 73, 554},{"D4", 74, 587},{"D#4", 75, 622},{"E4", 76, 659},{"F4", 77, 698},{"F#4", 78, 739},{"G4", 79, 783},{"G#4", 80, 830},{"A5", 81, 880},{"A#5", 82, 932},{"B5", 83, 987},{"C5", 84, 1046},{"C#5", 85, 1108},{"D5", 86, 1174},{"D#5", 87, 1244},{"E5", 88, 1318},{"F5", 89, 1396},{"F#5", 90, 1479},{"G5", 91, 1567},{"G#5", 92, 1661},{"A6", 93, 1760},{"A#6", 94, 1864},{"B6", 95, 1975},{"C6", 96, 2093},{"C#6", 97, 2217},{"D6", 98, 2349},{"D#6", 99, 2489},{"E6", 100, 2637},{"F6", 101, 2793},{"F#6", 102, 2959},{"G6", 103, 3135},{"G#6", 104, 3322},{"A7", 105, 3520},{"A#7", 106, 3729},{"B7", 107, 3951},{"C7", 108, 4186},{"C#7", 109, 4434},{"D7", 110, 4698},{"D#7", 111, 4978},{"E7", 112, 5274},{"F7", 113, 5587},{"F#7", 114, 5919},{"G7", 115, 6271},{"G#7", 116, 6644},{"A8", 117, 7040},{"A#8", 118, 7458},{"B8", 119, 7902},{"C8", 120, 8372},{"C#8", 121, 8869},{"D8", 122, 9397},{"D#8", 123, 9956},{"E8", 124, 10548},{"F8", 125, 11175},{"F#8", 126, 11839},{"G8", 127, 12543}
    } {}
};

/**
 * NOTE TO FUTURE PROGRAMMERS - What's a SoundInfo?
 * 
 * Welcome to the SoundInfo struct, a super handy set of ints and bools that will
 * help you keep track of important audio data. You might notice that just below the
 * struct SoundInfo definition is a declaration of a global array of 13 SoundInfos called
 * sounds. 13 SoundInfos for the 13 keys of the EasyPiano Addon. 
 * 
 * One very important thing I'd like to tell you is that some of the fields in these structs
 * will be set automatically, they won't do anything to your sound unless you explicitly
 * choose to use the information held by them. You can ignore these entirely and make a Bytebeat
 * player if you so desire. Or you could use them and make a Bytebeat piano! The possibilities
 * are endless!
 * 
 * Some of the fields will be set for you automatically by the Piano class. These are "key,"
 * which tells you which of the 13 piano keys is associated with the SoundInfo; "playing," 
 * which tells you the key associated with the SoundInfo is being held; "justPressed," 
 * letting you know that the piano key was just pressed (this is handy if you need to fill
 * an array, initialize some variables, or take care of other business that should only happen
 * once, at the start of the tone. Make sure you set it to false once you're done doing
 * all your initialization, because this WON'T happen automatically. It will be set to true
 * again next time the key is pressed); "stopping," which lets you know the key was just released,
 * which can be useful if you need to add a release envelope; and "freq," which gives you an
 * integer approximation of the frequency associated with the key (see more about the frequency
 * array by the Piano class).
 * 
 * Other fields are only set as needed. You can use them if you want them, or you can ignore them
 * and do your own thing. I like to use "phaseFramesElapsed" as a counter telling me how many frames
 * have passed. I use it pretty often when calculating phase, hence the name. If you need a general
 * "totalFramesElapsed" field, you can add one.  
 */
struct SoundInfo {
    int key; // which key does this sound info go to?
	bool playing; // is the note currently playing?
    bool justPressed; // was the note just initially pressed (different from held)
    bool stopping;
    int phaseFramesElapsed;
    int freq;
    int depopFramesElapsed;
    int lastSampleOutputted;
};

struct SoundInfo sounds[13];

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
    void printInfo() { printf("%s", description); }

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

class KonamiCodeDetector {
    int _sequence[9];
    int _current;
public:
    KonamiCodeDetector() 
    : 
        _sequence {KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_START},
        _current {0}
    {}

    bool next(int key) {
        if (key & _sequence[_current])
            _current++;
        else
            _current = 0;

        if (_current == 9) {
            _current = 0;
            return true;
        } else {
            return false;
        }
    }
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
        14, 15, 15, 16, 17, 18, 19, 21, 22, 23, 24, 26, 
        28, 29, 31, 33, 35, 37, 39, 41, 44, 46, 49, 52, 
        55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104, 
        110, 117, 123, 131, 139, 147, 156, 165, 175, 185, 196, 208, 
        220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415, 
        440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831, 
        880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 
        1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 
        3520, 3729, 3951, 4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 
        7040, 7459, 7902, 8372, 8870, 9397, 9956, 10548, 11175, 11840, 12544, 13290
    }, pitch{3}, octave{4} {}
    

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
                    playKey(i);
                }
                else if (held.VAL & 1<<bitfieldShift) {
                    // I'll keep this here in case I ever need it
                }
                // if the key was just released kill the sound.
                else if (up.VAL & 1<<bitfieldShift) {
                    stopKey(i);
                }
            }
        }
    }

    void playTestTone() { playKey(0); }
    void stopTestTone() { stopKey(0); }

    void printRoot() {
        pc->cursorX = 0;
        pc->cursorY = 23;
        int noteNameIndex = pitch;
        // code snippet taken from https://shadyf.com/blog/notes/2016-07-16-modulo-for-negative-numbers/
        printf("Root: %s", noteNames[((noteNameIndex %= 12) < 0) ? noteNameIndex+12 : noteNameIndex]);
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
    int pitches[12 * 10];
    int pitch;
    int octave;

    /**
     * @param i the key to be played. must be [0, 13)
     */
    void playKey(int i) {
        int pitchIndex = pitch + (12 * octave) + i;
        if (pitchIndex >= 0 && pitchIndex < 120) { // only play if pitch is within the table
            sounds[i].playing = true;
            sounds[i].freq = pitches[pitch + (12 * octave) + i];
            sounds[i].phaseFramesElapsed = 0;
        }
    }

    void holdKey(int i) {
        sounds[i].justPressed = false;
    }

    /**
     * @param i the key to be stopped. must be [0, 13)
     */
    void stopKey(int i) {
        sounds[i].playing = false;
        sounds[i].stopping = true;
        sounds[i].justPressed = true;
    }
};

class Table : public Editor {
public:
    Table(const char * description, s16 (&table_)[TABLE_LENGTH]) : Editor(description), table(table_) {
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

    /**
     * NOTE TO FUTURE PROGRAMMERS
     * 
     * Welcome to probably the derpiest line drawing algorithm in existance. It works,
     * but only barely, and honestly that depends on how we define "works." It does everything
     * it needs to, but boy oh boy it is a stretch to call the this a line drawing algorithm.
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

class MultiSlider : public Editor {
public:
    MultiSlider(const char * description, int (&vals)[8], int numSliders, int maxVal) :
        Editor(description),
        _vals (vals),
        _numSliders {numSliders},
        _maxVal {maxVal},
        _hasLifted {true}
    {
        for (int i = 0; i < _numSliders; i++) {
            _sliders[i].rawx = _vals[i] + SCREEN_PADDING;
        }
    }

    void handleTouch() {
        int keysH = keysHeld();
        if (keysH & KEY_TOUCH) {
            touchRead(&touch);
            if (_hasLifted) {
                _currentSliderHeld = (touch.py - SCREEN_PADDING) * _numSliders / TABLE_MAX;
               _hasLifted = false;
            }

            int x = touch.px;
            if (x < SCREEN_PADDING)
                x = SCREEN_PADDING;
            if (x > SCREEN_WIDTH - SCREEN_PADDING)
                x = SCREEN_WIDTH - SCREEN_PADDING;

            _sliders[_currentSliderHeld].rawx = x;
            _vals[_currentSliderHeld] = ((x - SCREEN_PADDING) * _maxVal) / TABLE_MAX;

            drawSlider(_currentSliderHeld);
            
        } else {
            _hasLifted = true;
        }
    }

    void draw() {
        printInfo();
        drawSliders();
    }
private:
    int (&_vals)[8];
    int _numSliders;
    int _currentSliderHeld;
    int _maxVal;

    touchPosition touch;
    bool _hasLifted;

    struct slider {
        int rawx;
    };

    struct slider _sliders[8]; // multislider has max of 8 sliders

    void drawSlider(int slider) {

        int x = _sliders[slider].rawx;
        int yLow = SCREEN_PADDING + (slider*TABLE_MAX / _numSliders);
        int yHigh = SCREEN_PADDING + ((slider + 1)*TABLE_MAX / _numSliders) - 1;
        for (int i = SCREEN_PADDING; i <= SCREEN_WIDTH - SCREEN_PADDING; i++) {
            for (int j = yLow; j < yHigh; j++) {
                if (i == x)
                    VRAM_A[256 * j + i] = RGB15(31, 31, 31);
                else
                    VRAM_A[256 * j + i] = RGB15(0, 0, 0);
            }
            if (yHigh < SCREEN_PADDING + SCREEN_HEIGHT)
                VRAM_A[256 * yHigh + i] = RGB15(10, 10, 10);
        }  
    }

    void drawSliders() {
        for (int i = 0; i < _numSliders; i++)
            drawSlider(i);
    }
};

class MultiSwitch : public Editor {
public:
    MultiSwitch(const char * description, int (&vals)[8], int numSwitches, int maxVal) :
        Editor(description),
        _vals (vals),
        _numSwitches {numSwitches},
        _maxVal {maxVal},
        _hasLifted {true}
    {
        for (int i = 0; i < _numSwitches; i++) {
            _switches[i].rawx = SCREEN_PADDING;
        }
    }

    void handleTouch() {
        int keysH = keysHeld();
        if (keysH & KEY_TOUCH) {
            touchRead(&touch);
            if (_hasLifted) {
                _currentSwitchHeld = (touch.py - SCREEN_PADDING) * _numSwitches / TABLE_MAX;
               _hasLifted = false;
            }

            int x = touch.px;
            if (x < SCREEN_PADDING)
                x = SCREEN_PADDING;
            if (x > SCREEN_WIDTH - SCREEN_PADDING)
                x = SCREEN_WIDTH - SCREEN_PADDING;

            _switches[_currentSwitchHeld].rawx = x;
            _vals[_currentSwitchHeld] = ((x - SCREEN_PADDING) * _maxVal) / TABLE_LENGTH;

            drawSwitch(_currentSwitchHeld);
            
        } else {
            _hasLifted = true;
        }
    }

    void draw() {
        printInfo();
        drawSwitches();
    }
private:
    int (&_vals)[8];
    int _numSwitches;
    int _currentSwitchHeld;
    int _maxVal;

    touchPosition touch;
    bool _hasLifted;

    struct switchInfo {
        int rawx;
    };

    struct switchInfo _switches[8]; // multiswitch has max of 8 switches

    void drawSwitch(int whichSwitch) {

        //int x = _switches[whichSwitch].rawx - SCREEN_PADDING;
        int yLow = SCREEN_PADDING + (whichSwitch*TABLE_MAX / _numSwitches);
        int yHigh = SCREEN_PADDING + ((whichSwitch + 1)*TABLE_MAX / _numSwitches) - 1;
        for (int i = SCREEN_PADDING; i <= SCREEN_WIDTH - SCREEN_PADDING; i++) {
            int x = i - SCREEN_PADDING;
            for (int j = yLow; j < yHigh; j++) {
                if ((_vals[whichSwitch]*TABLE_LENGTH)/_maxVal <= x && x < ((_vals[whichSwitch] + 1)*TABLE_LENGTH)/_maxVal)
                    VRAM_A[256 * j + i] = RGB15(31, 31, 31);
                else
                    VRAM_A[256 * j + i] = RGB15(0, 0, 0);
            }
            if (yHigh < SCREEN_PADDING + SCREEN_HEIGHT)
                VRAM_A[256 * yHigh + i] = RGB15(10, 10, 10);
        }  
    }

    void drawSwitches() {
        for (int i = 0; i < _numSwitches; i++)
            drawSwitch(i);
    }
};

class EmptyEditor : public Editor {
public:
    EmptyEditor(const char * description) : Editor(description) {}
    void draw() {
        clear();
        printInfo();
    }
    void handleTouch() {}
};

/**
 * Uses Xorshift algorithm copied from https://en.wikipedia.org/wiki/Xorshift.
 */
class Random {
public:
    Random() {
        state.a = 347810;
    }

    /**
     * @return true half of the time
     */
    bool coinFlip() {
        return xorshift32(&state) & 1;
    }

    /**
     * @return true a out of b times
     */
    bool prob(uint32_t a, uint32_t b) {
        if (a == 0) {
            return false;
        } else if (a == b) {
            return true;
        } else {
            return xorshift32(&state) % b <= a;
        }
    }

    /**
     * fills an array with random values
     */
    void randArray(s16 * arr, int length, int max) {
        for (int i = 0 ; i < length; i++) {
            arr[i] = xorshift32(&state) % max;
        }
    }
private:
    struct xorshift32_state {
        uint32_t a;
    };

    struct xorshift32_state state;

    /* The state must be initialized to non-zero */
    uint32_t xorshift32(struct xorshift32_state *state)
    {
        /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
        uint32_t x = state->a;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return state->a = x;
    }
};


mm_ds_system sys;
mm_stream mystream;

/**
 * sinLerp angle from -32768 to 32767. range of 65535
 */
class Sine {
public:
    int _framesElapsed;
    int64 _phase;

    Sine(int samplingRate) :
        _framesElapsed {0},
        _samplingRate{samplingRate},
        _t{0},
        _freq{0} {}

    void reset() { _t = 0; _framesElapsed = 0; }

    /*int largePhase() {
        return (_framesElapsed++ * _freq * 65536) / _samplingRate;
    }*/

    s16 sin(int freq) {
        _phase = ((_t++ - 32768) * (_freq = freq) * 65536) / _samplingRate;
        if (_phase > 32767) {
            _phase -= 65536;
            _t = 0;
        }
        return sinLerp(_phase);
    }
    
private:
    int _samplingRate;
    int64 _t;
    int _freq;
    
};

/**
 * NOTE TO FUTURE PROGRAMMERS - How to make your very own synth!
 * 
 * You, yes YOU, can be a synth designer. All you need to do is implement Synth's
 * one virtual method, "s16 getOutputSample(struct SoundInfo * sound)." To learn more
 * about the SoundInfo struct, read the note just above its definition.
 * 
 * You can do pretty much anything you want to. Anything. I made one synthesizer that
 * applies bubble sort to the sample! If you want you can ignore the piano entirely.
 * You could make a synth that only plays the 42 Melody (look it up; it's cool). You
 * don't even need to output audio if you don't want to! You could probably
 * build your own custom Editor (see note by Editor class) and make your synth play
 * Super Mario Bros using the piano keys.
 * 
 * Here are some words of advice.
 * 1. The application feeds the output of "s16 frameOutput()" directly to the audio
 *    stream without any interferance. "s16 frameOutput()" only adds up the output of
 *    "s16 getOutputSample(struct SoundInfo * sound)" for each key. You implement this
 *    method. You don't need to worry about anything messing with your audio but you.
 * 2. This is 16 bit signed audio. If you're output is too loud, it'll overflow
 *    and your ears may not like it (or you could do it intentionally because
 *    you're into that kind of thing). Make sure that your output is quiet enough
 *    to sound good when you're pressing several keys. If you're not sure whether
 *    or not it'll be too loud, just run the program and check.
 * 3. I know I said you can do pretty much anything, but unfortunately you'll probably
 *    have to worry about sample rate. If the sample rate is too high, you'll
 *    hear unpleasant popping noises as the CPU tries and fails to fill the audio buffer
 *    as quickly as it needs to. The more calculation intensive you're synth is,
 *    the lower the sample rate will have to be. Sorry.
 * 
 * For more information on how to connect your shiny new synth to the rest of the
 * application, go to the note above the App class.
 */
class Synth {
public:
    Synth(int gain, int samplingRate, bool sfzExportAvailable) :
        _gain{gain},
        _samplingRate{samplingRate},
        _sfzExportAvailable{sfzExportAvailable}
    {}

    virtual s16 frameOutput() {
        s16 output = 0;
        for (int i = 0; i < 13; i++) {
            output += getOutputSample(&sounds[i]);
        }
        return output;
    }

    void mmChangeSettings() {
        mmStreamClose();
        mystream.sampling_rate = _samplingRate;
        mmStreamOpen( &mystream );
    }
    
    bool isExporting() {
        return wavExport.exporting;
    }

    struct wav_header {
        char riff[4];
        int32_t flength;
        char wave[4];
        char fmt[4];
        int32_t chunk_size;
        int16_t format_tag;
        int16_t num_chans;
        int32_t sample_rate;
        int32_t bytes_per_second;
        int16_t bytes_per_sample;
        int16_t bits_per_sample;
        char data[4];
        int32_t dlength;
    };

    void exportSingleSample(FILE * sampleFile, int freq) {
        //printf("exporting sample... ");
        
        struct wav_header wavh;
        strncpy(wavh.riff, "RIFF", 4);
        strncpy(wavh.wave, "WAVE", 4);
        strncpy(wavh.fmt, "fmt ", 4);
        strncpy(wavh.data, "data", 4);

        wavh.chunk_size = 16;
        wavh.format_tag = 1;
        wavh.num_chans = 1;
        wavh.sample_rate = _samplingRate;
        wavh.bits_per_sample = 16;
        wavh.bytes_per_sample = (wavh.bits_per_sample * wavh.num_chans) / 8;
        wavh.bytes_per_second = wavh.sample_rate * wavh.bytes_per_sample;
        wavh.dlength = 0;
        wavh.flength = 0;

        
        wavExport.exporting = true;
        
        struct SoundInfo sampleInfo;
        sampleInfo.playing = true;
        sampleInfo.freq = freq;
        sampleInfo.phaseFramesElapsed = 0;
        sampleInfo.justPressed = true;
        while (wavExport.exporting) { // first we need to find out how long the sample is going to be
            getOutputSample(&sampleInfo);
        }

        wavh.dlength = (wavExport.exportFramesElapsed + 1) * wavh.bytes_per_sample;
        wavh.flength = wavh.dlength + 44;

        fwrite(&wavh, sizeof(wavh), 1, sampleFile);

        //fseek(sampleFile, 45, SEEK_SET);
        //struct SoundInfo sampleInfo;
        sampleInfo.playing = true;
        sampleInfo.freq = freq;
        sampleInfo.phaseFramesElapsed = 0;
        sampleInfo.justPressed = true;
        wavExport.exporting = true;
        while (wavExport.exporting) {
            s16 output = getOutputSample(&sampleInfo);
            fwrite(&output, sizeof(output), 1, sampleFile);
        }

        /*
        wavh.dlength = (wavExport.exportFramesElapsed + 1) * wavh.bytes_per_sample;
        wavh.flength = wavh.dlength + 44;
        fseek(sampleFile, 0, SEEK_SET);
        fwrite(&wavh, sizeof(wavh), 1, sampleFile);
        */
    }

    /**
     * NOTE FOR FUTURE PROGRAMMERS - How to implement sfz export
     * 
     * In order to implement sfz export in the synthesizer class of your choice, all you need
     * to do is make sure that the synthesizer properly fills all of the fields of the
     * exportFrameData struct called wavExport. wavExport is a protected member, so all children
     * of Synth have a their own copy.
     * 
     * All you need to do is make sure that the synthesizer will:
     * 1. increment wavExport.exportFramesElapsed every frame
     * 2. set wavExport.exporting to false to finish sampling (otherwise it will infinitely loop)
     * 3. set wavExport.loopStart and wavExport.loopEnd to the frames you will to loop around
     */
    void exportSFZ() {
        pc->cursorX = 0;
        pc->cursorY = 20;
        if (!_sfzExportAvailable) {
            printf("sfz export not available\n for this synth");
            return;
        }
        printf("exporting");
        FILE* sfz = fopen("sfz/export.sfz", "w");
        char global_parameters[] = "<global> loop_mode=loop_continuous\n\n";
        fwrite(global_parameters, sizeof(char), strlen(global_parameters), sfz);
        fclose(sfz);

        MidiInfo midi = MidiInfo();
        for (int midi_index = 0; midi_index < 128; midi_index++) {
            char file_name[64];
            sprintf(file_name, "sfz/%s.wav", midi.info[midi_index].name);
            FILE* sample = fopen(file_name, "w+");

            exportSingleSample(sample, midi.info[midi_index].pitch);

            //printf("past export sample");

            fclose(sample);

            sprintf(file_name, "%s.wav", midi.info[midi_index].name);

            char sfz_region_data[128];
            sprintf(
                sfz_region_data,
                "<region> sample=%s key=%d loop_start=%d loop_end=%d\n\n",
                file_name,
                midi.info[midi_index].midi_key_number,
                wavExport.loopStart,
                wavExport.loopEnd
            );
            sfz = fopen("sfz/export.sfz", "a");
            fwrite(sfz_region_data, sizeof(char), strlen(sfz_region_data), sfz);
            fclose(sfz);

            pc->cursorX = Lerp::lerp(0, PRINT_WIDTH, midi_index, 128);
            pc->cursorY = 21;
            printf("|");
        }

        fclose(sfz);
        pc->cursorX = 0;
        pc->cursorY = 20;
        printf("done                ");
    }
    
protected:
    int _gain;
    int _samplingRate;
    bool _sfzExportAvailable;
    
    struct exportFrameData {
        bool exporting; // while exporting is true, continue retrieving samples
        int exportFramesElapsed; // the frame of the export we are on
        int loopStart; // the sample at which the looping portion starts
        int loopEnd; // the sample at which the looping portion ends
    };

    struct exportFrameData wavExport;
    
    virtual s16 getOutputSample(struct SoundInfo * sound) = 0;
};

class EmptySynth : public Synth {
public:
    EmptySynth(int gain, int sampleRate) : Synth(gain, sampleRate, false) {}
private:
    s16 getOutputSample(struct SoundInfo * sound) {
        return sound->playing?(((sound->phaseFramesElapsed++%(_samplingRate/sound->freq))>_samplingRate/(2*sound->freq))?_gain:-_gain):0;
    }
};

/*class VariableLengthWavetable : public Synth {
public:
protected:
    struct info {
        s16 previous;
        int length;
        s16 table[3000];
    };
    struct info infos[13];
};*/

class ExcitedString : public Synth {
public:
    ExcitedString(int gain, int samplingRate, s16 (&table)[TABLE_LENGTH], int &slider1Val, int &switchVal) :
    Synth(gain, samplingRate, false),
    _table(table),
    _slider1Val(slider1Val),
    _switchVal(switchVal) {}

    void exportSFZ() {}
private:
    Random randy;

    s16 (&_table)[TABLE_LENGTH];
    int &_slider1Val;
    int &_switchVal;

    struct ESInfo {
        s16 previous;
        int length;
        s16 table[3000];
    };
    struct ESInfo infos[13];

    s16 getOutputSample(struct SoundInfo * sound) {
        struct ESInfo * info = &infos[sound->key];
        if (sound->playing) {
            info->length = _samplingRate / sound->freq;
            if (sound->justPressed) {
                switch (_switchVal) {
                    case 0: { // fill the burst table with random
                        for (int i = 0; i < info->length; i++) {
                           info->table[i] = rand() % TABLE_MAX;
                        }
                        break;
                    }
                    case 1: { // fill the burst table with a squeezed rendition of the _table (filled by a table editor)
                        for (int i = 0; i < info->length; i++) {
                            info->table[i] = _table[Lerp::lerp(0, TABLE_LENGTH - 1, i, info->length - 1)];
                        }
                        break;
                    }
                }
                info->previous = info->table[info->length];
                sound->justPressed = false;
            }
            s16 output;
            if (randy.prob(_slider1Val, TABLE_LENGTH - 1)) {
                output = info->table[sound->phaseFramesElapsed] += randy.coinFlip() ? 16 : -16;
            } else {
                output = info->previous =
                    info->table[sound->phaseFramesElapsed] =
                    (info->table[sound->phaseFramesElapsed] + info->previous) >> 1;
            }
            sound->phaseFramesElapsed++;
            sound->phaseFramesElapsed %= info->length;
            return _gain * output;
        } else {
            return 0;
        }
    }

};

class BubbleSort : public Synth {
public:
    BubbleSort(int gain, int samplingRate, s16 (&table)[TABLE_LENGTH], int &slider1Val, int &switchVal) :
    Synth(gain, samplingRate, false),
    _table(table),
    _slider1Val(slider1Val),
    _switchVal(switchVal) {}

    void exportSFZ() {}
private:
    s16 (&_table)[TABLE_LENGTH];
    int &_slider1Val; // used for probabalistic stretching
    int &_switchVal; // fill the table with random burst or user drawn table
    
    Random randy;

    struct bubbleInfo {
        int previousPhase;
        s16 table[TABLE_LENGTH];
    };

   struct bubbleInfo bubbles[13];

    int getWavePhase(struct SoundInfo * sound) {
        int phase = div32((sound->phaseFramesElapsed * sound->freq * TABLE_LENGTH), _samplingRate);
        if (phase > 8 * TABLE_LENGTH) {
            sound->phaseFramesElapsed = 0;
        }
        return phase % TABLE_LENGTH;
    }

    void incrementFrameCount(struct SoundInfo * sound) {
        sound->phaseFramesElapsed++;
    }

    s16 getOutputSample(struct SoundInfo * sound) {
        if (sound->playing) {
            struct bubbleInfo *bubble = &bubbles[sound->key];
            if (sound->justPressed) {
                switch (_switchVal) {
                    case 0:  { // fill random
                        randy.randArray(bubble->table, TABLE_LENGTH, TABLE_MAX);
                        break;
                    }
                    case 1: { // fill with table editor
                        for (int i = 0; i < TABLE_LENGTH; i++) {
                            bubble->table[i] = _table[i];
                        }
                        break;
                    }   
                }
                sound->justPressed = false;
                bubble->previousPhase = 0;
            }

            int phase = getWavePhase(sound);
            s16 previous = bubble->table[bubble->previousPhase];
            s16 current = bubble->table[phase];
            if (bubble->previousPhase < phase && previous > current && randy.prob(_slider1Val, TABLE_LENGTH - 1)) {
                bubble->table[bubble->previousPhase] = current;
                bubble->table[phase] = previous;
            }
            bubble->previousPhase = phase;

            incrementFrameCount(sound);
            
            return current * _gain;
        } else {
            return 0;
        }
    }
};

class XOR : public Synth {
public:
    XOR(int gain, int samplingRate, s16 (&table)[TABLE_LENGTH], int &slider1Val, int &switchVal) :
    Synth(gain, samplingRate, false),
    _table(table),
    _slider1Val(slider1Val),
    _switchVal(switchVal) {}

    void exportSFZ() {}
private:
    Random randy;

    s16 (&_table)[TABLE_LENGTH];
    int &_slider1Val;
    int &_switchVal;

    struct xorInfo {
        s16 previous;
        s16 table[TABLE_LENGTH];
    };

    struct xorInfo infos[13];

    int getWavePhase(struct SoundInfo * sound) {
        int phase = div32((sound->phaseFramesElapsed * sound->freq * TABLE_LENGTH), _samplingRate);
        if (phase > 8 * TABLE_LENGTH) {
            sound->phaseFramesElapsed = 0;
        }
        return phase % TABLE_LENGTH;
    }

    void incrementFrameCount(struct SoundInfo * sound) {
        sound->phaseFramesElapsed++;
    }


    s16 getOutputSample(struct SoundInfo * sound) {
        struct xorInfo * info = &infos[sound->key];
        if (sound->playing) {
            if (sound->justPressed) {
                switch (_switchVal) {
                case 0: // fill random
                randy.randArray(info->table, TABLE_LENGTH, TABLE_MAX);
                    break;
                case 1: // fill with table
                    for (int i = 0; i < TABLE_LENGTH; i++)
                        info->table[i] = _table[i];
                    break;
                }
                info->previous = info->table[TABLE_MAX - 1];
                sound->justPressed = false;
            }
        
            int phase = getWavePhase(sound);
            int current = info->table[phase];
            incrementFrameCount(sound);

            if (randy.prob(_slider1Val, TABLE_LENGTH)) {
                current ^= info->previous;
                info->table[phase] = current;
            }

            info->previous = current;
            return _gain * current;
        } else {
            return 0;
        }
    }
};


class Novelty : public Synth {
public:
    Novelty(int gain, int samplingRate, int &algorithm, s16 (&table)[TABLE_LENGTH], int &slider1Val, int &switchVal) :
    Synth(gain, samplingRate, false),
    _algorithm(algorithm),
    _table(table),
    _slider1Val(slider1Val),
    _switchVal(switchVal),
    bort(_gain, _samplingRate, _table, _slider1Val, _switchVal),
    exor(_gain, _samplingRate, _table, _slider1Val, _switchVal),
    erin(_gain, _samplingRate, _table, _slider1Val, _switchVal) {}

    s16 frameOutput() override {
        switch (_algorithm) {
            case 0: { // Bubble Sort
                return bort.frameOutput();
            }
            case 1: { // XOR
                return exor.frameOutput();
            }
            case 2: { // Excited String
                return erin.frameOutput();
            }
        }
        return 0;
    }

    void exportSFZ() {}
private:
    int &_algorithm;
    s16 (&_table)[TABLE_LENGTH];
    int &_slider1Val;
    int &_switchVal;
    
    BubbleSort bort;
    XOR exor;
    ExcitedString erin;

    s16 getOutputSample(struct SoundInfo * sound) { return 0; }
};

class FM : public Synth {
public:
    FM(int gain, int samplingRate, int (&amps)[8], int (&routings)[8], int (&ratios)[8]) :
        Synth(gain, samplingRate, false),
        _amps (amps),
        _routings (routings),
        _ratios (ratios)
    {
        for (int i = 0; i < 13; i++) {
            for (int j = 0; j < 4; j++) {
                infos[i].ops[j] = new Operator(_samplingRate, infos[i].ops, 4, j, _amps[j], _routings[j], _ratios[j]);
            }
        }
    }

    void exportSFZ() {}
private:
    int (&_amps)[8];
    int (&_routings)[8];
    int (&_ratios)[8];

    class Operator {
    public:
        Operator(int samplingRate, Operator **ops, int numOps, int id, int &amp, int &routing, int &ratio) :
            _sine(samplingRate),
            _ops {ops},
            _numOps {numOps},
            _id {id},
            _amp (amp),
            _routing (routing),
            _ratio (ratio) {}

        s16 evaluate(int freq) {
            if (_routing == 5) {
                return 0;
            } else {
                bool doReset = false; // _sine._phase > 65536;
                if (doReset)
                    resetSine();
                s16 modulatorOutput = 0;
                for (int i = 0; i < _numOps; i++) { // look for modulators
                    if (_ops[i]->_routing == _id) { // if another operator has this one as a carrier, then...
                        if (doReset)
                            _ops[i]->resetSine();
                        modulatorOutput += _ops[i]->evaluate(freq) / 256;
                    }
                }
                return _amp * _sine.sin(_ratio*freq + modulatorOutput) / TABLE_LENGTH;
            }
        }

        bool doOutput() {
            return _routing == 4; 
        }
    private:
        Sine _sine;
        Operator **_ops;
        int _numOps;
        int _id;
        int &_amp;
        int &_routing;
        int &_ratio;

        void resetSine() {
            _sine.reset();
        }
    };

    struct fmInfo {
        Operator *ops[4];
    };
    struct fmInfo infos[13];

    s16 getOutputSample(struct SoundInfo * sound) {
        if (sound->playing) {
            s16 output = 0;
            for (int i = 0; i < 4; i++) {
                if (infos[sound->key].ops[i]->doOutput()) {
                    output += infos[sound->key].ops[i]->evaluate(sound->freq);
                }
            }
            return output;
        } else {
            return 0;
        }
    }
};

class PluckedString : public Synth {
public:
// pling(27, 20000, blendFactor, burstType, burstArray),
    PluckedString(
        int gain,
        int samplingRate,
        int &blendFactor,
        int &burstType,
        s16 (&burstArray)[TABLE_LENGTH]
    ) : 
        Synth(gain, samplingRate, false),
        _blendFactor (blendFactor),
        _burstType (burstType),
        _burstArray (burstArray)
    {}

    void exportSFZ() {}
private:
    int &_blendFactor;
    int &_burstType;
    s16 (&_burstArray)[TABLE_LENGTH];

    struct pluckInfo {
        int length;
        int phase;
        int previous;
        int table[3000];
    };

    Random randy;

    struct pluckInfo plucks[13];
    
    s16 getOutputSample(struct SoundInfo * sound) {
        if (sound->playing) {
            struct pluckInfo *pluck = &plucks[sound->key];
            if (sound->justPressed) {
                // 1. calculate length
                pluck->length = _samplingRate / sound->freq;
                // 2. fill the burst table
                switch (_burstType) {
                    case 0: { // fill the burst table with random
                        for (int i = 0; i < pluck->length; i++) {
                            pluck->table[i] = rand() % TABLE_MAX;
                        }
                        break;
                    }
                    case 1: { // fill the burst table with a squeezed rendition of the burstArray (filled by a table editor)
                        for (int i = 0; i < pluck->length; i++) {
                            pluck->table[i] = _burstArray[Lerp::lerp(0, TABLE_LENGTH - 1, i, pluck->length - 1)];
                        }
                        break;
                    }
                }
                pluck->phase = 0;
                // 3. initialize previous
                pluck->previous = pluck->table[0];
                // 4. the key is no longer just pressed
                sound->justPressed = false;
            }
            int phase = pluck->phase++ % pluck->length;
            int current = pluck->table[phase];
            pluck->table[phase] = (randy.prob(_blendFactor, TABLE_LENGTH - 1) ? 1 : -1) * ((current + pluck->previous) >> 1);
            pluck->previous = current;
            return _gain * pluck->table[phase];
        } else {
            return 0;
        }
    }

};

class Wavetable : public Synth {
public:
    Wavetable(
        int gain,
        int samplingRate,
        s16 (&wave1Array)[TABLE_LENGTH],
        s16 (&wave2Array)[TABLE_LENGTH],
        s16 (&transition)[TABLE_LENGTH],
        int &transitionTime,
        int &algorithm,
        int &transitionCycle
     ) :
        Synth(gain, samplingRate, true),
        _wave1Array (wave1Array),
        _wave2Array (wave2Array),
        _transition (transition),
        _transitionTime (transitionTime),
        _algorithm (algorithm),
        _transitionCycle (transitionCycle)
    {
        for (int i = 0; i < TABLE_LENGTH; i++) {
            wave1Array[i] = 0;
            wave2Array[i] = 0;
            transition[i] = 0;
        }
    }

    
private:
    s16 (&_wave1Array)[TABLE_LENGTH];
    s16 (&_wave2Array)[TABLE_LENGTH];
    s16 (&_transition)[TABLE_LENGTH];
    int &_transitionTime;
    int &_algorithm;
    int &_transitionCycle;

    struct wableInfo {
        int transitionFramesElapsed;
        bool pingPongDirection;
    };

    struct wableInfo infos[13];

    

    int getWavePhase(struct SoundInfo * sound) {
	    int phase = div32((sound->phaseFramesElapsed * sound->freq * TABLE_LENGTH), _samplingRate);
        
        /*phase %= TABLE_LENGTH;
        sound->phaseFramesElapsed %= _samplingRate;
        if (sound->phaseFramesElapsed == 0) {
            if (wavExport.exporting) {
                // if the transition cycle is in forward mode and the export frames elapsed is greater than the max transition time,
                // then we need to start setting up loop points and end the exporting process
                if (_transitionCycle == 0 && wavExport.exportFramesElapsed > _transitionTime) {
                    if (wavExport.loopStart == -1) {
                        wavExport.loopStart = wavExport.exportFramesElapsed;
                    } else {
                        wavExport.loopEnd = wavExport.exportFramesElapsed;
                        wavExport.exporting = false;
                    }
                }
            }
        }
        return phase;*/

        if (phase > 8 * TABLE_LENGTH) {
            sound->phaseFramesElapsed = 0;
            if (wavExport.exporting) {
                // if the transition cycle is in forward mode and the export frames elapsed is greater than the max transition time,
                // then we need to start setting up loop points and end the exporting process
                if (_transitionCycle == 0 && wavExport.exportFramesElapsed > _transitionTime) {
                    if (wavExport.loopStart == -1) {
                        wavExport.loopStart = wavExport.exportFramesElapsed;
                    } else {
                        wavExport.loopEnd = wavExport.exportFramesElapsed - 1;
                        wavExport.exporting = false;
                    }
                }
            }
        }
        return phase % TABLE_LENGTH;
    }

    int getTransitionIndex(struct SoundInfo * sound) {
        struct wableInfo * info = &infos[sound->key];
        return Lerp::lerp(0, TABLE_LENGTH - 1, info->transitionFramesElapsed, _transitionTime);
    }

    void incrementFrameCount(struct SoundInfo * sound) {
        struct wableInfo * info = &infos[sound->key];
        sound->phaseFramesElapsed++;
        switch (_transitionCycle) {
            case 0:
                info->transitionFramesElapsed++;
                break;
            case 1:
                info->transitionFramesElapsed = (info->transitionFramesElapsed + 1) % _transitionTime;
                if (wavExport.exporting && wavExport.exportFramesElapsed >= _transitionTime) {
                    wavExport.loopStart = 0;
                    wavExport.loopEnd = wavExport.exportFramesElapsed - 1;
                    wavExport.exporting = false;
                }
                break;
            case 2: {
                if (info->pingPongDirection == true) {
                    if (info->transitionFramesElapsed++ >= _transitionTime)
                        info->pingPongDirection = false;
                } else {
                    if (info->transitionFramesElapsed-- <= 0) {
                        info->pingPongDirection = true;
                        if (wavExport.exporting) {
                            wavExport.loopStart = 0;
                            wavExport.loopEnd = wavExport.exportFramesElapsed - 1;
                            wavExport.exporting = false;
                        }
                    }
                }
                break;
            }
            
        }
    }

    s16 getOutputSample(struct SoundInfo * sound) {
        struct wableInfo * info = &infos[sound->key];
        if (sound->playing) {
            if (sound->justPressed) {
                if (wavExport.exporting) {
                    wavExport.exportFramesElapsed = 0;
                    wavExport.loopStart = -1;
                    wavExport.loopEnd = -1;
                }
                info->pingPongDirection = true;
                info->transitionFramesElapsed = 0;
                sound->justPressed = false;
            }
            int phase = getWavePhase(sound);
            s16 sample1 = _wave1Array[phase];
            s16 sample2 = _wave2Array[phase];
            
            int transitionValue = _transition[getTransitionIndex(sound)];

            int output;
            switch (_algorithm) {
                case 0: { // Morph algorithm
                    output = Lerp::lerp(sample1, sample2, transitionValue, TABLE_MAX - 1);
                    break;
                }
                case 1: { // Swipe algorithm
                    int split = Lerp::lerp(0, TABLE_LENGTH - 1, transitionValue, TABLE_MAX - 1);
                    if (phase > split)
                        output = sample1;
                    else
                        output = sample2;
                    break;
                }
                case 2: { // using a combination of both algorithms
                    int morph = Lerp::lerp(sample1, sample2, transitionValue, TABLE_MAX - 1);
                    int swipe;
                    int split = Lerp::lerp(0, TABLE_LENGTH - 1, transitionValue, TABLE_MAX - 1);
                    if (phase > split)
                        swipe = sample1;
                    else
                        swipe = sample2;
                    output = Lerp::lerp(swipe, morph, transitionValue, TABLE_MAX - 1);
                    break;
                }
                default: // if the default is reached, something went wrong
                    output = 0;
            }

            // if the note just started, depop by lerping to initial output
            if (sound->depopFramesElapsed < DEPOP_FRAMES) {
                output = Lerp::lerp(0, output, sound->depopFramesElapsed++, DEPOP_FRAMES);
            } else {
                incrementFrameCount(sound);
            }
            
            sound->lastSampleOutputted = _gain * output;

            // if the sound is exporting, increment export frame count
            if (wavExport.exporting)
                wavExport.exportFramesElapsed++;

            return _gain * output;
        } else {
            int output;
            if (sound->stopping) {
                output = Lerp::lerp(0, sound->lastSampleOutputted, sound->depopFramesElapsed--, DEPOP_FRAMES);
                if (sound->depopFramesElapsed <= 0)
                    sound->stopping = false;
            } else {
                output = 0;
            }

            return output;
        }
    }

};

/**
 * NOTE TO FUTURE PROGRAMMERS - The App Class.
 * 
 * Welcome to the App class. An app object is in charge of handling button presses, sending
 * the output of synthesizers to the audio stream, and connecting the many editors to
 * synths and to one another.
 * 
 * Before I explain how to connect your lovingly crafted synthesizer to the rest of the
 * application, I'd like you to come with me on a journey to the main loop. It's at the
 * bottom of the file.
 */
class App {
public:
    App() :
        synEdPairRing(), 

        tutorialEditorRing(),
        welcome("Welcome to the Wavetable Synth \n for the Nintendo DS!\n\nYou're probably wondering how to operate this application, but\n wonder no longer! I'm forcing\n you, yes YOU, to take part in\n a magnificent tutorial!\n\nPress the right shoulder button\n to continue. . ."),
        tableTutorial("This is a table editor. In this\n application you will navigate \n through various editors that \n you can use to control the \n sound in several ways.\n\nTry drawing on the touch screen\n\nTo cycle through editor screens\n use shoulder buttons. Use the \n right shouder button to \n continue. . .", tutorialTable),
        buttonsTutorial("There are three main ways to \n make sound with this app:\n 1. Pressing X to play a\n    testing tone\n 2. Using the Easy Piano Option\n    Pak\n 3. Exporting an sfz file to\n    use with other music\n    software\n\nUse the d-pad to change the\n root note of the app. Vertical\n directions for octave and \n horizontal for semitone.\n\nPress the right shoulder button\n to continue. . ."),
        sfzExportTutorial("If you use the Konami Code\n while in a synth mode that\n supports sfz exporting, then\n sfz exporting will begin.\n Consult the README for file\n setup.\n\nHeadphones are suggested as the\n DS's speakers can be rather\n quiet.\n\nUse the select button to cycle\n through synth modes and exit\n this tutorial. . ."),
        empth(1500, 20000),
        
        wavetableEditorRing(),
        waveTableOne("Wavetable One\n\nA wavetable synthesizer works\n by taking one period of a wave\n and looping through it at\n various frequencies.\n\nUse the table editor below to\n draw one period of a wave.", wave1Array),
        waveTableTwo("Wavetable Two\n\nUse this editor to draw another\n wave.", wave2Array),
        morphShapeTable("Transition Shape\n\nThis table editor isn't used to\n draw a wave. Instead, it is\n used to define how wave 1 will\n transition to wave 2 over time.\n\nFully up means only the first\n wave will play. Fully down\n means only the second wave\n plays. Halfway means a wave\n halfway between both waves\n plays.", transition),
        morphTimeSlider("Transition Time\n Left:  0 seconds\n Right: 10 seconds\n\nThis slider determines how long\n it takes to go through the\n transition shape.", transitionTime, SAMPLING_RATE * 10),
        algorithmSwitch("Transition Algorithm\n 1. Morph\n 2. Swipe\n 3. Combo\n\nWhat does halfway between two\n waves mean anyway?\n\nIn my opinion, I see two main\n ways of interpreting this:\n 1. morph: an average of both\n    waves\n 2. swipe: the first half of\n    wave 1 tacked onto the\n    second half of wave 2\n", algorithm, 3),
        transitionCycleSwitch("Transition Cycle Mode\n 1. Forward\n 2. Loop\n 3. Ping Pong\n\nIn forward mode, when the right\n of the transition shape is\n reached, it stays at the right\nIn loop mode, when the right is\n reached, it loops back to the\n left of the transition shape\nIn ping-pong mode, when the\n right is reached, it starts\n going backwards to the left,\n then back to the right, ad\n infinitum.", transitionCycle, 3),
        wable(31, 10000, wave1Array, wave2Array, transition, transitionTime, algorithm, transitionCycle),

        pluckedEditorRing(),
        drumSlider("Blend Factor\n Left:   ???\n Middle: Drum\n Right:  Plucked String", blendFactor, TABLE_LENGTH),
        burstTypeSwitch("Burst Type\n 1. Random\n 2. Wavetable", burstType, 2),
        burstTable("Burst Wavetable", burstArray),
        pling(27, 20000, blendFactor, burstType, burstArray),

        noveltyEditorRing(),
        novAlg{0},
        noveltyAlgorithmSwitch("Novelty Algorithms\n 0. Bubble Sort\n 1. XOR\n 2. Excited String", novAlg, 3),
        noveltyTable("Table", novTab),
        novSlid1{0},
        noveltySlider1("Slider", novSlid1, TABLE_LENGTH - 1),
        novSwitch{0},
        noveltySwitch("Switch", novSwitch, 2),
        novel(27, 20000, novAlg, novTab, novSlid1, novSwitch),

        fmEditorRing(),
        fmAmpVals {TABLE_LENGTH - 1, 0, 0, 0},
        fmAmpMultiSlider("Operator Amplitudes (Ops 1-4)", fmAmpVals, 4, TABLE_LENGTH),
        fmRouting {4, 5, 5, 5},
        fmRoutingMultiSwitch("Operator Routing", fmRouting, 4, 6),
        fmRatios {1, 1, 1, 1},
        fmRatioMultiSwitch("Operator Ratio", fmRatios, 4, 13),
        fam(1, 8192, fmAmpVals, fmRouting, fmRatios)
    {

        tutorialEditorRing.add(&sfzExportTutorial);
        tutorialEditorRing.add(&buttonsTutorial);
        tutorialEditorRing.add(&tableTutorial);
        tutorialEditorRing.add(&welcome);

        wavetableEditorRing.add(&transitionCycleSwitch);
        wavetableEditorRing.add(&algorithmSwitch);
        wavetableEditorRing.add(&morphTimeSlider);
        wavetableEditorRing.add(&morphShapeTable);
        wavetableEditorRing.add(&waveTableTwo);
        wavetableEditorRing.add(&waveTableOne);

        pluckedEditorRing.add(&burstTable);
        pluckedEditorRing.add(&burstTypeSwitch);
        pluckedEditorRing.add(&drumSlider);

        noveltyEditorRing.add(&noveltySwitch);
        noveltyEditorRing.add(&noveltySlider1);
        noveltyEditorRing.add(&noveltyTable);
        noveltyEditorRing.add(&noveltyAlgorithmSwitch);

        fmEditorRing.add(&fmRatioMultiSwitch);
        fmEditorRing.add(&fmRoutingMultiSwitch);
        fmEditorRing.add(&fmAmpMultiSlider);

        synEdPairRing.add(new SynEdPair("FM\n\n", &fmEditorRing, &fam));
        synEdPairRing.add(new SynEdPair("NOVELTY\n\n", &noveltyEditorRing, &novel));
        synEdPairRing.add(new SynEdPair("PLUCKED STRING\n\n", &pluckedEditorRing, &pling));
        synEdPairRing.add(new SynEdPair("WAVETABLE SYNTH\n\n", &wavetableEditorRing, &wable));
        synEdPairRing.add(new SynEdPair("TUTORIAL\n\n", &tutorialEditorRing, &empth));
        
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
        synEdPairRing.curr()->onEditorSwitch();
    }

    /**
     * Executes one main loop of the program
     */
    void ExecuteOneMainLoop() {
        handleButtons();
        synEdPairRing.curr()->getEditorRing()->curr()->handleTouch();
        piano.resamplePianoKeys();
    }

    /**
     * this handles each loop in the audio stream
     * 
     * it returns the sample to output
     */
    s16 ExecuteOneStreamLoop() {
        if (synEdPairRing.curr()->getSynth()->isExporting()) {
            return 0;
        } else {
            return synEdPairRing.curr()->getSynth()->frameOutput();
        }
    }

private:
    Piano piano;

    class SynEdPair {
    public:
        SynEdPair(const char *description_, LinkedRing<Editor *> *editorRing_, Synth *synth_) :
            description{description_},
            editorRing{editorRing_},
            synth{synth_} {}
        
        void onSynthSwitch() {
            synth->mmChangeSettings();
        }

        void onEditorSwitch() {
            consoleClear();
            printf("Wavetable Synthesizer for\n the Nintendo DS\n\n");
            printf("%s", description);
            editorRing->curr()->draw();
        }

        void onPairSwitch() {
            onSynthSwitch();
            onEditorSwitch();
        }

        LinkedRing<Editor *> *getEditorRing() { return editorRing; }
        Synth *getSynth() { return synth; }

    private:
        const char *description;
        LinkedRing<Editor *> *editorRing;
        Synth *synth;
    };

    LinkedRing<SynEdPair *> synEdPairRing;

    LinkedRing<Editor *> tutorialEditorRing;
    EmptyEditor welcome;
    s16 tutorialTable[TABLE_LENGTH];
    Table tableTutorial;
    EmptyEditor buttonsTutorial;
    EmptyEditor sfzExportTutorial;
    EmptySynth empth;

    LinkedRing<Editor *> wavetableEditorRing;
    s16 wave1Array[TABLE_LENGTH];
    Table waveTableOne;
    s16 wave2Array[TABLE_LENGTH];
    Table waveTableTwo;
    s16 transition[TABLE_LENGTH];
    Table morphShapeTable;
    int transitionTime = 0;
    Slider morphTimeSlider;
    int algorithm = 0; // 0. morph 1. swipe 2. combo
    Switch algorithmSwitch;
    int transitionCycle = 0; // 0. forward 1. loop 2. ping pong
    Switch transitionCycleSwitch;
    Wavetable wable;

    LinkedRing<Editor *> pluckedEditorRing;
    int blendFactor = 0;
    Slider drumSlider;
    int burstType = 0; // 0. random 1. wavetable
    Switch burstTypeSwitch;
    s16 burstArray[TABLE_LENGTH];
    Table burstTable;
    PluckedString pling;

    LinkedRing<Editor* > noveltyEditorRing;
    int novAlg;
    Switch noveltyAlgorithmSwitch;
    s16 novTab[TABLE_LENGTH];
    Table noveltyTable;
    int novSlid1;
    Slider noveltySlider1;
    int novSwitch;
    Switch noveltySwitch;
    Novelty novel;

    LinkedRing<Editor* > fmEditorRing;
    int fmAmpVals[8];
    MultiSlider fmAmpMultiSlider;
    int fmRouting[8];
    MultiSwitch fmRoutingMultiSwitch;
    int fmRatios[8];
    MultiSwitch fmRatioMultiSwitch;
    FM fam;

    KonamiCodeDetector komani;

    void handleButtons() {
        scanKeys();
		int keysD = keysDown();
        if (keysD && komani.next(keysD)) {
            synEdPairRing.curr()->getSynth()->exportSFZ();
            //wable.exportSingleSample(440);
        }
        if (keysD & KEY_L) {
            synEdPairRing.curr()->getEditorRing()->prev();
            synEdPairRing.curr()->onEditorSwitch();
            piano.printRoot();
        }
        if (keysD & KEY_R) {
            synEdPairRing.curr()->getEditorRing()->next();
            synEdPairRing.curr()->onEditorSwitch();
            piano.printRoot();
        }
        if (keysD & KEY_SELECT) {
            synEdPairRing.next()->onPairSwitch();
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
        if (keysD & KEY_X)
            piano.playTestTone();
        int keysU = keysUp();
        if (keysU & KEY_X)
            piano.stopTestTone();
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


int main( void ) {
	pc = consoleDemoInit();
	
    
    if (fatInitDefault())
	    printf("LibFat succesful init\n");
	else
	    printf("LibFat ini'nt succesful\n");
    //NF_SetRootFolder("sfz");

	videoSetMode(MODE_FB0);
	vramSetBankA(VRAM_A_LCD);

	lcdMainOnBottom();
    
    app.initScreen();

    
    for (int i = 0; i < 13; i++) {
        sounds[i].key = i;
        sounds[i].justPressed = true;
    }
    
    //----------------------------------------------------------------
    // initialize maxmod without any soundbank (unusual setup)
    //----------------------------------------------------------------
    sys.mod_count 			= 0;
    sys.samp_count			= 0;
    sys.mem_bank			= 0;
    sys.fifo_channel		= FIFO_MAXMOD;
	mmInit( &sys );
	
	//----------------------------------------------------------------
	// open stream
	//----------------------------------------------------------------
	
	mystream.sampling_rate	= SAMPLING_RATE;			// sampling rate = 25khz
	mystream.buffer_length	= BUFFER_SIZE;						// buffer length = 1200 samples
	mystream.callback		= on_stream_request;		// set callback function
	mystream.format			= MM_STREAM_16BIT_MONO;		// format = mono 8-bit
	mystream.timer			= MM_TIMER0;				// use hardware timer 0
	mystream.manual			= true;						// use manual filling
	mmStreamOpen( &mystream );

    /**
     * 
     */
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