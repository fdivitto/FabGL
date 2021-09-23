#pragma once

#include <time.h>


struct TestTimerFrame : public uiFrame {

  uiPaintBox * paintBox;
  uiTimerHandle timer;

  TestTimerFrame(uiFrame * parent)
    : uiFrame(parent, "Clock", Point(470, 10), Size(150, 140), false) {

    timer = nullptr;

    paintBox = new uiPaintBox(this, clientPos(), clientSize());
    paintBox->paintBoxStyle().backgroundColor = Color::Yellow;
    paintBox->anchors().right = true;
    paintBox->anchors().bottom = true;
    paintBox->onPaint = [&](Rect const & r) { onPaintPaintBox(r); };

    this->onShow = [&]() {
      timer = app()->setTimer(this, 1000);
    };
    this->onHide = [&]() {
      if (timer)
        app()->killTimer(timer);
      timer = nullptr;
    };
    this->onTimer = [&](uiTimerHandle tHandle) { paintBox->repaint(); };
  }

  void onPaintPaintBox(Rect const & r) {
    int width = r.width(), height = r.height();

    time_t now = time(0);
    tm * localtm = localtime(&now);

    double radius = min(width, height) / 1.6;
    double pointsRadius  = radius * 0.76;
    double secondsRadius = radius * 0.72;
    double minutesRadius = radius * 0.60;
    double hoursRadius   = radius * 0.48;

    double s = localtm->tm_sec / 60.0 * TWO_PI - HALF_PI;
    double m = (localtm->tm_min + localtm->tm_sec / 60.0) / 60.0 * TWO_PI - HALF_PI;
    double h = (localtm->tm_hour + localtm->tm_min / 60.0) / 24.0 * TWO_PI * 2.0 - HALF_PI;

    double cx = width / 2.0;
    double cy = height / 2.0;

    auto cv = canvas();

    cv->setPenColor(Color::BrightWhite);
    cv->drawLine(cx, cy, cx + cos(s) * secondsRadius, cy + sin(s) * secondsRadius);
    cv->drawLine(cx, cy, cx + cos(m) * minutesRadius, cy + sin(m) * minutesRadius);
    cv->drawLine(cx, cy, cx + cos(h) * hoursRadius,   cy + sin(h) * hoursRadius);

    for (int a = 0; a < 360; a += 6) {
      double arad = radians(a);
      double x = cx + cos(arad) * pointsRadius;
      double y = cy + sin(arad) * pointsRadius;
      cv->setPixel(x, y);
    }
  }

};

