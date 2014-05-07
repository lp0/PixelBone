/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011, 2012, 2013, 2014 Emanuel Eichhammer               **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: http://www.qcustomplot.com/                          **
**             Date: 07.04.14                                             **
**          Version: 1.2.1                                                **
****************************************************************************/

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

static fftwf_complex left_out[FRAMES_PER_BUFFER], right_out[FRAMES_PER_BUFFER];
static fftwf_plan lp, rp;
static float left_mid[FRAMES_PER_BUFFER], right_mid[FRAMES_PER_BUFFER];
std::mutex plan_mtx;           // mutex for plans
std::mutex mid_mtx;
std::mutex out_mtx;

bool plan_set;                 // indicates if plans exist

static PixelBone_Matrix *matrix;
std::mutex matrix_mtx;

inline static double average(vector<float> &v) {
  double sum = std::accumulate(std::begin(v), std::end(v), 0.0);
  return sum / v.size();
}

inline static float scale(float oldMin, float oldMax, float newMin, float newMax, float value){
  return (((newMax - newMin) * (value - oldMin)) / (oldMax - oldMin)) + newMin;
}


static void fftwProcess(const void *inputBuffer) {
  
  // printf("FFTW CALLBACK GOT CALLED\n");
  
  float **input_ptr_ary = (float **)inputBuffer;
  float *left_in = input_ptr_ary[0];
  float *right_in = input_ptr_ary[1];

  // if (lp == NULL && rp == NULL ) {
  //   plan_mtx.lock();
  //   printf("FFTW Plans are null\n");
  //   if (!plan_set) {
  //     plan_set = true;

  //   }
  //   plan_mtx.unlock();
  // }

  if (inputBuffer == NULL) return;

  mid_mtx.lock();
  /* Hanning window function */
  for (uint i = 0; i < FRAMES_PER_BUFFER; i++) {
    double multiplier = 0.5 * (1 - cos(2 * M_PI * i / (FRAMES_PER_BUFFER - 1)));
    left_mid[i] = multiplier * (left_in[i] + 1.0);
    right_mid[i] = multiplier * (right_in[i] + 1.0);
  }
  mid_mtx.unlock();

  out_mtx.lock();
  fftwf_execute(lp);
  fftwf_execute(rp);
  out_mtx.unlock();

  // Add data:
  vector<float> tempdata;
  for (uint i = 2; i < (FRAMES_PER_BUFFER / 8) - 1; i++) {
    //ticks << (i * SAMPLE_RATE / FRAMES_PER_BUFFER);
    //           right_out[i][0]);
     tempdata.push_back(abs(left_out[i][0]));
  }
  
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
  memcpy(((float**)outputBuffer)[0], ((float**)inputBuffer)[0], framesPerBuffer * sizeof(float));
  memcpy(((float**)outputBuffer)[1], ((float**)inputBuffer)[1], framesPerBuffer * sizeof(float));
  std::thread (fftwProcess, inputBuffer).detach();
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
