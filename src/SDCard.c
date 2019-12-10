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

#include "SDCard.h"
#include "SDCard_SPI.h"
#include "Utils.h"

/*SD timeout. 1000 ms*/
#define SD_TIMEOUT          1000

static uint8_t SD_CRC7_Table[256];

/*Functions for generating CRC7 for SD commands*/
static void SD_CRC7_GenTable(void);
static uint8_t SD_CRC7_GetCRC(uint8_t * buffer, uint32_t len);

/*Send CMD to SD card and get expected response*/
static SD_Error_t SD_SendCMD(SD_Parameters_t * sd, SD_Command_t cmd, uint32_t argument, SD_R1_t correctResponse);
/*Send ACMD to SD card and get expected response*/
static SD_Error_t SD_SendACMD(SD_Parameters_t * sd, SD_ACommand_t acmd, uint32_t argument, SD_R1_t correctResponse);
/*Get R1 response  from SD and compares it with expected response*/
static SD_Error_t SD_GetR1(SD_Parameters_t * sd,  SD_R1_t correctResponse);
/*Get other responses after R1 with different length*/
static SD_Error_t SD_GetResponse(SD_Parameters_t * sd, uint8_t * response, uint8_t len);
/*Send token to SD card*/
static SD_Error_t SD_SendToken(SD_Parameters_t * sd, SD_Block_Token_t token);
/*Get token from SD card*/
static SD_Error_t SD_GetToken(SD_Parameters_t * sd, SD_Block_Token_t token);
/*Read Data from SD. Always checks CRC16. Len is always even, because data blocks is SD card are always even*/
static SD_Error_t SD_ReadData(SD_Parameters_t * sd, uint8_t * data, uint32_t len);
/*Read Data from SD. Always send CRC16 after data. Len is always even, because data blocks is SD card are always even*/
static SD_Error_t SD_WriteData(SD_Parameters_t * sd, uint8_t * data, uint32_t len);
/*Wait while MISO line is pulled to zero*/
static SD_Error_t SD_WaitForBusy(SD_Parameters_t * sd);
/*Send dummy 8 clocks on SCK line*/
static SD_Error_t SD_SendDummyByte(SD_Parameters_t * sd, uint32_t num);
/*Stop transfer when reading multiple blocks is done or when write error occurred in multiple write mode*/
static SD_Error_t SD_StopTransfer(SD_Parameters_t * sd);
/*Put SD card in IDLE mode*/
static SD_Error_t SD_GoToIdleMode(SD_Parameters_t * sd);
/*Put SD card from IDLE to transfer mode*/
static SD_Error_t SD_GoToTransferMode(SD_Parameters_t * sd);
/*Checks is card apply the hist voltage source*/
static SD_Error_t SD_CheckVoltage(SD_Parameters_t * sd);
/*read OCR register and checks is the SD card is type of SDSC*/
static SD_Error_t SD_ReadOCR(SD_Parameters_t * sd);
/*Read CID register*/
static SD_Error_t SD_ReadCID(SD_Parameters_t * sd);
/*Read CSD register*/
static SD_Error_t SD_ReadCSD(SD_Parameters_t * sd);
/*Set block length for SDSC cards*/
static SD_Error_t SD_SetBlockLength(SD_Parameters_t *sd, uint16_t blockLen);
/*Handle error state of SD card, pulls CS to VDD and set state to inactive*/
static void SD_ErrorHandler(SD_Parameters_t * sd);

/** \brief Generate CRC7 lookup table
  * \param  None
  * \retval None
*/
static void SD_CRC7_GenTable(void)
{
    uint8_t polynom = 0x89;

    for(uint32_t i = 0; i < 256; ++i)
    {
        SD_CRC7_Table[i] = (i & 0x80) ? i ^ polynom : i;
        for(uint32_t j = 1; j < 8; ++j)
        {
            SD_CRC7_Table[i] <<= 1;
            if(SD_CRC7_Table[i] & 0x80)
                SD_CRC7_Table[i] ^= polynom;
        }
    }
}

/** \brief Calculate CRC7 for data buffer
  * \param  data: pointer to data buffer.
  * \param  len: number of bytes in buffer
  * \retval CRC7 value
*/
static uint8_t SD_CRC7_GetCRC(uint8_t * buffer, uint32_t len)
{
    uint8_t crc = 0;
    for(uint32_t i = 0; i < len; ++i)
        crc = SD_CRC7_Table[(crc << 1) ^ buffer[i]];
    return crc;
}


/** \brief Send CMD to SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  cmd: command from SD_Command_t enum
  * \param  argument: 4-byte field of command argument
  * \param  correctResponse: expected R1 from SD_R1_t enum
  * \retval SD error number
*/
static SD_Error_t SD_SendCMD(SD_Parameters_t * sd, SD_Command_t cmd, uint32_t argument, SD_R1_t correctResponse)
{
    uint8_t frame[6];
    /*Set 6th bit to 1*/
    frame[0] = ((uint8_t)cmd & 0x7F) | 0x40;
    frame[1] = (uint8_t)(argument >> 24);
    frame[2] = (uint8_t)(argument >> 16);
    frame[3] = (uint8_t)(argument >> 8);
    frame[4] = (uint8_t)(argument);
    /*left shift crc7 and setting to 1 lesser bit*/
    frame[5] = ((SD_CRC7_GetCRC(frame, 5)) << 1) | 0x01;
    /*If SPI has an error return SD_ERROR*/
    if(SD_SPI_Send8Data(sd->SPIx, frame, 6) != SD_SPI_OK)
        return SD_ERROR;
    /*Check R1 response*/
    return SD_GetR1(sd, correctResponse);
}

/** \brief Send ACMD to SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  acmd: command from SD_Command_t enum
  * \param  argument: 4-byte field of acommand argument
  * \param  correctResponse: expected R1 from SD_R1_t enum
  * \retval SD error number
*/
static SD_Error_t SD_SendACMD(SD_Parameters_t * sd, SD_ACommand_t acmd, uint32_t argument, SD_R1_t correctResponse)
{
    SD_R1_t response = SD_R1_IDLE_STATE;
    if(sd->mode == SD_MODE_TRANSFER)
        response = SD_R1_NORMAL_STATE;
    /*Send CMD55 before ACMD*/
    if(SD_SendCMD(sd, SD_CMD_55, 0, response) == SD_ERROR)
        return SD_ERROR;
    return SD_SendCMD(sd, (SD_Command_t)acmd, argument, correctResponse);
}

/** \brief Receive R1 response from SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  acmd: command from SD_Command_t enum
  * \param  argument: 4-byte field of acommand argument
  * \param  correctResponse: expected R1 from SD_R1_t enum
  * \retval SD error number
*/
static SD_Error_t SD_GetR1(SD_Parameters_t * sd,  SD_R1_t correctResponse)
{
    uint8_t response = 0xFF;
    /*try to get R1 for 10 times*/
    uint8_t repeats = 10;
    /*R1 response always has zero at 7 bit*/
    while((response & SD_R1_ALWAYS_ZERO) && repeats)
    {
        repeats--;
        if(SD_SPI_Receive8Data(sd->SPIx, &response, 1) == SD_SPI_ERROR)
            return SD_ERROR;
    }
    /*if we have no response - return SD_ERROR*/
    if(repeats == 0)
        return SD_ERROR;
    /*Save last R1 response and check with expected response*/
    sd->lastR1 = response;
    if(response != correctResponse)
        return SD_INCORRECT_RESPONSE;
    return SD_OK;
}

/** \brief Receive other responses from SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  response: pointer to buffer for response data
  * \param  len: length of expected response
  * \retval SD error number
*/
static SD_Error_t SD_GetResponse(SD_Parameters_t * sd, uint8_t * response, uint8_t len)
{
    if(SD_SPI_Receive8Data(sd->SPIx, response, len) == SD_SPI_ERROR)
        return SD_ERROR;
    return SD_OK;
}

/** \brief Send token to SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  token: token from SD_Block_Token_t enum
  * \retval SD error number
*/
static SD_Error_t SD_SendToken(SD_Parameters_t * sd, SD_Block_Token_t token)
{
    if(SD_SPI_Send8Data(sd->SPIx, &token, 1) == SD_SPI_ERROR)
        return SD_ERROR;
    return SD_OK;
}

/** \brief Receive token from SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  token: token from SD_Block_Token_t enum
  * \retval SD error number
*/
static SD_Error_t SD_GetToken(SD_Parameters_t * sd, SD_Block_Token_t token)
{
    /*Try to receive for 100 times*/
    uint8_t repeats = 100;
    SD_Block_Token_t response = 0xFF;
    while((response & (~token)) && repeats)
    {
        repeats--;
        if(SD_SPI_Receive8Data(sd->SPIx, &response, 1) == SD_SPI_ERROR)
            return SD_ERROR;
    }
    if(repeats == 0)
        return SD_ERROR;
    return SD_OK;
}

/** \brief Read data block from SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  data: pointer to buffer for response data
  * \param  len: length of expected response
  * \retval SD error number
*/
static SD_Error_t SD_ReadData(SD_Parameters_t * sd, uint8_t * data, uint32_t len)
{
    /*to faster transfer, read data in 16-bit mode. CRC16 is ENABLED*/
    if(SD_SPI_Receive16Data(sd->SPIx, (uint16_t*)data, len >> 1, ENABLE) == SD_SPI_ERROR)
        return SD_ERROR;
    return SD_OK;
}

/** \brief Write data block from SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  data: pointer to buffer for response data
  * \param  len: length of expected response
  * \retval SD error number
*/
static SD_Error_t SD_WriteData(SD_Parameters_t * sd, uint8_t * data, uint32_t len)
{
    uint8_t token = 0xFF;
    /*to faster transfer, write data in 16-bit mode. CRC16 is ENABLED*/
    if(SD_SPI_Send16Data(sd->SPIx, (uint16_t*)data, len >> 1, ENABLE) == SD_SPI_ERROR)
        return SD_ERROR;
    uint32_t timestamp = DWT_GetCycle();
    /*Wait for data tocken after writing block of data*/
    while((token != SD_DATA_ACCEPTED) && (token != SD_DATA_CRC_ERROR) && (token != SD_DATA_WRITE_ERROR))
    {
        if(SD_SPI_Receive8Data(sd->SPIx, &token, 1) == SD_SPI_ERROR)
            return SD_ERROR;
        if(DWT_Timeout(SD_TIMEOUT, timestamp))
            return SD_ERROR;
        token &= 0x1F;
    }
    /*Check if token is valid*/
    if(token == SD_DATA_CRC_ERROR)
        return SD_CRC_ERROR;
    else if(token == SD_DATA_WRITE_ERROR)
        return SD_WRITE_ERROR;
    if(SD_WaitForBusy(sd) != SD_OK)
        return SD_ERROR;
    return SD_OK;
}

/** \brief Wait while SD card is busy
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static SD_Error_t SD_WaitForBusy(SD_Parameters_t * sd)
{
    /*At busy state MISO is pulled to zero and all responses are zeros*/
    uint8_t busy = 0;
    /*If card is busy more than 1000 ms - return SD_ERROR*/
    uint32_t timestamp = DWT_GetCycle();
    while(busy != 0xFF)
    {
        if(SD_SPI_Receive8Data(sd->SPIx, &busy, 1) == SD_SPI_ERROR)
            return SD_ERROR;
        if(DWT_Timeout(SD_TIMEOUT, timestamp))
            return SD_ERROR;
    }
    return SD_OK;
}

/** \brief Send dummy clocks to SCK line
  * \param  sd: pointer to SD card parameters structure
  * \param  num: number of 8 bits packets to send
  * \retval SD error number
*/
static SD_Error_t SD_SendDummyByte(SD_Parameters_t * sd, uint32_t num)
{
    uint8_t dummy = 0xFF;
    while(num--)
    {
        if(SD_SPI_Receive8Data(sd->SPIx, &dummy, 1) == SD_SPI_ERROR)
            return SD_ERROR;
    }
    return SD_OK;
}

/** \brief Send stop transfer command to SD card
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static SD_Error_t SD_StopTransfer(SD_Parameters_t * sd)
{
    uint8_t token = 0;
    /*Send CMD12 command*/
    SD_SendCMD(sd, SD_CMD_12, 0, SD_R1_NORMAL_STATE);
    uint32_t timestamp = DWT_GetCycle();
    while(token != 0xFF)
    {
        if(SD_SPI_Receive8Data(sd->SPIx, &token, 1) == SD_SPI_ERROR)
            return SD_ERROR;
        if(DWT_Timeout(SD_TIMEOUT, timestamp))
            return SD_ERROR;
    }
    return SD_OK;
}

/** \brief Put SD_Card in identification mode
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static SD_Error_t SD_GoToIdleMode(SD_Parameters_t * sd)
{
    uint8_t response = 0;
    /*Send CMD0 command*/
    if(SD_SendCMD(sd, SD_CMD_0, 0, SD_R1_IDLE_STATE) != SD_OK)
        return SD_ERROR;
    sd->mode = SD_MODE_IDENTIFICATION;
    sd->state = SD_STATE_IDLE;
    return SD_OK;
}

/** \brief Put SD_Card in transfer mode
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static SD_Error_t SD_GoToTransferMode(SD_Parameters_t * sd)
{
    uint32_t timestamp = 0;
    uint32_t argument = 0;
    /*if SD spec version id 2 or higher, set HCS bit in first ACMD41*/
    if(sd->version == 2)
        argument = 1 << 30;
    if(SD_SendACMD(sd, SD_ACMD_41, argument, SD_R1_IDLE_STATE) != SD_OK)
        return SD_ERROR;
    timestamp = DWT_GetCycle();
    /*wait for R1 == 0x00*/
    while(sd->lastR1 != SD_R1_NORMAL_STATE)
    {
        /*If we wait more than 1000 ms - SD_ERROR*/
        if(DWT_Timeout(SD_TIMEOUT, timestamp))
            return SD_ERROR;
        if(SD_SendACMD(sd, SD_ACMD_41, 0,  SD_R1_NORMAL_STATE) == SD_ERROR)
            return SD_ERROR;
    }
    sd->mode = SD_MODE_TRANSFER;
    sd->state = SD_STATE_STANDBY;
    return SD_OK;
}

/** \brief Check if SD card works with host voltage supply
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static SD_Error_t SD_CheckVoltage(SD_Parameters_t * sd)
{
    uint8_t response[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    /*Send request with desired voltage bit and check pattern 0xAA*/
    if(SD_SendCMD(sd, SD_CMD_8, 0x01AA, SD_R1_IDLE_STATE) == SD_ERROR)
        return SD_ERROR;
    /*if cards doesn*t know this command -> card spec version is lesser than 2*/
    if(sd->lastR1 & SD_R1_ILLEGAL_CMD)
    {
        sd->version = 1;
        return SD_OK;
    }
    if(SD_GetResponse(sd, response, 4) == SD_ERROR)
        return SD_ERROR;
    /*if SD card responses correctly - check pattern and voltage bit*/
    if(!((response[2] == 0x01) && (response[3] == 0xAA)))
        return SD_ERROR;
    sd->version = 2;
    return SD_OK;
}

/** \brief Read OCR register
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static SD_Error_t SD_ReadOCR(SD_Parameters_t * sd)
{
    /*Send command to read OCR*/
    if(SD_SendCMD(sd, SD_CMD_58, 0, SD_R1_NORMAL_STATE) != SD_OK)
        return SD_ERROR;
    if(SD_GetResponse(sd, sd->rawOCR, 4) != SD_OK)
        return SD_ERROR;
    /*read OCR and check CCS bit, if it is set, card is HC or XC*/
    if(sd->rawOCR[0] & 0x40)
        sd->type = SD_TYPE_SDHC;
    return SD_OK;
}

/** \brief Read CID register
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static SD_Error_t SD_ReadCID(SD_Parameters_t * sd)
{
    if(SD_SendCMD(sd, SD_CMD_10, 0, SD_R1_NORMAL_STATE) != SD_OK)
        return SD_ERROR;
    if(SD_GetToken(sd, SD_START_RMW_BLOCK_TOKEN) == SD_ERROR)
        return SD_ERROR;
    return SD_ReadData(sd, sd->rawCID, 16);
}

/** \brief Read CSD register
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static SD_Error_t SD_ReadCSD(SD_Parameters_t * sd)
{
    /*Send command to get CSD*/
    if(SD_SendCMD(sd, SD_CMD_9, 0, SD_R1_NORMAL_STATE) != SD_OK)
        return SD_ERROR;
    if(SD_GetToken(sd, SD_START_RMW_BLOCK_TOKEN) == SD_ERROR)
        return SD_ERROR;
    if(SD_ReadData(sd, sd->rawCSD, 16) == SD_ERROR)
        return SD_ERROR;
    /*if SD speck version is 2 or higher - it is HC or XC cards*/
    if(sd->version == 2)
    {
        /*HC and XC cards blocksize is fixed to 512 bytes*/
        sd->blockSize = 512;
        /*Calculate capacity in bytes*/
        sd->capacity = (uint64_t)(((((sd->rawCSD[7] & 0x3f) << 16) | (sd->rawCSD[8] << 8) | sd->rawCSD[9]) + 1) << 9) << 10;
        /*if it is more than 32 GiB - SD card is XC*/
        if(sd->capacity > (uint64_t)34360000000)
            sd->type = SD_TYPE_SDXC;
    }
    else
    {
        /*set blocksize in SC card to 512 bytes*/
        sd->blockSize = 512;
        if(SD_SetBlockLength(sd, sd->blockSize) != SD_OK)
            return SD_ERROR;
        uint8_t mult = 1 << ((((sd->rawCSD[9] & 0x3) << 1) | ((sd->rawCSD[10] >> 7) & 0x1)) + 2);
        /*calculate SD capacity*/
        sd->capacity = (((((sd->rawCSD[6] & 0x3) << 10) | (sd->rawCSD[7] << 2) | ((sd->rawCSD[8] >> 6) & 0x3)) + 1) * mult)  << 9;
    }
    return SD_OK;
}

/** \brief Set SDSC card block length
  * \param  sd: pointer to SD card parameters structure
  * \param  blockLen: number of bytes in block (should be even)
  * \retval SD error number
*/
static SD_Error_t SD_SetBlockLength(SD_Parameters_t *sd, uint16_t blockLen)
{
    return SD_SendCMD(sd, SD_CMD_16, blockLen, SD_R1_NORMAL_STATE);
}

/** \brief Handle error of SD card
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
static void SD_ErrorHandler(SD_Parameters_t * sd)
{
    /*Set inactive mode and state*/
    sd->state = SD_STATE_INACTIVE;
    sd->mode = SD_MODE_INACTIVE;
    /*Pull CS to high*/
    SD_SPI_CS_Set(sd->CS_Port, sd->CS_Pin);
}

/** \brief Init SD card
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
SD_Error_t SD_Init(SD_Parameters_t * sd)
{
    /*Generate CRC7 table*/
    SD_CRC7_GenTable();
    /*Configure SPIx*/
    SD_SPI_Config(sd->SPIx, sd->CS_Pin, sd->CS_Port);
    /*Set SPI clocks < 400 kHz*/
    SD_SPI_SetSpeed(sd->SPIx, sd->SPIx_Clk, SD_SPI_INIT_SPEED);

    /*Semd 72 dummy clocks to SD*/
    if(SD_SendDummyByte(sd, 9) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }

    sd->type = SD_TYPE_SDSC;
    sd->state = SD_STATE_READY;                         //set start state
    sd->mode = SD_MODE_IDENTIFICATION;                  //set start mode
    /*Pull CS low*/
    SD_SPI_CS_Reset(sd->CS_Port, sd->CS_Pin);

    /*Put S in identification mode*/
    if(SD_GoToIdleMode(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Check for host voltage supply and spec version*/
    if(SD_CheckVoltage(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Put SD in transfer mode*/
    if(SD_GoToTransferMode(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Read OCR register*/
    if(sd->version == 2)
    {
        if(SD_ReadOCR(sd) != SD_OK)
        {
            SD_ErrorHandler(sd);
            return SD_ERROR;
        }
    }
    /*Now set SPI speed to transfer mode < 50 MHz*/
    SD_SPI_SetSpeed(sd->SPIx, sd->SPIx_Clk, SD_SPI_TRANSFER_SPEED);
    /*Read CID*/
    if(SD_ReadCID(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Read CSD*/
    if(SD_ReadCSD(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Pull CS high*/
    SD_SPI_CS_Set(sd->CS_Port, sd->CS_Pin);
    /*Card is ready for work*/
    return SD_OK;
}

/** \brief Set read SD status
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
SD_Error_t SD_ReadStatus(SD_Parameters_t * sd)
{
    SD_SPI_CS_Reset(sd->CS_Port, sd->CS_Pin);
    /*Send CMD13*/
    if(SD_SendCMD(sd, SD_CMD_13, 0, SD_R1_NORMAL_STATE) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Get response in lastR2 */
    if(SD_GetResponse(sd, &sd->lastR2, 1) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    SD_SPI_CS_Set(sd->CS_Port, sd->CS_Pin);
    return SD_OK;
}

/** \brief Get number of successfully written blocks
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
SD_Error_t SD_GetWrittenBlocks(SD_Parameters_t * sd)
{
    /*You can check number of written blocks after using multiple write function*/
    uint8_t temp[4];
    SD_SPI_CS_Reset(sd->CS_Port, sd->CS_Pin);
    if(SD_SendACMD(sd, SD_ACMD_22, 0, SD_R1_NORMAL_STATE) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    if(SD_GetToken(sd, SD_START_RMW_BLOCK_TOKEN) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    if(SD_ReadData(sd, temp, 4) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    sd->writtenBlocks = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
    SD_SPI_CS_Set(sd->CS_Port, sd->CS_Pin);
    return SD_OK;
}

/** \brief Read data block from SD
  * \param  sd: pointer to SD card parameters structure
  * \param  address: address of SD data block from where to read
  * \param  data: pointer to buffer where to put data
  * \retval SD error number
*/
SD_Error_t SD_ReadBlock(SD_Parameters_t * sd, uint32_t address, uint8_t * data)
{
    SD_SPI_CS_Reset(sd->CS_Port, sd->CS_Pin);
    sd->state = SD_STATE_RECEIVE;
    /*SDSC has absolute address*/
    if(sd->type == SD_TYPE_SDSC)
        address *= sd->blockSize;
    /*Send CMD17*/
    if(SD_SendCMD(sd, SD_CMD_17, address, SD_R1_NORMAL_STATE) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Try to get read data token*/
    if(SD_GetToken(sd, SD_START_RMW_BLOCK_TOKEN) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Read block with CRC16*/
    if(SD_ReadData(sd, data, sd->blockSize) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    sd->state == SD_STATE_STANDBY;
    SD_SPI_CS_Set(sd->CS_Port, sd->CS_Pin);
    return SD_OK;
}

/** \brief Read multiple data blocks from SD
  * \param  sd: pointer to SD card parameters structure
  * \param  address: address of SD data block from where to read
  * \param  data: pointer to buffer where to put data
  * \param  num: number of blocks to read
  * \retval SD error number
*/
SD_Error_t SD_ReadMultipleBlock(SD_Parameters_t * sd, uint32_t address, uint8_t * data, uint32_t num)
{
    SD_SPI_CS_Reset(sd->CS_Port, sd->CS_Pin);
    sd->state = SD_STATE_RECEIVE;
    /*SDSC has absolute address*/
    if(sd->type == SD_TYPE_SDSC)
        address *= sd->blockSize;
    /*Send CMD18*/
    if(SD_SendCMD(sd, SD_CMD_18, address, SD_R1_NORMAL_STATE) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    while(num--)
    {
        /*Try to get read data token*/
        if(SD_GetToken(sd, SD_START_RMW_BLOCK_TOKEN) != SD_OK)
        {
            SD_ErrorHandler(sd);
            return SD_ERROR;
        }
        /*Read data with CRC16*/
        if(SD_ReadData(sd, data, sd->blockSize) != SD_OK)
        {
            SD_ErrorHandler(sd);
            return SD_ERROR;
        }
        /*increment data pointer*/
        data += sd->blockSize;
    }
    /*Stop tranfer after last block*/
    if(SD_StopTransfer(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    sd->state = SD_STATE_STANDBY;
    SD_SPI_CS_Set(sd->CS_Port, sd->CS_Pin);
    return SD_OK;
}

/** \brief Write block of data to SD card
  * \param  sd: pointer to SD card parameters structure
  * \param  address: address of SD data block where to write
  * \param  data: pointer to buffer from where we get data
  * \retval SD error number
*/
SD_Error_t SD_WriteBlock(SD_Parameters_t * sd, uint32_t address, uint8_t * data)
{
    SD_SPI_CS_Reset(sd->CS_Port, sd->CS_Pin);
    /*SDSC has absolute address*/
    sd->state = SD_STATE_SENDING;
    if(sd->type == SD_TYPE_SDSC)
        address *= sd->blockSize;
    /*Send CMD24*/
    if(SD_SendCMD(sd, SD_CMD_24, address, SD_R1_NORMAL_STATE) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Send write data token*/
    if(SD_SendToken(sd, SD_START_RMW_BLOCK_TOKEN) == SD_SPI_ERROR)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Write data block with CRC16*/
    if(SD_WriteData(sd, data, sd->blockSize) != SD_OK)
    {
        SD_ReadStatus(sd);
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Wait while busy state*/
    if(SD_WaitForBusy(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Read status after writing block*/
    if(SD_ReadStatus(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    sd->state = SD_STATE_STANDBY;
    SD_SPI_CS_Set(sd->CS_Port, sd->CS_Pin);
    return SD_OK;
}


/** \brief Write multiple data blocks to SD
  * \param  sd: pointer to SD card parameters structure
  * \param  address: address of SD data block where to write
  * \param  data: pointer to buffer where we get data
  * \param  num: number of blocks to write
  * \retval SD error number
*/
SD_Error_t SD_WriteMultipleBlock(SD_Parameters_t * sd, uint32_t address, uint8_t * data, uint32_t num)
{
    SD_SPI_CS_Reset(sd->CS_Port, sd->CS_Pin);
    sd->state = SD_STATE_SENDING;
    /*SDSC has absolute address*/
    if(sd->type == SD_TYPE_SDSC)
        address *= sd->blockSize;
    /*Send CMD25*/
    if(SD_SendCMD(sd, SD_CMD_25, address, SD_R1_NORMAL_STATE) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Send dummy byte*/
    if(SD_SendDummyByte(sd, 1) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    while(num--)
    {
        SD_Error_t error = SD_OK;
        /*Send multiple write token*/
        if(SD_SendToken(sd, SD_START_WM_BLOCK_TOKEN) != SD_OK)
        {
            SD_ErrorHandler(sd);
            return SD_ERROR;
        }
        /*Write data with CRC16*/
        error = SD_WriteData(sd, data, sd->blockSize);
        if(error == SD_ERROR)
        {
            SD_ErrorHandler(sd);
            return SD_ERROR;
        }
        /*if write error or crc error occurred - get number of written blocks*/
        else if((error == SD_CRC_ERROR) || (error == SD_WRITE_ERROR))
        {
            SD_StopTransfer(sd);
            SD_GetWrittenBlocks(sd);
            SD_ErrorHandler(sd);
            return SD_ERROR;
        }
        data += sd->blockSize;
    }
    /*Send stop transmission token*/
    if(SD_SendToken(sd, SD_STOP_WE_BLOCK_TOKEN) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Send dummy byte*/
    if(SD_SendDummyByte(sd, 1) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    /*Weit while card is busy*/
    if(SD_WaitForBusy(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    sd->state = SD_STATE_STANDBY;
    SD_SPI_CS_Set(sd->CS_Port, sd->CS_Pin);
    /*Read status*/
    if(SD_ReadStatus(sd) != SD_OK)
    {
        SD_ErrorHandler(sd);
        return SD_ERROR;
    }
    return SD_OK;
}

/** \brief Parse raw OCR to OCR struct
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
SD_OCR_t SD_GetOCR(SD_Parameters_t * sd)
{
    SD_OCR_t OCR;
    OCR.reserved    = sd->rawOCR[3] & 0x7F;
    OCR.LVR         = sd->rawOCR[3] >> 7;
    OCR.reserved1   = sd->rawOCR[2] & 0x7F;
    OCR.v27_28      = sd->rawOCR[2] >> 7;
    OCR.v28_29      = sd->rawOCR[1] & 0x01;
    OCR.v29_30      = ((sd->rawOCR[1] >> 1) & 0x01);
    OCR.v30_31      = ((sd->rawOCR[1] >> 2) & 0x01);
    OCR.v31_32      = ((sd->rawOCR[1] >> 3) & 0x01);
    OCR.v32_33      = ((sd->rawOCR[1] >> 4) & 0x01);
    OCR.v33_34      = ((sd->rawOCR[1] >> 5) & 0x01);
    OCR.v34_35      = ((sd->rawOCR[1] >> 6) & 0x01);
    OCR.v35_36      = ((sd->rawOCR[1] >> 7) & 0x01);
    OCR.S18A        =  sd->rawOCR[0] & 0x01;
    OCR.reserved2   = (sd->rawOCR[0] >> 1) & 0x0F;
    OCR.UHSII_Status = ((sd->rawOCR[0] >> 5) & 0x01);
    OCR.CCS         = ((sd->rawOCR[0] >> 6) & 0x01);
    OCR.busy        = ((sd->rawOCR[0] >> 7) & 0x01);
    return OCR;
}

/** \brief Parse raw CID to CID struct
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
SD_CID_t SD_GetCID(SD_Parameters_t * sd)
{
    SD_CID_t CID;
    CID.reserved    = sd->rawCID[15] & 0x01;
    CID.crc         = sd->rawCID[15] >> 1;
    CID.MDT         = ((sd->rawCID[13] & 0x0F) << 8) | sd->rawCID[14];
    CID.reserved1   = 0;
    CID.PSN         = ((sd->rawCID[9] << 24) | (sd->rawCID[10] << 16) | (sd->rawCID[11] << 8) | (sd->rawCID[12]));
    CID.PRV         = sd->rawCID[8];
    CID.PNM[4]      = sd->rawCID[7];
    CID.PNM[3]      = sd->rawCID[6];
    CID.PNM[2]      = sd->rawCID[5];
    CID.PNM[1]      = sd->rawCID[4];
    CID.PNM[0]      = sd->rawCID[3];
    CID.OID         = (sd->rawCID[1] << 8) | sd->rawCID[2];
    CID.MID         = sd->rawCID[0];
    return CID;
}

/** \brief Parse raw CSD to CSD struct version 1
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
SD_CSDv1_t SD_GetCSD_v1(SD_Parameters_t * sd)
{
    SD_CSDv1_t CSD;
    CSD.reserved = 1;
    CSD.crc = sd->rawCSD[15] >> 1;
    CSD.reserved1 = 0;
    CSD.fileFormat = (sd->rawCSD[14] >> 2) & 0x3;
    CSD.tmpWriteProtect = (sd->rawCSD[14] >> 4) & 0x1;
    CSD.permWriteProtect = (sd->rawCSD[14] >> 5) & 0x1;
    CSD.copyFlag = (sd->rawCSD[14] >> 6) & 0x1;
    CSD.fileFormatGrp = (sd->rawCSD[14] >> 7) & 0x1;
    CSD.reserved2 = 0;
    CSD.writeBlockPartial = (sd->rawCSD[13] >> 5) & 0x1;
    CSD.writeBlockLen = ((sd->rawCSD[12] & 0x3) << 2) | ((sd->rawCSD[13] >> 6) & 0x3);
    CSD.r2wFactor = (sd->rawCSD[12] >> 2) & 0x7;
    CSD.reserved3 = 0;
    CSD.wpGrpEnable = (sd->rawCSD[12] >> 7) & 0x1;
    CSD.wpGrpSize = (sd->rawCSD[11] & 0x7F);
    CSD.sectorSize = ((sd->rawCSD[10] & 0x3F) << 1) | ((sd->rawCSD[11] >> 7) & 0x1);
    CSD.eraseBlockEnable = (sd->rawCSD[10] >> 6) & 0x1;
    CSD.sizeMultiplier = ((sd->rawCSD[9] & 0x3) << 1) | ((sd->rawCSD[10] >> 7) & 0x1);
    CSD.vddWriteCurrentMax = (sd->rawCSD[9] >> 2) & 0x7;
    CSD.vddWriteCurrentMin = (sd->rawCSD[9] >> 5) & 0x7;
    CSD.vddReadCurrentMax = (sd->rawCSD[8] & 0x7);
    CSD.vddReadCurrentMin = (sd->rawCSD[8] >> 3) & 0x7;
    CSD.cSize =  ((sd->rawCSD[6] & 0x3) << 10) | (sd->rawCSD[7] << 2) | ((sd->rawCSD[8] >> 6) & 0x3);
    CSD.reserved4 = 0;
    CSD.DSRImp = (sd->rawCSD[6] >> 4) & 0x1;
    CSD.readBlockMisalignment = (sd->rawCSD[6] >> 5) & 0x1;
    CSD.writeBlockMisalignment = (sd->rawCSD[6] >> 6) & 0x1;
    CSD.readBlockPartial = (sd->rawCSD[6] >> 7) & 0x1;
    CSD.readBlockLen = (sd->rawCSD[5] & 0x0F);
    CSD.CCC = (sd->rawCSD[4] << 8) | ((sd->rawCSD[5] >> 4) & 0xF);
    CSD.transferSpeed = sd->rawCSD[3];
    CSD.NSAC = sd->rawCSD[2];
    CSD.TAAC = sd->rawCSD[1];
    CSD.reserved5 = 0;
    CSD.CSDStructure = (sd->rawCSD[0] >> 6) & 0x3;
    return CSD;
}

/** \brief Parse raw CSD to CSD struct version 2
  * \param  sd: pointer to SD card parameters structure
  * \retval SD error number
*/
SD_CSDv2_t SD_GetCSD_v2(SD_Parameters_t * sd)
{
    SD_CSDv2_t CSD;
    CSD.reserved = 1;
    CSD.crc = sd->rawCSD[15] >> 1;
    CSD.reserved1 = 0;
    CSD.fileFormat = (sd->rawCSD[14] >> 2) & 0x3;
    CSD.tmpWriteProtect = (sd->rawCSD[14] >> 4) & 0x1;
    CSD.permWriteProtect = (sd->rawCSD[14] >> 5) & 0x1;
    CSD.copyFlag = (sd->rawCSD[14] >> 6) & 0x1;
    CSD.fileFormatGrp = (sd->rawCSD[14] >> 7) & 0x1;
    CSD.reserved2 = 0;
    CSD.writeBlockPartial = (sd->rawCSD[13] >> 5) & 0x1;
    CSD.writeBlockLen = ((sd->rawCSD[12] & 0x3) << 2) | ((sd->rawCSD[13] >> 6) & 0x3);
    CSD.r2wFactor = (sd->rawCSD[12] >> 2) & 0x7;
    CSD.reserved3 = 0;
    CSD.wpGrpEnable = (sd->rawCSD[12] >> 7) & 0x1;
    CSD.wpGrpSize = (sd->rawCSD[11] & 0x7F);
    CSD.sectorSize = ((sd->rawCSD[10] & 0x3F) << 1) | ((sd->rawCSD[11] >> 7) & 0x1);
    CSD.eraseBlockEnable = (sd->rawCSD[10] >> 6) & 0x1;
    CSD.reserved4 = 0;
    CSD.cSize = ((sd->rawCSD[7] & 0x3f) << 16) | (sd->rawCSD[8] << 8) | sd->rawCSD[9];
    CSD.reserved5 = 0;
    CSD.DSRImp = (sd->rawCSD[6] >> 4) & 0x1;
    CSD.readBlockMisalignment = (sd->rawCSD[6] >> 5) & 0x1;
    CSD.writeBlockMisalignment = (sd->rawCSD[6] >> 6) & 0x1;
    CSD.readBlockPartial = (sd->rawCSD[6] >> 7) & 0x1;
    CSD.readBlockLen = (sd->rawCSD[5] & 0x0F);
    CSD.CCC = (sd->rawCSD[4] << 8) | ((sd->rawCSD[5] >> 4) & 0xF);
    CSD.transferSpeed = sd->rawCSD[3];
    CSD.NSAC = sd->rawCSD[2];
    CSD.TAAC = sd->rawCSD[1];
    CSD.reserved6 = 0;
    CSD.CSDStructure = (sd->rawCSD[0] >> 6) & 0x3;
    return CSD;
}
