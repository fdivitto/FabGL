#pragma once

/* Actually this tests:

  - uiPaintBox widget
  - horizontal scroll bar
  - anchors

*/



struct TestPaintBoxFrame : public uiFrame {

  uiPaintBox * paintBox;

  static constexpr int count = 1000;
  int8_t * values = nullptr;

  TestPaintBoxFrame(uiFrame * parent)
    : uiFrame(parent, "Test Paint Box", Point(130, 10), Size(300, 210), false) {

    values = new int8_t[count];
    for (int i = 0; i < count; ++i)
      values[i] = random(-50, 50);

    paintBox = new uiPaintBox(this, clientPos(), clientSize());
    paintBox->anchors().right = true;
    paintBox->anchors().bottom = true;
    paintBox->setScrollBar(uiOrientation::Horizontal, 0, paintBox->clientSize().width, count);
    paintBox->onPaint = [&](Rect const & r) { onPaintPaintBox(r); };
    paintBox->onChangeHScrollBar = [&]() { paintBox->repaint(); };
  }

  void onPaintPaintBox(Rect const & r) {
    int w = r.width(), h = r.height();
    int midY = h / 2;

    // paint occurs even on resize, so we need to make sure it has the right "width" (w)
    paintBox->setScrollBar(uiOrientation::Horizontal, paintBox->HScrollBarPos(), w, count, true);

    Canvas.setPenColor(RGB(3, 3, 0));
    Canvas.selectFont(Canvas.getPresetFontInfoFromHeight(12, false));
    for (int i = paintBox->HScrollBarPos(), x = 1; i < paintBox->HScrollBarPos() + paintBox->HScrollBarVisible(); ++i, ++x) {
      Canvas.drawLine(x, midY, x, midY + values[i]);
      if (i % 50 == 0) {
        Canvas.setPenColor(RGB(0, 0, 3));
        Canvas.drawLine(x, midY - 15, x, midY + 15);
        Canvas.drawTextFmt(x, h - 25, "%d", i);
        Canvas.setPenColor(RGB(3, 3, 0));
      }
    }
    Canvas.setPenColor(RGB(0, 0, 3));
    Canvas.drawLine(0, midY, w - 1, midY);
  }

};
