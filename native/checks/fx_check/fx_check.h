#include <stdint.h>
typedef struct { uint32_t pc; void (*run)(void); const char *name; } FxEntry;
extern const FxEntry fx_entries[];
typedef struct { uint16_t r[16]; uint8_t z, cy, s, ov, rombr, rambr, colr, por, scmr, scbr; uint16_t ramaddr; uint8_t romdr; } CheckRegs;
extern CheckRegs gsu;
#define fx_code(a, n) ((void)(a))
