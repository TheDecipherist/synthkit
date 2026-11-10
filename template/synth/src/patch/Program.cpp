#include "Program.h"

namespace examplesynth {

std::vector<Program> factoryBank()
{
    std::vector<Program> out;
    out.reserve ((size_t) kNumFactory);
    for (int i = 0; i < kNumFactory; ++i)
    {
        const auto& f = kFactory[i];
        Program p;
        p.id = f.id;
        p.name = f.name;
        p.category = f.category;
        p.bank = "FACTORY";
        for (int c = 0; c < kNumStored; ++c)
            p.value[kStoredParam[c]] = f.value[c];
        // On a real machine this is where the unnamed remainder of the record
        // goes. Sixteen bytes of nothing stand in for it so the round-trip test
        // is exercising the path rather than an empty vector.
        p.raw.assign (16, (uint8_t) 0);
        for (size_t k = 0; k < p.raw.size(); ++k)
            p.raw[k] = (uint8_t) ((i * 7 + (int) k * 13) & 0x7F);
        out.push_back (p);
    }
    return out;
}

namespace {
void putStr (std::vector<uint8_t>& b, const std::string& s, size_t n)
{
    for (size_t i = 0; i < n; ++i) b.push_back (i < s.size() ? (uint8_t) s[i] : (uint8_t) ' ');
}
std::string getStr (const uint8_t* d, size_t n)
{
    std::string s ((const char*) d, n);
    while (! s.empty() && s.back() == ' ') s.pop_back();
    return s;
}
} // namespace

std::vector<uint8_t> toRecord (const Program& p)
{
    std::vector<uint8_t> b;
    b.push_back (0xF0);
    b.push_back (0x7D);                       // TODO(new machine): the real manufacturer id
    b.push_back (0x01);                       // record type
    putStr (b, p.id, 4);
    putStr (b, p.name, 16);
    putStr (b, p.category, 12);
    // Every parameter, as a 7-bit pair, in the generated table's own order. A
    // float goes over the wire as its position in its declared range, which is
    // what a real machine's byte IS - the machine never stores a Hz value.
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = kParams[i];
        const float span = d.maximum - d.minimum;
        const float u = span > 0.0f ? (p.value[i] - d.minimum) / span : 0.0f;
        const int q = (int) (u * 16383.0f + 0.5f);
        b.push_back ((uint8_t) ((q >> 7) & 0x7F));
        b.push_back ((uint8_t) (q & 0x7F));
    }
    b.push_back ((uint8_t) (p.raw.size() & 0x7F));
    for (uint8_t v : p.raw) b.push_back ((uint8_t) (v & 0x7F));
    b.push_back (0xF7);
    return b;
}

bool fromRecord (const uint8_t* d, size_t n, Program& out)
{
    const size_t fixed = 3 + 4 + 16 + 12 + (size_t) kNumParams * 2 + 1 + 1;
    if (d == nullptr || n < fixed || d[0] != 0xF0 || d[n - 1] != 0xF7) return false;
    size_t at = 3;
    out.id = getStr (d + at, 4); at += 4;
    out.name = getStr (d + at, 16); at += 16;
    out.category = getStr (d + at, 12); at += 12;
    for (int i = 0; i < kNumParams; ++i)
    {
        const int q = ((int) d[at] << 7) | (int) d[at + 1];
        at += 2;
        const auto& p = kParams[i];
        out.value[i] = p.minimum + (p.maximum - p.minimum) * (float) q / 16383.0f;
    }
    const size_t rawN = d[at++];
    if (at + rawN + 1 > n) return false;
    out.raw.assign (d + at, d + at + rawN);
    return true;
}

} // namespace examplesynth
