#include <nds.h>
#include <maxmod9.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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
 * 1. Wavetable No. 1 Editor <-- Table
 * 2. Wavetable No. 2 Editor <-- Table
 * 3. Transition Shape Editor <-- Table
 * 4. Transition Time Slider <-- Slider
 * 5. Swipe/Lerp Toggle <-- Toggle
 */

struct SoundInfo {
    int key; // which key does this sound info go to?
	bool playing; // is the note currently playing?
    bool justPressed; // was the note just initially pressed (different from held)
    bool stopping; // used for depopping
    bool pingPongDirection; // true for forward, false for backwards
	int transitionFramesElapsed; // frames elapsed since start of tone
    int phaseFramesElapsed; // frames elapsed used for calculating phase. will be modded and deviate from actual frames elapsed
    int morphTableIndex; // the current position in the morph table
    int freq; // the frequency of the sound to be played
    int depopFramesElapsed; // used for depopping
    int lastSampleOutputted; // used for depopping
};

struct SoundInfo sounds[13];

/**
 * Variables for Wavetable
 */
s16 wave1Array[TABLE_LENGTH];
s16 wave2Array[TABLE_LENGTH];
s16 transition[TABLE_LENGTH];
int transitionTime = 0;
int algorithm = 0; // 0. morph 1. swipe 2. combo
int transitionCycle = 0; // 0. forward 1. loop 2. ping pong

/**
 * variables for Plucked String
 */
int blendFactor = 0; 
int burstType = 0; // 0. random 1. wavetable
s16 burstArray[TABLE_LENGTH];

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
        pc->cursorY = 10;
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
            sounds[i].transitionFramesElapsed = 0;
            sounds[i].phaseFramesElapsed = 0;
            sounds[i].morphTableIndex = 0;
            sounds[i].pingPongDirection = true;
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
            _sliders[i].rawx = SCREEN_PADDING;
            _vals[i] = 0;
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

            _sliders[_currentSliderHeld].rawx = touch.px;
            _vals[_currentSliderHeld] = ((touch.px - SCREEN_PADDING) * _maxVal) / TABLE_MAX;

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
        int yHigh = SCREEN_PADDING + ((slider + 1)*TABLE_MAX / _numSliders);
        for (int i = SCREEN_PADDING; i <= SCREEN_WIDTH - SCREEN_PADDING; i++) {
            for (int j = yLow; j < yHigh; j++) {
                if (i == x)
                    VRAM_A[256 * j + i] = RGB15(31, 31, 31);
                else
                    VRAM_A[256 * j + i] = RGB15(0, 0, 0);
            }
        }  
    }

    void drawSliders() {
        for (int i = SCREEN_PADDING; i <= SCREEN_WIDTH - SCREEN_PADDING; i++) {
            for (int j = SCREEN_PADDING; j <= SCREEN_HEIGHT - SCREEN_PADDING; j++) {
                struct slider *current = &_sliders[(j - SCREEN_PADDING) * _numSliders / TABLE_MAX];
                if (i == current->rawx)
                    VRAM_A[256 * j + i] = RGB15(31, 31, 31);
                else
                    VRAM_A[256 * j + i] = RGB15(0, 0, 0);
            }
        }
    }
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
    Sine(int samplingRate) :
    _samplingRate{samplingRate},
    _t{0} {}

    s16 sin(int freq) {
        return sinLerp((_t++ * freq * 65536) / _samplingRate);
    }
private:
    int _samplingRate;
    s16 _t;
};

class Synth {
public:
    Synth(int gain, int samplingRate) : _gain{gain}, _samplingRate{samplingRate} {}
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
protected:
    int _gain;
    int _samplingRate;
    virtual s16 getOutputSample(struct SoundInfo * sound) = 0;
};

class VariableLengthWavetable : public Synth {
public:
protected:
    struct info {
        s16 previous;
        int length;
        s16 table[3000];
    };
    struct info infos[13];
};

class ExcitedString : public Synth {
public:
    ExcitedString(int gain, int samplingRate, s16 (&table)[TABLE_LENGTH], int &slider1Val, int &switchVal) :
    Synth(gain, samplingRate),
    _table(table),
    _slider1Val(slider1Val),
    _switchVal(switchVal) {}
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
    Synth(gain, samplingRate),
    _table(table),
    _slider1Val(slider1Val),
    _switchVal(switchVal) {}
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
    Synth(gain, samplingRate),
    _table(table),
    _slider1Val(slider1Val),
    _switchVal(switchVal) {}
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
    Synth(gain, samplingRate),
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
    FM(int gain, int samplingRate) :
        Synth(gain, samplingRate),
        _op1(_samplingRate),
        _op2(_samplingRate),
        _op3(_samplingRate),
        _op4(_samplingRate)
    {
        for (int i = 0; i < 13; i++) {
            infos[i].sine = new Sine(_samplingRate);
        }

        _op1.setMod(&_op2);
        _op2.setRatio(2);
    }
private:
    class Operator {
    public:
        Operator(int samplingRate) :
            sine(samplingRate),
            _modulator {NULL},
            _ratio {1},
            _operation {1} {}
         
        s16 evaluate(int freq) {
            if (_modulator == NULL) {
                return sine.sin(freq);
            } else {
                switch (_operation) {
                    case 0: //add
                        return sine.sin(freq) + _modulator->evaluate(freq);
                    case 1: // modulate
                        return sine.sin(freq + _modulator->evaluate(freq));
                }
                return 0;
            }
        }

        void setMod(Operator *op) {
            _modulator = op;
        }

        void setRatio(int ratio) {
            _ratio = ratio;
        }

        void setOperation(int operation) {
            _operation = operation;
        }
    private:
        Sine sine;
        Operator * _modulator;
        int _ratio;
        int _operation;
    };

    Operator _op1;
    Operator _op2;
    Operator _op3;
    Operator _op4;

    struct fmInfo {
        Sine *sine;
    };

    struct fmInfo infos[13];

    s16 getOutputSample(struct SoundInfo * sound) {
        if (sound->playing) {
            return infos[sound->key].sine->sin(sound->freq);
        } else {
            return 0;
        }
    }
};

class PluckedString : public Synth {
public:
    PluckedString(int gain, int samplingRate) : Synth(gain, samplingRate) {}
private:
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
                switch (burstType) {
                    case 0: { // fill the burst table with random
                        for (int i = 0; i < pluck->length; i++) {
                            pluck->table[i] = rand() % TABLE_MAX;
                        }
                        break;
                    }
                    case 1: { // fill the burst table with a squeezed rendition of the burstArray (filled by a table editor)
                        for (int i = 0; i < pluck->length; i++) {
                            pluck->table[i] = burstArray[Lerp::lerp(0, TABLE_LENGTH - 1, i, pluck->length - 1)];
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
            pluck->table[phase] = (randy.prob(blendFactor, TABLE_LENGTH - 1) ? 1 : -1) * ((current + pluck->previous) >> 1);
            pluck->previous = current;
            return _gain * pluck->table[phase];
        } else {
            return 0;
        }
    }
};

class Wavetable : public Synth {
public:
    Wavetable(int gain, int samplingRate) : Synth(gain, samplingRate) {}
private:

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
	    int phase = div32((sound->phaseFramesElapsed * sound->freq * TABLE_LENGTH), _samplingRate);
        if (phase > 8 * TABLE_LENGTH) {
            sound->phaseFramesElapsed = 0;
        }
        return phase % TABLE_LENGTH;
    }

    int getTransitionIndex(struct SoundInfo * sound) {
        return lerp(0, TABLE_LENGTH - 1, sound->transitionFramesElapsed, transitionTime);
    }

    void incrementFrameCount(struct SoundInfo * sound) {
        sound->phaseFramesElapsed++;
        switch (transitionCycle) {
            case 0:
                sound->transitionFramesElapsed++;
                break;
            case 1:
                sound->transitionFramesElapsed = (sound->transitionFramesElapsed + 1) % transitionTime;
                break;
            case 2: {
                if (sound->pingPongDirection == true) {
                    if (sound->transitionFramesElapsed++ >= transitionTime)
                        sound->pingPongDirection = false;
                } else {
                    if (sound->transitionFramesElapsed-- <= 0)
                        sound->pingPongDirection = true;
                }
                break;
            }
            
        }
    }

    s16 getOutputSample(struct SoundInfo * sound) {
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
                    int split = lerp(0, TABLE_LENGTH - 1, transitionValue, TABLE_MAX - 1);
                    if (phase > split)
                        output = sample1;
                    else
                        output = sample2;
                    break;
                }
                case 2: { // using a combination of both algorithms
                    int morph = lerp(sample1, sample2, transitionValue, TABLE_MAX - 1);
                    int swipe;
                    int split = lerp(0, TABLE_LENGTH - 1, transitionValue, TABLE_MAX - 1);
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
            
            sound->lastSampleOutputted = _gain * output;

            return _gain * output;
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
        synEdPairRing(), 
        
        wavetableEditorRing(),
        waveTableOne("Wavetable One", wave1Array),
        waveTableTwo("Wavetable Two", wave2Array),
        morphShapeTable("Transition Shape", transition),
        morphTimeSlider("Transition Time\n Left:  0 seconds\n Right: 10 seconds", transitionTime, SAMPLING_RATE * 10),
        algorithmSwitch("Transition Algorithm\n 1. Morph\n 2. Swipe\n 3. Combos", algorithm, 3),
        transitionCycleSwitch("Transition Cycle Mode\n 1. Forward\n 2. Loop\n 3. Ping Pong", transitionCycle, 3),
        wable(54, 10000),

        pluckedEditorRing(),
        drumSlider("Blend Factor\n Left:   ???\n Middle: Drum\n Right:  Plucked String", blendFactor, TABLE_LENGTH),
        burstTypeSwitch("Burst Type\n 1. Random\n 2. Wavetable", burstType, 2),
        burstTable("Burst Wavetable", burstArray),
        pling(27, 20000),

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
        fmAmpMultiSlider("Operator Amplitudes", fmAmpVals, 4, TABLE_LENGTH),
        fam(1, 32768)
    {
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

        fmEditorRing.add(&fmAmpMultiSlider);

        synEdPairRing.add(new SynEdPair("FM\n\n", &fmEditorRing, &fam));
        synEdPairRing.add(new SynEdPair("NOVELTY\n\n", &noveltyEditorRing, &novel));
        synEdPairRing.add(new SynEdPair("PLUCKED STRING\n\n", &pluckedEditorRing, &pling));
        synEdPairRing.add(new SynEdPair("WAVETABLE SYNTH\n\n", &wavetableEditorRing, &wable));
        
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
        return synEdPairRing.curr()->getSynth()->frameOutput();
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

    LinkedRing<Editor *> wavetableEditorRing;
    Table waveTableOne;
    Table waveTableTwo;
    Table morphShapeTable;
    Slider morphTimeSlider;
    Switch algorithmSwitch;
    Switch transitionCycleSwitch;
    Wavetable wable;

    LinkedRing<Editor *> pluckedEditorRing;
    Slider drumSlider;
    Switch burstTypeSwitch;
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
    FM fam;

    void handleButtons() {
        scanKeys();
		int keysD = keysDown();
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
        int keysH = keysHeld();
        if (keysH & KEY_X)
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