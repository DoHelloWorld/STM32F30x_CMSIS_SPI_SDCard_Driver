/*MIT License

Copyright (c) 2019 DoHelloWorld

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/

#include "Utils.h"

/*struct of DWT module*/
typedef struct
{
    volatile uint32_t CTRL;
    volatile uint32_t CYCCNT;
}DWT_TypeDef;

/*DWT module base address*/
#define DWT         ((DWT_TypeDef *)0xE0001000UL)

/*calculate number of cycles in millisecond*/
static uint32_t DWT_CyclesInMs(void);


/** \brief Configure DWT module
 *
 * \param None
 * \return None
 *
 */
void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= 1 << 0;
}

/** \brief Getting current number of cycle
 *
 * \param None
 * \return Current number of cycle
 *
 */
uint32_t DWT_GetCycle(void)
{
    return DWT->CYCCNT;
}

/** \brief Check if DWT cycles reaches timeout
 *
 * \param timeout_ms : timeout in milliseconds
 * \param timestamp : timestamp from where we should count timeout
 * \return 1 if reach timeout. 0 if not.
 *
 */
uint8_t DWT_Timeout(uint32_t timeout_ms, uint32_t timestamp)
{
    uint32_t timeout = timeout_ms * DWT_CyclesInMs();
    if(timeout >= (DWT_GetCycle() - timestamp))
        return 0;
    return 1;
}

/** \brief Get cycles count in millisecond
 *
 * \param None
 * \return Number of cycles in millisecond
 *
 */
static uint32_t DWT_CyclesInMs(void)
{
    return SystemCoreClock / 1000;
}
