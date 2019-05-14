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


#include "collisiondetector.h"
#include "fabutils.h"



namespace fabgl {




//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// QuadTree implementation


QuadTree::QuadTree(CollisionDetector * collisionDetector, QuadTree * parent, QuadTreeQuadrant quadrant, int x, int y, int width, int height)
{
  m_collisionDetector     = collisionDetector;
  m_parent                = parent;
  m_quadrant              = quadrant;
  m_x                     = x;
  m_y                     = y;
  m_width                 = width;
  m_height                = height;
  m_objects               = nullptr;
  m_objectsCount          = 0;
  m_children[TopLeft]     = nullptr;
  m_children[TopRight]    = nullptr;
  m_children[BottomLeft]  = nullptr;
  m_children[BottomRight] = nullptr;
}


bool QuadTree::isEmpty()
{
  return m_objectsCount == 0 &&
         m_children[TopLeft] == nullptr &&
         m_children[TopRight] == nullptr &&
         m_children[BottomLeft] == nullptr &&
         m_children[BottomRight] == nullptr;
}


void QuadTree::detachFromParent()
{
  if (m_parent) {
    m_parent->m_children[m_quadrant] = nullptr;
    m_parent = nullptr;
  }
}


void QuadTree::insert(QuadTreeObject * object)
{
  QuadTreeQuadrant quadrant = getQuadrant(object);
  if (quadrant != None && m_children[quadrant]) {
    m_children[quadrant]->insert(object);
    return;
  }

  object->owner = this;
  object->next = m_objects;
  m_objects = object;
  ++m_objectsCount;

  if (m_objectsCount < QUADTREE_LEVEL_SPLIT_THRESHOLD)
    return;

  // split m_objects inside sub trees (4 quadrants)

  QuadTreeObject * obj = m_objects;
  QuadTreeObject * prev = nullptr;
  while (obj) {
    QuadTreeObject * next = obj->next;
    QuadTreeQuadrant quadrant = getQuadrant(obj);
    if (quadrant != None) {
      createQuadrant(quadrant);
      m_children[quadrant]->insert(obj);
      --m_objectsCount;
      if (prev == nullptr)
        m_objects = next;
      else
        prev->next = next;
    } else {
      prev = obj;
    }
    obj = next;
  }
}


void QuadTree::remove(QuadTreeObject * object)
{
  // rebuild the list removing this object
  QuadTreeObject * obj = object->owner->m_objects;
  QuadTreeObject * prev = nullptr;
  while (obj) {
    if (obj == object) {
      if (prev == nullptr)
        object->owner->m_objects = object->next;
      else
        prev->next = object->next;
      break;
    }
    prev = obj;
    obj = obj->next;
  }
  object->owner->m_objectsCount -= 1;
  object->owner = nullptr;
  object->next  = nullptr;
}


void QuadTree::createQuadrant(QuadTreeQuadrant quadrant)
{
  if (m_children[quadrant] == nullptr) {
    int halfWidth  = m_width >> 1;
    int halfHeight = m_height >> 1;
    switch (quadrant) {
      case TopLeft:
        m_children[TopLeft]     = m_collisionDetector->initEmptyQuadTree(this, TopLeft,     m_x,             m_y,              halfWidth, halfHeight);
        break;
      case TopRight:
        m_children[TopRight]    = m_collisionDetector->initEmptyQuadTree(this, TopRight,    m_x + halfWidth, m_y,              halfWidth, halfHeight);
        break;
      case BottomLeft:
        m_children[BottomLeft]  = m_collisionDetector->initEmptyQuadTree(this, BottomLeft,  m_x,             m_y + halfHeight, halfWidth, halfHeight);
        break;
      case BottomRight:
        m_children[BottomRight] = m_collisionDetector->initEmptyQuadTree(this, BottomRight, m_x + halfWidth, m_y + halfHeight, halfWidth, halfHeight);
        break;
      default:
        break;
    }
  }
}


// check if object is inside rect
bool QuadTree::objectInRect(QuadTreeObject * object, int x, int y, int width, int height)
{
  return (object->sprite->x >= x) &&
         (object->sprite->y >= y) &&
         (object->sprite->x + object->sprite->getWidth() <= x + width) &&
         (object->sprite->y + object->sprite->getHeight() <= y + height);
}


QuadTreeQuadrant QuadTree::getQuadrant(QuadTreeObject * object)
{
  int hWidth  = m_width >> 1;
  int hHeight = m_height >> 1;
  if (objectInRect(object, m_x,          m_y,           hWidth, hHeight))
    return TopLeft;
  if (objectInRect(object, m_x + hWidth, m_y,           hWidth, hHeight))
    return TopRight;
  if (objectInRect(object, m_x,          m_y + hHeight, hWidth, hHeight))
    return BottomLeft;
  if (objectInRect(object, m_x + hWidth, m_y + hHeight, hWidth, hHeight))
    return BottomRight;
  return None;
}


void QuadTree::update(QuadTreeObject * object)
{
  QuadTree * qtree = object->owner;
  while (true) {
    if (qtree->m_parent == nullptr || objectInRect(object, qtree->m_x, qtree->m_y, qtree->m_width, qtree->m_height)) {
      // need to reinsert?
      QuadTreeQuadrant quadrant = qtree->getQuadrant(object);
      if (qtree == object->owner && qtree->m_children[quadrant] == nullptr)
        return; // don't need to reinsert

      // need to be reinserted, remove from owner...
      remove(object);

      // ...and reinsert in qtree node
      qtree->insert(object);
      break;
    }
    qtree = qtree->m_parent;
  }
}


// get the first detected collision
// ret nullptr = no collision detected
QuadTreeObject * QuadTree::detectCollision(QuadTreeObject * object, CollisionDetectionCallback callbackFunc, void * callbackObj)
{
  if (!object->sprite->visible)
    return nullptr;

  // find rectangle level collision with objects of this level
  QuadTreeObject * obj = m_objects;
  while (obj) {
    QuadTreeObject * next = obj->next;  // this allows object to be removed if collision is detected
    // check intersection and masks collision
    Point collisionPoint;
    if (object != obj && obj->sprite->visible && objectsIntersect(object, obj) && checkMaskCollision(object, obj, &collisionPoint)) {
      // collision!
      if (callbackFunc)
        callbackFunc(callbackObj, object->sprite, obj->sprite, collisionPoint); // call function and continue
      else
        return obj; // return first detected object and stop
    }
    obj = next;
  }

  // check in children
  QuadTreeQuadrant quadrant = getQuadrant(object);
  if (quadrant != None) {
    // look in a specific quadrant
    if (m_children[quadrant])
      return m_children[quadrant]->detectCollision(object, callbackFunc, callbackObj);
  } else {
    // look in all quadrants
    for (int i = 0; i < 4; ++i) {
      QuadTreeObject * obj = m_children[i] && objectIntersectsQuadTree(object, m_children[i]) ? m_children[i]->detectCollision(object, callbackFunc, callbackObj) : nullptr;
      if (obj && !callbackFunc)
        return obj;
    }
  }

  return nullptr;
}


bool QuadTree::objectsIntersect(QuadTreeObject * objectA, QuadTreeObject * objectB)
{
  return objectA->sprite->x + objectA->sprite->getWidth()  >= objectB->sprite->x &&
         objectB->sprite->x + objectB->sprite->getWidth()  >= objectA->sprite->x &&
         objectA->sprite->y + objectA->sprite->getHeight() >= objectB->sprite->y &&
         objectB->sprite->y + objectB->sprite->getHeight() >= objectA->sprite->y;
}



bool QuadTree::objectIntersectsQuadTree(QuadTreeObject * object, QuadTree * quadTree)
{
  return object->sprite->x + object->sprite->getWidth()  >= quadTree->m_x     &&
         quadTree->m_x     + quadTree->m_width           >= object->sprite->x &&
         object->sprite->y + object->sprite->getHeight() >= quadTree->m_y     &&
         quadTree->m_y     + quadTree->m_height          >= object->sprite->y;
}


bool QuadTree::checkMaskCollision(QuadTreeObject * objectA, QuadTreeObject * objectB, Point * collisionPoint)
{
  // intersection rectangle
  int x1 = tmax(objectA->sprite->x, objectB->sprite->x);
  int y1 = tmax(objectA->sprite->y, objectB->sprite->y);
  int x2 = tmin(objectA->sprite->x + objectA->sprite->getWidth() - 1, objectB->sprite->x + objectB->sprite->getWidth() - 1);
  int y2 = tmin(objectA->sprite->y + objectA->sprite->getHeight() - 1, objectB->sprite->y + objectB->sprite->getHeight() - 1);

  // look for matching non trasparent pixels inside the intersection area
  for (int y = y1; y <= y2; ++y) {
    uint8_t const * rowA = objectA->sprite->getFrame()->data + objectA->sprite->getWidth() * (y - objectA->sprite->y);
    uint8_t const * rowB = objectB->sprite->getFrame()->data + objectB->sprite->getWidth() * (y - objectB->sprite->y);
    for (int x = x1; x <= x2; ++x) {
      int alphaA = rowA[x - objectA->sprite->x] >> 6;
      int alphaB = rowB[x - objectB->sprite->x] >> 6;
      if (alphaA && alphaB) {
        *collisionPoint = (Point){(int16_t)x, (int16_t)y};
        return true;  // collision
      }
    }
  }

  return false;
}


/*
void QuadTree::dump(int level)
{
  printf("%*sLevel %d x:%d y:%d w:%d h:%d\n", level, "", level, m_x, m_y, m_width, m_height);
  printf("%*sObjects: ", level, "");
  QuadTreeObject * obj = m_objects;
  while (obj) {
    printf("[%p x:%d y:%d w:%d h:%d] ", obj, obj->sprite->x, obj->sprite->y, obj->sprite->getWidth(), obj->sprite->getHeight());
    obj = obj->next;
  }
  printf("\n");
  printf("%*sChildren:\n", level, "");
  for (int i = TopLeft; i <= BottomRight; ++i)
    if (m_children[i])
      m_children[i]->dump(level + 1);
}
*/


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// CollisionDetector implementation


CollisionDetector::CollisionDetector(int maxObjectsCount, int width, int height)
  : m_quadTreePool(nullptr), m_objectPool(nullptr)
{
  m_objectPoolSize = maxObjectsCount;
  m_quadTreePoolSize = (5 * maxObjectsCount + 1) / 3;

  if (maxObjectsCount > 0) {

    // preallocate and initialize quad trees
    m_quadTreePool = (QuadTree*) malloc(sizeof(QuadTree) * m_quadTreePoolSize);

    // initialize root quadtree
    m_quadTreePool[0] = QuadTree(this, nullptr, None, 0, 0, width, height);
    m_rootQuadTree = m_quadTreePool;

    // initialize other quadtrees
    for (int i = 1; i < m_quadTreePoolSize; ++i)
      m_quadTreePool[i] = QuadTree(this, nullptr, None, 0, 0, 0, 0);

    // preallocate and initialize objects
    m_objectPool = (QuadTreeObject*) malloc(sizeof(QuadTreeObject) * m_objectPoolSize);
    for (int i = 0; i < m_objectPoolSize; ++i)
      m_objectPool[i] = QuadTreeObject(nullptr, nullptr);

  }
}


CollisionDetector::~CollisionDetector()
{
  free(m_quadTreePool);
  free(m_objectPool);
}


QuadTree * CollisionDetector::initEmptyQuadTree(QuadTree * parent, QuadTreeQuadrant quadrant, int x, int y, int width, int height)
{
  // look for a unused quadtree inside the pool
  for (int i = 1; i < m_quadTreePoolSize; ++i) {
    if (m_quadTreePool[i].isEmpty()) {
      m_quadTreePool[i].detachFromParent();
      m_quadTreePool[i] = QuadTree(this, parent, quadrant, x, y, width, height);
      return &m_quadTreePool[i];
    }
  }
  assert(false && "No enough quadtrees");  // should never happen
}


void CollisionDetector::addSprite(Sprite * sprite)
{
  // look for unused object inside the pool
  for (int i = 0; i < m_objectPoolSize; ++i)
    if (m_objectPool[i].sprite == nullptr) {
      QuadTreeObject * obj = &m_objectPool[i];
      obj->sprite = sprite;
      sprite->collisionDetectorObject = obj;
      m_rootQuadTree->insert(obj);
      return;
    }
  assert(false && "No enough objects"); // should never happen
}


void CollisionDetector::removeSprite(Sprite * sprite)
{
  QuadTree::remove(sprite->collisionDetectorObject);
  sprite->collisionDetectorObject->sprite = nullptr;
}


/*
void CollisionDetector::dump()
{
  m_rootQuadTree->dump();
}
*/


Sprite * CollisionDetector::detectCollision(Sprite * sprite, bool removeCollidingSprites)
{
  QuadTreeObject * obj = m_rootQuadTree->detectCollision(sprite->collisionDetectorObject);
  if (obj) {
    Sprite * cSprite = obj->sprite;
    removeSprite(sprite);
    removeSprite(cSprite);
    return cSprite;
  } else
    return nullptr;
}


void CollisionDetector::detectCollision(Sprite * sprite, CollisionDetectionCallback callbackFunc, void * callbackObj)
{
  m_rootQuadTree->detectCollision(sprite->collisionDetectorObject, callbackFunc, callbackObj);
}


void CollisionDetector::update(Sprite * sprite)
{
  m_rootQuadTree->update(sprite->collisionDetectorObject);
}


Sprite * CollisionDetector::updateAndDetectCollision(Sprite * sprite, bool removeCollidingSprites)
{
  update(sprite);
  return detectCollision(sprite, removeCollidingSprites);
}


void CollisionDetector::updateAndDetectCollision(Sprite * sprite, CollisionDetectionCallback callbackFunc, void * callbackObj)
{
  update(sprite);
  detectCollision(sprite, callbackFunc, callbackObj);
}




} // end of namespace
