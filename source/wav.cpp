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

class WavBuilder {
public:
    WavBuilder() : _sampleCount {0} {}
    
    void addSample(short sample) {
        _sampleCount++;
        // push the sample to the file
    }
private:
    const char * filename;
    int _sampleCount;

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
};