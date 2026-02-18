#include "daisy_seed.h"
#include "daisysp.h"

#include "griesinger_reverb.h"

using namespace daisy;

DaisySeed                  hw;
griesinger::GriesingerReverb reverb;

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        const float in_l = in[0][i];
        const float in_r = in[1][i];

        float out_l;
        float out_r;
        reverb.Process(in_l, in_r, out_l, out_r);

        out[0][i] = out_l;
        out[1][i] = out_r;
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();

    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    reverb.Init(hw.AudioSampleRate());
    reverb.SetDecay(0.84f);
    reverb.SetDamping(4500.0f);
    reverb.SetMix(0.35f);
    reverb.SetWidth(1.0f);

    hw.StartAudio(AudioCallback);

    while(1)
    {
    }
}
