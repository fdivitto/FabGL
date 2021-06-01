/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.

  This library and related software is available under GPL v3 or commercial license. It is always free for students, hobbyists, professors and researchers.
  It is not-free if embedded as firmware in commercial boards.


* Contact for commercial license: fdivitto2013@gmail.com


* GPL license version 3, for non-commercial use:

  FabGL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FabGL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FabGL.  If not, see <http://www.gnu.org/licenses/>.
 */


 /*
  * OLED - SDA => GPIO 4
  * OLED - SCL => GPIO 15
  */


#include "fabgl.h"

#include "bitmaps.h"


#define OLED_SDA       GPIO_NUM_4
#define OLED_SCL       GPIO_NUM_15
#define OLED_ADDR      0x3C


fabgl::I2C               I2C;
fabgl::SSD1306Controller DisplayController;



#define SPACESHIP_COUNT  2
#define ASTEROID_COUNT   2

#define OBJECTS_COUNT (SPACESHIP_COUNT + ASTEROID_COUNT)


struct MySprite : Sprite {
  int  velX;
  int  velY;
  bool isAlive;
  bool isAsteroid;
};


struct MyScene : public Scene {

  MySprite objects_[OBJECTS_COUNT];

  MyScene()
    : Scene(OBJECTS_COUNT, 20, DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight())
  {
  }

  void startSprite(MySprite * sprite)
  {
    // set initial position to one of the four sides of the screen
    int q = random(4);  // select beginning screen side
    int x = (q == 0 ? random(getWidth()) : (q == 1 ? getWidth() - sprite->getWidth() : (q == 2 ? random(getWidth())                : 0)));
    int y = (q == 0 ? 0                  : (q == 1 ? random(getHeight())             : (q == 2 ? getHeight() - sprite->getHeight() : random(getHeight()))));
    sprite->moveTo(x, y);
    sprite->visible = true;
    sprite->velX    = - sprite->getWidth() / 4  + random(sprite->getWidth() / 2);
    sprite->velY    = - sprite->getHeight() / 4 + random(sprite->getHeight() / 2);
    sprite->isAlive = true;
  }

  void init()
  {
    paintSpace();

    // setup spaceship and asteroid sprites
    for (int i = 0; i < OBJECTS_COUNT; ++i) {
      MySprite * sprite = &objects_[i];
      if (i <= SPACESHIP_COUNT) {
        // each spaceship sprite of type Ship has 9 bitmaps: one "spaceship" bitmap and eight "explosion[]" bitmaps
        sprite->addBitmap(&spaceship)->addBitmap(explosion, 8);
        sprite->isAsteroid = false;
        addSprite(sprite); // add sprite to collision detector
        startSprite(sprite);
      } else {
        // each spaceship sprite of type Asteroid has just one bitmap: "asteroid"
        sprite->addBitmap(&asteroid);
        sprite->isAsteroid = true;
        addSprite(sprite); // add sprite to collision detector
        startSprite(sprite);
      }
    }

    DisplayController.setSprites(objects_, OBJECTS_COUNT);
  }

  void update(int updateCount)
  {
    for (int i = 1; i < OBJECTS_COUNT; ++i) {

      MySprite * sprite = &objects_[i];

      // just alive objects can be moved or checked for collisions
      if (sprite->isAlive) {

        // this object is alive, move to the next position
        sprite->moveBy(sprite->velX, sprite->velY, DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight());

        // update the collision detector and check for a possible collision
        updateSpriteAndDetectCollisions(sprite);

      } else if ((updateCount % 8) == 0) {

        // this sprite is dead, go to next frame (animate explosion) every 8 updates
        sprite->nextFrame();
        if (sprite->currentFrame == 0)
          startSprite(sprite);  // explosion animation ended, live again

      }
    }

    DisplayController.refreshSprites();
  }

  void collisionDetected(Sprite * spriteA, Sprite * spriteB, Point collisionPoint)
  {
    MySprite * a = (MySprite*) spriteA;
    MySprite * b = (MySprite*) spriteB;
    // two asteroids cannot colide
    if (a->isAsteroid && b->isAsteroid)
      return;
    // collision ok, mark both objects as dead
    a->isAlive = b->isAlive = false;
    // if collided object is an asteroid no explosion is shown, just hide it. Otherwise go to next frame.
    if (a->isAsteroid)
      a->visible = false;
    else
      a->nextFrame();
    if (b->isAsteroid)
      b->visible = false;
    else
      b->nextFrame();
  }

  void paintSpace()
  {
    Canvas cv(&DisplayController);
    cv.setBrushColor(Color::Black);
    cv.clear();
    cv.setPenColor(Color::White);
    cv.setBrushColor(Color::White);
    // stars
    cv.setPenColor(255, 255, 255);
    for (int i = 0; i < 50; ++i)
      cv.setPixel(random(getWidth()), random(getHeight()));
    // galaxy
    cv.drawBitmap(5, 5, &galaxy);
    cv.drawBitmap(90, 16, &galaxy);
  }

};


void setup()
{
  // just for debug
  Serial.begin(115200);

  I2C.begin(OLED_SDA, OLED_SCL);

  DisplayController.begin(&I2C, OLED_ADDR);
  DisplayController.setResolution(OLED_128x32);

  while (DisplayController.available() == false) {
    Serial.write("Error, SSD1306 not available!\n");
    delay(5000);
  }
}


void loop()
{
  MyScene scene;
  scene.start();
  vTaskSuspend(nullptr);
}
