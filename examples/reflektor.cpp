/**
 * Copyright 2014 Katherine Whitlock
 * aka toroidal-code
 * Licensed under the MIT License
 */

#include <cstdlib>
#include <cstdio>
#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <complex.h>

#include <fftw3.h>
#include <string>
#include <iostream>
#include <sys/time.h>

#include <vector>
#include <portaudio.h>
#include <numeric>
#include "../matrix.hpp"
#define NUM_BINS 6

using namespace std;

/*
** Note that many of the older ISA sound cards on PCs do NOT support
** full duplex audio (simultaneous record and playback).
** And some only support full duplex at lower sample rates.
*/
#define SAMPLE_RATE (44100)
#define PA_SAMPLE_TYPE paFloat32 | paNonInterleaved;
#define FRAMES_PER_BUFFER (2048)
#define PORT 7681

double gInOutScaler = 1.0;
#define CONVERT_IN_TO_OUT(in)  ((float) ((in) * gInOutScaler))

static int gNumNoInputs = 0;
/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static fftwf_complex left_out[FRAMES_PER_BUFFER], right_out[FRAMES_PER_BUFFER];
static fftwf_plan lp, rp;

static int fftwCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags, void *userData) {
  float **input_ptr_ary = (float **)inputBuffer;
  float *left_in = input_ptr_ary[0];
  float *right_in = input_ptr_ary[1];

  if (lp == NULL && rp == NULL) {
    lp = fftwf_plan_dft_r2c_1d(FRAMES_PER_BUFFER, left_in, left_out, FFTW_MEASURE);
    rp = fftwf_plan_dft_r2c_1d(FRAMES_PER_BUFFER, right_in, right_out, FFTW_MEASURE);
  }

  (void)timeInfo; /* Prevent unused variable warnings. */
  (void)statusFlags;
  (void)userData;
  (void)outputBuffer;

  if (inputBuffer == NULL) {
    gNumNoInputs += 1;
  }

  /* Hanning window function */
  for (uint i = 0; i < framesPerBuffer; i++) {
    double multiplier = 0.5 * (1 - cos(2 * M_PI * i / (framesPerBuffer - 1)));
    left_in[i] = multiplier * (left_in[i] + 1.0);
    right_in[i] = multiplier * (right_in[i] + 1.0);
  }

  fftwf_execute(lp);
  fftwf_execute(rp);

  float *in;
  float *out;
  int inDone = 0;
  int outDone = 0;
  unsigned int i;
  int inChannel, outChannel;
  
  /* This may get called with NULL inputBuffer during initial setup. */
  if( inputBuffer == NULL) return 0;
  
  inChannel=0, outChannel=0;
  while( !(inDone && outDone) ) {
    in = ((float**)inputBuffer)[inChannel];
    out = ((float**)outputBuffer)[outChannel];
    
    for( i=0; i<framesPerBuffer; i++ ) {
      *out = CONVERT_IN_TO_OUT( *in );
      out += 1;
      in += 1;
    }
    
    if(inChannel < (2 - 1)) inChannel++;
    else inDone = 1;
    if(outChannel < (2 - 1)) outChannel++;
    else outDone = 1;
  }
  return paContinue;
}

void setupAudio(PaStream *stream) {
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


  err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE,
                      FRAMES_PER_BUFFER, 0,
                      /* paClipOff, */ /* we won't output out of range samples
                                          so don't bother clipping them */
                      fftwCallback, NULL);
  if (err != paNoError)
    Pa_Terminate();

  err = Pa_StartStream(stream);
  if (err != paNoError)
    Pa_Terminate();
}




inline static double average(vector<float> &v) {
  double sum = std::accumulate(std::begin(v), std::end(v), 0.0);
  return sum / v.size();
}

inline static float scale(float oldMin, float oldMax, float newMin, float newMax, float value){
  return (((newMax - newMin) * (value - oldMin)) / (oldMax - oldMin)) + newMin;
}


/*******************************************************************/
int main(void) {
  PixelBone_Matrix matrix(16,8,4,1,
                          TILE_TOP   + TILE_LEFT   + TILE_ROWS   + TILE_PROGRESSIVE +
                          MATRIX_TOP + MATRIX_LEFT + MATRIX_ROWS + MATRIX_ZIGZAG);

 PaStream *stream = nullptr;

 setupAudio(stream);

 while (true) {
   matrix.clear();
   // Add data:
   vector<float> tempdata;
   for (uint i = 2; i < (FRAMES_PER_BUFFER / 8) - 1; i++) {
     //ticks << (i * SAMPLE_RATE / FRAMES_PER_BUFFER);
     //           right_out[i][0]);
     tempdata.push_back(abs(left_out[i][0]));
   }
   
   uint x =0;
   for (auto it = tempdata.begin(); !(it >= tempdata.end()); it+=4) { 
     vector<float> temp(it,it+4); 
     int datum = (int) floor(scale(0, 75, 0, 8, average(temp)));
     matrix.drawFastVLine(x++, 8, -datum, PixelBone_Pixel::Color(150,150,150));
   }  
   matrix.wait();
   matrix.show();
   matrix.moveToNextBuffer();
 }

 Pa_CloseStream(stream);
 Pa_Terminate();
 std::cout << "Quiting!" << std::endl;
 return 0;
}
