#pragma once

#include "daisysp.h"

namespace griesinger
{
using namespace daisysp;

class OnePoleLP
{
  public:
    void Init(float sample_rate)
    {
        sample_rate_ = sample_rate;
        z_           = 0.0f;
        SetCutoff(6000.0f);
    }

    void SetCutoff(float hz)
    {
        const float x = expf(-2.0f * PI_F * hz / sample_rate_);
        a_            = x;
        b_            = 1.0f - x;
    }

    float Process(float in)
    {
        z_ = b_ * in + a_ * z_;
        return z_;
    }

  private:
    float sample_rate_ = 48000.0f;
    float a_           = 0.0f;
    float b_           = 1.0f;
    float z_           = 0.0f;
};

template <size_t max_size>
class DelayLine
{
  public:
    void Init()
    {
        for(size_t i = 0; i < max_size; i++)
        {
            buffer_[i] = 0.0f;
        }

        write_index_ = 0;
        delay_       = 1;
    }

    void SetDelay(size_t delay_samples)
    {
        delay_ = delay_samples < 1 ? 1 : (delay_samples >= max_size ? max_size - 1 : delay_samples);
    }

    float Read() const
    {
        const size_t read_index = (write_index_ + max_size - delay_) % max_size;
        return buffer_[read_index];
    }

    void Write(float x)
    {
        buffer_[write_index_] = x;
        write_index_          = (write_index_ + 1) % max_size;
    }

  private:
    float  buffer_[max_size];
    size_t write_index_ = 0;
    size_t delay_       = 1;
};

template <size_t max_size>
class Comb
{
  public:
    void Init(float sample_rate)
    {
        delay_.Init();
        damping_.Init(sample_rate);
        feedback_ = 0.75f;
    }

    void SetDelay(size_t delay_samples) { delay_.SetDelay(delay_samples); }

    void SetFeedback(float feedback) { feedback_ = feedback; }

    void SetDamping(float hz) { damping_.SetCutoff(hz); }

    float Process(float in)
    {
        const float delayed  = delay_.Read();
        const float filtered = damping_.Process(delayed);
        delay_.Write(in + filtered * feedback_);
        return delayed;
    }

  private:
    DelayLine<max_size> delay_;
    OnePoleLP           damping_;
    float               feedback_ = 0.75f;
};

template <size_t max_size>
class Allpass
{
  public:
    void Init()
    {
        delay_.Init();
        gain_ = 0.7f;
    }

    void SetDelay(size_t delay_samples) { delay_.SetDelay(delay_samples); }

    void SetGain(float gain) { gain_ = gain; }

    float Process(float in)
    {
        const float delayed = delay_.Read();
        const float out     = -gain_ * in + delayed;
        delay_.Write(in + gain_ * out);
        return out;
    }

  private:
    DelayLine<max_size> delay_;
    float               gain_ = 0.7f;
};

class GriesingerReverb
{
  public:
    void Init(float sample_rate)
    {
        sample_rate_ = sample_rate;

        in_diff_l_.Init();
        in_diff_r_.Init();
        in_diff_l_.SetDelay(113);
        in_diff_r_.SetDelay(149);
        in_diff_l_.SetGain(0.65f);
        in_diff_r_.SetGain(0.65f);

        static const size_t comb_l_delays[4] = {1557, 1617, 1491, 1422};
        static const size_t comb_r_delays[4] = {1277, 1356, 1188, 1116};
        static const size_t ap_l_delays[2]   = {225, 556};
        static const size_t ap_r_delays[2]   = {341, 441};

        for(size_t i = 0; i < 4; i++)
        {
            comb_l_[i].Init(sample_rate_);
            comb_r_[i].Init(sample_rate_);
            comb_l_[i].SetDelay(comb_l_delays[i]);
            comb_r_[i].SetDelay(comb_r_delays[i]);
        }

        for(size_t i = 0; i < 2; i++)
        {
            ap_l_[i].Init();
            ap_r_[i].Init();
            ap_l_[i].SetDelay(ap_l_delays[i]);
            ap_r_[i].SetDelay(ap_r_delays[i]);
            ap_l_[i].SetGain(0.7f);
            ap_r_[i].SetGain(0.7f);
        }

        SetDecay(0.82f);
        SetDamping(4800.0f);
        SetMix(0.35f);
        SetWidth(1.0f);
    }

    void SetDecay(float decay)
    {
        decay_ = fclamp(decay, 0.1f, 0.98f);
        for(size_t i = 0; i < 4; i++)
        {
            comb_l_[i].SetFeedback(decay_);
            comb_r_[i].SetFeedback(decay_);
        }
    }

    void SetDamping(float hz)
    {
        damping_hz_ = fclamp(hz, 800.0f, 12000.0f);
        for(size_t i = 0; i < 4; i++)
        {
            comb_l_[i].SetDamping(damping_hz_);
            comb_r_[i].SetDamping(damping_hz_);
        }
    }

    void SetMix(float mix) { mix_ = fclamp(mix, 0.0f, 1.0f); }

    void SetWidth(float width) { width_ = fclamp(width, 0.0f, 1.0f); }

    void Process(float in_l, float in_r, float &out_l, float &out_r)
    {
        const float mono = 0.5f * (in_l + in_r);

        const float diff_l = in_diff_l_.Process(mono + 0.2f * prev_r_);
        const float diff_r = in_diff_r_.Process(mono + 0.2f * prev_l_);

        float comb_sum_l = 0.0f;
        float comb_sum_r = 0.0f;

        for(size_t i = 0; i < 4; i++)
        {
            comb_sum_l += comb_l_[i].Process(diff_l);
            comb_sum_r += comb_r_[i].Process(diff_r);
        }

        comb_sum_l *= 0.25f;
        comb_sum_r *= 0.25f;

        float wet_l = ap_l_[1].Process(ap_l_[0].Process(comb_sum_l));
        float wet_r = ap_r_[1].Process(ap_r_[0].Process(comb_sum_r));

        const float mid  = 0.5f * (wet_l + wet_r);
        const float side = 0.5f * (wet_l - wet_r) * width_;
        wet_l            = mid + side;
        wet_r            = mid - side;

        out_l = (1.0f - mix_) * in_l + mix_ * wet_l;
        out_r = (1.0f - mix_) * in_r + mix_ * wet_r;

        prev_l_ = wet_l;
        prev_r_ = wet_r;
    }

  private:
    float sample_rate_ = 48000.0f;
    float decay_       = 0.82f;
    float damping_hz_  = 4800.0f;
    float mix_         = 0.35f;
    float width_       = 1.0f;

    Allpass<1024> in_diff_l_;
    Allpass<1024> in_diff_r_;

    Comb<8192> comb_l_[4];
    Comb<8192> comb_r_[4];

    Allpass<2048> ap_l_[2];
    Allpass<2048> ap_r_[2];

    float prev_l_ = 0.0f;
    float prev_r_ = 0.0f;
};
} // namespace griesinger
