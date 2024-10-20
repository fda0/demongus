// ---
// Scalar math
// ---
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(min, max, val) (((val)<(min)) ? (min) : ((val)>(max))?(max):(val))

static float LerpF(float a, float b, float t)
{
    return a + (b - a) * t;
}
static float SqrtF(float a)
{
    return SDL_sqrtf(a);
}
static float SignF(float a)
{
    return (a < 0.f ? -1.f : 1.f);
}

// ---
// Vector math
// ---
typedef union
{
    struct { float x, y; };
    float E[2];
} V2;

static V2 V2_Scale(V2 a, float scale)
{
    return (V2){a.x*scale, a.y*scale};
}
static V2 V2_Add(V2 a, V2 b)
{
    return (V2){a.x + b.x, a.y + b.y};
}
static V2 V2_Sub(V2 a, V2 b)
{
    return (V2){a.x - b.x, a.y - b.y};
}
static V2 V2_Mul(V2 a, V2 b)
{
    return (V2){a.x * b.x, a.y * b.y};
}
static float V2_Inner(V2 a, V2 b)
{
    return a.x*b.x + a.y*b.y;
}
static float V2_LengthSq(V2 a)
{
    return V2_Inner(a, a);
}
static float V2_Length(V2 a)
{
    return SqrtF(V2_LengthSq(a));
}
static V2 V2_Normalize(V2 a)
{
    float length = V2_Length(a);
    if (length) {
        float length_inv = 1.f / length;
        a = V2_Scale(a, length_inv);
    }
    return a;
}

// ---
// Color
// ---
typedef union
{
    struct { float r, g, b, a; };
    float E[4];
} ColorF;
static ColorF ColorF_Normalize(ColorF f)
{
    ForArray(i, f.E)
        f.E[i] = Clamp(0.f, 1.f, f.E[i]);
    return f;
}
static ColorF ColorF_Lerp(ColorF a, ColorF b, float t)
{
    ColorF res = {};
    ForArray(i, res.E)
        res.E[i] = LerpF(a.E[i], b.E[i], t);
    return res;
}

static ColorF ColorF_RGBA(float r, float g, float b, float a)
{
    return (ColorF){r,g,b,a};
}
static ColorF ColorF_RGB(float r, float g, float b)
{
    return (ColorF){r,g,b};
}
static ColorF ColorF_ChangeA(ColorF f, float a)
{
    f.a = a;
    return f;
}

// Packed version but SDL api accepts floats too
static SDL_Color ColorF_To_SDL(ColorF f)
{
    float inv = 1.f / 255.f;
    Uint32 r = (Uint32)(f.r * inv);
    Uint32 g = (Uint32)(f.g * inv);
    Uint32 b = (Uint32)(f.b * inv);
    Uint32 a = (Uint32)(f.a * inv);

    SDL_Color res = {
        r & 0xff,
        (g & 0xff) <<  8,
        (b & 0xff) << 16,
        (a & 0xff) << 24,
    };
    return res;
}
