// WHAT THE MORPH POINT SOUNDS LIKE ALONG EACH EDGE OF THE GLASS.
// The real block from dspblocks, driven by applyMacro exactly as the screen drives it,
// with the hand walking one edge over six seconds. A curve is a claim; this is the sound.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <algorithm>
#include "components/filter/ui/FilterScreen.h"
#include "dsp/SuperFilter.h"
using namespace dsp::filter;

static const double SR = 48000.0;
static const int    SECS = 6;

struct Saw { double p = 0, inc = 0; float next() { p += inc; if (p >= 1) p -= 1; return (float) (2 * p - 1); } };

static void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r)
{
    const int n = (int) l.size();
    FILE* f = fopen (path.c_str(), "wb");
    if (!f) { printf("cannot write %s\n", path.c_str()); return; }
    auto u32=[&](unsigned v){ fwrite(&v,4,1,f); }; auto u16=[&](unsigned short v){ fwrite(&v,2,1,f); };
    const unsigned data = (unsigned) n * 2 * 2;
    fwrite("RIFF",1,4,f); u32(36+data); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); u32(16); u16(1); u16(2); u32((unsigned)SR); u32((unsigned)SR*4); u16(4); u16(16);
    fwrite("data",1,4,f); u32(data);
    for (int i=0;i<n;i++) { auto q=[&](float v){ int s=(int)std::lround(std::clamp(v,-1.f,1.f)*32767.f); u16((unsigned short)(short)s); };
        q(l[i]); q(r[i]); }
    fclose(f);
}

int main (int argc, char** argv)
{
    const std::string tag = argc > 1 ? argv[1] : "x";
    const std::string dir = argc > 2 ? argv[2] : ".";

    std::map<std::string, float> P = {{"cutoff",1000.f},{"resonance",.2f},{"shape",0.f},
        {"width",.5f},{"drive",0.f},{"slope",24.f},{"env_amount",0.f}};
    Screen s([&](const char* k){ return P.count(k)?P[k]:0.f; },
             [&](const char* k, float v){ P[k]=v; });

    struct Path { const char* name; float x0,y0,x1,y1; };
    const Path paths[] = {
        { "1-left-edge-bottom-to-top",  0.00f, 0.00f, 0.00f, 1.00f },
        { "2-right-edge-bottom-to-top", 1.00f, 0.00f, 1.00f, 1.00f },
        { "3-bottom-edge-left-to-right",0.00f, 0.00f, 1.00f, 0.00f },
        { "4-top-edge-left-to-right",   0.00f, 1.00f, 1.00f, 1.00f },
        { "5-diagonal-lowleft-to-topright", 0.00f, 0.00f, 1.00f, 1.00f },
    };

    for (const auto& pa : paths)
    {
        // SEVEN DETUNED SAWS plus a little noise, so there is something to hear at
        // every part of the axis. A source with a hole in it makes a filter look dead.
        Saw osc[7]; unsigned rng = 22222;
        for (int i = 0; i < 7; ++i)
            osc[i].inc = 110.0 * std::pow (2.0, (i - 3) * 0.09 / 12.0) / SR;
        SuperFilter filt; filt.prepare (SR);

        const int N = (int) (SR * SECS);
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i)
        {
            if ((i % 64) == 0)
            {
                const float u = (float) i / (float) (N - 1);
                s.applyMacro (pa.x0 + (pa.x1 - pa.x0) * u, pa.y0 + (pa.y1 - pa.y0) * u);
                filt.set (P["cutoff"], P["resonance"], P["shape"], P["width"],
                          P["drive"], P["slope"], 0);
            }
            float in = 0.0f;
            for (auto& o : osc) in += o.next();
            rng = rng * 1664525u + 1013904223u;
            in = in * (1.0f / 7.0f) * 0.7f + ((float)(int)(rng >> 8) / 8388608.0f - 1.0f) * 0.03f;
            const float o = filt.process (in * 0.35f);
            L[i] = o; R[i] = o;
        }
        // a short fade so an edge does not click
        const int fade = (int) (SR * 0.01);
        for (int i = 0; i < fade; ++i) { const float g = (float) i / fade;
            L[i]*=g; R[i]*=g; L[N-1-i]*=g; R[N-1-i]*=g; }

        double sum = 0; float pk = 0;
        for (int i = 0; i < N; ++i) { sum += (double) L[i]*L[i]; pk = std::max (pk, std::fabs (L[i])); }
        printf ("%-34s  %s   rms %7.4f  peak %7.4f\n", pa.name, tag.c_str(),
                std::sqrt (sum / N), pk);
        writeWav (dir + "/" + pa.name + "--" + tag + ".wav", L, R);
    }
    return 0;
}
