#pragma once

#include "hal.h"
#include "gpio.h"
#include "dma.h"

namespace hal
{

namespace dac
{

template<uint8_t NO> struct dac_traits {};

#if defined(HAVE_PERIPHERAL_DAC)
template<> struct dac_traits<1>
{
    typedef device::dac_t T;
    static inline T& DAC() { return device::DAC; }
    static constexpr gpio::gpio_pin_t ch1_pin = gpio::PA4;
    static constexpr gpio::gpio_pin_t ch2_pin = gpio::PA5;
};
#elif defined(HAVE_PERIPHERAL_DAC1)
template<> struct dac_traits<1>
{
    typedef device::dac1_t T;
    static inline T& DAC() { return device::DAC1; }
    static constexpr gpio::gpio_pin_t ch1_pin = gpio::PA4;
    static constexpr gpio::gpio_pin_t ch2_pin = gpio::PA5;
};
#endif

#if defined(HAVE_PERIPHERAL_DAC2)
template<> struct dac_traits<2>
{
    typedef device::dac2_t T;
    static inline T& DAC() { return device::DAC2; }
    static constexpr gpio::gpio_pin_t ch1_pin = gpio::PA7;  // FIXME: check this!
};
#endif

template<uint8_t NO, uint8_t CH> struct dac_channel_traits {};

template<uint8_t NO> struct dac_channel_traits<NO, 1>
{
    typedef typename dac_traits<NO>::T _;
    static inline typename dac_traits<NO>::T& DAC() { return dac_traits<NO>::DAC(); }
    static constexpr gpio::gpio_pin_t pin = dac_traits<NO>::ch1_pin;
    static constexpr uint32_t CR_EN = _::CR_EN1;
    static constexpr uint32_t CR_TEN = _::CR_TEN1;
    static constexpr uint32_t CR_DMAEN = _::CR_DMAEN1;
    static constexpr uint32_t CR_DMAUDRIE = _::CR_DMAUDRIE1;
    static constexpr uint32_t SWTRGR_SWTRIG = _::SWTRGR_SWTRIG1;
    template<uint32_t X> static constexpr uint32_t CR_TSEL = _::template CR_TSEL1<X>;
    template<uint32_t X> static constexpr uint32_t CR_WAVE = _::template CR_WAVE1<X>;
    template<uint32_t X> static constexpr uint32_t STMODR_STINCTRIGSEL = _::template STMODR_STINCTRIGSEL1<X>;
    template<uint32_t X> static constexpr uint32_t STMODR_STRSTTRIGSEL = _::template STMODR_STRSTTRIGSEL1<X>;
    static inline volatile uint32_t& STR() { return DAC().STR1; }
    static inline volatile uint32_t& DHR12R() { return DAC().DHR12R1; }
    static inline void write(uint16_t x) { dac_traits<NO>::DAC().DHR12R1 = x; }
};

template<uint8_t NO> struct dac_channel_traits<NO, 2>
{
    typedef typename dac_traits<NO>::T _;
    static inline typename dac_traits<NO>::T& DAC() { return dac_traits<NO>::DAC(); }
    static constexpr gpio::gpio_pin_t pin = dac_traits<NO>::ch2_pin;
    static constexpr uint32_t CR_EN = _::CR_EN2;
    static constexpr uint32_t CR_TEN = _::CR_TEN2;
    static constexpr uint32_t CR_DMAEN = _::CR_DMAEN2;
    static constexpr uint32_t CR_DMAUDRIE = _::CR_DMAUDRIE2;
    static constexpr uint32_t SWTRGR_SWTRIG = _::SWTRGR_SWTRIG2;
    template<uint32_t X> static constexpr uint32_t CR_TSEL = _::template CR_TSEL2<X>;
    template<uint32_t X> static constexpr uint32_t CR_WAVE = _::template CR_WAVE2<X>;
    template<uint32_t X> static constexpr uint32_t STMODR_STINCTRIGSEL = _::template STMODR_STINCTRIGSEL2<X>;
    template<uint32_t X> static constexpr uint32_t STMODR_STRSTTRIGSEL = _::template STMODR_STRSTTRIGSEL2<X>;
    static inline volatile uint32_t& STR() { return DAC().STR2; }
    static inline volatile uint32_t& DHR12R() { return DAC().DHR12R2; }
    static inline void write(uint16_t x) { dac_traits<NO>::DAC().DHR12R2 = x; }
};

template<uint8_t NO>
struct dac_t
{
    typedef typename dac_traits<NO>::T _;
    static inline typename dac_traits<NO>::T& DAC() { return dac_traits<NO>::DAC(); }

    static void setup()
    {
        device::peripheral_traits<_>::enable();                 // enable dac clock

        DAC().CR = _::CR_RESET_VALUE;                   // reset control register
        DAC().MCR = _::MCR_RESET_VALUE                  // reset mode control register
#if !defined(STM32G070)
                  | _::template MCR_HFSEL<0x2>          // high-frequency mode (AHB > 160MHz)
#endif
                  ;
    }

    template<uint8_t CH>
    static inline void setup_pin()
    {
        gpio::analog_t<dac_channel_traits<NO, CH>::pin>::template setup<gpio::floating>();
    }

    template<uint8_t CH, uint8_t RST, uint8_t INC>
    static inline void setup()
    {
        DAC().CR |= dac_channel_traits<NO, CH>::CR_TEN                                  // enable trigger
                     |  dac_channel_traits<NO, CH>::template CR_TSEL<RST>                   // trigger selection
                     ;
        DAC().STMODR |= dac_channel_traits<NO, CH>::template STMODR_STRSTTRIGSEL<RST>   // reset trigger selection
                         |  dac_channel_traits<NO, CH>::template STMODR_STINCTRIGSEL<INC>   // increment trigger selection
                         ;
    }

    template<uint8_t CH>
    static inline void enable()
    {
        DAC().CR |= dac_channel_traits<NO, CH>::CR_EN;      // enable dac channel
        sys_tick::delay_us(8);                              // wait for voltage to settle
    }

    template<uint8_t CH>
    static inline void disable()
    {
        DAC().CR &= ~dac_channel_traits<NO, CH>::CR_EN;     // disable dac channel
    }

    template<uint8_t CH, typename DMA, uint8_t DMACH, typename T>
    static inline void enable_dma(const T *source, uint16_t nelem)
    {
        volatile uint32_t& reg = dac_channel_traits<NO, CH>::DHR12R();

        DAC().CR |= dac_channel_traits<NO, CH>::CR_DMAEN;       // enable dac channel dma
        DAC().CR |= dac_channel_traits<NO, CH>::CR_DMAUDRIE;    // enable dac dma underrun interrupt
        DMA::template disable<DMACH>();                                 // disable dma channel
        DMA::template mem_to_periph<DMACH>(source, nelem, &reg);    // configure dma from memory
        DMA::template enable<DMACH>();                                  // enable dma channel
#if defined(STM32G431)
        dma::dmamux_traits<DMA::INST, DMACH>::CCR() = device::dmamux_t::C0CR_DMAREQ_ID<6>;
#elif defined(STM32G070)
        dma::dmamux_traits<DMA::INST, DMACH>::CCR() = device::dmamux_t::C0CR_DMAREQ_ID<8>;
#endif
        enable<CH>();                                               // enable dac channel
    }

    template<uint8_t CH, typename DMA, uint8_t DMACH>
    static inline void disable_dma()
    {
        DAC().CR &= ~dac_channel_traits<NO, CH>::CR_DMAEN;      // disable dac channel dma
        disable<CH>();                                              // disable dac channel
        sys_tick::delay_us(500);                                // ensure miniumum wait before next enable
        DMA::template abort<DMACH>();                               // stop dma on relevant dma channel
        DAC().CR &= ~dac_channel_traits<NO, CH>::CR_DMAUDRIE;   // disable dac channel underrun interrupt
    }

    template<uint8_t CH, uint32_t RST, uint32_t INC>
    static inline void enable_wave()
    {
        dac_channel_traits<NO, CH>::STR() |= _::template STR1_STRSTDATA1<RST>   // reset value
                                          |  _::template STR1_STINCDATA1<INC>   // increment value
                                          |  _::STR1_STDIR1                     // direction increment
                                          ;
        DAC().CR |= dac_channel_traits<NO, CH>::template CR_WAVE<0x3>;          // sawtooth wave enable
    }

    template<uint8_t CH, uint8_t SEL = 0>
    static inline void enable_trigger()
    {
        DAC().CR |= dac_channel_traits<NO, CH>::CR_TEN                  // enable trigger
                     |  dac_channel_traits<NO, CH>::template CR_TSEL<SEL>   // select trigger source
                     ;
    }

    template<uint8_t CH>
    static inline void write(uint16_t x)
    {
        dac_channel_traits<NO, CH>::write(x);
    }

    template<uint8_t CH>
    static inline void trigger()
    {
        DAC().SWTRGR |= dac_channel_traits<NO, CH>::SWTRGR_SWTRIG;
    }
};

} // namespace adc

} // namespace hal

