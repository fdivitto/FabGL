// enemy_fire: 2x 3x7
const uint8_t enemy_fire_data[][7] = {
  // frame 0
  { 0b10000000,
    0b01000000,
    0b00100000,
    0b01000000,
    0b10000000,
    0b01000000,
    0b00100000 },
  // frame 1
  { 0b00100000,
    0b01000000,
    0b10000000,
    0b01000000,
    0b00100000,
    0b01000000,
    0b10000000 },
};
Bitmap bmpEnemiesFire[2] = { Bitmap(3, 7, &enemy_fire_data[0], PixelFormat::Mask, RGB888(255, 255, 255)),
                             Bitmap(3, 7, &enemy_fire_data[1], PixelFormat::Mask, RGB888(255, 255, 255)), };


// player_fire: 1x6
const uint8_t player_fire_data[][6] = {
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
};
Bitmap bmpPlayerFire = Bitmap(1, 6, player_fire_data, PixelFormat::Mask, RGB888(0, 255, 255));


// player_explosion: 2x 16x8
const uint8_t player_explosion_data[][16] = {
  // frame 0
  { 0b00010000, 0b00000100,
    0b10000010, 0b00011001,
    0b00010000, 0b11000000,
    0b00000010, 0b00000010,
    0b01001011, 0b00110001,
    0b00100001, 0b11000100,
    0b00011111, 0b11110000,
    0b00110111, 0b11110000 },
  // frame 1
  { 0b00000100, 0b00000000,
    0b00000000, 0b00100000,
    0b00000101, 0b01000000,
    0b00100100, 0b00000000,
    0b00000011, 0b01100000,
    0b10001011, 0b01010000,
    0b00111111, 0b11001000,
    0b01111111, 0b11101000 },
};
Bitmap bmpPlayerExplosion[2] = { Bitmap(16, 8, &player_explosion_data[0], PixelFormat::Mask, RGB888(0, 255, 255)),
                                 Bitmap(16, 8, &player_explosion_data[1], PixelFormat::Mask, RGB888(0, 255, 255)), };


// player: 14x8
const uint8_t player_data[] = {
  0b00000001, 0b00000000,
  0b00000011, 0b10000000,
  0b00000011, 0b10000000,
  0b00111111, 0b11111000,
  0b01111111, 0b11111100,
  0b01111111, 0b11111100,
  0b01111111, 0b11111100,
  0b01111111, 0b11111100,
};
Bitmap bmpPlayer = Bitmap(14, 8, player_data, PixelFormat::Mask, RGB888(0, 255, 255));


// shield: 22x16
const uint8_t shield_data[] = {
  0x0f, 0xff, 0xc0, 0x1f, 0xff, 0xe0, 0x3f, 0xff, 0xf0, 0x7f, 0xff, 0xf8, 0xff, 0xff, 0xfc, 0xff,
  0xff, 0xfc, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xfc, 0xff, 0xff,
  0xfc, 0xff, 0xff, 0xfc, 0xfe, 0x03, 0xfc, 0xfc, 0x01, 0xfc, 0xf8, 0x00, 0xfc, 0xf8, 0x00, 0xfc,
};


// enemy_explosion: 12x8
const uint8_t enemy_explosion_data[] = {
  0b00001000, 0b10000000,
  0b01000101, 0b00010000,
  0b00100000, 0b00100000,
  0b00010000, 0b01000000,
  0b11000000, 0b00010000,
  0b00010000, 0b01000000,
  0b00100101, 0b00100000,
  0b01001000, 0b10010000,
};
Bitmap bmpEnemyExplosion    = Bitmap(12, 8, enemy_explosion_data, PixelFormat::Mask, RGB888(255, 0, 0));
Bitmap bmpEnemyExplosionRed = Bitmap(12, 8, enemy_explosion_data, PixelFormat::Mask, RGB888(255, 0, 0));


// enemyA: 2x 12x8
const uint8_t enemyA_data[][16] = {
  // frame 0
  { 0b00000110, 0b00000000,
    0b00001111, 0b00000000,
    0b00011111, 0b10000000,
    0b00110110, 0b11000000,
    0b00111111, 0b11000000,
    0b00001001, 0b00000000,
    0b00010110, 0b10000000,
    0b00101001, 0b01000000, },
  // frame 1
  { 0b00000110, 0b00000000,
    0b00001111, 0b00000000,
    0b00011111, 0b10000000,
    0b00110110, 0b11000000,
    0b00111111, 0b11000000,
    0b00010110, 0b10000000,
    0b00100000, 0b01000000,
    0b00010000, 0b10000000, },
};
Bitmap bmpEnemyA[2] = { Bitmap(12, 8, enemyA_data[0], PixelFormat::Mask, RGB888(0, 255, 0)),
                        Bitmap(12, 8, enemyA_data[1], PixelFormat::Mask, RGB888(0, 255, 0)) };


// enemyB: 2x 12x8
const uint8_t enemyB_data[][16] = {
  // frame 0
  { 0b00010000, 0b01000000,
    0b01001000, 0b10010000,
    0b01011111, 0b11010000,
    0b01110111, 0b01110000,
    0b01111111, 0b11110000,
    0b00111111, 0b11100000,
    0b00010000, 0b01000000,
    0b00100000, 0b00100000, },
  // frame 1
  { 0b00010000, 0b01000000,
    0b00001000, 0b10000000,
    0b00011111, 0b11000000,
    0b00110111, 0b01100000,
    0b01111111, 0b11110000,
    0b01011111, 0b11010000,
    0b01010000, 0b01010000,
    0b00001101, 0b10000000, },
};
Bitmap bmpEnemyB[2] = { Bitmap(12, 8, enemyB_data[0], PixelFormat::Mask, RGB888(255, 255, 0)),
                        Bitmap(12, 8, enemyB_data[1], PixelFormat::Mask, RGB888(255, 255, 0)) };


// enemyC: 2x 12x8
const uint8_t enemyC_data[][16] = {
  // frame 0
  { 0b00001111, 0b00000000,
    0b01111111, 0b11100000,
    0b11111111, 0b11110000,
    0b11100110, 0b01110000,
    0b11111111, 0b11110000,
    0b00111001, 0b11000000,
    0b01100110, 0b01100000,
    0b00110000, 0b11000000 },
  // frame 1
  { 0b00001111, 0b00000000,
    0b01111111, 0b11100000,
    0b11111111, 0b11110000,
    0b11100110, 0b01110000,
    0b11111111, 0b11110000,
    0b00011001, 0b10000000,
    0b00110110, 0b11000000,
    0b11000000, 0b00110000 },
};
Bitmap bmpEnemyC[2] = { Bitmap(12, 8, enemyC_data[0], PixelFormat::Mask, RGB888(255, 0, 0)),
                        Bitmap(12, 8, enemyC_data[1], PixelFormat::Mask, RGB888(255, 0, 0)) };


// enemyD: 16x7
const uint8_t enemyD_data[] = {
  0x07, 0xe0, 0x1f, 0xf8, 0x3f, 0xfc, 0x6d, 0xb6, 0xff, 0xff, 0x39, 0x9c, 0x10, 0x08,
};
Bitmap bmpEnemyD = Bitmap(16, 7, enemyD_data, PixelFormat::Mask, RGB888(255, 255, 255));

