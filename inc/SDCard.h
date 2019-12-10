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

#ifndef SDCARD_H_INCLUDED
#define SDCARD_H_INCLUDED
#include "stm32f30x.h"

/// SD card API functions return value
typedef enum
{
    SD_OK,                          ///< API function executed correctly
    SD_INCORRECT_RESPONSE,          ///< API function received wrong response from SD
    SD_CRC_ERROR,                   ///< SD card gets wrong CRC16 after data block
    SD_WRITE_ERROR,                 ///< Error occured while programming flash
    SD_ERROR                        ///< Some hardware problems with SD
}SD_Error_t;

/// SD card possible states
typedef enum
{
    SD_STATE_INACTIVE,          ///< Inactive state when card is not initialized or after error occured
    SD_STATE_IDLE,              ///< Idle state after SD card responses correctly on CMD0 command
    SD_STATE_READY,             ///< SD card state ready when it received 72 dummy clock and ready to initialize
    SD_STATE_STANDBY,           ///< SD card initialized and ready to transfer
    SD_STATE_SENDING,           ///< Sending data to SD card
    SD_STATE_RECEIVE,           ///< Receiving data from SD card
}SD_State_t;

/// SD card possible modes
typedef enum
{
    SD_MODE_INACTIVE,           ///< Inactive mode when card is not initialized or after error occured
    SD_MODE_IDENTIFICATION,     ///< Identification mode after received CMD0
    SD_MODE_TRANSFER            ///< Transfer mode sets when initialization ended successfully
}SD_Mode_t;

/// List of used commands
typedef enum
{
    SD_CMD_0 = 0,               ///< Go to idle mode
    SD_CMD_8 = 8,               ///< Checks working parameters
    SD_CMD_9 = 9,               ///< Read CSD
    SD_CMD_10 = 10,             ///< Read CID
    SD_CMD_12 = 12,             ///< Stop transfer
    SD_CMD_13 = 13,             ///< Get status
    SD_CMD_16 = 16,             ///< Set block size
    SD_CMD_17 = 17,             ///< Read block
    SD_CMD_18 = 18,             ///< Read multiple blocks
    SD_CMD_24 = 24,             ///< Write block
    SD_CMD_25 = 25,             ///< Write multiple blocks
    SD_CMD_55 = 55,             ///< Preceed ACMD
    SD_CMD_58 = 58              ///< Read OCR
}SD_Command_t;

///List of used acommands
typedef enum
{
    SD_ACMD_22 = 22,            ///< Get number of successfully written blocks
    SD_ACMD_41 = 41             ///< Checks initialization status
}SD_ACommand_t;

///List of SD card types
typedef enum
{
    SD_TYPE_SDSC,       ///< 128 MiB < capacity < 2GiB
    SD_TYPE_SDHC,       ///< 2 GiB < capacity < 32 GiB
    SD_TYPE_SDXC        ///< 32 GiB < capacity < 2 TiB
}SD_Type_t;

///List of R1 flags
typedef enum
{
    SD_R1_NORMAL_STATE     = 0x00,      ///< Normal R1 at transfer mode
    SD_R1_IDLE_STATE       = 0x01,      ///< R1 at identification mode
    SD_R1_ERASE_RESET      = 0x02,      ///< An erase sequence was cleared before executing
    SD_R1_ILLEGAL_CMD      = 0x04,      ///< Illegal command received
    SD_R1_CRC_ERROR        = 0x08,      ///< CRC7 command error
    SD_R1_ERASE_SEQ_ERROR  = 0x10,      ///< Error in erasing commands
    SD_R1_ADDRESS_ERROR    = 0x20,      ///< Misaligned address
    SD_R1_PARAMETER_ERROR  = 0x40,      ///< Command parameter not valid
    SD_R1_ALWAYS_ZERO      = 0x80,      ///< MSB always set to zero
    SD_R1_NOT_RESPONSE     = 0xFF
}SD_R1_t;

///List of R2 flags
typedef enum
{
    SD_R2_NORMAL_STATE     = 0x00,      ///< Normal R2 at transfer mode
    SD_R2_CARD_LOCKED      = 0x01,      ///< SD Card is locked
    SD_R2_WP_ERASE_SKIP    = 0x02,      ///< Cant erase write protected region or password to lock/unlock is wrong
    SD_R2_ERROR            = 0x04,      ///< General or unknown error during operation
    SD_R2_CC_ERROR         = 0x08,      ///< Internal card controller error
    SD_R2_ECC_FAILED       = 0x10,      ///< Internal ECC applied but failed to correct data
    SD_R2_WP_VIOLATION     = 0x20,      ///< Cant write to write protected block
    SD_R2_ERASE_PARAM      = 0x40,      ///< Invalid erase range
    SD_R2_OUT_OF_RANGE     = 0x80       ///< Invalid write range
}SD_R2_t;

///List of block tokens
typedef enum
{
    SD_START_RMW_BLOCK_TOKEN    = 0xFE, ///< Start of Read, Multiple Read or Write operation
    SD_START_WM_BLOCK_TOKEN     = 0xFC, ///< Start of Multiple write operation
    SD_STOP_WE_BLOCK_TOKEN      = 0xFD, ///< Stop of erasing or multiple write operations
}SD_Block_Token_t;

///List of receive data tokens
typedef enum
{
    SD_ERROR_TOKEN              = 0x01, ///< General or unknown error during operation
    SD_CC_ERROR_TOKEN           = 0x02, ///< Internal card controller error
    SD_ECC_ERROR_TOKEN          = 0x04, ///< Internal ECC applied but failed to correct data
    SD_RANGE_ERROR_TOKEN        = 0x08  ///< Invalid read range
}SD_DataReceive_Token_t;

///List of written tokens
typedef enum
{
    SD_DATA_ACCEPTED    = 0x05, ///< Data has been written correctly
    SD_DATA_CRC_ERROR   = 0x0B, ///< Wrong CRC16
    SD_DATA_WRITE_ERROR = 0x0D  ///< Internal write error
}SD_DataWritten_Token_t;

/*OCR structure*/
typedef struct __attribute__((packed))
{
    uint8_t reserved : 7;
    uint8_t LVR : 1;
    uint8_t reserved1 : 7;
    uint8_t v27_28 : 1;
    uint8_t v28_29 : 1;
    uint8_t v29_30 : 1;
    uint8_t v30_31 : 1;
    uint8_t v31_32 : 1;
    uint8_t v32_33 : 1;
    uint8_t v33_34 : 1;
    uint8_t v34_35 : 1;
    uint8_t v35_36 : 1;
    uint8_t S18A : 1;
    uint8_t reserved2 : 4;
    uint8_t UHSII_Status : 1;
    uint8_t CCS : 1;
    uint8_t busy;
}SD_OCR_t;

/*CID structure*/
typedef struct __attribute__((packed))
{
    uint8_t reserved : 1;
    uint8_t crc : 7;
    uint16_t MDT : 12;
    uint8_t reserved1 : 4;
    uint32_t PSN;
    uint8_t PRV;
    uint8_t PNM[5];
    uint16_t OID;
    uint8_t MID;
}SD_CID_t;

/*CSD structure version 1.0*/
typedef struct __attribute__((packed))
{
    uint16_t reserved : 1;
    uint16_t crc : 7;
    uint16_t reserved1 : 2;
    uint16_t fileFormat : 2;
    uint16_t tmpWriteProtect : 1;
    uint16_t permWriteProtect : 1;
    uint16_t copyFlag   : 1;
    uint16_t fileFormatGrp : 1;
    uint16_t reserved2 : 5;
    uint16_t writeBlockPartial : 1;
    uint16_t writeBlockLen : 4;
    uint16_t r2wFactor : 3;
    uint16_t reserved3 : 2;
    uint16_t wpGrpEnable : 1;
    uint16_t wpGrpSize : 7;
    uint16_t sectorSize : 7;
    uint16_t eraseBlockEnable : 1;
    uint16_t sizeMultiplier : 3;
    uint16_t vddWriteCurrentMax : 3;
    uint16_t vddWriteCurrentMin : 3;
    uint16_t vddReadCurrentMax : 3;
    uint16_t vddReadCurrentMin : 3;
    uint16_t cSize : 12;
    uint16_t reserved4 : 2;
    uint16_t DSRImp : 1;
    uint16_t readBlockMisalignment : 1;
    uint16_t writeBlockMisalignment : 1;
    uint16_t readBlockPartial : 1;
    uint16_t readBlockLen : 4;
    uint16_t CCC : 12;
    uint16_t transferSpeed;
    uint16_t NSAC;
    uint16_t TAAC;
    uint16_t reserved5 : 6;
    uint16_t CSDStructure : 2;
}SD_CSDv1_t;

/*CSD structure version 2.0*/
typedef struct __attribute__((packed))
{
    uint16_t reserved : 1;
    uint16_t crc : 7;
    uint16_t reserved1 : 2;
    uint16_t fileFormat : 2;
    uint16_t tmpWriteProtect : 1;
    uint16_t permWriteProtect : 1;
    uint16_t copyFlag   : 1;
    uint16_t fileFormatGrp : 1;
    uint16_t reserved2 : 5;
    uint16_t writeBlockPartial : 1;
    uint16_t writeBlockLen : 4;
    uint16_t r2wFactor : 3;
    uint16_t reserved3 : 2;
    uint16_t wpGrpEnable : 1;
    uint16_t wpGrpSize : 7;
    uint16_t sectorSize : 7;
    uint16_t eraseBlockEnable : 1;
    uint16_t reserved4 : 1;
    uint32_t cSize : 22;
    uint16_t reserved5 : 6;
    uint16_t DSRImp : 1;
    uint16_t readBlockMisalignment : 1;
    uint16_t writeBlockMisalignment : 1;
    uint16_t readBlockPartial : 1;
    uint16_t readBlockLen : 4;
    uint16_t CCC : 12;
    uint16_t transferSpeed;
    uint16_t NSAC;
    uint16_t TAAC;
    uint16_t reserved6 : 6;
    uint16_t CSDStructure : 2;
}SD_CSDv2_t;

/*SD parameters
    Used in all API functions*/
typedef struct
{
    /*SPIx module: SPI1, SPI2 or SPI3*/
    SPI_TypeDef * SPIx;
    /*SPIx bus frequency*/
    uint32_t SPIx_Clk;
    /*CS port: GPIOA..GPIOF*/
    GPIO_TypeDef * CS_Port;
    /*CS pin: 0..15*/
    uint8_t CS_Pin;

    /*Current SD card state*/
    SD_State_t state;
    /*Current SD card mode*/
    SD_Mode_t mode;
    /*SD card version*/
    uint8_t version;
    /*SD card capacity type*/
    SD_Type_t type;
    /*Response on last command*/
    SD_R1_t lastR1;
    /*Last status of SD card*/
    SD_R2_t lastR2;
    /*Last successfully written blocks*/
    uint32_t writtenBlocks;
    /*raw OCR register*/
    uint8_t rawOCR[4];
    /*raw CID register*/
    uint8_t rawCID[16];
    /*raw CSD register*/
    uint8_t rawCSD[16];
    /*block size in bytes*/
    uint16_t blockSize;
    /*capacity in bytes*/
    uint64_t capacity;
}SD_Parameters_t;

/*Initialize SD card*/
SD_Error_t SD_Init(SD_Parameters_t * params);

/*Reads status of SD card in lastR2 member of sd struct*/
SD_Error_t SD_ReadStatus(SD_Parameters_t * sd);

/*Get number of successfully written blocks in a writtenBlocks member of sd struct*/
SD_Error_t SD_GetWrittenBlocks(SD_Parameters_t * sd);

/*Transfer functions*/
SD_Error_t SD_ReadBlock(SD_Parameters_t * sd, uint32_t address, uint8_t * buffer);
SD_Error_t SD_ReadMultipleBlock(SD_Parameters_t * sd, uint32_t address, uint8_t * data, uint32_t num);
SD_Error_t SD_WriteBlock(SD_Parameters_t * sd, uint32_t address, uint8_t * data);
SD_Error_t SD_WriteMultipleBlock(SD_Parameters_t * sd, uint32_t address, uint8_t * data, uint32_t num);

/*Functions to make user-friendly structs from the raw OCR, CID, CSD registers*/
SD_OCR_t SD_GetOCR(SD_Parameters_t * sd);
SD_CID_t SD_GetCID(SD_Parameters_t * sd);
SD_CSDv1_t SD_GetCSD_v1(SD_Parameters_t * sd);
SD_CSDv2_t SD_GetCSD_v2(SD_Parameters_t * sd);


#endif /* SDCARD_H_INCLUDED */
