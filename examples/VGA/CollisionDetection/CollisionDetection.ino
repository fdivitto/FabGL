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


#include "fabgl.h"

#include "bitmaps.h"



fabgl::VGAController DisplayController;


#define SPACESHIP_COUNT 10
#define ASTEROID_COUNT   4

#define OBJECTS_COUNT (SPACESHIP_COUNT + ASTEROID_COUNT + 1)    // +1 = is for jupiter!


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
      if (i == 0) {
        // setup Jupiter sprite
        sprite->addBitmap(&jupiter);
        sprite->moveTo(0, getHeight() / 2);
      } else if (i <= SPACESHIP_COUNT) {
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

    // move Jupiter every 20 updates
    if ((updateCount % 20) == 0)
      objects_[0].moveBy(1, 0, DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight());

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
    // far stars
    cv.setPenColor(64, 64, 64);
    for (int i = 0; i < 400; ++i)
      cv.setPixel(random(getWidth()), random(getHeight()));
    // near stars
    cv.setPenColor(128, 128, 128);
    for (int i = 0; i < 50; ++i)
      cv.setPixel(random(getWidth()), random(getHeight()));
    // galaxy
    cv.drawBitmap(80, 35, &galaxy);
    cv.drawBitmap(220, 130, &galaxy);
  }

};


void setup()
{
  // just for debug
  Serial.begin(115200);

  DisplayController.begin();
  DisplayController.setResolution(VGA_320x200_75Hz);
  //DisplayController.moveScreen(20, 0);
  DisplayController.moveScreen(-8, 0);
}


void loop()
{
  MyScene scene;
  scene.start();
  vTaskSuspend(nullptr);
}
