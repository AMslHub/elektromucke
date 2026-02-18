+++
date = '2026-02-18T10:00:00+01:00'
draft = false
title = 'Griesinger-Type Reverb für das Daisy Seed'
+++

Ja — hier ist ein kompakter **Griesinger-inspirierter Reverb** (Schroeder/Moorer-Topologie mit diffuser Input-Stufe), der auf dem **Daisy Seed** direkt in C++ läuft.

## Idee

- 4 parallele Feedback-Combs pro Kanal für die Dichte.
- 2 Allpass-Diffusoren pro Kanal im Ausgang für „Schimmer“.
- Leichte L/R-Offset-Delayzeiten für Breite.
- Damping im Feedback (1-Pol LP), um Höhen natürlich abklingen zu lassen.

## `reverb_griesinger.h`

```cpp
#pragma once
#include "daisysp.h"

using namespace daisysp;

class OnePoleLP
{
  public:
    void Init(float samplerate)
    {
        sr_ = samplerate;
        z_  = 0.0f;
        SetCutoff(6000.0f);
    }

    void SetCutoff(float hz)
    {
        float x = expf(-2.0f * PI_F * hz / sr_);
        a_      = x;
        b_      = 1.0f - x;
    }

    float Process(float in)
    {
        z_ = b_ * in + a_ * z_;
        return z_;
    }

  private:
    float sr_ = 48000.0f;
    float a_  = 0.0f;
    float b_  = 1.0f;
    float z_  = 0.0f;
};

template <size_t max_size>
class DelayLineF
{
  public:
    void Init()
    {
        for(size_t i = 0; i < max_size; i++)
            buf_[i] = 0.0f;
        write_ = 0;
        delay_ = 1;
    }

    void SetDelay(size_t d)
    {
        delay_ = (d < 1) ? 1 : (d >= max_size ? max_size - 1 : d);
    }

    float Read() const
    {
        size_t r = (write_ + max_size - delay_) % max_size;
        return buf_[r];
    }

    void Write(float x)
    {
        buf_[write_] = x;
        write_       = (write_ + 1) % max_size;
    }

  private:
    float  buf_[max_size];
    size_t write_ = 0;
    size_t delay_ = 1;
};

template <size_t max_size>
class Comb
{
  public:
    void Init(float samplerate)
    {
        dl_.Init();
        lp_.Init(samplerate);
        feedback_ = 0.75f;
    }

    void SetDelay(size_t d) { dl_.SetDelay(d); }
    void SetFeedback(float f) { feedback_ = f; }
    void SetDamping(float hz) { lp_.SetCutoff(hz); }

    float Process(float in)
    {
        float y  = dl_.Read();
        float fb = lp_.Process(y) * feedback_;
        dl_.Write(in + fb);
        return y;
    }

  private:
    DelayLineF<max_size> dl_;
    OnePoleLP            lp_;
    float                feedback_ = 0.75f;
};

template <size_t max_size>
class Allpass
{
  public:
    void Init()
    {
        dl_.Init();
        gain_ = 0.7f;
    }

    void SetDelay(size_t d) { dl_.SetDelay(d); }
    void SetGain(float g) { gain_ = g; }

    float Process(float in)
    {
        float buf = dl_.Read();
        float y   = -gain_ * in + buf;
        dl_.Write(in + gain_ * y);
        return y;
    }

  private:
    DelayLineF<max_size> dl_;
    float                gain_ = 0.7f;
};

class GriesingerVerb
{
  public:
    void Init(float samplerate)
    {
        sr_ = samplerate;

        // Delayzeiten (48k): Prime-ish Werte, L/R leicht versetzt.
        const size_t cL[4] = {1557, 1617, 1491, 1422};
        const size_t cR[4] = {1277, 1356, 1188, 1116};
        const size_t aL[2] = {225, 556};
        const size_t aR[2] = {341, 441};

        for(int i = 0; i < 4; i++)
        {
            comb_l_[i].Init(sr_);
            comb_r_[i].Init(sr_);
            comb_l_[i].SetDelay(cL[i]);
            comb_r_[i].SetDelay(cR[i]);
        }

        for(int i = 0; i < 2; i++)
        {
            ap_l_[i].Init();
            ap_r_[i].Init();
            ap_l_[i].SetDelay(aL[i]);
            ap_r_[i].SetDelay(aR[i]);
            ap_l_[i].SetGain(0.7f);
            ap_r_[i].SetGain(0.7f);
        }

        SetDecay(0.78f);
        SetDamping(5500.0f);
        SetMix(0.28f);
        SetWidth(0.9f);
    }

    void SetDecay(float d)
    {
        decay_ = fclamp(d, 0.1f, 0.98f);
        for(int i = 0; i < 4; i++)
        {
            comb_l_[i].SetFeedback(decay_);
            comb_r_[i].SetFeedback(decay_);
        }
    }

    void SetDamping(float hz)
    {
        damp_hz_ = fclamp(hz, 800.0f, 12000.0f);
        for(int i = 0; i < 4; i++)
        {
            comb_l_[i].SetDamping(damp_hz_);
            comb_r_[i].SetDamping(damp_hz_);
        }
    }

    void SetMix(float m) { mix_ = fclamp(m, 0.0f, 1.0f); }
    void SetWidth(float w) { width_ = fclamp(w, 0.0f, 1.0f); }

    void Process(float inL, float inR, float &outL, float &outR)
    {
        float mono = 0.5f * (inL + inR);

        // Input diffusion (kurzer AP über Kreuz)
        float dL = in_diff_l_.Process(mono + 0.2f * prev_r_);
        float dR = in_diff_r_.Process(mono + 0.2f * prev_l_);

        float sumL = 0.0f;
        float sumR = 0.0f;

        for(int i = 0; i < 4; i++)
        {
            sumL += comb_l_[i].Process(dL);
            sumR += comb_r_[i].Process(dR);
        }

        sumL *= 0.25f;
        sumR *= 0.25f;

        // Output diffusion
        float wetL = ap_l_[1].Process(ap_l_[0].Process(sumL));
        float wetR = ap_r_[1].Process(ap_r_[0].Process(sumR));

        // Stereo width
        float mid  = 0.5f * (wetL + wetR);
        float side = 0.5f * (wetL - wetR) * width_;
        wetL       = mid + side;
        wetR       = mid - side;

        outL = (1.0f - mix_) * inL + mix_ * wetL;
        outR = (1.0f - mix_) * inR + mix_ * wetR;

        prev_l_ = wetL;
        prev_r_ = wetR;
    }

  private:
    float sr_      = 48000.0f;
    float decay_   = 0.78f;
    float damp_hz_ = 5500.0f;
    float mix_     = 0.28f;
    float width_   = 0.9f;

    // Kleine Input-APs (fest)
    Allpass<1024> in_diff_l_;
    Allpass<1024> in_diff_r_;

    Comb<8192>    comb_l_[4];
    Comb<8192>    comb_r_[4];
    Allpass<2048> ap_l_[2];
    Allpass<2048> ap_r_[2];

    float prev_l_ = 0.0f;
    float prev_r_ = 0.0f;

  public:
    GriesingerVerb()
    {
        in_diff_l_.Init();
        in_diff_r_.Init();
        in_diff_l_.SetDelay(113);
        in_diff_r_.SetDelay(149);
        in_diff_l_.SetGain(0.65f);
        in_diff_r_.SetGain(0.65f);
    }
};
```

## Verwendung auf Daisy Seed (`main.cpp`)

```cpp
#include "daisy_seed.h"
#include "daisysp.h"
#include "reverb_griesinger.h"

using namespace daisy;

DaisySeed      hw;
GriesingerVerb verb;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i + 0];
        float inR = in[i + 1];

        float outL, outR;
        verb.Process(inL, inR, outL, outR);

        out[i + 0] = outL;
        out[i + 1] = outR;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    verb.Init(hw.AudioSampleRate());
    verb.SetDecay(0.82f);      // 0.1 .. 0.98
    verb.SetDamping(4800.0f);  // 800 .. 12000 Hz
    verb.SetMix(0.35f);        // 0..1
    verb.SetWidth(1.0f);       // 0..1

    hw.StartAudio(AudioCallback);
    while(1) {}
}
```

## Tuning-Tipps

- **Mehr Hallfahne**: `SetDecay(0.86f..0.92f)`.
- **Dunkler Vintage-Charakter**: `SetDamping(2200..4200 Hz)`.
- **Klarer/hi-fi**: `SetDamping(7000..10000 Hz)` + `mix` etwas runter.
- Wenn es „metallisch“ klingt: einzelne Comb-Delays minimal ändern (±5…20 Samples).

Wenn du willst, kann ich dir im nächsten Schritt eine Version mit
- **modulierten Delays** (gegen Flutter/Metallik),
- **Freeze-Funktion**,
- und **Potis + CV Mapping** für das Daisy Seed
bauen.
