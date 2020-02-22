#include <hal.h>
#include <algorithm>

namespace hal
{

using namespace device;

namespace internal
{

template<uint32_t x, uint32_t b, uint8_t nbits>
static constexpr uint32_t encode()
{
    static_assert(x < (1 << nbits), "bit field overflow");
    return ((x & (1 << 0)) ? (b << 0) : 0)
         | ((x & (1 << 1)) ? (b << 1) : 0)
         | ((x & (1 << 2)) ? (b << 2) : 0)
         | ((x & (1 << 3)) ? (b << 3) : 0)
         | ((x & (1 << 4)) ? (b << 4) : 0)
         | ((x & (1 << 5)) ? (b << 5) : 0)
         | ((x & (1 << 6)) ? (b << 6) : 0)
         | ((x & (1 << 7)) ? (b << 7) : 0)
         | ((x & (1 << 8)) ? (b << 8) : 0)
         ;
}

} // namespace internal

void sys_tick::delay_ms(uint32_t ms)
{
    uint32_t now = ms_counter, then = now + ms;

    while (ms_counter >= now && ms_counter < then)
        ;   // busy wait
}

void sys_tick::delay_us(uint32_t us)
{
#if defined(STM32F051) || defined (STM32F767) || defined(STM32H743) || defined(STM32G070)
    volatile uint32_t& c = STK.CVR;
    const uint32_t c_max = STK.RVR;
#elif defined(STM32F103) || defined(STM32F411) || defined(STM32G431)
    volatile uint32_t& c = STK.VAL;
    const uint32_t c_max = STK.LOAD;
#else
    static_assert(false, "no featured sys-tick micro-second delay");
#endif
    const uint32_t fuzz = ticks_per_us - (ticks_per_us >> 2);   // approx 3/4
    const uint32_t n = us * ticks_per_us - fuzz;
    const uint32_t now = c;
    const uint32_t then = now >= n ? now - n : (c_max - n + now);

    if (now > then)
        while (c > then && c < now);
    else
        while (!(c > now && c < then));
}

void sys_tick::init(uint32_t n)
{
    using namespace hal;
    typedef stk_t _;

    ms_counter = 0;                             // start new epoq
    ticks_per_us = n / 1000;                    // for us in delay_us

#if defined(STM32F051) || defined (STM32F767) || defined(STM32H743) || defined(STM32G070)
    STK.CSR = _::CSR_RESET_VALUE;               // reset controls
    STK.RVR = n - 1;                            // reload value
    STK.CVR = _::CVR_RESET_VALUE;               // current counter value
    STK.CSR |= _::CSR_CLKSOURCE;                // systick clock source
    STK.CSR |= _::CSR_ENABLE | _::CSR_TICKINT;  // enable counter & interrupts
#elif defined(STM32F103) || defined(STM32F411) || defined(STM32G431)
    STK.CTRL = _::CTRL_RESET_VALUE;                 // reset controls
    STK.LOAD = n - 1;                               // reload value
    STK.VAL = _::VAL_RESET_VALUE;                   // current counter value
    STK.CTRL |= _::CTRL_CLKSOURCE;                  // systick clock source
    STK.CTRL |= _::CTRL_ENABLE | _::CTRL_TICKINT;   // enable counter & interrupts
#else
    static_assert(false, "no featured sys-tick initialzation");
#endif
}

volatile uint32_t sys_tick::ms_counter = 0;
uint32_t sys_tick::ticks_per_us = 0;

inline void sys_tick_init(uint32_t n) { sys_tick::init(n); }
inline void sys_tick_update() { ++sys_tick::ms_counter; } // N.B. wraps in 49 days!

uint32_t sys_clock::m_freq;

void sys_clock::init()
{
    using namespace hal;
    typedef rcc_t _;

    // reset clock control registers (common for all families)

    RCC.CR = _::CR_RESET_VALUE;
    RCC.CFGR = _::CFGR_RESET_VALUE;

#if defined(STM32F051)
    m_freq = 48000000;

    // reset clock control registers

    RCC.CFGR2 = _::CFGR2_RESET_VALUE;
    RCC.CFGR3 = _::CFGR3_RESET_VALUE;
    RCC.CR2 = _::CR2_RESET_VALUE;
    RCC.CIR = _::CIR_RESET_VALUE;

    // set system clock to HSI-PLL 48MHz

    FLASH.ACR = flash_t::ACR_PRFTBE | flash_t::ACR_LATENCY<0x1>;

    RCC.CFGR |= _::CFGR_PLLMUL<0xa>;        // PLL multiplier 12
    RCC.CR |= _::CR_PLLON;                  // enable PLL
    while (!(RCC.CR & _::CR_PLLRDY));       // wait for PLL to be ready
    RCC.CFGR |= _::CFGR_SW<0x2>;            // select PLL as system clock

    // wait for PLL as system clock

    while ((RCC.CFGR & _::CFGR_SWS<0x3>) != _::CFGR_SWS<0x2>);
#elif defined(STM32F103)
    m_freq = 72000000;

    // set system clock to HSE-PLL 72MHz

    FLASH.ACR |= flash_t::ACR_PRFTBE | flash_t::template ACR_LATENCY<0x2>;

    RCC.CR |= _::CR_HSEON;                  // enable external clock
    while (!(RCC.CR & _::CR_HSERDY));       // wait for HSE ready
    RCC.CFGR |= _::CFGR_PLLSRC              // clock from PREDIV1
              | _::CFGR_PLLMUL<0x7>         // pll multiplier = 9
              | _::CFGR_PPRE1<0x4>          // APB low-speed prescale = 2
              ;
    RCC.CR |= _::CR_PLLON;                  // enable external clock
    while (!(RCC.CR & _::CR_PLLRDY));       // wait for pll ready
    RCC.CFGR |= _::CFGR_SW<0x2>;            // use pll as clock source

    // wait for clock switch completion

    while ((RCC.CFGR & _::CFGR_SWS<0x3>) != _::CFGR_SWS<0x2>);
#elif defined(STM32F411)
    m_freq = 100000000;

    // reset clock control registers

    RCC.CIR = _::CIR_RESET_VALUE;

    // set system clock to HSI-PLL 100MHz

    constexpr uint8_t wait_states = 0x3;    // 3 wait-states for 100MHz at 2.7-3.3V

    FLASH.ACR = flash_t::ACR_PRFTEN | flash_t::ACR_LATENCY<wait_states>;
    while ((FLASH.ACR & flash_t::ACR_LATENCY<0x7>) != flash_t::ACR_LATENCY<wait_states>); // wait to take effect

    enum pllP_t { pllP_2 = 0x0, pllP_4 = 0x1, pllP_6 = 0x2, pllP_8 = 0x3 };

    // fVCO = hs[ei] * pllN / pllM                      // must be 100MHz - 400MHz
    // fSYS = fVCO / pllP                               // <= 100MHz
    // fUSB = fVCO / pllQ                               // <= 48MHz

    // pllN = 200, pllM = 8, pllP = 4, pllQ = 9, fSYS = 1.0e8, fUSB = 4.4445e7

    constexpr uint16_t pllN = 200;                      // 9 bits, valid range [50..432]
    constexpr uint8_t pllM = 8;                         // 6 bits, valid range [2..63]
    constexpr pllP_t pllP = pllP_4;                     // 2 bits, enum range [2, 4, 6, 8]
    constexpr uint8_t pllQ = 9;                         // 4 bits, valid range [2..15]
    constexpr uint8_t pllSRC = 0;                       // 1 bit, 0 = HSI, 1 = HSE

    using internal::encode;

    RCC.PLLCFGR = encode<pllSRC, _::PLLCFGR_PLLSRC, 1>()
                | encode<pllN, _::PLLCFGR_PLLN0, 9>()
                | encode<pllM, _::PLLCFGR_PLLM0, 6>()
                | encode<pllP, _::PLLCFGR_PLLP0, 2>()
                | encode<pllQ, _::PLLCFGR_PLLQ0, 4>()
                ;

    RCC.CR |= _::CR_PLLON;                              // enable PLL
    while (!(RCC.CR & _::CR_PLLRDY));                   // wait for PLL to be ready
    RCC.CFGR |= encode<0x2, _::CFGR_SW0, 2>();          // select PLL as system clock

    // wait for PLL as system clock

    while ((RCC.CFGR & encode<0x3, _::CFGR_SWS0, 2>()) != encode<0x2, _::CFGR_SWS0, 2>());
#elif defined(STM32F767)
    m_freq = 16000000;
#elif defined(STM32H743)
    m_freq = 8000000;
#elif defined(STM32G070)
    m_freq = 64000000;

    // set system clock to HSI-PLL 64MHz and p-clock = 64MHz

    constexpr uint8_t wait_states = 0x2;                // 2 wait-states for 64Hz at Vcore range 1

    FLASH.ACR = flash_t::ACR_PRFTEN | flash_t::ACR_LATENCY<wait_states>;
    while ((FLASH.ACR & flash_t::ACR_LATENCY<0x7>) != flash_t::ACR_LATENCY<wait_states>); // wait to take effect

    // fR (fSYS) = fVCO / pllR                          // <= 64MHz
    // fP = fVCO / pllP                                 // <= 122MHz

    // pllN = 8.0, pllM = 1.0, pllP = 2.0, pllR = 2.0, fSYS = 6.4e7, fPllP = 6.4e7, fVCO = 1.28e8

    constexpr uint16_t pllN = 8;                        // 7 bits, valid range [8..86]
    constexpr uint8_t pllM = 1 - 1;                     // 3 bits, valid range [1..8]-1
    constexpr uint8_t pllP = 2 - 1;                     // 5 bits, valid range [2..32]-1
    constexpr uint8_t pllR = 2 - 1;                     // 3 bits, valid range [2..8]-1
    constexpr uint8_t pllSRC = 2;                       // 2 bits, 2 = HSI16, 3 = HSE

    RCC.PLLSYSCFGR = _::PLLSYSCFGR_PLLSRC<pllSRC>
                   | _::PLLSYSCFGR_PLLN<pllN>
                   | _::PLLSYSCFGR_PLLM<pllM>
                   | _::PLLSYSCFGR_PLLP<pllP>
                   | _::PLLSYSCFGR_PLLR<pllR>
                   | _::PLLSYSCFGR_PLLREN
                   | _::PLLSYSCFGR_PLLPEN
                   ;

    RCC.CR |= _::CR_PLLON;                              // enable PLL
    while (!(RCC.CR & _::CR_PLLRDY));                   // wait for PLL to be ready

    RCC.CFGR |= _::CFGR_SW<0x2>;                        // select PLL as system clock

    // wait for PLL as system clock

    while ((RCC.CFGR & _::CFGR_SWS<0x3>) != _::CFGR_SWS<0x2>);
#elif defined(STM32G431)
    m_freq = 170000000;

    // set system clock to HSI-PLL 170MHz (R) and same for P and Q clocks

    constexpr uint8_t wait_states = 0x8;                // 8 wait-states for 170MHz at Vcore range 1

    FLASH.ACR = flash_t::ACR_PRFTEN | flash_t::ACR_LATENCY<wait_states>;
    while ((FLASH.ACR & flash_t::ACR_LATENCY<0xf>) != flash_t::ACR_LATENCY<wait_states>); // wait to take effect

#if defined(HSE)
    RCC.CR |= _::CR_HSEON;                              // enable external clock
    while (!(RCC.CR & _::CR_HSERDY));                   // wait for HSE ready

#if HSE == 24000000
    constexpr uint8_t pllM = 6 - 1;                     // 3 bits, valid range [1..15]-1
#elif HSE == 8000000
    constexpr uint8_t pllM = 2 - 1;                     // 3 bits, valid range [1..15]-1
#else
    static_assert(false, "unsupported HSE frequency");
#endif
    constexpr uint8_t pllSRC = 3;                       // 2 bits, 2 = HSI16, 3 = HSE
#else
    // pllN = 85.0, pllM = 4.0, pllP = 7.0, pllPDIV = 2.0, pllQ = 2.0, pllR = 2.0, fSYS = 1.7e8, fPllP = 1.7e8, fPllQ = 1.7e8, fVCO = 3.4e8
    constexpr uint8_t pllM = 4 - 1;                     // 3 bits, valid range [1..15]-1
    constexpr uint8_t pllSRC = 2;                       // 2 bits, 2 = HSI16, 3 = HSE
#endif
    constexpr uint16_t pllN = 85;                       // 7 bits, valid range [8..127]
    constexpr uint8_t pllPDIV = 2;                      // 5 bits, valid range [2..31]
    constexpr uint8_t pllR = 0;                         // 2 bits, 0 = 2, 1 = 4, 2 = 6, 3 = 8
    constexpr uint8_t pllQ = 0;                         // 2 bits, 0 = 2, 1 = 4, 2 = 6, 3 = 8

    RCC.PLLSYSCFGR = _::PLLSYSCFGR_PLLSRC<pllSRC>
                   | _::PLLSYSCFGR_PLLSYSN<pllN>
                   | _::PLLSYSCFGR_PLLSYSM<pllM>
                   | _::PLLSYSCFGR_PLLSYSPDIV<pllPDIV>
                   | _::PLLSYSCFGR_PLLSYSQ<pllQ>
                   | _::PLLSYSCFGR_PLLSYSR<pllR>
                   | _::PLLSYSCFGR_PLLPEN
                   | _::PLLSYSCFGR_PLLSYSQEN
                   | _::PLLSYSCFGR_PLLSYSREN
                   ;

    RCC.CR |= _::CR_PLLSYSON;                           // enable PLL
    while (!(RCC.CR & _::CR_PLLSYSRDY));                // wait for PLL to be ready

    RCC.CFGR |= _::CFGR_SW<0x3>;                        // select PLL as system clock

    // wait for PLL as system clock

    while ((RCC.CFGR & _::CFGR_SWS<0x3>) != _::CFGR_SWS<0x3>);

    // enable FPU

    FPU_CPACR.CPACR |= fpu_cpacr_t::CPACR_CP<0xf>;      // enable co-processors
    __asm volatile ("dsb");                             // data pipe-line reset
    __asm volatile ("isb");                             // instruction pipe-line reset
#else
    static_assert(false, "no featured clock initialzation");
#endif
    // initialize sys-tick for milli-second counts

    hal::sys_tick_init(m_freq / 1000);
}

} //  namespace hal

template<> void handler<interrupt::SYSTICK>()
{
    hal::sys_tick_update();
}

extern void system_init(void)
{
    hal::sys_clock::init();
}

