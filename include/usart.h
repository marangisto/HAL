#pragma once

#include <gpio.h>

namespace hal
{

namespace usart
{

using namespace device;
using namespace gpio;

template<int NO> struct usart_traits {};

template<> struct usart_traits<1>
{
    typedef usart1_t T;
    static inline T& USART() { return USART1; }
    static const gpio::internal::alternate_function_t tx = gpio::internal::USART1_TX;
    static const gpio::internal::alternate_function_t rx = gpio::internal::USART1_RX;
};

template<> struct usart_traits<2>
{
    typedef usart2_t T;
    static inline T& USART() { return USART2; }
    static const gpio::internal::alternate_function_t tx = gpio::internal::USART2_TX;
    static const gpio::internal::alternate_function_t rx = gpio::internal::USART2_RX;
};

#if defined(HAVE_PERIPHERAL_USART3)
template<> struct usart_traits<3>
{
    typedef usart3_t T;
    static inline T& USART() { return USART3; }
    static const gpio::internal::alternate_function_t tx = gpio::internal::USART3_TX;
    static const gpio::internal::alternate_function_t rx = gpio::internal::USART3_RX;
};
#endif

#if defined(HAVE_PERIPHERAL_USART4)
template<> struct usart_traits<4>
{
    typedef usart4_t T;
    static inline T& USART() { return USART4; }
    static const gpio::internal::alternate_function_t tx = gpio::internal::USART4_TX;
    static const gpio::internal::alternate_function_t rx = gpio::internal::USART4_RX;
};
#endif

template<int NO, gpio_pin_t TX, gpio_pin_t RX> struct usart_t
{
private:
    typedef typename usart_traits<NO>::T _;

public:
    template
        < uint32_t              baud_rate = 9600
        , uint8_t               data_bits = 8
        , uint8_t               stop_bits = 1
        , bool                  parity = false
        , output_speed_t        speed = high_speed
        >
    static inline void setup()
    {
        using namespace gpio::internal;

        alternate_t<TX, usart_traits<NO>::tx>::template setup<speed>();
        alternate_t<RX, usart_traits<NO>::rx>::template setup<pull_up>();   // FIXME: should this be pull-up?

        // baud-rate = fsck / usartdiv
        // usartdiv = 64000000 / 9600
        peripheral_traits<_>::enable();         // enable usart peripheral clock
        USART().BRR = 64000000 / baud_rate;     // set baud-rate FIXME: need clock reference!
        USART().CR1 |= _::CR1_RESET_VALUE       // reset control register 1
                    | _::CR1_TE                 // enable transmitter
                    | _::CR1_RE                 // enable receiver
                    | _::CR1_RXNEIE             // interrupt on rx buffer not empty
                    | _::CR1_UE                 // enable usart itself
                    ;
        USART().CR2 |= _::CR2_RESET_VALUE;      // reset control register 2
        USART().CR3 |= _::CR3_RESET_VALUE;      // reset control register 3
    }


    __attribute__((always_inline))
    static inline void write(uint8_t x)
    {
        while (!tx_empty());
        USART2.TDR = x;
    }

    static inline void write(const char *s)
    {
        while (*s)
            write(*s++);
    }

    __attribute__((always_inline))
    static inline char read()
    {
        while (!rx_not_empty());
        return USART2.RDR;
    }

    __attribute__((always_inline))
    static inline bool tx_empty()
    {
        return USART().ISR & _::ISR_TXE;
    }

    __attribute__((always_inline))
    static inline bool rx_not_empty()
    {
        return USART().ISR & _::ISR_RXNE;
    }

private:
    static inline typename usart_traits<NO>::T& USART() { return usart_traits<NO>::USART(); }
};

} // namespace usart

} // namespace hal

