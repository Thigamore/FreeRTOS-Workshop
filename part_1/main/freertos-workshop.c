#include "FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "portmacro.h"
#include "driver/gpio.h"
#include "soc/gpio_num.h"
#include "esp_sleep.h"
#include "esp_random.h"

#include <string.h>

// ------------ MACROS -----------

// LEDS
#define LED_NUM_ROWS (3)
#define LED_NUM_COLS (3)
#define NUM_LEDS (LED_NUM_ROWS * LED_NUM_COLS)

static const gpio_num_t LED_ROW_PINS[LED_NUM_ROWS] = {GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18};
static const gpio_num_t LED_COL_PINS[LED_NUM_COLS] = {GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14};

#define LED_ROW_MASK ((1ULL << LED_ROW_PINS[0]) | (1ULL << LED_ROW_PINS[1]) | (1ULL << LED_ROW_PINS[2]))
#define LED_COL_MASK ((1ULL << LED_COL_PINS[0]) | (1ULL << LED_COL_PINS[1]) | (1ULL << LED_COL_PINS[2]))

#define SHOW_ON_MS  (400) // can change this later
#define SHOW_OFF_MS (200) // can change this later

#define SHOW_CORRECT_ON_MS (60)
#define SHOW_CORRECT_OFF_MS (10)

#define SHOW_WRONG_MS (250)
#define WRONG_DELAY_MS (250)

#define SHOW_WIN_MS (100)

// Tasks
#define STACK_DEPTH (10000)
#define POLL_PRIO (1)

// Buttons
#define SCAN_PERIOD_MS  (20)

#define BUTTON_NUM_ROWS (3)
#define BUTTON_NUM_COLS (3)

static const gpio_num_t BUTTON_ROW_PINS[BUTTON_NUM_ROWS] = { GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6 };
static const gpio_num_t BUTTON_COL_PINS[BUTTON_NUM_COLS] = { GPIO_NUM_21, GPIO_NUM_47, GPIO_NUM_48 };
#define NUM_BUTTONS (BUTTON_NUM_COLS * BUTTON_NUM_ROWS)

#define BUTTON_ROW_MASK ((1ULL << BUTTON_ROW_PINS[0]) | (1ULL << BUTTON_ROW_PINS[1]) | (1ULL << BUTTON_ROW_PINS[2]))
#define BUTTON_COL_MASK  ((1ULL << BUTTON_COL_PINS[0]) | (1ULL << BUTTON_COL_PINS[1]) | (1ULL << BUTTON_COL_PINS[2]))

#define BOUNCE_ITERATIONS 5

// Misc
static const char* POLL_TAG = "Poll" ;
static const char* MAIN_TAG = "Main";

#define DEBUG (1)

#define MAX_LEVEL (10)

// ------------ GLOBALS ---------------

// ------------------ ADD GLOBALS HERE ----------


// -------------  LED Helper funcs -----------

void allLedsOff() {
    for (int r = 0; r < LED_NUM_ROWS; r++) 
        gpio_set_level(LED_ROW_PINS[r], 0);   // rows LOW
    for (int c = 0; c < LED_NUM_COLS; c++) 
        gpio_set_level(LED_COL_PINS[c], 1);  // cols HIGH
}

void ledOn(int index) {
    allLedsOff(); // clear
    if (index < 0 || index >= NUM_LEDS) 
        return;        // invalid input
    gpio_set_level(LED_COL_PINS[index % LED_NUM_COLS], 0);   // col LOW
    gpio_set_level(LED_ROW_PINS[index / LED_NUM_COLS], 1);  // row HIGH
}

void flashLed(int index, int onMs, int offMs) {
    ledOn(index);
    vTaskDelay(pdMS_TO_TICKS(onMs)); // block the main task. pdms converts ms to RTOS ticks (1 tick = 1 ms for ESP32)
    allLedsOff();
    vTaskDelay(pdMS_TO_TICKS(offMs));
}

// light any combo of LEDs for ms (bit i = LED i)
// this is for showWrong() and showLevel()
void showPattern(uint16_t mask, int ms) {
    TickType_t start = xTaskGetTickCount();
    int r = 0;
    while (xTaskGetTickCount() - start < pdMS_TO_TICKS(ms)) {
        allLedsOff();
        for (int c = 0; c < LED_NUM_COLS; c++) {
            if (mask & (1 << (r * LED_NUM_COLS + c))) 
                gpio_set_level(LED_COL_PINS[c], 0);
        }
        gpio_set_level(LED_ROW_PINS[r], 1);
        vTaskDelay(1);
        r = (r + 1) % LED_NUM_ROWS;
    }
    allLedsOff();
}

void allLedsOnFor(int ms) {
  showPattern((1 << NUM_LEDS) - 1, ms);   // all 9 bits set
}

void blink_all_leds(int times, int ms) {
    for (int i = 0; i < times; i++) {
        allLedsOnFor(ms);
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}

// ------------- BUTTON HELPER FUNCTION ------------
// Returns the first button pressed that isn't in was_pressed
int button_scan() {
    for(int row = 0; row < BUTTON_NUM_ROWS; row++) {
        gpio_set_level(BUTTON_ROW_PINS[row], 0);
        vTaskDelay(1);

        for(int col = 0; col < BUTTON_NUM_COLS; col++) {
            if(!gpio_get_level(BUTTON_COL_PINS[col])) {
                gpio_set_level(BUTTON_ROW_PINS[row], 1);
                return row * 3 + col;
            }
        }

        gpio_set_level(BUTTON_ROW_PINS[row], 1);
    }
    return -1;
}

// ----------- CONFIG -------------
void led_init() {
    gpio_config_t row_cfg = {
        .pin_bit_mask = LED_ROW_MASK,
        .mode         = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&row_cfg));

    gpio_config_t col_cfg = {
        .pin_bit_mask = LED_COL_MASK,
        .mode         = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&col_cfg));
    allLedsOff();
    ESP_LOGI(MAIN_TAG, "Initialized LEDs");
}

void gpio_init() {
    gpio_config_t col_cfg = {
        .pin_bit_mask = BUTTON_COL_MASK,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE
    };
    ESP_ERROR_CHECK(gpio_config(&col_cfg));

    gpio_config_t row_cfg = {
        .pin_bit_mask = BUTTON_ROW_MASK,
        .mode         = GPIO_MODE_OUTPUT
    };
    gpio_config(&row_cfg);

    for (int row = 0; row < BUTTON_NUM_ROWS; row++) {
        gpio_set_level(BUTTON_ROW_PINS[row], 1);
    }
    ESP_LOGI(MAIN_TAG, "Initialized GPIOs");
}

// ------------- GAME FUNCTIONS ------------

// show the pattern for this level (seq: main's sequence array, level: # of steps).
void showSequence(const int *seq, int level) {
    for (int i = 0; i < level; i++) 
        flashLed(seq[i], SHOW_ON_MS, SHOW_OFF_MS);
}

// player got it right: LEDs 0-9 briefly flash one at a time (60ms ON, 10ms OFF)
void showCorrect() {
    for (int i = 0; i < NUM_LEDS; i++) 
        flashLed(i, SHOW_CORRECT_ON_MS, SHOW_CORRECT_OFF_MS);
}

// player got it wrong (or ran out of time): all LEDs blink 3 times (250ms ON, 250ms OFF)
void showWrong() {
    blink_all_leds(3, SHOW_WRONG_MS);
}

// player beat MAX_LVL: LEDs flash clockwise around the edge 4 times, then middle lights up briefly, then all LEDs blink 3 times
void showWin() {
    const int ring[] = {0, 1, 2, 5, 8, 7, 6, 3}; // 8 outer LEDs in clockwise order
    for (int n = 0; n < 4; n++) {
        for (int i = 0; i < 8; i++) 
            flashLed(ring[i], 60, 0);
    }
    flashLed(4, 500, 100);   // finish on the middle
    blink_all_leds(3, SHOW_WIN_MS);
}

// quick flash when a button is pressed so the player sees it registered.
void showPress(int index) {
    flashLed(index, 150, 0);
}

// 6. "show lvl" from the outline. level goes up to 20 and there are 9 LEDs,
// so it's shown in binary on LEDs 0-4 (LED 0 = 1, LED 1 = 2, LED 2 = 4 ...)
// e.g. level 5 = 101 -> LEDs 0 and 2
void showLevel(uint16_t level) {
    showPattern(level, 1500);
    vTaskDelay(pdMS_TO_TICKS(300));
}

void generate_random_sequence(int sequence[MAX_LEVEL]) {
    for(int i = 0; i < MAX_LEVEL; i++) {
        sequence[i] = (esp_random() % 9);
    }
}

// ------------- POLL THREAD -----------

void poll_task(void* params) {
    
    ESP_LOGI(POLL_TAG, "Starting Poll Task");

    for (;;) {
        ESP_LOGI(POLL_TAG, "Poll");

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


// ------------- MAIN THREAD -----------

void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "Starting Main Task");
    
    // Initialize gpio/leds
    led_init();
    gpio_init();

    // Determine the sequence
    int sequence[MAX_LEVEL];
    generate_random_sequence(sequence);
    int level = 1;

    // Initialize the poll thread
    xTaskCreate(poll_task, "Poll Task", STACK_DEPTH, NULL, POLL_PRIO, NULL);

    while(true) {
        showSequence(sequence, level);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
