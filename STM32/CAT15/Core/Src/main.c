/* main.c - STM32F401RB + BME280 + ESP8266 (Wemos) sequential workflow
 *
 * Execution order:
 *   STAGE 1 : Initialise and verify the BME280 over I2C1, take one reading.
 *   STAGE 2 : Interactive AT-command bridge (PuTTY -> STM32 -> Wemos -> PuTTY).
 *             Type AT commands to join Wi-Fi and start the TCP server.
 *             Type START (or DONE) to leave the bridge.
 *   STAGE 3 : HTTP responder. The Wemos (in CIPMUX=1 + CIPSERVER=1,80 mode)
 *             forwards every browser request to the STM32 as
 *                 +IPD,<link>,<len>:<HTTP request>
 *             The STM32 builds a complete raw HTTP response whose BODY is a
 *             JSON document, and pushes it back with AT+CIPSEND.
 *
 * Hardware:
 *   USART1 = ESP8266 / Wemos  115200 8N1   (PA9 TX -> ESP RX, PA10 RX <- ESP TX)
 *   USART2 = PuTTY / PC       115200 8N1   (PA2 TX, PA3 RX)
 *   I2C1   = BME280           PB8 SCL / PB9 SDA, address 0x76 (or 0x77)
 *
 * IMPORTANT - ESP8266 wiring note:
 *   The Wemos runs at 3.3 V logic, same as the STM32. Do NOT power the Wemos
 *   from the Nucleo 3.3 V pin (Wi-Fi TX bursts draw ~300 mA and will brown it
 *   out); use a separate 3.3 V supply with a common GND, or a dedicated
 *   3.3 V regulator. Also tie ESP CH_PD/EN high.
 *
 * WHAT CHANGED IN THIS VERSION (latency / reliability pass)
 *   - USART1 receive is now interrupt driven into a 1024 byte ring buffer.
 *     The old code polled one byte at a time and echoed each byte to PuTTY
 *     with a blocking transmit; any burst from the ESP (a 400-600 byte HTTP
 *     request) overran the 1-byte receive register and bytes were lost, so
 *     the +IPD parser never saw a complete request and the browser hung.
 *   - Nothing in the main loop blocks for more than a few microseconds, so
 *     a request is answered in a few hundred milliseconds.
 *   - PuTTY echo of ESP traffic is OFF in Stage 3 (press 'V' to toggle).
 *   - Every failure path closes the ESP link, so a stuck link cannot hold a
 *     socket for the ESP's 180 s server timeout.
 *   - AT+CIPSTO=10 is sent on entering Stage 3 to reap idle links quickly.
 *   - Sensor readings are cached for 1.5 s so a burst of clients does not
 *     re-read the BME280 for every request.
 *
 * NOTE ON THE BOSCH API VERSION
 *   This file keeps the 3-argument form of bme280_set_sensor_settings()
 *   (older BME280_SensorAPI, the one your working Stage 1 already compiles
 *   against). If you pull the current 2023+ Bosch release, the call becomes
 *       dev.settings.osr_h = ...;
 *       bme280_set_sensor_settings(BME280_SEL_OSR_PRESS | ..., &dev);
 *   and BME280_POWERMODE_NORMAL becomes BME280_MODE_NORMAL.
 */

#include "main.h"
#include "bme280.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ============================== TUNING ================================== */

#define REQ_MAX        512U    /* max HTTP request bytes we keep           */
#define JSON_MAX       160U
#define HTTP_MAX       512U
#define CMD_MAX        160U
#define ECHO_BUF       64U     /* PuTTY echo is flushed in blocks          */
#define QDEPTH         8U      /* pending browser links                    */
#define AT_LINE_MAX    128U
#define RX_RING        1024U   /* USART1 interrupt ring buffer (power of 2) */
#define SENSOR_CACHE_MS 1500U  /* reuse a reading this fresh               */

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1, huart2;

/* ============================ SENSOR STATE ============================== */

static struct bme280_dev     dev;
static struct bme280_data    data;
static struct bme280_settings settings;
static int8_t  sensor_result = BME280_OK;
static uint8_t dev_addr      = BME280_I2C_ADDR_PRIM;
static uint8_t sensor_ready  = 0;
static float   temperature, humidity, pressure_hpa;

/* ============================== ESP STATE =============================== */

/* USART1 receive ring, filled by the interrupt handler ------------------- */
static volatile uint8_t  rx_buf[RX_RING];
static volatile uint16_t rx_head, rx_tail;
static uint8_t           rx_byte;

static uint8_t  echo_esp     = 1U;   /* mirror ESP traffic to PuTTY      */
static uint8_t  mux_enabled  = 1U;   /* CIPMUX=1  -> +IPD,<link>,<len>:  */
static uint8_t  server_on    = 0U;   /* CIPSERVER=1 seen                 */

/* +IPD parser ------------------------------------------------------------ */
typedef enum { IPD_IDLE = 0, IPD_TAG, IPD_NUM, IPD_DATA } ipd_state_t;

static ipd_state_t ipd_state;
static const char  ipd_tag[] = "+IPD,";
static uint8_t     ipd_tag_i;
static uint32_t    ipd_num;      /* digits being accumulated             */
static uint32_t    ipd_len;      /* declared payload length              */
static uint32_t    ipd_got;      /* payload bytes consumed               */
static uint8_t     ipd_link;
static uint8_t     ipd_have_link;
static char        req[REQ_MAX];
static uint16_t    req_n;

/* pending-link FIFO ------------------------------------------------------ */
static uint8_t q_link[QDEPTH];
static uint8_t q_head, q_tail, q_count;

/* echo staging ----------------------------------------------------------- */
static char    echo_buf[ECHO_BUF];
static uint8_t echo_n;

static char out[320], json[JSON_MAX], http[HTTP_MAX], command[CMD_MAX];

/* ============================ PROTOTYPES ================================ */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
int8_t user_i2c_read(uint8_t, uint8_t *, uint32_t, void *);
int8_t user_i2c_write(uint8_t, const uint8_t *, uint32_t, void *);
void user_delay_us(uint32_t, void *);

/* ======================== UART HELPER FUNCTIONS ========================= */

static void echo_flush(void)
{
    if (echo_n) {
        HAL_UART_Transmit(&huart2, (uint8_t *)echo_buf, (uint16_t)echo_n, 500);
        echo_n = 0U;
    }
}

static void echo_put(uint8_t c)
{
    if (echo_n >= ECHO_BUF) echo_flush();
    echo_buf[echo_n++] = (char)c;
}

static void pc_print(const char *s)
{
    if (!s) return;
    echo_flush();                     /* keep PuTTY output in order */
    HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 2000);
}

static void line(const char *s)
{
    pc_print("\r\n================================================\r\n");
    pc_print(s);
    pc_print("\r\n================================================\r\n");
}

static void esp_send(const char *s)
{
    if (s) HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 1500);
}

static void esp_send_cmd(const char *s)
{
    esp_send(s);
    esp_send("\r\n");
}

/* Case-insensitive substring search (AT commands may be typed in any case) */
static uint8_t str_has_ci(const char *hay, const char *needle)
{
    size_t i, j, hn = strlen(hay), nn = strlen(needle);
    if (nn == 0U || hn < nn) return 0U;
    for (i = 0U; i + nn <= hn; i++) {
        for (j = 0U; j < nn; j++) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'a' && a <= 'z') a = (char)(a - 32);
            if (b >= 'a' && b <= 'z') b = (char)(b - 32);
            if (a != b) break;
        }
        if (j == nn) return 1U;
    }
    return 0U;
}

/* ====================== +IPD PARSER AND REQUEST QUEUE =================== */

static void queue_push(uint8_t link)
{
    if (q_count >= QDEPTH) {           /* drop the oldest, keep the newest */
        q_head = (uint8_t)((q_head + 1U) % QDEPTH);
        q_count--;
    }
    q_link[q_tail] = link;
    q_tail = (uint8_t)((q_tail + 1U) % QDEPTH);
    q_count++;
}

static uint8_t queue_pop(uint8_t *link)
{
    if (q_count == 0U) return 0U;
    *link = q_link[q_head];
    q_head = (uint8_t)((q_head + 1U) % QDEPTH);
    q_count--;
    return 1U;
}

/* One complete HTTP request has been collected for `ipd_link`. */
static void request_complete(void)
{
    queue_push(ipd_link);
    req_n      = 0U;
    req[0]     = '\0';
    ipd_state  = IPD_IDLE;
    ipd_tag_i  = 0U;
    ipd_num    = 0U;
    ipd_got    = 0U;
    ipd_have_link = 0U;
}

/* Feed one byte received from the ESP8266 into the +IPD state machine.
 * Handles both mux formats:
 *      +IPD,<len>:<payload>            (CIPMUX=0)
 *      +IPD,<link>,<len>:<payload>     (CIPMUX=1)
 * The declared length is used to know exactly when the payload ends, so the
 * parser never desynchronises on a request that arrives in two TCP segments.
 */
static void esp_feed(uint8_t c)
{
    switch (ipd_state) {

    case IPD_IDLE:
        if (c == '+') { ipd_state = IPD_TAG; ipd_tag_i = 1U; }
        break;

    case IPD_TAG:
        if (c == (uint8_t)ipd_tag[ipd_tag_i]) {
            ipd_tag_i++;
            if (ipd_tag[ipd_tag_i] == '\0') {
                ipd_state     = IPD_NUM;
                ipd_num       = 0U;
                ipd_len       = 0U;
                ipd_got       = 0U;
                ipd_link      = 0U;
                ipd_have_link = 0U;
            }
        } else {
            ipd_state = IPD_IDLE;
        }
        break;

    case IPD_NUM:
        if (c >= '0' && c <= '9') {
            ipd_num = ipd_num * 10U + (uint32_t)(c - '0');
            if (ipd_num > 8192U) { ipd_state = IPD_IDLE; }
        } else if (c == ',') {
            ipd_link      = (uint8_t)(ipd_num & 0x07U);
            ipd_have_link = 1U;
            ipd_num       = 0U;
        } else if (c == ':') {
            if (ipd_have_link) ipd_len = ipd_num;
            else { ipd_link = 0U; ipd_len = ipd_num; }
            ipd_num = 0U;
            ipd_got = 0U;
            req_n   = 0U;
            req[0]  = '\0';
            ipd_state = IPD_DATA;
            if (ipd_len == 0U) request_complete();   /* empty payload */
        } else {
            ipd_state = IPD_IDLE;
        }
        break;

    case IPD_DATA:
        if (req_n < (REQ_MAX - 1U)) {
            req[req_n++] = (char)c;
            req[req_n]   = '\0';
        }
        ipd_got++;
        if (ipd_got >= ipd_len) {
            request_complete();
        } else if (req_n >= 4U &&
                   memcmp(&req[req_n - 4U], "\r\n\r\n", 4U) == 0) {
            /* Header block finished: answer now even if more bytes follow. */
            request_complete();
        }
        break;

    default:
        ipd_state = IPD_IDLE;
        break;
    }
}

/* ================== USART1 INTERRUPT RECEIVE (RING BUFFER) ============== */
/* The ESP8266 can push 500+ bytes back to back. At 115200 that is one byte
 * every 87 us, and the F401 has a single byte receive register, so anything
 * that blocks for longer than that loses data. All receiving is therefore
 * done by the interrupt below into a ring buffer; the rest of the program
 * just pops bytes out of it and can never miss one.                        */

static uint8_t rx_pop(uint8_t *c)
{
    uint16_t t = rx_tail;
    if (t == rx_head) return 0U;
    *c = rx_buf[t];
    rx_tail = (uint16_t)((t + 1U) & (RX_RING - 1U));
    return 1U;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        uint16_t next = (uint16_t)((rx_head + 1U) & (RX_RING - 1U));
        if (next != rx_tail) {          /* drop on overflow, never block   */
            rx_buf[rx_head] = rx_byte;
            rx_head = next;
        }
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
    }
}

/* Set this to 0 if stm32f4xx_it.c already defines USART1_IRQHandler
 * (CubeMX does that only when the USART1 NVIC interrupt was enabled). */
#ifndef OWN_USART1_IRQ
#define OWN_USART1_IRQ 0
#endif
#if OWN_USART1_IRQ
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
#endif

static void esp_rx_start(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
}

/* ========================== ESP READ / ECHO / WAIT ====================== */

/* Drain the ring, echo it to PuTTY (if enabled) and run the +IPD parser. */
static uint16_t poll_esp(void)
{
    uint16_t n = 0U;
    uint8_t  c;

    while (rx_pop(&c)) {
        if (echo_esp) echo_put(c);
        esp_feed(c);
        n++;
        if (n >= 2048U) break;
    }
    if (echo_esp) echo_flush();
    return n;
}

/* Rolling-tail token matcher. Returns 1 for `a`, 2 for `b`, 0 on timeout.
 * Busy-waits on the ring buffer: no blocking HAL call, no lost bytes.     */
static uint8_t esp_wait(const char *a, const char *b, uint32_t ms)
{
    char     tail[64];
    uint8_t  tn = 0U;
    uint32_t start = HAL_GetTick();
    uint8_t  res = 0U;
    uint8_t  c;

    tail[0] = '\0';
    while ((HAL_GetTick() - start) < ms) {
        if (!rx_pop(&c)) continue;

        if (echo_esp) echo_put(c);
        esp_feed(c);

        if (tn >= sizeof(tail) - 1U) {
            memmove(tail, tail + 1, sizeof(tail) - 2U);
            tn = sizeof(tail) - 2U;
        }
        tail[tn++] = (char)c;
        tail[tn]   = '\0';

        if (a && strstr(tail, a)) { res = 1U; break; }
        if (b && strstr(tail, b)) { res = 2U; break; }
    }
    if (echo_esp) echo_flush();
    return res;
}

/* Close a link without waiting for the answer: the ESP frees the slot either
 * way and we save a round trip per request. */
static void link_close(uint8_t link)
{
    if (mux_enabled) snprintf(command, sizeof(command), "AT+CIPCLOSE=%u", link);
    else             snprintf(command, sizeof(command), "AT+CIPCLOSE");
    esp_send_cmd(command);
}

static uint8_t at_command(const char *s, uint32_t timeout)
{
    uint8_t r;
    pc_print("\r\n[STM32 -> WEMOS] ");
    pc_print(s);
    pc_print("\r\n");
    esp_send_cmd(s);
    r = esp_wait("OK", "ERROR", timeout);
    if (r == 1U)      pc_print("\r\n[PASS] Wemos returned OK.\r\n");
    else if (r == 2U) pc_print("\r\n[FAIL] Wemos returned ERROR.\r\n");
    else              pc_print("\r\n[INFO/TIMEOUT] Command window elapsed.\r\n");
    return (uint8_t)(r == 1U);
}

/* ====================== STAGE 1: BME280 INTERFACE ======================= */

static int32_t scaled100(float v)
{
    return (int32_t)((v >= 0.0f) ? (v * 100.0f + 0.5f) : (v * 100.0f - 0.5f));
}

static void fmt2(char *dst, size_t n, int32_t s)
{
    uint32_t a = (uint32_t)((s < 0) ? -s : s);
    snprintf(dst, n, "%s%lu.%02lu", (s < 0) ? "-" : "",
             (unsigned long)(a / 100U), (unsigned long)(a % 100U));
}

static void sensor_init(void)
{
    memset(&dev, 0, sizeof(dev));
    dev.intf_ptr  = &dev_addr;
    dev.intf      = BME280_I2C_INTF;
    dev.read      = user_i2c_read;
    dev.write     = user_i2c_write;
    dev.delay_us  = user_delay_us;

    pc_print("[INIT] Trying BME280 I2C address 0x76...\r\n");
    sensor_result = bme280_init(&dev);

    if (sensor_result != BME280_OK) {
        dev_addr = BME280_I2C_ADDR_SEC;
        pc_print("[INIT] Trying alternate address 0x77...\r\n");
        sensor_result = bme280_init(&dev);
    }
    if (sensor_result != BME280_OK) {
        snprintf(out, sizeof(out),
                 "[FAIL] BME280 init result=%d, HAL I2C error=0x%08lX\r\n",
                 sensor_result, (unsigned long)HAL_I2C_GetError(&hi2c1));
        pc_print(out);
        return;
    }

    settings.osr_h  = BME280_OVERSAMPLING_1X;
    settings.osr_p  = BME280_OVERSAMPLING_1X;
    settings.osr_t  = BME280_OVERSAMPLING_1X;
    settings.filter = BME280_FILTER_COEFF_OFF;

    sensor_result = bme280_set_sensor_settings(
        BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP |
        BME280_SEL_OSR_HUM   | BME280_SEL_FILTER, &settings, &dev);

    if (sensor_result == BME280_OK) {
        sensor_result = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
    }
    if (sensor_result != BME280_OK) {
        snprintf(out, sizeof(out), "[FAIL] BME280 setup result=%d\r\n", sensor_result);
        pc_print(out);
        return;
    }

    HAL_Delay(100);
    sensor_ready = 1U;
    snprintf(out, sizeof(out),
             "[PASS] BME280 ready; address=0x%02X, chip ID=0x%02X\r\n",
             dev_addr, dev.chip_id);
    pc_print(out);
}

/* Read once and convert raw driver units to degC / %RH / hPa.
 * The scaling depends on which Bosch build options are active:            */
static uint8_t sensor_read(void)
{
    if (!sensor_ready) { sensor_init(); }
    if (!sensor_ready) return 0U;

    if (bme280_get_sensor_data(BME280_ALL, &data, &dev) != BME280_OK) return 0U;

    /* The unit of the driver output depends on which Bosch API version you
     * cloned, NOT on a macro you can rely on:
     *
     *   old API, integer build : T in 0.01 degC, H in 1/1024 %RH, P in Pa
     *   old API, float build   : T in degC,      H in %RH,        P in Pa
     *   new API (2023+)        : fields are double, T in degC,
     *                            H in %RH, P in Pa  - always, with no macro
     *
     * Guessing wrong here gives exactly the symptom of a sensor reporting
     * 0.30 degC and 0.07 %RH while the pressure looks perfect: the values
     * are right, they were simply divided by 100 and 1024 a second time.
     * sizeof() settles it at compile time, so no runtime cost.           */
    if (sizeof(((struct bme280_data *)0)->temperature) == sizeof(double)) {
        temperature  = (float)data.temperature;          /* already degC */
        humidity     = (float)data.humidity;             /* already %RH  */
        pressure_hpa = (float)data.pressure / 100.0f;    /* Pa -> hPa    */
    } else {
#if defined(BME280_64BIT_ENABLE)
        temperature  = (float)data.temperature / 100.0f; /* 0.01 C       */
        humidity     = (float)data.humidity / 1024.0f;   /* Q22.10       */
        pressure_hpa = (float)data.pressure / 25600.0f;  /* Q24.8 Pa     */
#else
        temperature  = (float)data.temperature / 100.0f; /* 0.01 C       */
        humidity     = (float)data.humidity / 1024.0f;   /* Q22.10       */
        pressure_hpa = (float)data.pressure / 100.0f;    /* Pa -> hPa    */
#endif
    }
    return 1U;
}

static uint32_t last_sensor_tick = 0U;
static uint8_t  last_sensor_ok   = 0U;

static uint8_t sensor_read_cached(uint32_t max_age_ms)
{
    if (last_sensor_ok && (HAL_GetTick() - last_sensor_tick) < max_age_ms) {
        return 1U;
    }
    if (!sensor_read()) { last_sensor_ok = 0U; return 0U; }
    last_sensor_tick = HAL_GetTick();
    last_sensor_ok   = 1U;
    return 1U;
}

static void read_sensor(void)
{
    char ts[16], hs[16], ps[16];

    pc_print("[SENSOR] Reading temperature, humidity and pressure...\r\n");
    if (!sensor_read()) {
        snprintf(out, sizeof(out),
                 "[FAIL] BME280 read result=%d; HAL I2C error=0x%08lX\r\n",
                 sensor_result, (unsigned long)HAL_I2C_GetError(&hi2c1));
        pc_print(out);
        return;
    }
    fmt2(ts, sizeof(ts), scaled100(temperature));
    fmt2(hs, sizeof(hs), scaled100(humidity));
    fmt2(ps, sizeof(ps), scaled100(pressure_hpa));
    snprintf(out, sizeof(out),
             "[DATA] Temperature: %s C | Humidity: %s %% | Pressure: %s hPa\r\n",
             ts, hs, ps);
    pc_print(out);
}


/* ===================== BME280 REGISTER DIAGNOSTIC ======================= */
/* Press 'D' (or look at the Stage 1 banner) to dump exactly what the chip is
 * reporting. This is what tells you whether a bad reading is a sensor that
 * never measured, a chip that has no humidity channel, or a wiring fault.  */
static void sensor_debug(void)
{
    uint8_t  id = 0U;
    uint8_t  ctrl[3] = {0U, 0U, 0U};
    uint8_t  raw[8]  = {0};
    uint32_t adc_p, adc_t, adc_h;

    if (user_i2c_read(0xD0, &id, 1U, &dev_addr) != BME280_OK) {
        pc_print("[DBG] I2C read of chip ID failed - check SDA/SCL and pull-ups\r\n");
        return;
    }
    snprintf(out, sizeof(out),
             "[DBG] chip ID = 0x%02X  (0x60 = BME280, 0x58 = BMP280, no humidity)\r\n",
             id);
    pc_print(out);

    (void)user_i2c_read(0xF2, &ctrl[0], 1U, &dev_addr);   /* ctrl_hum  */
    (void)user_i2c_read(0xF4, &ctrl[1], 1U, &dev_addr);   /* ctrl_meas */
    (void)user_i2c_read(0xF5, &ctrl[2], 1U, &dev_addr);   /* config    */
    snprintf(out, sizeof(out),
             "[DBG] ctrl_hum=0x%02X ctrl_meas=0x%02X config=0x%02X"
             "  (mode bits 00=sleep 01+10=forced 11=normal)\r\n",
             ctrl[0], ctrl[1], ctrl[2]);
    pc_print(out);

    (void)user_i2c_read(0xF7, raw, 8U, &dev_addr);
    snprintf(out, sizeof(out),
             "[DBG] raw F7..FE: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
             raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
    pc_print(out);

    adc_p = ((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | (raw[2] >> 4);
    adc_t = ((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | (raw[5] >> 4);
    adc_h = ((uint32_t)raw[6] << 8)  |  (uint32_t)raw[7];

    snprintf(out, sizeof(out),
             "[DBG] ADC  P=0x%05lX T=0x%05lX H=0x%04lX"
             "   (0x80000 / 0x8000 = never measured)\r\n",
             (unsigned long)adc_p, (unsigned long)adc_t, (unsigned long)adc_h);
    pc_print(out);

    snprintf(out, sizeof(out),
             "[DBG] driver fields are %s -> temperature in %s\r\n",
             (sizeof(((struct bme280_data *)0)->temperature) == sizeof(double))
                 ? "double (new API)" : "int32 (classic API)",
             (sizeof(((struct bme280_data *)0)->temperature) == sizeof(double))
                 ? "degC, no scaling" : "0.01 degC, divide by 100");
    pc_print(out);
}

static void stage1_bme280_setup(void)
{
    line("STAGE 1: INTERFACING BME280 SENSOR");
    sensor_init();
    read_sensor();
    if (sensor_ready) sensor_debug();
    HAL_Delay(300);
}

/* ================= STAGE 2: PuTTY -> STM32 -> WEMOS AT BRIDGE =========== */

static void stage2_configure_wemos(void)
{
    uint16_t len = 0U;
    uint8_t  c;

    echo_esp = 1U;                 /* you want to see the AT dialogue    */
    line("STAGE 2: CONFIGURE WEMOS VIA AT COMMANDS");
    pc_print("Type AT commands and press Enter (PuTTY -> STM32 -> Wemos -> PuTTY).\r\n");
    pc_print("Recommended order for the HTTP server:\r\n");
    pc_print("  1. AT\r\n");
    pc_print("  2. AT+CWMODE=1                      (station mode)\r\n");
    pc_print("  3. AT+CWJAP=\"SSID\",\"PASSWORD\"       (join the router)\r\n");
    pc_print("  4. AT+CIFSR                         (note the STAIP address)\r\n");
    pc_print("  5. AT+CIPMUX=1                      (required for CIPSERVER)\r\n");
    pc_print("  6. AT+CIPSERVER=1,80                (TCP server on port 80)\r\n");
    pc_print("Type START (or DONE) + Enter to enter Stage 3.\r\n");
    pc_print("------------------------------------------------\r\nAT> ");

    command[0] = '\0';

    while (1) {
        poll_esp();

        if (HAL_UART_Receive(&huart2, &c, 1U, 0U) == HAL_OK) {
            if (c == '\r' || c == '\n') {
                pc_print("\r\n");
                if (len > 0U) {
                    if (str_has_ci(command, "START") || str_has_ci(command, "DONE")) {
                        pc_print("[STAGE 2 COMPLETE] Leaving AT config mode...\r\n");
                        break;
                    }
                    /* Remember the server topology chosen by the user. */
                    if (str_has_ci(command, "CIPMUX=1"))     mux_enabled = 1U;
                    if (str_has_ci(command, "CIPMUX=0"))     mux_enabled = 0U;
                    if (str_has_ci(command, "CIPSERVER=1"))  server_on   = 1U;
                    if (str_has_ci(command, "CIPSERVER=0"))  server_on   = 0U;

                    {
                        uint32_t timeout = 4000U;
                        if (str_has_ci(command, "CWJAP") ||
                            str_has_ci(command, "CWLAP") ||
                            str_has_ci(command, "RST")   ||
                            str_has_ci(command, "CIPSTART")) {
                            timeout = 20000U;
                        }
                        (void)at_command(command, timeout);
                    }
                    len = 0U;
                    command[0] = '\0';
                }
                pc_print("AT> ");
            } else if ((c == 8U || c == 127U) && len > 0U) {
                command[--len] = '\0';
                pc_print("\b \b");
            } else if (c >= 32U && len < (AT_LINE_MAX - 1U)) {
                command[len++] = (char)c;
                command[len]   = '\0';
                echo_put(c);
                echo_flush();
            }
        }
    }
    echo_flush();
}

/* ===================== STAGE 3: HTTP REQUEST / RESPONSE ================= */

static uint8_t http_response(uint8_t link)
{
    int      n;
    uint8_t  r;
    uint32_t t0 = HAL_GetTick();
    char     ts[16], hs[16], ps[16];

    /* The reading is reused if it is younger than SENSOR_CACHE_MS, so a
     * browser opening several sockets does not stall on repeated I2C reads. */
    if (!sensor_read_cached(SENSOR_CACHE_MS)) {
        snprintf(json, sizeof(json),
                 "{\"status\":\"error\",\"message\":\"BME280 read failed\"}");
    } else {
        fmt2(ts, sizeof(ts), scaled100(temperature));
        fmt2(hs, sizeof(hs), scaled100(humidity));
        fmt2(ps, sizeof(ps), scaled100(pressure_hpa));
        snprintf(json, sizeof(json),
                 "{\"status\":\"success\",\"temperature\":%s,"
                 "\"humidity\":%s,\"pressure\":%s}", ts, hs, ps);
    }

    /* Raw HTTP response. Content-Length is derived from the real body. */
    snprintf(http, sizeof(http),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Cache-Control: no-store\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             (int)strlen(json), json);
    n = (int)strlen(http);

    /* Ask the Wemos for a send window. */
    if (mux_enabled) snprintf(command, sizeof(command), "AT+CIPSEND=%u,%d", link, n);
    else             snprintf(command, sizeof(command), "AT+CIPSEND=%d", n);

    esp_send_cmd(command);
    r = esp_wait(">", "ERROR", 2000U);
    if (r != 1U) {
        link_close(link);                    /* never leave a stuck link  */
        return 0U;
    }

    if (HAL_UART_Transmit(&huart1, (uint8_t *)http, (uint16_t)n, 3000U) != HAL_OK) {
        link_close(link);
        return 0U;
    }

    r = esp_wait("SEND OK", "SEND FAIL", 3000U);
    link_close(link);                        /* fire and forget           */

    /* One line per request: link, payload size, round trip and outcome.
     * This is what you point at when asked whether the server is fast.  */
    snprintf(out, sizeof(out),
             "[HTTP] link %u, %d bytes, %lu ms, %s\r\n",
             link, n, (unsigned long)(HAL_GetTick() - t0),
             (r == 1U) ? "SEND OK" : "no confirmation");
    pc_print(out);

    return (uint8_t)(r == 1U);
}

/* ======================= BOSCH API I2C CALLBACKS ======================== */

int8_t user_i2c_read(uint8_t reg, uint8_t *buf, uint32_t len, void *ptr)
{
    uint8_t addr = *(uint8_t *)ptr;
    return (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr << 1), reg,
            I2C_MEMADD_SIZE_8BIT, buf, (uint16_t)len, 1000) == HAL_OK)
            ? BME280_OK : BME280_E_COMM_FAIL;
}

int8_t user_i2c_write(uint8_t reg, const uint8_t *buf, uint32_t len, void *ptr)
{
    uint8_t addr = *(uint8_t *)ptr;
    return (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr << 1), reg,
            I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, (uint16_t)len, 1000) == HAL_OK)
            ? BME280_OK : BME280_E_COMM_FAIL;
}

void user_delay_us(uint32_t period, void *ptr)
{
    (void)ptr;
    HAL_Delay((period + 999U) / 1000U);
}

/* ============================== MAIN ENTRY ============================== */

int main(void)
{
    uint8_t key;
    uint8_t link;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    esp_rx_start();                /* interrupts armed before the ESP talks */

    /* Keep the +IPD parser in a known state. */
    ipd_state = IPD_IDLE;
    ipd_tag_i = 0U;
    req_n     = 0U;
    q_head = q_tail = q_count = 0U;

    pc_print("\r\n[BOOT] STM32F401RB + BME280 + Wemos sequential system (115200)\r\n");

    stage1_bme280_setup();
    stage2_configure_wemos();

    echo_esp = 0U;                 /* silence keeps the loop fast        */
    line("STAGE 3: HTTP RESPONDER ACTIVE");
    esp_send_cmd("AT+CIPSTO=10");  /* reap idle links in 10 s, not 180 s  */
    if (!server_on) {
        pc_print("[WARN] AT+CIPSERVER=1,80 was not seen. Press 'C' and start it.\r\n");
    }
    pc_print("[INFO] Open http://<STAIP>/ in a browser; the JSON body is shown raw.\r\n");
    pc_print("[TIP]  'R' = local sensor read, 'D' = sensor register dump,"
             " 'C' = AT config, 'V' = verbose echo.\r\n");

    while (1) {
        poll_esp();

        while (queue_pop(&link)) {
            (void)http_response(link);
        }

        if (HAL_UART_Receive(&huart2, &key, 1U, 0U) == HAL_OK) {
            if (key == 'C' || key == 'c') {
                stage2_configure_wemos();
                echo_esp = 0U;
                line("STAGE 3: HTTP RESPONDER RESUMED");
            } else if (key == 'R' || key == 'r') {
                last_sensor_ok = 0U;         /* force a fresh conversion  */
                read_sensor();
            } else if (key == 'D' || key == 'd') {
                sensor_debug();
            } else if (key == 'V' || key == 'v') {
                echo_esp = (uint8_t)!echo_esp;
                pc_print(echo_esp ? "\r\n[ECHO ON]\r\n" : "\r\n[ECHO OFF]\r\n");
            }
        }
    }
}

/* ==================== CUBEMX PERIPHERAL INITIALIZATION ================== */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_BYPASS;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 4; osc.PLL.PLLN = 84; osc.PLL.PLLP = RCC_PLLP_DIV2; osc.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();

    /* Receive interrupts for the ring buffer. Done here so the project does
     * not have to be regenerated from CubeMX.                            */
    HAL_NVIC_SetPriority(USART1_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
