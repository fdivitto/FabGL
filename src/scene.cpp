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


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "fabutils.h"
#include "scene.h"



#pragma GCC optimize ("O2")


namespace fabgl {


Scene::Scene(int maxSpritesCount, int updateTimeMS, int width, int height, int stackSize)
 : m_width(width),
   m_height(height),
   m_updateTimeMS(updateTimeMS),
   m_collisionDetector(maxSpritesCount, width, height),
   m_suspendedTask(nullptr),
   m_running(false)
{
  m_mutex = xSemaphoreCreateMutex();
  xSemaphoreTake(m_mutex, portMAX_DELAY);  // suspend update task
  xTaskCreate(updateTask, "", FABGL_DEFAULT_SCENETASK_STACKSIZE, this, 5, &m_updateTaskHandle);
}


Scene::~Scene()
{
  stop();
  xSemaphoreTake(m_mutex, portMAX_DELAY); // suspend update task
  vTaskDelete(m_updateTaskHandle);
  vSemaphoreDelete(m_mutex);
}


void Scene::start(bool suspendTask)
{
  if (!m_running) {
    m_running = true;
    m_updateCount = 0;
    init();
    xSemaphoreGive(m_mutex);  // resume update task
    if (suspendTask) {
      m_suspendedTask = xTaskGetCurrentTaskHandle();
      vTaskSuspend(m_suspendedTask);
    } else
      m_suspendedTask = nullptr;
  }
}


void Scene::stop()
{
  if (m_running) {
    // are we inside update task?
    if (xTaskGetCurrentTaskHandle() != m_updateTaskHandle)
      xSemaphoreTake(m_mutex, portMAX_DELAY); // no, suspend update task
    m_running = false;
    if (m_suspendedTask)
      vTaskResume(m_suspendedTask);
  }
}


void Scene::updateTask(void * pvParameters)
{
  Scene * scene = (Scene*) pvParameters;

  while (true) {

    xSemaphoreTake(scene->m_mutex, portMAX_DELAY);

    int64_t t0 = esp_timer_get_time();  // us

    if (scene->m_running) {
      scene->m_updateCount += 1;
      scene->update(scene->m_updateCount);
    }

    xSemaphoreGive(scene->m_mutex);

    int64_t t1 = esp_timer_get_time();  // us
    int delayMS = (scene->m_updateTimeMS - (t1 - t0) / 1000);
    if (delayMS > 0)
      vTaskDelay(delayMS / portTICK_PERIOD_MS);
  }
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
