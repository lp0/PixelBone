/**
 * Copyright 2014 Katherine Whitlock
 * aka toroidal-code
 * Licensed under the MIT License
 */

#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <complex.h>

#include <fftw3.h>
#include <string>

#include <vector>
#include <portaudio.h>
#include <numeric>
#include <thread>
#include <mutex>
#include <unordered_map>
#include "../matrix.hpp"

using namespace std;

/*
** Note that many of the older ISA sound cards on PCs do NOT support
** full duplex audio (simultaneous record and playback).
** And some only support full duplex at lower sample rates.
*/
#define SAMPLE_RATE (44100)
#define PA_SAMPLE_TYPE paFloat32 | paNonInterleaved;
#define FRAMES_PER_BUFFER (2048)

double gInOutScaler = 1.0;
#define CONVERT_IN_TO_OUT(in)  ((float) ((in) * gInOutScaler))

static fftwf_complex *left_out, *right_out;
static fftwf_plan lp, rp;
static float *left_mid, *right_mid;
mutex plan_mtx;           // mutex for plans
mutex mid_mtx;
mutex out_mtx;

static PixelBone_Matrix *matrix;
mutex matrix_mtx;

// 1/3 octave middle frequency array
static const float omf[] = { 15.6, 31.3, 62.5, 125,  250,  500,
                             1000, 2000, 4000, 8000, 16000 };

// 1/3 octave middle frequency array
static const float tomf[] = { 15.6,    19.7,   24.8,   31.3,   39.4,    49.6,
                              62.5,    78.7,   99.2,   125.0,  157.5,   198.4,
                              250,     315,    396.9,  500.0,  630,     793.7,
                              1000.0,  1259.9, 1587.4, 2000,   2519.8,  3174.8,
                              4000.0,  5039.7, 6349.6, 8000.0, 10079.4, 12699.2,
                              16000.0, 20158.7 };

static float lbtomf[32] = { 0 };
static float ubtomf[32] = { 0 };

/**
 * Calculate the gain for a given frequency.
 * based on http://www.ap.com/kb/show/480
 * band: bandwidth designator (1 for full octave, 3 for 1/3-octave,… etc.)
 * freq: frequency
 * fm: the mid-band frequency of the 1/b-octave filter
 */
template <typename T> inline T calculate_gain(T band, T freq, T fm) {
  return sqrt(1.0 /
              (1.0 + pow(((freq / fm) - (fm / freq)) * (1.507 * band), 6.0)));
}

inline float upper_freq_bound(float mid, float band) {
  return mid * pow(sqrt(2), 1.0 / band);
}

inline float lower_freq_bound(float mid, float band) {
  return mid / pow(sqrt(2), 1.0 / band);
}

void populate_bound_arrays() {
  for (uint i = 0; i < 32; i++) {
    lbtomf[i] = lower_freq_bound(tomf[i], 3);
    ubtomf[i] = upper_freq_bound(tomf[i], 3);
  }
}

static unordered_map<int, int> freq_octave_map;

// TODO: make this some sort of tree thing
// Instead of an O(N) lookup
inline static int get_octave_bin(float freq) {
  auto got = freq_octave_map.find(freq);
  if ( got != freq_octave_map.end() ) {  // we found a freq -> octave map
    return got->second;                  // so return the octave
  } else {
    for (int i = 0; i < 3 * 11; i++) {
      if (lbtomf[i] <= freq && freq < ubtomf[i]) {
        freq_octave_map[freq] = i;
        return i;
      }
    }
  }
  return -1;
}

inline static double average(vector<float> &v) {
  double sum = std::accumulate(std::begin(v), std::end(v), 0.0);
  return sum / v.size();
}

inline static float scale(float oldMin, float oldMax, float newMin, float newMax, float value){
  return (((newMax - newMin) * (value - oldMin)) / (oldMax - oldMin)) + newMin;
}


static void fftwProcess(const void *inputBuffer) {
  float *left_in = ((float **)inputBuffer)[0];

  mid_mtx.lock();
  /* Hanning window function */
  for (uint i = 0; i < FRAMES_PER_BUFFER; i++) {
    double multiplier = 0.5 * (1 - cos(2 * M_PI * i / (FRAMES_PER_BUFFER - 1)));
    left_mid[i] = multiplier * (left_in[i] + 1.0);
  }


  out_mtx.lock();
  fftwf_execute(lp);
  mid_mtx.unlock();

  // // Add data:
  vector<float> tempdata;
  for (uint i = 2; i < (FRAMES_PER_BUFFER / 8) - 1; i++) {
     tempdata.push_back(abs(left_out[i][0]));
  }
  out_mtx.unlock();  

  matrix_mtx.lock();
  matrix->clear();
  uint x =0;
  for (auto it = tempdata.begin(); !(it >= tempdata.end()); it+=4) { 
    vector<float> temp(it,it+4); 
     int datum = (int) floor(scale(0, 30, 0, 8, average(temp)));
     matrix->drawFastVLine(x++, 9, -datum, PixelBone_Pixel::Color(150,150,150));
  }  
  matrix->wait();
  matrix->show();
  //matrix->moveToNextBuffer();
  matrix_mtx.unlock();


  // 0 is DC freq (O Hz)
  // n/2 is nyquist freq
  // float octave_bins[32] = {0};
  // for (uint i = 2; i < (FRAMES_PER_BUFFER / 16) - 1; i++) {
  //   float freq   = (i * SAMPLE_RATE / FRAMES_PER_BUFFER);
  //   int   octave = get_octave_bin(freq);
  //   float val    = abs(left_out[i][0]);
  //   if (val > octave_bins[octave]) octave_bins[octave] = val;
  // }
  
  // matrix_mtx.lock();
  // matrix->clear();
  // for (uint i = 0; i < 32; i++) { 
  //   if (octave_bins[i] != 0) {
  //     int datum = (int) floor(scale(0, 30, 0, 8, octave_bins[i]));
  //     matrix->drawFastVLine(i, 8, -datum, PixelBone_Pixel::Color(150,150,150));
  //     //matrix->drawFastVLine(i, 8, -datum, PixelBone_Pixel::HSL(octave_bins[i] * 5,100,50));
  //   }
  // } 
  // matrix->wait();
  // matrix->show();
  // matrix->moveToNextBuffer();
  // matrix_mtx.unlock();

}

/** 
 * This routine will be called by the PortAudio engine when audio is needed.
 * It may be called at interrupt level on some machines so don't do anything
 * that could mess up the system like calling malloc() or free().
 */
static int copyCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags, void *userData) {
  //    Copy stuff
  if( inputBuffer == NULL) return 0;
  //memcpy(((float**)outputBuffer)[0], ((float**)inputBuffer)[0], framesPerBuffer * sizeof(float));
  //memcpy(((float**)outputBuffer)[1], ((float**)inputBuffer)[1], framesPerBuffer * sizeof(float));
  thread (fftwProcess, inputBuffer).detach();
  return paContinue;
}


void setupAudio(PaStream *stream, PaStreamCallback *streamCallback) {
  PaStreamParameters inputParameters, outputParameters;
  PaError err;

  err = Pa_Initialize();
  if (err != paNoError)
    Pa_Terminate();

  /* default input device */
  inputParameters.device = Pa_GetDefaultInputDevice();
  outputParameters.device = Pa_GetDefaultOutputDevice();

  if (inputParameters.device == paNoDevice) {
    fprintf(stderr, "Error: No default input device.\n");
    Pa_Terminate();
  }

  inputParameters.channelCount = 2; /* stereo input */
  inputParameters.sampleFormat = PA_SAMPLE_TYPE;
  inputParameters.suggestedLatency =
      Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
  inputParameters.hostApiSpecificStreamInfo = NULL;

  outputParameters.channelCount = 2; /* stereo output */
  outputParameters.sampleFormat = PA_SAMPLE_TYPE;
  outputParameters.suggestedLatency =
      Pa_GetDeviceInfo(outputParameters.device)->defaultLowInputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  // PaAlsa_EnableRealtimeScheduling(stream, true);

  err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE,
                      FRAMES_PER_BUFFER, 0,
                      /* paClipOff, */ /* we won't output out of range samples
                                          so don't bother clipping them */
                      streamCallback, NULL);
  if (err != paNoError) {
    printf("Error opening stream\n");
    Pa_Terminate();
  }

  err = Pa_StartStream(stream);
  if (err != paNoError) {
    printf("Error starting stream\n");
    Pa_Terminate();
  }
}






/*******************************************************************/
int main(void) {

  // Initialize processing arrays with special 16-byte alligned allocators
  left_mid = (float *)fftwf_malloc(sizeof(float) * FRAMES_PER_BUFFER);
  right_mid = (float *)fftwf_malloc(sizeof(float) * FRAMES_PER_BUFFER);

  left_out = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * FRAMES_PER_BUFFER);
  right_out = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * FRAMES_PER_BUFFER);


  lp = fftwf_plan_dft_r2c_1d(FRAMES_PER_BUFFER, left_mid, left_out, FFTW_MEASURE);
  rp = fftwf_plan_dft_r2c_1d(FRAMES_PER_BUFFER, right_mid, right_out, FFTW_MEASURE);
  matrix = new PixelBone_Matrix(16,8,4,1,
                        TILE_TOP   + TILE_LEFT   + TILE_ROWS   + TILE_PROGRESSIVE +
                        MATRIX_TOP + MATRIX_LEFT + MATRIX_ROWS + MATRIX_ZIGZAG);
  PaStream *stream = nullptr;
  setupAudio(stream, copyCallback);
  
  while (true) {
    ;
  }
  
  Pa_CloseStream(stream);
  Pa_Terminate();
  printf("Quiting!\n");
  return 0;
}
