#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
  uart_dev_t uart;

  uint32_t pm1_attr;
  uint32_t pm25_attr;
  uint32_t pm10_attr;

  // Persistent UART transmit buffer.
  // Keep this alive while Wokwi transmits the data.
  uint8_t frame[32];

} chip_state_t;


// =====================================================
// Store a 16-bit number as two bytes
// =====================================================

static void write_u16(
  uint8_t *buffer,
  int index,
  uint16_t value
) {

  buffer[index] =
    (value >> 8) & 0xFF;

  buffer[index + 1] =
    value & 0xFF;
}


// =====================================================
// Build and send PMS5003 frame
// =====================================================

static void send_pms_frame(
  chip_state_t *chip
) {

  uint16_t pm1 =
    (uint16_t)attr_read(
      chip->pm1_attr
    );

  uint16_t pm25 =
    (uint16_t)attr_read(
      chip->pm25_attr
    );

  uint16_t pm10 =
    (uint16_t)attr_read(
      chip->pm10_attr
    );


  // Clear the persistent frame
  for (int i = 0; i < 32; i++) {
    chip->frame[i] = 0;
  }


  // ---------------------------------------------------
  // PMS5003 header
  // ---------------------------------------------------

  chip->frame[0] = 0x42;
  chip->frame[1] = 0x4D;


  // ---------------------------------------------------
  // Frame length = 28 bytes
  // ---------------------------------------------------

  chip->frame[2] = 0x00;
  chip->frame[3] = 0x1C;


  // ---------------------------------------------------
  // Standard particle values
  // ---------------------------------------------------

  write_u16(
    chip->frame,
    4,
    pm1
  );

  write_u16(
    chip->frame,
    6,
    pm25
  );

  write_u16(
    chip->frame,
    8,
    pm10
  );


  // ---------------------------------------------------
  // Atmospheric/environment values
  // ---------------------------------------------------

  write_u16(
    chip->frame,
    10,
    pm1
  );

  write_u16(
    chip->frame,
    12,
    pm25
  );

  write_u16(
    chip->frame,
    14,
    pm10
  );


  // ---------------------------------------------------
  // Calculate checksum
  // Bytes 0 through 29
  // ---------------------------------------------------

  uint16_t checksum = 0;

  for (int i = 0; i < 30; i++) {
    checksum += chip->frame[i];
  }


  chip->frame[30] =
    (checksum >> 8) & 0xFF;

  chip->frame[31] =
    checksum & 0xFF;


  // ---------------------------------------------------
  // Send UART frame
  // ---------------------------------------------------

  bool transmitted =
    uart_write(
      chip->uart,
      chip->frame,
      32
    );


  if (transmitted) {

    printf(
      "PMS5003 UART TX OK: "
      "PM1=%u PM2.5=%u PM10=%u\n",
      pm1,
      pm25,
      pm10
    );

  } else {

    printf(
      "PMS5003 UART TX BUSY\n"
    );
  }
}


// =====================================================
// Timer callback
// =====================================================

static void on_timer(
  void *user_data
) {

  chip_state_t *chip =
    (chip_state_t *)user_data;

  send_pms_frame(chip);
}


// =====================================================
// Initialize simulated PMS5003
// =====================================================

void chip_init() {

  chip_state_t *chip =
    (chip_state_t *)malloc(
      sizeof(chip_state_t)
    );


  // ---------------------------------------------------
  // PM attributes
  // ---------------------------------------------------

  chip->pm1_attr =
    attr_init(
      "pm1",
      10
    );

  chip->pm25_attr =
    attr_init(
      "pm25",
      25
    );

  chip->pm10_attr =
    attr_init(
      "pm10",
      40
    );


  // ---------------------------------------------------
  // UART
  // ---------------------------------------------------

  const uart_config_t uart_config = {

    .tx =
      pin_init(
        "TX",
        INPUT_PULLUP
      ),

    .rx =
      pin_init(
        "RX",
        INPUT
      ),

    .baud_rate = 9600,

    .rx_data = NULL,

    .write_done = NULL,

    .user_data = chip
  };


  chip->uart =
    uart_init(
      &uart_config
    );


  // ---------------------------------------------------
  // Repeating timer
  // ---------------------------------------------------

  timer_config_t timer_config = {

    .callback =
      on_timer,

    .user_data =
      chip
  };


  timer_t timer =
    timer_init(
      &timer_config
    );


  // 1,000,000 microseconds = 1 second
  timer_start(
    timer,
    1000000,
    true
  );


  printf(
    "PMS5003 simulator initialized\n"
  );
}