/* 
  SN76489 emulation
  by Maxim in 2001 and 2002
*/

#pragma once

#define PSG_FEEDBACK 0x0009
#define PSG_SRW 16

enum psg_vol_modes {
  PSG_VOL_TRUNC   =   0,      /* Volume levels 13-15 are identical */
  PSG_VOL_FULL    =   1,      /* Volume levels 13-15 are unique */
};

enum psg_mute_values {
  PSG_MUTE_ALLOFF =   0,      /* All channels muted */
  PSG_MUTE_TONE1  =   1,      /* Tone 1 mute control */
  PSG_MUTE_TONE2  =   2,      /* Tone 2 mute control */
  PSG_MUTE_TONE3  =   4,      /* Tone 3 mute control */
  PSG_MUTE_NOISE  =   8,      /* Noise mute control */
  PSG_MUTE_ALLON  =   15,     /* All channels enabled */
};

typedef struct {
  int Mute; // per-channel muting
  int BoostNoise; // double noise volume when non-zero
  
  /* Variables */
  float Clock;
  float dClock;
  int PSGStereo;
  int NumClocksForSample;
  int WhiteNoiseFeedback;
  int SRWidth;
  
  /* PSG registers: */
  int Registers[8];        /* Tone, vol x4 */
  int LatchedRegister;
  int NoiseShiftRegister;
  int NoiseFreq;            /* Noise channel signal generator frequency */
  
  /* Output calculation variables */
  int ToneFreqVals[4];      /* Frequency register values (counters) */
  int ToneFreqPos[4];        /* Frequency channel flip-flops */
  int Channels[4];          /* Value of each channel, before stereo is applied */
  float IntermediatePos[4];   /* intermediate values used at boundaries between + and - (does not need double accuracy)*/

  float panning[4][2];            /* fake stereo */

  int NgpFlags;    /* bit 7 - NGP Mode on/off, bit 0 - is 2nd NGP chip */
  void* NgpChip2;
} PSGCONTEXT;

/* Function prototypes */
PSGCONTEXT* PSGInit(int clockValue, int samplingRate);
void PSGReset(PSGCONTEXT* chip);
void PSGShutdown(PSGCONTEXT* chip);
void PSGConfig(PSGCONTEXT* chip, /*int mute,*/ int feedback, int sw_width, int boost_noise);
void PSGWrite(PSGCONTEXT* chip, int data);
void PSGStereoWrite(PSGCONTEXT* chip, int data);
void PSGUpdate(PSGCONTEXT* chip, int **buffer, int length);

/* Non-standard getters and setters */
void PSGSetMute(PSGCONTEXT* chip, int val);
void PSGSetPanning(PSGCONTEXT* chip, int ch0, int ch1, int ch2, int ch3);
