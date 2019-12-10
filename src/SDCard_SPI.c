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

#include "SDCard_SPI.h"
#include "Utils.h"

#define SD_SPI_TIMEOUT      100     /**< SPI timeout in milliseconds */
#define SD_SPI_CRC16_POL    0x1021  /**< Polynomial for CRC-16-CCITT */

/// Specifies SPI working mode
typedef enum
{
    SD_SPI_8BIT,    ///< Sets data transfer to 8-bit
    SD_SPI_16BIT    ///< Sets data transfer to 16-bit
}SD_SPI_Bit_t;

/*Configure transfer data size function*/
static void SD_SPI_SetDataSize(SPI_TypeDef * SPIx, SD_SPI_Bit_t bitnum);

/*CRC configuration functions*/
static void SD_SPI_CRC_Cmd(SPI_TypeDef * SPIx, uint16_t polynomial, FunctionalState state);
static void SD_SPI_CRC_Reset(SPI_TypeDef * SPIx);
static SD_SPI_Status_t SD_SPI_CRC_Check(SPI_TypeDef * SPIx);

/*RCC configuration functions*/
static void SD_SPI_SetSPI_RCC(SPI_TypeDef * SPIx);
static void SD_SPI_SetGPIO_RCC(GPIO_TypeDef * GPIOx);

/** \brief Configure transfer data size for SPIx
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \param  bitnum: specifies the transfer data size.
  *   This parameter can be one of the following values:
  *     \arg SD_SPI_8BIT: transfer size 8-bit
  *     \arg SD_SPI_16BIT: transfer size 16-bit
  * \retval None
*/
static void SD_SPI_SetDataSize(SPI_TypeDef * SPIx, SD_SPI_Bit_t bitnum)
{
    SPIx->CR1 &= ~SPI_CR1_SPE;
    if(bitnum == SD_SPI_8BIT)
    {
        /*Set FIFO indication level to 1/4*/
        SPIx->CR2 |= SPI_CR2_FRXTH;
        SPIx->CR2 &= ~(SPI_CR2_DS_3);
    }
    else if(bitnum == SD_SPI_16BIT)
    {
        /*Set FIFO indication level to 1/2*/
        SPIx->CR2 &= ~(SPI_CR2_FRXTH);
        SPIx->CR2 |= (SPI_CR2_DS);
    }
    SPIx->CR1 |= SPI_CR1_SPE;
}

/** \brief Enable/Disable CRC-16 calculation.
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \param  polynomial: polynomial for calculated CRC-16
  * \param  state: state of CRC module
  *   This parameter can be one of the following values:
  *     \arg ENABLE: CRC16 calculation is enabled.
  *     \arg DISABLE: CRC16 calculation is enabled.
  * \retval None
*/
static void SD_SPI_CRC_Cmd(SPI_TypeDef * SPIx, uint16_t polynomial, FunctionalState state)
{
    if(state == ENABLE)
    {
        SD_SPI_CRC_Reset(SPIx);
        SPIx->CRCPR = polynomial;
    }
    else
    {
        /* Disable SPIx module */
        SPIx->CR1 &= ~(SPI_CR1_SPE);
        /* Switching off all CRC bits*/
        SPIx->CR1 &= ~(SPI_CR1_CRCEN);
        SPIx->CR1 &= ~(SPI_CR1_CRCL);
        SPIx->CR1 &= ~(SPI_CR1_CRCNEXT);
        /* Enable SPIx module */
        SPIx->CR1 |= SPI_CR1_SPE;
    }
}

/** \brief Resets CRC16 module in SPIx.
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \retval None
*/
static void SD_SPI_CRC_Reset(SPI_TypeDef * SPIx)
{
    /* Disable SPIx module */
    SPIx->CR1 &= ~(SPI_CR1_SPE);
    /* Switching off all CRC bits*/
    SPIx->CR1 &= ~(SPI_CR1_CRCEN);
    SPIx->CR1 &= ~(SPI_CR1_CRCL);
    /* Switching on all CRC bits*/
    SPIx->CR1 |= SPI_CR1_CRCEN;
    SPIx->CR1 |= SPI_CR1_CRCL;
    /* Enable SPIx module */
    SPIx->CR1 |= SPI_CR1_SPE;
}

/** \brief Checks CRC16 validity for received data.
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \retval None
*/
static SD_SPI_Status_t SD_SPI_CRC_Check(SPI_TypeDef * SPIx)
{
    if(SPIx->RXCRCR != 0)
        return SD_SPI_ERROR;
    return SD_SPI_OK;
}

/** \brief Enabled RCC for SPix.
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \retval None
*/
static void SD_SPI_SetSPI_RCC(SPI_TypeDef * SPIx)
{
    if(SPIx == SPI1)
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    else if (SPIx == SPI2)
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    else if (SPIx == SPI3)
        RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
}

/** \brief Enabled RCC for CS pin.
  * \param  GPIOx: where x can be A, B, C, D, E or F to select the GPIO peripheral.
  * \retval None
*/
static void SD_SPI_SetGPIO_RCC(GPIO_TypeDef * GPIOx)
{
    if(GPIOx == GPIOA)
        RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    else if(GPIOx == GPIOB)
        RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    else if(GPIOx == GPIOC)
        RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    else if(GPIOx == GPIOD)
        RCC->AHBENR |= RCC_AHBENR_GPIODEN;
    else if(GPIOx == GPIOE)
        RCC->AHBENR |= RCC_AHBENR_GPIOEEN;
    else if(GPIOx == GPIOF)
        RCC->AHBENR |= RCC_AHBENR_GPIOFEN;
}

/** \brief Configure SPIx module. Configure GPIO for CS pin.
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \param  CS_Pin: pin in range 0..15
  * \param  CS_Port: port in range GPIOA..GPIOF
  * \retval None
*/
void SD_SPI_Config(SPI_TypeDef * SPIx, uint8_t CS_Pin, GPIO_TypeDef * CS_Port)
{
    /*Configure DWT module to count Sysclock cycles*/
    DWT_Init();

    /*Configure SPI CS pin*/
    SD_SPI_SetGPIO_RCC(CS_Port);

    /*  Mode: output
        Speed: 50 MHz
        Push-Pull: no pull
    */
    CS_Port->MODER      &=  ~(GPIO_MODER_MODER0 << (CS_Pin << 1));
    CS_Port->MODER      |=   (GPIO_MODER_MODER0_0 << (CS_Pin << 1));
    CS_Port->OTYPER     &=  ~(GPIO_OTYPER_OT_0 << CS_Pin);
    CS_Port->PUPDR      &=  ~(GPIO_PUPDR_PUPDR0 << (CS_Pin << 1));
    CS_Port->OSPEEDR    |=  (GPIO_OSPEEDER_OSPEEDR0 << (CS_Pin << 1));

    /*Set CS to high level*/
    SD_SPI_CS_Set(CS_Port, CS_Pin);


    /*Configure SPI*/
    SD_SPI_SetSPI_RCC(SPIx);
    SPIx->CR1 = 0;
    SPIx->CR2 = 0;
    /*  Mode: master
        Polarity: high
        Phase: second edge
        SS: software control
    */
    SPIx->CR1 |= (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_CPHA | SPI_CR1_CPOL);
    SPIx->CRCPR = 0x1021;
    SPIx->CR1 |= SPI_CR1_SPE;
    /**************************************************************************************/
}

/** \brief Set CS pin to high level.
  * \param  CS_Pin: pin in range 0..15
  * \param  CS_Port: port in range GPIOA..GPIOF
  * \retval None
*/
void SD_SPI_CS_Set(GPIO_TypeDef * port, uint8_t pin)
{
    port->BSRR = 1 << pin;
}

/** \brief Set CS pin to low level.
  * \param  CS_Pin: pin in range 0..15
  * \param  CS_Port: port in range GPIOA..GPIOF
  * \retval None
*/
void SD_SPI_CS_Reset(GPIO_TypeDef * port, uint8_t pin)
{
    port->BRR = 1 << pin;
}

/** \brief Configure SPIx frequency
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \param  clk: current SPIx bus frequency.
  * \param  speed: state of CRC module
  *   This parameter can be one of the following values:
  *     \arg SD_SPI_INIT_SPEED: frequency < 400 kHz
  *     \arg SD_SPI_TRANSFER_SPEED: frequency < 50 kHz
  * \retval None
*/
void SD_SPI_SetSpeed(SPI_TypeDef * SPIx, uint32_t clk, SD_SPI_Speed_t speed)
{
    SPIx->CR1 &= ~(SPI_CR1_BR);
    if(speed == SD_SPI_INIT_SPEED)
    {
        /*Divider - 256*/
        if(clk >= 50000000)
            SPIx->CR1 |= (SPI_CR1_BR_0 | SPI_CR1_BR_1 | SPI_CR1_BR_2);
        /*Divider - 128*/
        else if(clk >= 24000000 && clk < 50000000)
            SPIx->CR1 |= (SPI_CR1_BR_1 | SPI_CR1_BR_2);
        /*Divider - 64*/
        else if(clk >= 12000000 && clk < 24000000)
            SPIx->CR1 |= (SPI_CR1_BR_0 | SPI_CR1_BR_2);
        /*Divider - 32*/
        else if(clk >= 6000000 && clk < 12000000)
            SPIx->CR1 |= (SPI_CR1_BR_2);
        /*Divider - 16*/
        else
            SPIx->CR1 |= (SPI_CR1_BR_0 | SPI_CR1_BR_1);
    }
    else if(speed == SD_SPI_TRANSFER_SPEED)
    {
        /*Divider - 2*/
        if(clk >= 50000000)
            SPIx->CR1 |= (SPI_CR1_BR_0);
    }
}


/** \brief Sends 8-bit data to slave
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \param  data: pointer to data buffer.
  * \param  len: number of bytes to send
  * \retval SPIx state
*/
SD_SPI_Status_t SD_SPI_Send8Data(SPI_TypeDef * SPIx, uint8_t * data, uint32_t len)
{
    uint32_t timestamp = 0;
    /*Configure SPIx to 8-bit transfer mode*/
    SD_SPI_SetDataSize(SPIx, SD_SPI_8BIT);
    for(uint32_t i = 0; i < len; ++i)
    {
        /*Get timestamp to calculate timeout*/
        timestamp = DWT_GetCycle();
        /*Wait for empty buffer to transmit*/
        while(!(SPIx->SR & SPI_SR_TXE));
        {
            /*if timeout returns 1 - SPI has an error*/
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*in 8-bit mode we shoult have 8-bit access to SPIx->DR register*/
        *(__IO uint8_t*)&SPIx->DR = data[i];
        timestamp = DWT_GetCycle();
        /*Wait for received byte*/
        while(!(SPIx->SR & SPI_SR_RXNE));
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*read out received byte from FIFO*/
        *(__IO uint8_t*)&SPIx->DR;
    }
    timestamp = DWT_GetCycle();
    /*Wait while SPIx busy */
    while(SPIx->SR & SPI_SR_BSY);
    {
        if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
            return SD_SPI_ERROR;
    }
    return SD_SPI_OK;
}

/** \brief Receive 8-bit data from slave
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \param  data: pointer to data buffer.
  * \param  len: number of bytes to send
  * \retval SPIx state
*/
SD_SPI_Status_t SD_SPI_Receive8Data(SPI_TypeDef * SPIx, uint8_t * data, uint32_t len)
{
    uint32_t timestamp = 0;
    /*Configure SPIx to 8-bit transfer mode*/
    SD_SPI_SetDataSize(SPIx, SD_SPI_8BIT);
    for(uint32_t i = 0; i < len; ++i)
    {
        /*Get timestamp to calculate timeout*/
        timestamp = DWT_GetCycle();
        /*Wait for empty buffer to transmit*/
        while(!(SPIx->SR & SPI_SR_TXE));
        {
            /*if timeout returns 1 - SPI has an error*/
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*Send 8 clocks to SCK line*/
        *(__IO uint8_t*)&SPIx->DR = 0xFF;
        timestamp = DWT_GetCycle();
        /*Wait for received byte*/
        while(!(SPIx->SR & SPI_SR_RXNE));
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*read out received byte from FIFO*/
        data[i] = *(__IO uint8_t*)&SPIx->DR;
    }
    timestamp = DWT_GetCycle();
    /*Wait while SPIx busy */
    while(SPIx->SR & SPI_SR_BSY);
    {
        if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
            return SD_SPI_ERROR;
    }
    return SD_SPI_OK;
}

/** \brief Sends 16-bit data to slave
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \param  data: pointer to data buffer.
  * \param  len: number of bytes to send
  * \param  crcState: state of CRC module
  *   This parameter can be one of the following values:
  *     \arg ENABLE: CRC16 calculation is enabled.
  *     \arg DISABLE: CRC16 calculation is enabled.
  * \retval SPIx state
*/
SD_SPI_Status_t SD_SPI_Send16Data(SPI_TypeDef * SPIx, uint16_t * data, uint32_t len, FunctionalState crcState)
{
    uint32_t timestamp = 0;
    /*Configure SPIx to 16-bit transfer mode*/
    SD_SPI_SetDataSize(SPIx, SD_SPI_16BIT);
    /*If crcState enabled SPIx module start to calculate CRC16*/
    if(crcState == ENABLE)
        SD_SPI_CRC_Cmd(SPIx, SD_SPI_CRC16_POL, ENABLE);
    for(uint32_t i = 0; i < len; ++i)
    {
        /*Get timestamp to calculate timeout*/
        timestamp = DWT_GetCycle();
        /*Wait for empty buffer to transmit*/
        while(!(SPIx->SR & SPI_SR_TXE))
        {
             /*if timeout returns 1 - SPI has an error*/
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*We should send with low byte first because we use uint16_t pointer*/
        uint16_t tmp = (data[i] << 8) | (data[i] >> 8);
        SPIx->DR = tmp;
        timestamp = DWT_GetCycle();
        /*Wait for received byte*/
        while(!(SPIx->SR & SPI_SR_RXNE))
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*read out received byte from FIFO*/
        SPIx->DR;
    }
    timestamp = DWT_GetCycle();
    /*We should wait for Busy flag resets, because of CRC16 calculating */
    while(SPIx->SR & SPI_SR_BSY)
    {
        if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
            return SD_SPI_ERROR;
    }
    /*if CRC16 calculated - send calculated value after data block*/
    if(crcState == ENABLE)
    {
        uint16_t crc = SPIx->TXCRCR;
        timestamp = DWT_GetCycle();
        while(!(SPIx->SR & SPI_SR_TXE))
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*Sending CRC16*/
        SPIx->DR = crc;
        timestamp = DWT_GetCycle();
        while(SPIx->SR & SPI_SR_BSY)
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*Clear receive FIFO*/
        SPIx->DR;
    }
    return SD_SPI_OK;
}

/** \brief Receive 16-bit data from slave
  * \param  SPIx: where x can be 1, 2 or 3 to select the SPI peripheral.
  * \param  data: pointer to data buffer.
  * \param  len: number of bytes to send
  * \param  crcState: state of CRC module
  *   This parameter can be one of the following values:
  *     \arg ENABLE: CRC16 calculation is enabled.
  *     \arg DISABLE: CRC16 calculation is enabled.
  * \retval SPIx state
*/
SD_SPI_Status_t SD_SPI_Receive16Data(SPI_TypeDef * SPIx, uint16_t * data, uint32_t len, FunctionalState crcState)
{
    uint32_t timestamp = DWT_GetCycle();
    /*Configure SPIx to 16-bit transfer mode*/
    SD_SPI_SetDataSize(SPIx, SD_SPI_16BIT);
    /*If crcState enabled SPIx module start to calculate CRC16*/
    if(crcState == ENABLE)
        SD_SPI_CRC_Cmd(SPIx, SD_SPI_CRC16_POL, ENABLE);
    for(uint32_t i = 0; i < len; ++i)
    {
        /*Get timestamp to calculate timeout*/
        timestamp = DWT_GetCycle();
        /*Wait for empty buffer to transmit*/
        while(!(SPIx->SR & SPI_SR_TXE))
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*Send 16 clocks on SCK line*/
        SPIx->DR = 0xFFFF;
        timestamp = DWT_GetCycle();
        /*Wait for received byte*/
        while(!(SPIx->SR & SPI_SR_RXNE))
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*Switching high byte with lo because of uint16_t pointer*/
        uint16_t tmp = SPIx->DR;
        data[i] = (tmp >> 8) | (tmp << 8);
    }
    /*If CRC16 is enabled get crc after data block*/
    if(crcState == ENABLE)
    {
        while(!(SPIx->SR & SPI_SR_TXE))
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*Send 16 clocks on SCK line*/
        SPIx->DR = 0xFFFF;
        timestamp = DWT_GetCycle();
        while(!(SPIx->SR & SPI_SR_RXNE))
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
         /*Clear receive FIFO*/
        SPIx->DR;
        while((SPIx->SR & SPI_SR_BSY))
        {
            if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
                return SD_SPI_ERROR;
        }
        /*Check for received crc validation*/
        if(SD_SPI_CRC_Check(SPIx) == SD_SPI_ERROR)
            return SD_SPI_ERROR;
    }
    /*Wait while SPIx busy */
    while((SPIx->SR & SPI_SR_BSY))
    {
        if(DWT_Timeout(SD_SPI_TIMEOUT, timestamp))
            return SD_SPI_ERROR;
    }
    return SD_SPI_OK;
}

