#include "sd_spi.h"
// We will include your team's SPI driver header here later (e.g., #include "spi_bus.h")
#include "spi_bus.h" // Include the SPI bus driver header  
#include "main.h" // Include the main header for GPIO pin definitions

// ---------------------------------------------------------
// HELPER FUNCTIONS (Hidden from FatFs)
// ---------------------------------------------------------

/* 
 * WHY THIS EXISTS: The SD card ignores us unless we pull its specific CS pin to 0 Volts.
 * We do this manually because SD card commands require the pin to stay low for multiple SPI transfers.
 */
static void SD_Select(void) {
    HAL_GPIO_WritePin(microSD_CS_GPIO_Port, microSD_CS_Pin, GPIO_PIN_RESET); // 0V = Active
}

/* 
 * WHY THIS EXISTS: When we are done, we MUST pull CS back to 3.3 Volts.
 * If we don't, the SD card will stay awake and corrupt the bus when the IMU tries to talk.
 */
static void SD_Deselect(void) {
    HAL_GPIO_WritePin(microSD_CS_GPIO_Port, microSD_CS_Pin, GPIO_PIN_SET); // 3.3V = Inactive
    
    // SD Card Spec requires one extra "dummy clock" byte after deselecting
    uint8_t dummy = 0xFF; 
    spi_bus_transfer(NULL, 0, &dummy, NULL, 1, 10); 
}

/*
 * WHY THIS EXISTS: A wrapper to easily send/receive 1 byte to the SD card.
 * Notice we pass NULL for the port/pin so spi_bus_transfer doesn't auto-toggle the CS line
 */
static uint8_t SD_TxRxByte(uint8_t data) {
    uint8_t rx_data = 0xFF;
    spi_bus_transfer(NULL, 0, &data, &rx_data, 1, 100);
    return rx_data;
}

// ---------------------------------------------------------
// SD CARD PROTOCOL COMMANDS
// ---------------------------------------------------------

// SD Card Command Definitions
#define CMD0   (0)         // GO_IDLE_STATE
#define CMD8   (8)         // SEND_IF_COND
#define CMD17  (17)        // READ_SINGLE_BLOCK
#define CMD24  (24)        // WRITE_SINGLE_BLOCK
#define CMD55  (55)        // APP_CMD
#define ACMD41 (41 | 0x80) // SEND_OP_COND (Application specific)

/*
 * WHY THIS EXISTS: All SD commands are exactly 6 bytes long.
 * Byte 1: The command number (with bit 6 set to 1)
 * Bytes 2-5: The 32-bit argument (e.g., the sector address)
 * Byte 6: The CRC checksum (required for CMD0 and CMD8)
 */
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg) {
    uint8_t n, res;

    // If it's an Application Specific Command (ACMD), we must send CMD55 first
    if (cmd & 0x80) {
        cmd &= 0x7F; // Strip the ACMD flag
        res = SD_SendCmd(CMD55, 0);
        if (res > 1) return res;
    }

    // Select the card so it listens to us
    SD_Select();

    // 1. Send Command Byte (0x40 is the required prefix bit)
    SD_TxRxByte(cmd | 0x40);

    // 2. Send 32-bit Argument (MSB first)
    SD_TxRxByte((uint8_t)(arg >> 24));
    SD_TxRxByte((uint8_t)(arg >> 16));
    SD_TxRxByte((uint8_t)(arg >> 8));
    SD_TxRxByte((uint8_t)arg);

    // 3. Send CRC Checksum
    // CMD0 requires 0x95. CMD8 requires 0x87. For everything else in SPI mode, CRC is ignored.
    n = 0x01; 
    if (cmd == CMD0) n = 0x95;
    if (cmd == CMD8) n = 0x87;
    SD_TxRxByte(n);

    // 4. Wait for the SD card to process and respond (Timeout after 10 attempts)
    n = 10;
    do {
        res = SD_TxRxByte(0xFF);
    } while ((res & 0x80) && --n);

    return res; // Returns the SD card's response code
}

// ---------------------------------------------------------
// HARDWARE ABSTRACTION LAYER FUNCTIONS
// ---------------------------------------------------------

DSTATUS SD_SPI_Init(BYTE pdrv) {
    uint8_t n, cmd, ty, ocr[4];
    uint16_t timeout;

    // 1. Wake up sequence: The card requires at least 74 clock pulses with CS HIGH
    HAL_Delay(10); // Wait 10ms for power to stabilize
    SD_Deselect(); // Ensure CS is HIGH
    for (n = 10; n; n--) {
        SD_TxRxByte(0xFF); // 10 bytes * 8 bits = 80 clock pulses
    }

    // 2. Send CMD0 to force the card into SPI Mode
    if (SD_SendCmd(CMD0, 0) == 1) { // 1 means "Idle State" (Success)
        
        // 3. Send CMD8 to check if it's a modern SDHC/SDXC card (V2.0+)
        if (SD_SendCmd(CMD8, 0x1AA) == 1) {
            // Read the rest of the 4-byte response
            for (n = 0; n < 4; n++) ocr[n] = SD_TxRxByte(0xFF);
            
            // If it supports 2.7-3.6V, we can proceed to initialize
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                // Send ACMD41 continuously until the card says it is ready (returns 0)
                for (timeout = 1000; timeout; timeout--) {
                    if (SD_SendCmd(ACMD41, 1UL << 30) == 0) break;
                    HAL_Delay(1);
                }
                
                if (timeout) {
                    SD_Deselect();
                    return 0; // Return 0 (RES_OK) - Card is initialized!
                }
            }
        }
    }
    
    // If we reach here, initialization failed
    SD_Deselect();
    return STA_NOINIT; // FatFs error code for "Hardware not initialized"
}

DSTATUS SD_SPI_Status(BYTE pdrv) {
    /*
     * WHY THIS EXISTS: FatFs calls this to see if the card is physically present.
     * We check the mechanical insertion spring switch.
     * Empty = 3.3V (Pulled up), Inserted = 0V (Shorted to Ground by the spring)
     */
    if (HAL_GPIO_ReadPin(microSD_detect_GPIO_Port, microSD_detect_Pin) == GPIO_PIN_RESET) {
        return 0; // Return 0 (RES_OK) - Card is seated in the slot!
    } else {
        return STA_NODISK; // FatFs error code - "No Disk"
    }
}

DRESULT SD_SPI_ReadBlocks(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
    uint16_t timeout;

    // The SD card expects us to read 512 bytes at a time, regardless of FatFs settings
    for (UINT i = 0; i < count; i++) {
        
        // Send CMD17 (Read Single Block) for the specific sector address
        if (SD_SendCmd(CMD17, sector + i) != 0) {
            SD_Deselect();
            return RES_ERROR; // Command failed
        }

        // Wait for the SD Card to send the Data Token (0xFE) meaning "Data is ready"
        timeout = 10000;
        while (SD_TxRxByte(0xFF) != 0xFE) {
            if (--timeout == 0) {
                SD_Deselect();
                return RES_ERROR; // Card took too long to prepare data
            }
        }

        // We got the token! Now pull exactly 512 bytes of file data into our buffer
        for (uint16_t j = 0; j < 512; j++) {
            *buff++ = SD_TxRxByte(0xFF);
        }

        // The SD card always sends a 2-byte CRC checksum at the end. 
        // In basic SPI mode, we just read them and throw them away.
        SD_TxRxByte(0xFF);
        SD_TxRxByte(0xFF);
    }

    SD_Deselect(); // Release the bus for the IMU
    return RES_OK; // Success!
}

DRESULT SD_SPI_WriteBlocks(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
    uint16_t timeout;

    for (UINT i = 0; i < count; i++) {
        
        // Send CMD24 (Write Single Block) to the specific sector address
        if (SD_SendCmd(CMD24, sector + i) != 0) {
            SD_Deselect();
            return RES_ERROR; 
        }

        // Send exactly one dummy byte before the data token to give the card a moment
        SD_TxRxByte(0xFF);

        // Send the Data Token (0xFE) to announce "Here comes the file data!"
        SD_TxRxByte(0xFE);

        // Blast the 512 bytes of file data over the MOSI wire
        for (uint16_t j = 0; j < 512; j++) {
            SD_TxRxByte(*buff++);
        }

        // Send 2 dummy CRC bytes (required by protocol, even if we don't calculate them)
        SD_TxRxByte(0xFF);
        SD_TxRxByte(0xFF);

        // The SD card sends a 1-byte response token. 
        // If the lower 5 bits equal 0x05, the data was accepted perfectly.
        if ((SD_TxRxByte(0xFF) & 0x1F) != 0x05) {
            SD_Deselect();
            return RES_ERROR; // Data was rejected or corrupted
        }

        // CRITICAL SAFETY STEP: The card is now physically burning the data to flash memory.
        // It will hold the MISO line LOW (0x00) while busy. We must wait until it returns to HIGH (0xFF).
        timeout = 10000;
        while (SD_TxRxByte(0xFF) == 0x00) {
            if (--timeout == 0) {
                SD_Deselect();
                return RES_ERROR; // Timeout: Card got permanently stuck writing
            }
        }
    }

    SD_Deselect(); // Release the bus for the IMU
    return RES_OK; // Success
}

DRESULT SD_SPI_Ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    DRESULT res = RES_ERROR;
    uint8_t csd[16];
    uint32_t c_size;

    if (pdrv != 0) return RES_PARERR; // We only support drive 0

    switch (cmd) {
        case CTRL_SYNC:
            // FatFs asks if the card is done writing. We check if the MISO line is HIGH.
            SD_Select();
            if (SD_TxRxByte(0xFF) == 0xFF) res = RES_OK;
            SD_Deselect();
            break;

        case GET_SECTOR_COUNT:
            // Send CMD9 to read the 16-byte CSD register
            if (SD_SendCmd(9, 0) == 0) {
                uint16_t timeout = 10000;
                // Wait for the 0xFE Data Token
                while (SD_TxRxByte(0xFF) != 0xFE && --timeout);
                
                if (timeout) {
                    // Read the 16 bytes of CSD data
                    for (uint8_t i = 0; i < 16; i++) csd[i] = SD_TxRxByte(0xFF);
                    SD_TxRxByte(0xFF); // Throw away the 2 CRC bytes
                    SD_TxRxByte(0xFF);
                    
                    // Bit-shifting math to calculate capacity for modern SDHC/SDXC cards (>2GB)
                    // Extracts the 22-bit C_SIZE field from the CSD array
                    c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint16_t)csd[8] << 8) | csd[9];
                    *(DWORD*)buff = (c_size + 1) * 1024; // Convert to total sector count
                    res = RES_OK;
                }
            }
            SD_Deselect();
            break;

        case GET_SECTOR_SIZE:
            // Standard SD cards always read/write in 512-byte chunks at the hardware level
            *(WORD*)buff = 512; 
            res = RES_OK;
            break;

        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1; // Erase block size
            res = RES_OK;
            break;

        default:
            res = RES_PARERR; // Command not supported
            break;
    }

    return res;
}



