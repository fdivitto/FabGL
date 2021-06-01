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


class BallsTest : public Test {
public:

  virtual ~BallsTest()
  {
    free(balls_);
  }

  void update()
  {
    canvas.setBrushColor(Color::Black);
    canvas.clear();

    for (int i = 0; i < ballsCount_; ++i) {
      Ball * ball = &balls_[i];

      // test collision with borders and bounce
      if (ball->x < ball->size / 2 || ball->x > canvas.getWidth() - ball->size / 2)
        ball->dir = PI - ball->dir;
      else if (ball->y < ball->size / 2 || ball->y > canvas.getHeight() - ball->size / 2)
        ball->dir = 2 * PI - ball->dir;

      ball->x += ball->vel * cos(ball->dir);
      ball->y += ball->vel * sin(ball->dir);

      canvas.setBrushColor(ball->color);
      canvas.fillEllipse(ceil(ball->x), ceil(ball->y), ball->size, ball->size);
    }
  }

  bool nextState()
  {
    if (counter_++ % 5)
      return true;
    if (ballsCount_ == MAXBALLS)
      return false;
    // create a new ball
    ++ballsCount_;
    balls_ = (Ball*) realloc(balls_, sizeof(Ball) * ballsCount_);
    Ball * newBall = &balls_[ballsCount_ - 1];
    newBall->x     = canvas.getWidth() / 2;
    newBall->y     = canvas.getHeight() / 2;
    newBall->size  = random(6, canvas.getHeight() / 6);
    newBall->dir   = random(360) * PI / 180.0;
    newBall->vel   = 0.1 + random(10) / 2.0;
    newBall->color = (Color) random(1, 16);
    return true;
  }

  int testState()
  {
    return ballsCount_;
  }

  char const * name() { return "Balls"; }

private:

  static const int MAXBALLS = 300;

  struct Ball {
    double x;
    double y;
    double dir; // radians
    double vel;
    int    size;
    Color  color;
  } * balls_ = nullptr;

  int ballsCount_ = 0;

  int counter_ = 0;

};
