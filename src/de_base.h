#define ArrayCount(Array) (sizeof(Array)/sizeof(Array[0]))
#define ForU32(I, Size) for (Uint32 I = 0; I < (Size); I += 1)
#define ForU64(I, Size) for (Uint32 I = 0; I < (Size); I += 1)
#define ForArray(I, Array) ForU64(I, ArrayCount(Array))

#define Assert(Expr) assert(Expr) // @todo(mg) this might be compiled out for release builds
#define MemsetZeroStructPtr(Ptr) memset(Ptr, 0, sizeof(*Ptr))