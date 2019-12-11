# STM32F30x_CMSIS_SPI_SDCard_Driver
Simple SD card driver for stm32f30x devices on CMSIS without SPL and HAL.

Driver uses SPI to connect with SD card and CRC16 hardware calculation.
For timeout measurement driver uses DWT module.

## Getting Started

You can use your own CMSIS library or get it from here.
CMSIS library folder: [cmsis](cmsis) 
System files folder: [boot](boot)

Driver supports SPI1, SPI2 and SPI3. At your project you should init SPIx GPIO before init SD card e.g for SPI3:

``` c
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
```
To be able to work with several SD cards at the same time driver API uses SD identification structure
```
SD_Parameters_t
```
You should init some fields in this struct for each SD card. Example below:

``` c
    SD_Parameters_t SD;               // SD identification struct
    SD.SPIx         =   SPI3;         // SPI module
    SD.SPIx_Clk     =   36000000;     // SPI APB clock
    SD.CS_Pin       =   9;            // Chip select pin number
    SD.CS_Port      =   GPIOC;        // Chip select port
```
You don't need to init SPI module and CS pin driver will do it in SD init function.

Driver supports SDSC, SDHC and SDXC card types. Information about successfuully initialized SD card will be in SD udentification struct.

API functions are not thread-safe, use mutexes!

Have fun!

## Authors

* **Styazhkin Artyom** - [DoHelloWorld](https://github.com/DoHelloWorld)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

