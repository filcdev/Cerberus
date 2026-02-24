#ifndef DZ_CONFIG_H
#define DZ_CONFIG_H

#define FW_VERSION "1.0.3"


// SD
#define SD_CS_PIN (5)
#define SD_MMC_CMD (38)
#define SD_MMC_CLK (39)
#define SD_MMC_D0 (40)

#define DOOR_PIN (45)
#define DOOR_OPEN_DURATION_MS (5000)
#define BUZZER_PIN (18) // TODO: Not used yet
#define BUTTON_PIN (10)

#define LED_PIN (48)
#define LED_COUNT (16) //8 inside and 8 outside

// I2C
#define PN532_ADDRESS (0x48)
#define I2C_SDA_PIN (21)
#define I2C_SCL_PIN (47)

#define LCD_ADDRESS (0x27)
#define LCD_COLUMNS (16)
#define LCD_ROWS (2)

// PN532 HSU
#define PN532_HSU_BAUD (115200)
#define PN532_HSU_RX_PIN (42)
#define PN532_HSU_TX_PIN (41)

#endif