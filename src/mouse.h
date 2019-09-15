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
 * @brief This file contains fabgl::MouseClass definition and the Mouse instance.
 */


#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "ps2device.h"
#include "fabui.h"


namespace fabgl {




/**
 * @brief Describes mouse movement and buttons status.
 */
struct MouseDelta {
  int16_t      deltaX;             /**< Horizontal movement since last report. Moving to the right generates positive values. */
  int16_t      deltaY;             /**< Vertical movement since last report. Upward movement generates positive values. */
  int8_t       deltaZ;             /**< Scroll wheel movement since last report. Downward movement genrates positive values. */
  MouseButtons buttons;            /**< Mouse buttons status. */
  uint8_t      overflowX : 1;      /**< Contains 1 when horizontal overflow has been detected. */
  uint8_t      overflowY : 1;      /**< Contains 1 when vertical overflow has been detected. */
};


/** \ingroup Enumerations
 * @brief Describes mouse type.
 */
enum MouseType {
  LegacyMouse,    /**< Legacy PS2 mouse with three buttons, X and Y movements. */
  Intellimouse,   /**< Microsoft Intellimouse compatible with three buttons, X and Y movements and a scroll wheel. */
};


/**
 * @brief The PS2 Mouse controller class.
 *
 * MouseClass connects to one port of the PS2 Controller class (fabgl::PS2ControllerClass) to decode and get mouse movements.<br>
 * At the moment MouseClass supports standard PS/2 mouse (X and Y axis with three buttons) and Microsoft Intellimouse compatible mouse
 * (X and Y axis, scroll wheel with three buttons).<br>
 * MouseClass allows to set movement parameters, like sample rate, resolution and scaling.<br>
 * The PS2 controller uses ULP coprocessor and RTC slow memory to communicate with the PS2 device.<br>
 * <br>
 * Applications do not need to create an instance of MouseClass because an instance named Mouse is created automatically.<br>
 * Because fabgl::PS2ControllerClass supports up to two PS/2 ports, it is possible to have connected two PS/2 devices. The most common
 * configuration is Keyboard on port 0 and Mouse on port 1. However you may have two mice connected at the same time using the Mouse
 * instance on port 0 and creating a new one on port 1.<br>
 *
 * Example:
 *
 *     // Setup pins GPIO26 for CLK and GPIO27 for DATA
 *     Mouse.begin(GPIO_NUM_26, GPIO_NUM_27);
 *
 *     if (Mouse.deltaAvailable()) {
 *       MouseDelta mouseDelta;
 *       Mouse.getNextDelta(&mouseDelta);
 *
 *       Serial.printf("deltaX = %d  deltaY = %d  deltaZ = %d  leftBtn = %d  midBtn = %d  rightBtn = %d\n",
 *                     mouseDelta.deltaX, mouseDelta.deltaY, mouseDelta.deltaZ,
 *                     mouseDelta.leftButton, mouseDelta.middleButton, mouseDelta.rightButton);
 *     }
 *
 */
class MouseClass : public PS2DeviceClass {

public:

  MouseClass();

  ~MouseClass();

  /**
   * @brief Initializes MouseClass specifying CLOCK and DATA GPIOs.
   *
   * A reset command (MouseClass.reset() method) is automatically sent to the mouse.<br>
   * This method also initializes the PS2ControllerClass to use port 0 only.
   *
   * @param clkGPIO The GPIO number of Clock line
   * @param dataGPIO The GPIO number of Data line
   *
   * Example:
   *
   *     // Setup pins GPIO26 for CLK and GPIO27 for DATA
   *     Mouse.begin(GPIO_NUM_26, GPIO_NUM_27);
   */
  void begin(gpio_num_t clkGPIO, gpio_num_t dataGPIO);

  /**
   * @brief Initializes MouseClass without initializing the PS/2 controller.
   *
   * A reset command (MouseClass.reset() method) is automatically sent to the mouse.<br>
   * This method does not initialize the PS2ControllerClass.
   *
   * @param PS2Port The PS/2 port to use (0 or 1).
   *
   * Example:
   *
   *     // Setup pins GPIO26 for CLK and GPIO27 for DATA on port 0
   *     PS2Controller.begin(GPIO_NUM_26, GPIO_NUM_27); // clk, dat
   *     Mouse.begin(0); // port 0
   */
  void begin(int PS2Port);

  /**
   * @brief Sends a Reset command to the mouse.
   *
   * @return True if the mouse is correctly initialized.
   */
  bool reset();

  /**
   * @brief Checks if mouse has been detected and correctly initialized.
   *
   * isMouseAvailable() returns a valid value only after MouseClass.begin() or MouseClass.reset() has been called.
   *
   * @return True if the mouse is correctly initialized.
   */
  bool isMouseAvailable() { return m_mouseAvailable; }

  /**
   * @brief Determines the number of mouse movements available in the queue.
   *
   * @return The number of mouse movements (deltas) available to read.
   */
  int deltaAvailable();

  /**
   * @brief Gets a mouse movement from the queue.
   *
   * @param delta Pointer to MouseDelta structure to be filled with mouse movement. May be nullptr if not required (in this case only Status().buttons is updated.
   * @param timeOutMS Timeout in milliseconds. -1 means no timeout (infinite time).
   * @param requestResendOnTimeOut If true and timeout has expired then asks the mouse to resend the mouse movement.
   *
   * @return True if the mouse movement structure has been filled. False when there is no data in the queue.
   *
   * Example:
   *
   *     MouseDelta mouseDelta;
   *     Mouse.getNextDelta(&mouseDelta);
   */
  bool getNextDelta(MouseDelta * delta, int timeOutMS = -1, bool requestResendOnTimeOut = false);

  /**
   * @brief Sets the maximum rate of mouse movements reporting.
   *
   * The default sample rate is 60 samples/sec.
   *
   * @param value Sample rate as samples per second. Valid values: 10, 20, 40, 60, 80, 100, and 200.
   *
   * @return True if command has been successfully delivered to the mouse.
   */
  bool setSampleRate(int value) { return send_cmdSetSampleRate(value); }

  /**
   * @brief Sets the resolution.
   *
   * Resolution is the amount by which the movement counters are incremented/decremented measured as counts per millimeter.<br>
   * The default resolution is 4 counts/mm.
   *
   * @param value Resolution encoded as follows: 0 = 1 count/mm (25 dpi), 1 = 2 counts/mm (50 dpi), 2 = 4 counts/mm (100 dpi), 3 = 8 counts/mm (200 dpi).
   *
   * @return True if command has been successfully delivered to the mouse.
   */
  bool setResolution(int value) { return send_cmdSetResolution(value); }

  /**
   * @brief Sets the scaling.
   *
   * The default scaling is 1:1.
   *
   * @param value Scaling encoded as follows: 1 = 1:1, 2 = 1:2.
   *
   * @return True if command has been successfully delivered to the mouse.
   */
  bool setScaling(int value) { return send_cmdSetScaling(value); }

  /**
   * @brief Initializes absolute position handler.
   *
   * Use this method to specify the absolute mouse area inside the rectangle (0, 0) to (width - 1, height - 1).<br>
   * Optinally this method creates a queue that stores absolute positions generated by updateAbsolutePosition().<br>
   * This method must be called one time to initialize absolute positioning.<br>
   *
   * @param width Absolute mouse area width. Mouse can travel from 0 up to width - 1.
   * @param height Absolute mouse area height. Mouse can travel from 0 up to height - 1.
   * @param createAbsolutePositionsQueue If true a queue of absolute positions is created.
   * @param updateVGAController If true VGA controller mouse pointer is automatically updated.
   * @param app Optional fabgl::uiApp where to send mouse events.
   *
   * Example:
   *
   *     Mouse.setupAbsolutePositioner(Canvas.getWidth(), Canvas.getHeight(), true, true, nullptr);
   */
  void setupAbsolutePositioner(int width, int height, bool createAbsolutePositionsQueue, bool updateVGAController, uiApp * app);

  /**
   * @brief Sets current UI app
   *
   * @param app The UI app where to send mouse events
   */
  void setUIApp(uiApp * app) { m_uiApp = app; }

  /**
   * @brief Terminates absolute position handler.
   */
  void terminateAbsolutePositioner();

  /**
   * @brief Updates absolute position from the specified mouse delta event.
   *
   * This method updates absolute mouse position, mouse wheel and buttons status.<br>
   * In order to improve quality of acceleration it is important to call updateAbsolutePosition() often and at constant frequency.<br>
   * updateAbsolutePosition() is automatically executed when updateVGAController or createAbsolutePositionsQueue parameters of MouseClass.setupAbsolutePositioner() is true.<br>
   *
   * @param delta Mouse event to process.
   *
   * Example:
   *
   *     // move a sprite (previously defined) at mouse absolute position
   *     void loop() {
   *       MouseDelta delta;
   *       if (getNextDelta(&delta)) {
   *         Mouse.updateAbsolutePosition(&delta);
   *         mouseSprite.moveTo(Mouse.position().X, Mouse.position().Y);
   *       }
   *     }
   */
  void updateAbsolutePosition(MouseDelta * delta);

  /**
   * @brief Gets or sets current mouse status.
   */
  MouseStatus & status() { return m_status; }

  /**
   * @brief Gets the number of available mouse status.
   *
   * Mouse status contains absolute mouse position, scroll wheel delta and buttons status.<br>
   * Mouse status queue is populated only when createAbsolutePositionsQueue parameter of MouseClass.setupAbsolutePositioner() is true.
   *
   * @return Number of available mouse status.
   */
  int availableStatus();

  /**
   * @brief Gets the next status from the status queue.
   *
   * Mouse status contains absolute mouse position, scroll wheel delta and buttons status.<br>
   * Mouse status queue is populated only when createAbsolutePositionsQueue parameter of MouseClass.setupAbsolutePositioner() is true.
   *
   * @param timeOutMS Timeout in milliseconds to wait for a new status. -1 = wait forever.
   *
   * @return The next status in queue.
   *
   * Example:
   *
   *     // wait for events from mouse and draw a pixel at the mouse position if left button is down
   *     MouseStatus status = Mouse.getNextStatus();
   *     if (status.buttons.left) {
   *       Canvas.setPenColor(Color::BrightRed);
   *       Canvas.setPixel(status.X, status.Y);
   *     }
   */
  MouseStatus getNextStatus(int timeOutMS = -1);

  /**
   * @brief Gets or set mouse movement acceleration factor.
   *
   * Mouse movement acceleration is calculated in MouseClass.updateAbsolutePosition() and depends by the acceleration factor and time of call.<br>
   * Lower values generate little acceleration, high values generate a lot of acceleration.<br>
   * Suggested range is 0 ... 2000. Default value is 180.
   */
  int & movementAcceleration() { return m_movementAcceleration; }

  /**
   * @brief Gets or sets wheel acceleration factor.
   *
   * Wheel acceleration is calculated in MouseClass.updateAbsolutePosition() and depends by the acceleration factor and time of call.<br>
   * Lower values generate little acceleration, high values generate a lot of acceleration.<br>
   * Suggested range is 0 ... 100000. Default value is 60000.
   */
  int & wheelAcceleration()    { return m_wheelAcceleration; }


private:

  int getPacketSize();
  static void absoluteUpdateTimerFunc(TimerHandle_t xTimer);


  bool          m_mouseAvailable;
  MouseType     m_mouseType;

  // absolute position support
  Size          m_area;
  MouseStatus   m_status;
  MouseStatus   m_prevStatus;
  int64_t       m_prevDeltaTime;
  int           m_movementAcceleration;  // reasonable values: 0...2000
  int           m_wheelAcceleration;     // reasonable values: 0...100000
  TimerHandle_t m_absoluteUpdateTimer;
  QueueHandle_t m_absoluteQueue;         // a queue of messages generated by updateAbsolutePosition()
  bool          m_updateVGAController;

  uiApp *       m_uiApp;
};



} // end of namespace



extern fabgl::MouseClass Mouse;






