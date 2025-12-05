#include <U8g2lib.h>
#include <SPI.h>

U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

void setup() {
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
}

void loop() {
  u8g2.firstPage();
  do {
    u8g2.drawStr(0, 20, "Hello World!");
  } while (u8g2.nextPage());
  delay(1000);
}