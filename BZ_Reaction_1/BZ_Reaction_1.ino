// Belousov-Zhabotinsky reaction 1 //

#include "hardware/structs/rosc.h"
#include "st7789_lcd.pio.h"

#define PIN_DIN   11
#define PIN_CLK   10
#define PIN_CS    9
#define PIN_DC    8
#define PIN_RESET 12
#define PIN_BL    13
#define KEY_A     15

PIO pio = pio0;
uint sm = 0;
uint offset = pio_add_program(pio, &st7789_lcd_program);

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define FULLW   240
#define FULLH   320
#define WIDTH   120
#define HEIGHT  120
#define SCR     (WIDTH*HEIGHT)
#define SCR2    (FULLW*FULLH)

  float adjust = 1.2f;
  float a [WIDTH][HEIGHT][2];
  float b [WIDTH][HEIGHT][2];
  float c [WIDTH][HEIGHT][2];
  int p = 0, q = 1;
  int x,y,i,j;
  uint16_t col[SCR2];

uint16_t color565(uint8_t red, uint8_t green, uint8_t blue) { return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3); }
float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);} 

#define SERIAL_CLK_DIV 1.f

static const uint8_t st7789_init_seq[] = {
        1, 20, 0x01,                        // Software reset
        1, 10, 0x11,                        // Exit sleep mode
        2, 2, 0x3a, 0x55,                   // Set colour mode to 16 bit
        2, 0, 0x36, 0x00,                   // Set MADCTL: row then column, refresh is bottom to top ????
        5, 0, 0x2a, 0x00, 0x00, 0x00, 0xf0, // CASET: column addresses from 0 to 240 (f0)
        5, 0, 0x2b, 0x00, 0x00, 0x01, 0x40, // RASET: row addresses from 0 to 320
        1, 2, 0x21,                         // Inversion on, then 10 ms delay (supposedly a hack?)
        1, 2, 0x13,                         // Normal display on, then 10 ms delay
        1, 2, 0x29,                         // Main screen turn on, then wait 500 ms
        0                                   // Terminate list
};

static inline void lcd_set_dc_cs(bool dc, bool cs) {

  sleep_us(1);
  gpio_put_masked((1u << PIN_DC) | (1u << PIN_CS), !!dc << PIN_DC | !!cs << PIN_CS);
  sleep_us(1);

}

static inline void lcd_write_cmd(PIO pio, uint sm, const uint8_t *cmd, size_t count) {

  st7789_lcd_wait_idle(pio, sm);
  lcd_set_dc_cs(0, 0);
  st7789_lcd_put(pio, sm, *cmd++);
  if (count >= 2) {
  st7789_lcd_wait_idle(pio, sm);
  lcd_set_dc_cs(1, 0);
  for (size_t i = 0; i < count - 1; ++i) st7789_lcd_put(pio, sm, *cmd++);
  }
  st7789_lcd_wait_idle(pio, sm);
  lcd_set_dc_cs(1, 1);

}

static inline void lcd_init(PIO pio, uint sm, const uint8_t *init_seq) {

  const uint8_t *cmd = init_seq;
  while (*cmd) {
  lcd_write_cmd(pio, sm, cmd + 2, *cmd);
  sleep_ms(*(cmd + 1) * 5);
  cmd += *cmd + 2;
  }
}

static inline void st7789_start_pixels(PIO pio, uint sm) {

  uint8_t cmd = 0x2c; // RAMWR
  lcd_write_cmd(pio, sm, &cmd, 1);
  lcd_set_dc_cs(1, 0);

}

static inline void seed_random_from_rosc(){
  
  uint32_t random = 0;
  uint32_t random_bit;
  volatile uint32_t *rnd_reg = (uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);

  for (int k = 0; k < 32; k++) {
    while (1) {
      random_bit = (*rnd_reg) & 1;
      if (random_bit != ((*rnd_reg) & 1)) break;
    }

    random = (random << 1) | random_bit;
  }

  srand(random);
}

void rndrule(){

  memset(col, 0, 2*SCR2);

  adjust = randomf(0.75f, 1.35f);

  for (y = 0; y < HEIGHT; y++) {

    for (x = 0; x < WIDTH; x++) {

      a[x][y][0] = randomf(0.0f, 1.0f);
      b[x][y][0] = randomf(0.0f, 1.0f);
      c[x][y][0] = randomf(0.0f, 1.0f);

    }
  }

}

void setup() {

  seed_random_from_rosc();

  st7789_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, SERIAL_CLK_DIV);

  gpio_init(PIN_CS);
  gpio_init(PIN_DC);
  gpio_init(PIN_RESET);
  gpio_init(PIN_BL);
  gpio_init(KEY_A);
  gpio_set_dir(PIN_CS, GPIO_OUT);
  gpio_set_dir(PIN_DC, GPIO_OUT);
  gpio_set_dir(PIN_RESET, GPIO_OUT);
  gpio_set_dir(PIN_BL, GPIO_OUT);
  gpio_set_dir(KEY_A, GPIO_IN);

  gpio_put(PIN_CS, 1);
  gpio_put(PIN_RESET, 1);
  lcd_init(pio, sm, st7789_init_seq);
  gpio_put(PIN_BL, 1);
  gpio_pull_up(KEY_A);

  rndrule();
  
}

void loop() {

  if (gpio_get(KEY_A) == false) rndrule();
  
  st7789_start_pixels(pio, sm);

  for (y = 0; y < HEIGHT; y++) {

    for (x = 0; x < WIDTH; x++) {

      float c_a = 0.0f;
      float c_b = 0.0f;
      float c_c = 0.0f;

      for (i = x - 1; i <= x+1; i++) {

        for (j = y - 1; j <= y+1; j++) {

          c_a += a[(i+WIDTH)%WIDTH][(j+HEIGHT)%HEIGHT][p];
          c_b += b[(i+WIDTH)%WIDTH][(j+HEIGHT)%HEIGHT][p];
          c_c += c[(i+WIDTH)%WIDTH][(j+HEIGHT)%HEIGHT][p];

        }
      }

      c_a /= 9.0f;
      c_b /= 9.0f;
      c_c /= 9.0f;

      a[x][y][q] = constrain(c_a + c_a * (adjust * c_b - c_c), 0.0f, 1.0f);
      b[x][y][q] = constrain(c_b + c_b * (c_c - adjust * c_a), 0.0f, 1.0f);
      c[x][y][q] = constrain(c_c + c_c * (c_a - c_b), 0.0f, 1.0f);
      
      uint16_t coll = (255.0f * a[x][y][q]);
      col[(2*x)+(2*(20+y))*FULLW] = color565(coll, coll, coll);

    }

  }
  
  if (p == 0) { p = 1; q = 0; } else { p = 0; q = 1; }

  for(int i = 0; i < SCR2; i++){
 
      uint16_t image = col[i];
      st7789_lcd_put(pio, sm, image >> 8);
      st7789_lcd_put(pio, sm, image & 0xff);

  }

}