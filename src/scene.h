/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019 Fabrizio Di Vittorio.
  All rights reserved.

  This file is part of FabGL Library.

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


#pragma once



/**
 * @file
 *
 * @brief This file contains fabgl::Scene definition.
 */


#include "freertos/FreeRTOS.h"

#include "fabglconf.h"
#include "canvas.h"
#include "collisiondetector.h"



namespace fabgl {



/**
 * @brief Scene is an abstract class useful to encapsulate functionalities of a scene (sprites, collision detector and updates).
 */
class Scene {

public:

  /**
   * @brief The Scene constructor.
   *
   * @param maxSpritesCount Specifies maximum number of sprites. This is required to size the collision detector object.
   * @param updateTimeMS Number of milliseconds between updates. Scene.update() is called whenever an update occurs.
   * @param width The scene width in pixels.
   * @param height The scene height in pixels.
   */
  Scene(int maxSpritesCount, int updateTimeMS = 20, int width = Canvas.getWidth(), int height = Canvas.getHeight());

  virtual ~Scene();

  /**
   * @brief Determines scene width.
   *
   * @return Scene width in pixels.
   */
  int getWidth()  { return m_width; }

  /**
   * @brief Determines scene height.
   *
   * @return Scene height in pixels.
   */
  int getHeight() { return m_height; }

  /**
   * @brief Starts scene updates and suspends current task.
   *
   * @param suspendTask If true (default) current calling task is suspended immeditaly.
   *
   * Example:
   *
   *     void loop()
   *     {
   *       GameScene gameScene;
   *       gameScene.start();
   *     }
   */
  void start(bool suspendTask = true);

  /**
   * @brief Stops scene updates and resumes suspended task.
   */
  void stop();

  /**
   * @brief This is an abstract method called when the scene needs to be initialized.
   */
  virtual void init() = 0;

  /**
   * @brief This is an abstract method called whenever the scene needs to be updated.
   *
   * @param updateCount Indicates number of times Scane.update() has been called.
   */
  virtual void update(int updateCount) = 0;

  /**
   * @brief This is an abstract method called whenever a collision has been detected.
   *
   * This method is called one o more times as a result of calling Scene.updateSpriteAndDetectCollisions() method when
   * one o more collisions has been detected.
   *
   * @param spriteA One of the two sprites collided. This is the same sprite specified in Scene.updateSpriteAndDetectCollisions() call.
   * @param spriteB One of the two sprites collided.
   * @param collisionPoint Coordinates of a collision point.
   */
  virtual void collisionDetected(Sprite * spriteA, Sprite * spriteB, Point collisionPoint) = 0;

  /**
   * @brief Adds the specified sprite to collision detector.
   *
   * The collision detector is updated calling Scene.updateSprite() or updateSpriteAndDetectCollisions().<br>
   * The number of sprites cannot exceed the value specified in Scene constructor.
   *
   * @param sprite The sprite to add.
   */
  void addSprite(Sprite * sprite)    { m_collisionDetector.addSprite(sprite); }

  /**
   * @brief Removes the specified sprite from collision detector.
   *
   * @param sprite The sprite to remove.
   */
  void removeSprite(Sprite * sprite) { m_collisionDetector.removeSprite(sprite); }

  /**
   * @brief Updates collision detector.
   *
   * When a sprite changes its position or size it is necessary to update the collision detector.<br>
   * This method just updates the detector without generate collision events.<br>
   * To generate collision events call Scene.updateSpriteAndDetectCollisions instead.
   *
   * @param sprite The sprite to update.
   */
  void updateSprite(Sprite * sprite) { m_collisionDetector.update(sprite); }

  /**
   * @brief Updates collision detector and generate collision events.
   *
   * When a sprite changes its position or size it is necessary to update the collision detector.<br>
   * This method updates the detector and generates collision events accordly.
   *
   * @param sprite The sprite to update and to check for collisions.
   */
  void updateSpriteAndDetectCollisions(Sprite * sprite);

private:

  static void updateTimerFunc(TimerHandle_t xTimer);


  int               m_width;
  int               m_height;

  TimerHandle_t     m_updateTimer;
  int               m_updateCount;

  CollisionDetector m_collisionDetector;

  TaskHandle_t      m_suspendedTask;

};





} // end of namespace






