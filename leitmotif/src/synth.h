/* A tiny additive synthesiser: short plucked partials for motifs and
   chords, plus up to four sustained tones (beats, harmonics) that scenes
   hold while they are on screen. */
#ifndef SYNTH_H
#define SYNTH_H
void synth_init(void);
void synth_fill(float *out, int frames, int rate);
void synth_pluck(float freq, float amp, float dur);
void synth_hold(int slot, float freq, float amp);   /* amp 0 releases */
void synth_release_all(void);
void synth_mute(int m);
int  synth_muted(void);
#endif
