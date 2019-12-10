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

#ifndef SDCARD_SPI_H_INCLUDED
#define SDCARD_SPI_H_INCLUDED
#include "stm32f30x.h"

/// SD SPI return states
typedef enum
{
    SD_SPI_OK,      ///< API function executed successfully
    SD_SPI_ERROR    ///< API function executed with a fail
}SD_SPI_Status_t;

/// Arguments for function SD_SPI_SetSpeed
typedef enum
{
    SD_SPI_INIT_SPEED,      ///< Initialize mode clock < 400 kHz
    SD_SPI_TRANSFER_SPEED   ///< Transfer mode clock < 50 MHz
}SD_SPI_Speed_t;

/* Functions to configure SPI module */
void SD_SPI_Config(SPI_TypeDef * SPIx, uint8_t CS_Pin, GPIO_TypeDef * CS_Port);
void SD_SPI_SetSpeed(SPI_TypeDef * SPIx, uint32_t clk, SD_SPI_Speed_t speed);

/* Functions to select/deselect SPI slave */
void SD_SPI_CS_Set(GPIO_TypeDef * port, uint8_t pin);
void SD_SPI_CS_Reset(GPIO_TypeDef * port, uint8_t pin);

/* Functions to transfer data */
SD_SPI_Status_t SD_SPI_Send8Data(SPI_TypeDef * SPIx, uint8_t * data, uint32_t len);
SD_SPI_Status_t SD_SPI_Receive8Data(SPI_TypeDef * SPIx, uint8_t * data, uint32_t len);
SD_SPI_Status_t SD_SPI_Send16Data(SPI_TypeDef * SPIx, uint16_t * data, uint32_t len, FunctionalState crcState);
SD_SPI_Status_t SD_SPI_Receive16Data(SPI_TypeDef * SPIx, uint16_t * data, uint32_t len, FunctionalState crcState);


#endif /* SDCARD_SPI_H_INCLUDED */
