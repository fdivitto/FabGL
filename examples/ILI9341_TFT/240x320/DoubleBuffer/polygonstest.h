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


class PolygonsTest : public Test {

  static const int MAXPOINTS   = 7;
  static const int MAXPOLYGONS = 60;
  static const int MAXPOLYSIZE = 80;

  struct DPoint {
    double X;
    double Y;
  };

  struct Polygon {
    DPoint  dpoints[MAXPOINTS];
    DPoint  center;
    Point   ipoints[MAXPOINTS];
    double  avel;
    int     pointsCount;
    Color   color;
  };

public:

  virtual ~PolygonsTest()
  {
    free(polygons_);
  }

  void update()
  {
    canvas.setBrushColor(Color::Black);
    canvas.clear();

    for (int i = 0; i < polygonsCount_; ++i) {
      Polygon * polygon = &polygons_[i];

      rotate(polygon);

      canvas.setBrushColor(polygon->color);
      canvas.fillPath(polygon->ipoints, polygon->pointsCount);
    }
  }

  bool nextState()
  {
    if (counter_++ % 5)
      return true;
    if (polygonsCount_ == MAXPOLYGONS)
      return false;
    // create a new polygon
    ++polygonsCount_;
    polygons_ = (Polygon*) realloc(polygons_, sizeof(Polygon) * polygonsCount_);
    Polygon * newPolygon = &polygons_[polygonsCount_ - 1];
    newPolygon->pointsCount = random(3, MAXPOINTS);
    int translateX = random(-MAXPOLYSIZE / 2, canvas.getWidth() - MAXPOLYSIZE / 2);
    int translateY = random(-MAXPOLYSIZE / 2, canvas.getHeight() - MAXPOLYSIZE / 2);
    newPolygon->center.X = 0;
    newPolygon->center.Y = 0;
    for (int i = 0; i < newPolygon->pointsCount; ++i) {
      newPolygon->dpoints[i].X = random(0, MAXPOLYSIZE) + translateX;
      newPolygon->dpoints[i].Y = random(0, MAXPOLYSIZE) + translateY;
      newPolygon->center.X += newPolygon->dpoints[i].X;
      newPolygon->center.Y += newPolygon->dpoints[i].Y;
    }
    newPolygon->center.X /= newPolygon->pointsCount;
    newPolygon->center.Y /= newPolygon->pointsCount;
    newPolygon->color = (Color) random(1, 16);
    newPolygon->avel = PI / random(5, 300) * (random(2) ? 1 : -1);
    return true;
  }

  int testState()
  {
    return polygonsCount_;
  }

  void rotate(Polygon * polygon)
  {
    for (int i = 0; i < polygon->pointsCount; ++i) {
      polygon->dpoints[i].X -= polygon->center.X;
      polygon->dpoints[i].Y -= polygon->center.Y;
      double x = polygon->dpoints[i].X * cos(polygon->avel) - polygon->dpoints[i].Y * sin(polygon->avel);
      double y = polygon->dpoints[i].X * sin(polygon->avel) + polygon->dpoints[i].Y * cos(polygon->avel);
      polygon->dpoints[i].X = x + polygon->center.X;
      polygon->dpoints[i].Y = y + polygon->center.Y;
      polygon->ipoints[i].X = ceil(polygon->dpoints[i].X);
      polygon->ipoints[i].Y = ceil(polygon->dpoints[i].Y);
    }
  }

  char const * name() { return "Polygons"; }

private:


  Polygon * polygons_ = nullptr;

  int polygonsCount_ = 0;

  int counter_ = 0;

};
