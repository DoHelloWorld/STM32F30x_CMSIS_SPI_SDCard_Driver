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

#include "stm32f30x.h"
#include "SDCard.h"

uint8_t buffer_Tx[2048];
uint8_t buffer_Rx[2048];

/*Init SPI pins*/
void SD_SPI_GPIO_Init(void);
/*Error handler*/
void ErrorHandler(void);


int main(void)
{
    SD_SPI_GPIO_Init();

    for(uint16_t i = 0; i < 2048; i++)
        buffer_Tx[i] = i;

    SD_Parameters_t SD;
    SD.SPIx         =   SPI3;
    SD.SPIx_Clk     =   36000000;
    SD.CS_Pin       =   9;
    SD.CS_Port      =   GPIOC;

    SD_Error_t status = SD_OK;

    status = SD_Init(&SD);
    if(status != SD_OK)
        ErrorHandler();

    status = SD_WriteMultipleBlock(&SD, 0, buffer_Tx, 4);
    if(status != SD_OK)
        ErrorHandler();

    status = SD_ReadMultipleBlock(&SD, 0, buffer_Rx, 4);
     if(status != SD_OK)
        ErrorHandler();

    while(1)
    {

    }
}

void SD_SPI_GPIO_Init(void)
{
    /*Configure SPI GPIO************************************************************************/
    /*You should configure MISO, MOSI and SCK pins here!*/
    RCC->AHBENR     |=  RCC_AHBENR_GPIOCEN;
    GPIOC->MODER    &=  ~(GPIO_MODER_MODER10 | GPIO_MODER_MODER11 | GPIO_MODER_MODER12);
    GPIOC->MODER    |=  (GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1 | GPIO_MODER_MODER12_1);
    GPIOC->OTYPER   &=  ~(GPIO_OTYPER_OT_10 | GPIO_OTYPER_OT_11 | GPIO_OTYPER_OT_12);
    GPIOC->OSPEEDR  |=  (GPIO_OSPEEDER_OSPEEDR10 | GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12);
    GPIOC->PUPDR    &=  ~(GPIO_PUPDR_PUPDR10 | GPIO_PUPDR_PUPDR11 | GPIO_PUPDR_PUPDR12);
    GPIOC->PUPDR    |=  (GPIO_PUPDR_PUPDR10_0 | GPIO_PUPDR_PUPDR11_0 | GPIO_PUPDR_PUPDR12_0);

    GPIOC->AFR[1]   &=  ~(GPIO_AFRH_AFRH2 | GPIO_AFRH_AFRH3 | GPIO_AFRH_AFRH4);
    GPIOC->AFR[1]   |=  ((6 << (4 * 2)) | (6 << (4 * 3)) | (6 << (4 * 4)));
    /*********************************************************************************************/
}

void ErrorHandler(void)
{
    while(1)
    {
        //handle error
    }
}


