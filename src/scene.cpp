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


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "fabutils.h"
#include "scene.h"





namespace fabgl {


Scene::Scene(int maxSpritesCount, int updateTimeMS, int width, int height)
 : m_width(width), m_height(height), m_collisionDetector(maxSpritesCount, width, height), m_suspendedTask(nullptr)
{
  m_updateTimer = xTimerCreate("", pdMS_TO_TICKS(updateTimeMS), pdTRUE, this, updateTimerFunc);
}


Scene::~Scene()
{
  xTimerDelete(m_updateTimer, portMAX_DELAY);
}


void Scene::start(bool suspendTask)
{
  m_updateCount = 0;
  init();
  xTimerStart(m_updateTimer, portMAX_DELAY);
  if (suspendTask) {
    m_suspendedTask = xTaskGetCurrentTaskHandle();
    vTaskSuspend(m_suspendedTask);
  } else
    m_suspendedTask = nullptr;
}


void Scene::stop()
{
  VGAController.removeSprites();
  xTimerStop(m_updateTimer, portMAX_DELAY);
  if (m_suspendedTask)
    vTaskResume(m_suspendedTask);
}


void Scene::updateTimerFunc(TimerHandle_t xTimer)
{
  Scene * scene = (Scene*) pvTimerGetTimerID(xTimer);
  scene->m_updateCount += 1;
  scene->update(scene->m_updateCount);
}


void collisionDetectionCallback(void * callbackObj, Sprite * spriteA, Sprite * spriteB, Point collisionPoint)
{
  ((Scene*)callbackObj)->collisionDetected(spriteA, spriteB, collisionPoint);
}


void Scene::updateSpriteAndDetectCollisions(Sprite * sprite)
{
  m_collisionDetector.updateAndDetectCollision(sprite, collisionDetectionCallback, this);
}




} // end of namespace
