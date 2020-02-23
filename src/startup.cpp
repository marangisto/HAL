#include <hal.h>

extern uint32_t __sbss, __ebss;
extern uint32_t __sdata, __edata;
extern uint32_t __sidata;
extern uint32_t __estack;

extern void system_init(void);
extern int main(void);

template<> void handler<interrupt::RESET>()
{
    uint32_t *bss = &__sbss;
    uint32_t *data = &__sdata;
    uint32_t *idata = &__sidata;

    while (data < &__edata)
        *data++ = *idata++;

    while (bss < &__ebss)
        *bss++ = 0;

    system_init();

    main();

    while (1)
        ;
}

extern "C" void __reset() __attribute__ ((alias("_Z7handlerILN9interrupt11interrupt_tEn15EEvv")));

extern void __default_handler(void) {}

#if defined(STM32F051)
#include "vector/stm32f0x1.cpp"
#elif defined(STM32F072)
#include "vector/stm32f0x2.cpp"
#elif defined (STM32F103)
#include "vector/stm32f103.cpp"
#elif defined (STM32F411)
#include "vector/stm32f411.cpp"
#elif defined (STM32F412)
#include "vector/stm32f412.cpp"
#elif defined (STM32F767)
#include "vector/stm32f7x7.cpp"
#elif defined (STM32H743)
#include "vector/stm32h7x3.cpp"
#elif defined (STM32G070)
#include "vector/stm32g07x.cpp"
#elif defined (STM32G431)
#include "vector/stm32g431.cpp"
#else
    _Static_assert (0, "no startup sequence for MCU");
#endif

