#ifndef DZ_CONFIG_H
#define DZ_CONFIG_H

#define FW_VERSION "1.0.6"


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

// PN532 HSU
#define PN532_HSU_BAUD (115200)
#define PN532_HSU_RX_PIN (20)
#define PN532_HSU_TX_PIN (21)

// DESFire
#define DESFIRE_DEFAULT_AID {0x00, 0x00, 0x00}
#define DESFIRE_DEFAULT_AID_LEN (3)
#define DESFIRE_DEFAULT_FILE_ID (0x00)
#define DESFIRE_DEFAULT_KEY_NUM (0x00)

#endif