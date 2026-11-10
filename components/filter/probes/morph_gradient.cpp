#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <algorithm>
#include "components/filter/ui/FilterScreen.h"
using namespace dsp::filter;

// HOW MUCH THE SOUND MOVES FOR A SMALL MOVE OF THE HAND, at each place on the glass.
// Endpoint-to-endpoint distance lies: the bottom edge measures "dead" only because a
// highpass at 60 Hz and a notch at 16 kHz happen to look alike, while everything
// between them is loud. A gradient cannot be fooled that way.
int main() {
    std::map<std::string, float> P = {{"cutoff",1000.f},{"resonance",.2f},{"shape",0.f},
        {"width",.5f},{"drive",0.f},{"slope",24.f},{"env_amount",0.f}};
    Screen s([&](const char* k){ return P.count(k)?P[k]:0.f; },
             [&](const char* k, float v){ P[k]=v; });
    const int N = 256;
    auto curve = [&](float x, float y) {
        s.applyMacro(x, y);
        const float cn = Screen::cutNorm(P["cutoff"]);
        std::vector<float> c(N);
        for (int i=0;i<N;i++)
            c[i] = Screen::db((float)i/(N-1), cn, P["resonance"], P["shape"],
                              P["width"], P["drive"], P["slope"]);
        return c;
    };
    auto dist=[&](const std::vector<float>&a,const std::vector<float>&b){
        double t=0; for(int i=0;i<N;i++){double d=a[i]-b[i]; t+=d*d;} return std::sqrt(t/N); };

    const float h = 0.02f;   // a small move of the hand: 2 per cent of the glass
    printf("dB of change per 10%% of hand travel ACROSS (side to side)\n");
    printf("  %-6s", "y \\ x");
    for (float x=0; x<=1.001f; x+=0.125f) printf("%7.2f", x);
    printf("\n");
    for (float y=1.0f; y>=-0.001f; y-=0.125f) {
        printf("  %-6.2f", y);
        for (float x=0; x<=1.001f; x+=0.125f) {
            const float a=std::max(0.f,x-h), b=std::min(1.f,x+h);
            printf("%7.1f", dist(curve(a,y), curve(b,y)) / (b-a) * 0.1);
        }
        printf("\n");
    }
    printf("\ndB of change per 10%% of hand travel UP AND DOWN\n");
    printf("  %-6s", "y \\ x");
    for (float x=0; x<=1.001f; x+=0.125f) printf("%7.2f", x);
    printf("\n");
    for (float y=1.0f; y>=-0.001f; y-=0.125f) {
        printf("  %-6.2f", y);
        for (float x=0; x<=1.001f; x+=0.125f) {
            const float a=std::max(0.f,y-h), b=std::min(1.f,y+h);
            printf("%7.1f", dist(curve(x,a), curve(x,b)) / (b-a) * 0.1);
        }
        printf("\n");
    }
    printf("\nWHAT THE FILTER IS at each place: cutoff Hz / shape (0 LP 1 BP 2 HP 3 notch)\n");
    printf("  %-6s", "y \\ x");
    for (float x=0; x<=1.001f; x+=0.25f) printf("%14.2f", x);
    printf("\n");
    for (float y=1.0f; y>=-0.001f; y-=0.25f) {
        printf("  %-6.2f", y);
        for (float x=0; x<=1.001f; x+=0.25f) {
            s.applyMacro(x,y);
            printf("%9.0f/%-4.2f", P["cutoff"], P["shape"]);
        }
        printf("\n");
    }
    return 0;
}
