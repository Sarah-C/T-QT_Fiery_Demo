#include "Arduino.h"
#include "tft.h"
#include "tft_driver.h"
#include "OneButton.h"
#include "pin_config.h"

#define MAXPAL 4

OneButton btn_left(PIN_BTN_L, true);
OneButton btn_right(PIN_BTN_R, true);

uint16_t matrix[16384 + 128];
uint16_t backBuffer565[16384];
uint16_t color[200 * (MAXPAL + 1)]; // 2 palettes and current pallet space.
uint8_t pallet = 1;
uint8_t maxPal = 0;
uint32_t XORRand = 0;

// A standard XOR Shift PRNG but with a floating point twist.
// https://www.doornik.com/research/randomdouble.pdf
float random2(){
  XORRand ^= XORRand << 13;
  XORRand ^= XORRand >> 17;
  XORRand ^= XORRand << 5;
  return (float)((float)XORRand * 2.32830643653869628906e-010f);
}

void makePallets(){
  // 0b00011111 00000000 : blue
  // 0b00000000 11111000 : red
  // 0blll00000 00000hhh : green
  // Flame effect pallet
  for (int i = 0; i < 64; i++){
    uint8_t r = i * 4;
    uint8_t g = 0;
    uint8_t b = 0;
    color[200 + i] = ((g & 0b00011100) << 11) | ((g & 0b11100000) >> 5) | ((b & 0b11111000) << 5) | ((r & 0b11111000));
    r = 255;
    g = i * 4;
    b = 0;
    color[200 + i + 64] = ((g & 0b00011100) << 11) | ((g & 0b11100000) >> 5) | ((b & 0b11111000) << 5) | ((r & 0b11111000));
    r = 255;
    g = 255;
    b = i * 2;
    color[200 + i + 128] = ((g & 0b00011100) << 11) | ((g & 0b11100000) >> 5) | ((b & 0b11111000) << 5) | ((r & 0b11111000));
  }
  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 64 * 2;
  for (int i = 192; i < 200; i++){
    color[200 + i] = ((g & 0b00011100) << 11) | ((g & 0b11100000) >> 5) | ((b & 0b11111000) << 5) | ((r & 0b11111000));;
  }   
  // Cold flame effect pallet
  for (int i = 0; i < 200; i++){
    uint8_t r = (i > 100) ? (float)(i-100) * 1.775f: i / 3.0f;
    uint8_t g = (i > 100) ? (float)(i-100) * 1.775f: i / 3.0f;
    uint8_t b = (float)i * 1.275f;
    color[400 + i] = ((g & 0b00011100) << 11) | ((g & 0b11100000) >> 5) | ((b & 0b11111000) << 5) | ((r & 0b11111000));
  }
  // Black and white pallet
  for (int i = 0; i < 200; i++){
    uint8_t r = (float)i * 1.275f;
    uint8_t g = (float)i * 1.275f;
    uint8_t b = (float)i * 1.275f;
    color[600 + i] = ((g & 0b00011100) << 11) | ((g & 0b11100000) >> 5) | ((b & 0b11111000) << 5) | ((r & 0b11111000));
  }
  // Green flame effect pallet
  for (int i = 0; i < 200; i++){
    uint8_t r = (i > 100) ? (float)(i-100) * 1.175f: i / 5.0f;
    uint8_t g = (float)i * 1.275f;
    uint8_t b = (i > 100) ? (float)(i-100) * 1.775f: i / 3.0f;
    color[800 + i] = ((g & 0b00011100) << 11) | ((g & 0b11100000) >> 5) | ((b & 0b11111000) << 5) | ((r & 0b11111000));
  }
}

void usePalette(uint8_t pal){
  uint16_t palOffset = pal * 200;
  for(uint16_t i = 0; i < 200; i++){
    color[i] = color[palOffset + i];
  }
}

void prevPal(){
  if(--pallet == 0) pallet = MAXPAL;
  usePalette(pallet);
}

void nextPal(){
  if(++pallet == MAXPAL + 1) pallet = 1;
  usePalette(pallet);
}

void setup(){
  Serial.begin(115200);
  Serial.println("T-QT booting...");
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
  lcd_init();
  lcd_setRotation(2);
  digitalWrite(PIN_LCD_BL, LOW);
  XORRand = esp_random();
  makePallets();
  usePalette(1);
  // Draw the palette for a bit.
  //for (int i = 0; i < 16384; i++){
  //  backBuffer565[i] = color[(i << 1) % 200];
  //}
  //lcd_PushColors(0, 0, 128, 128, (uint16_t *)backBuffer565, 128 * 128);
  //delay(2000);
  btn_left.attachClick(prevPal);
  btn_right.attachClick(nextPal);
}

void loop(){
  btn_left.tick();
  btn_right.tick();
  // Heat up the bottom of the fire.
  for (uint16_t i = 16384; i < 16384 + 128; i++) {
    matrix[i] = 300.0f * random2();
  }
  // Nasty floating point maths to produce the billowing and nice blending.
  for (uint16_t i = 0; i < 16384; i++) {
    uint16_t pixel = (float)i + 128.0f - random2() + 0.8f;
    float sum = matrix[pixel] + matrix[pixel + 1] + matrix[pixel - 128] + matrix[pixel - 128 + 1];
    uint16_t value = sum * 0.49f * random2() + 0.5f;
    matrix[i] = value;
    if(value > 199) value = 199;
    backBuffer565[i] = color[value];
  }
  backBuffer565[0] = 0;
  backBuffer565[1] = 0;
  backBuffer565[2] = 0;
  backBuffer565[3] = 0;
  // Shift the whole thing out in one SPI transaction.
  lcd_PushColors(0, 0, 128, 128, backBuffer565, 16384);  
}
