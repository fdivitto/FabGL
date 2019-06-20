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
 * @brief This file contains fabgl::CollisionDetector class definition
 */


#include <stdint.h>
#include <stddef.h>

#include "fabglconf.h"
#include "vgacontroller.h"



namespace fabgl {



class CollisionDetector;

class QuadTree;


struct QuadTreeObject {
  QuadTree *       owner;
  QuadTreeObject * next;
  Sprite *         sprite;

  QuadTreeObject(QuadTreeObject * next_, Sprite * sprite_)
    : owner(nullptr), next(next_), sprite(sprite_)
  {
  }
};


#define QUADTREE_LEVEL_SPLIT_THRESHOLD 3


enum QuadTreeQuadrant {
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight,
  None,
};


typedef void (*CollisionDetectionCallback)(void * callbackObj, Sprite * spriteA, Sprite * spriteB, Point collisionPoint);


class QuadTree {

public:

  QuadTree(CollisionDetector * collisionDetector, QuadTree * parent, QuadTreeQuadrant quadrant, int x, int y, int width, int height);

  void insert(QuadTreeObject * object);

  static void remove(QuadTreeObject * object);

  //void dump(int level = 0);

  QuadTreeObject * detectCollision(QuadTreeObject * object, CollisionDetectionCallback callbackFunc = nullptr, void * callbackObj = nullptr);

  bool isEmpty();

  void detachFromParent();

  static void update(QuadTreeObject * object);

private:

  QuadTreeQuadrant getQuadrant(QuadTreeObject * object);
  void createQuadrant(QuadTreeQuadrant quadrant);
  bool objectsIntersect(QuadTreeObject * objectA, QuadTreeObject * objectB);
  bool objectIntersectsQuadTree(QuadTreeObject * object, QuadTree * quadTree);
  bool checkMaskCollision(QuadTreeObject * objectA, QuadTreeObject * objectB, Point * collisionPoint);

  static bool objectInRect(QuadTreeObject * object, int x, int y, int width, int height);

  CollisionDetector * m_collisionDetector;
  QuadTree *          m_parent;
  QuadTreeQuadrant    m_quadrant;
  int                 m_x;
  int                 m_y;
  int                 m_width;
  int                 m_height;
  QuadTreeObject *    m_objects;
  int                 m_objectsCount;
  QuadTree *          m_children[4]; // index is Quadrant

};



/**
 * @brief A class to detect sprites collisions.
 *
 * CollisionDetector uses a Quad-tree data structure to efficiently store sprites size and position and quick detect collisions.<br>
 * CollisionDetector is embedded in Scene class, so usually you don't need to instantiate it.
 */
class CollisionDetector {

friend class QuadTree;

public:

  /**
   * @brief Creates an instance of CollisionDetector.
   *
   * CollisionDetector is embedded in Scene class, so usually you don't need to instantiate it.
   *
   * @param maxObjectsCount Specifies maximum number of sprites. This is required to size the underlying quad-tree data structure.
   * @param width The scene width in pixels.
   * @param height The scene height in pixels.
   */
  CollisionDetector(int maxObjectsCount, int width, int height);

  ~CollisionDetector();

  /**
   * @brief Adds the specified sprite to collision detector.
   *
   * The collision detector is updated calling CollisionDetector.update() or CollisionDetector.updateAndDetectCollision().<br>
   * The number of sprites cannot exceed the value specified in CollisionDetector constructor.
   *
   * @param sprite The sprite to add.
   */
  void addSprite(Sprite * sprite);

  /**
   * @brief Removes the specified sprite from collision detector.
   *
   * @param sprite The sprite to remove.
   */
  void removeSprite(Sprite * sprite);

  /**
   * @brief Detects first collision with the specified sprite.
   *
   * It is necessary to call CollisionDetector.update() before detectCollision() so sprites position and size are correctly updated.
   *
   * @param sprite The sprite to detect collision.
   * @param removeCollidingSprites If true the collided sprites are automatically removed from the collision detector.
   *
   * @return The other sprite that collided with the specified sprite.
   */
  Sprite * detectCollision(Sprite * sprite, bool removeCollidingSprites = true);

  /**
   * @brief Detects multiple collisions with the specified sprite.
   *
   * It is necessary to call CollisionDetector.update() before detectCollision() so sprites position and size are correctly updated.
   *
   * @param sprite The sprite to detect collision.
   * @param callbackFunc The callback function called whenever a collision is detected.
   * @param callbackObj Pointer passed as parameter to the callback function.
   */
  void detectCollision(Sprite * sprite, CollisionDetectionCallback callbackFunc, void * callbackObj);

  /**
   * @brief Updates collision detector.
   *
   * When a sprite changes its position or size it is necessary to update the collision detector.<br>
   * This method just updates the detector without generate collision events.
   *
   * @param sprite The sprite to update.
   */
  void update(Sprite * sprite);

  /**
   * @brief Updates collision detector and detect collision with the specified sprite.
   *
   * When a sprite changes its position or size it is necessary to update the collision detector.<br>
   * This method updates the detector and detects collisions.
   *
   * @param sprite The sprite to update and to check for collisions.
   * @param removeCollidingSprites If true the collided sprites are automatically removed from the collision detector.
   *
   * @return The other sprite that collided with the specified sprite.
   */
  Sprite * updateAndDetectCollision(Sprite * sprite, bool removeCollidingSprites = true);

  /**
   * @brief Updates collision detector and detect multiple collisions with the specified sprite.
   *
   * When a sprite changes its position or size it is necessary to update the collision detector.<br>
   * This method updates the detector and detects multiple collisions.
   *
   * @param sprite The sprite to update and to check for collisions.
   * @param callbackFunc The callback function called whenever a collision is detected.
   * @param callbackObj Pointer passed as parameter to the callback function.
   */
  void updateAndDetectCollision(Sprite * sprite, CollisionDetectionCallback callbackFunc, void * callbackObj);

  //void dump();


private:

  QuadTree * initEmptyQuadTree(QuadTree * parent, QuadTreeQuadrant quadrant, int x, int y, int width, int height);


  QuadTree *       m_rootQuadTree;
  QuadTree *       m_quadTreePool;
  int              m_quadTreePoolSize;
  QuadTreeObject * m_objectPool;
  int              m_objectPoolSize;
};





} // end of namespace




