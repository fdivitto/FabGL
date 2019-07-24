/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
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


#include "fabgl.h"
#include "fabutils.h"

#include "sprites.h"




using fabgl::iclamp;



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IntroScene

struct IntroScene : public Scene {

  static const int TEXTROWS = 4;
  static const int TEXT_X   = 130;
  static const int TEXT_Y   = 122;

  static int controller_; // 1 = keyboard, 2 = mouse

  int textRow_  = 0;
  int textCol_  = 0;
  int starting_ = 0;

  IntroScene()
    : Scene(0)
  {
  }

  void init()
  {
    Canvas.setBrushColor(Color::Black);
    Canvas.clear();
    Canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    Canvas.selectFont(Canvas.getPresetFontInfo(40, 14));
    Canvas.setPenColor(Color::BrightWhite);
    Canvas.setGlyphOptions(GlyphOptions().DoubleWidth(1));
    Canvas.drawText(50, 15, "SPACE INVADERS");
    Canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));

    Canvas.setPenColor(Color::Green);
    Canvas.drawText(10, 40, "ESP32 version by Fabrizio Di Vittorio");
    Canvas.drawText(105, 55, "www.fabgl.com");

    Canvas.setPenColor(Color::Yellow);
    Canvas.drawText(72, 97, "* SCORE ADVANCE TABLE *");
    Canvas.drawBitmap(TEXT_X - 20 - 2, TEXT_Y, &bmpEnemyD);
    Canvas.drawBitmap(TEXT_X - 20, TEXT_Y + 15, &bmpEnemyA[0]);
    Canvas.drawBitmap(TEXT_X - 20, TEXT_Y + 30, &bmpEnemyB[0]);
    Canvas.drawBitmap(TEXT_X - 20, TEXT_Y + 45, &bmpEnemyC[0]);

    Canvas.setBrushColor(Color::Black);

    controller_ = 0;
  }

  void update(int updateCount)
  {
    static const char * scoreText[] = {"= ? MISTERY", "= 30 POINTS", "= 20 POINTS", "= 10 POINTS" };

    if (starting_) {

      if (starting_ > 50)
        stop();

      ++starting_;
      Canvas.scroll(0, -5);

    } else {
      if (updateCount > 30 && updateCount % 5 == 0 && textRow_ < 4) {
        int x = TEXT_X + textCol_ * Canvas.getFontInfo()->width;
        int y = TEXT_Y + textRow_ * 15 - 4;
        Canvas.setPenColor(Color::White);
        Canvas.drawChar(x, y, scoreText[textRow_][textCol_]);
        ++textCol_;
        if (scoreText[textRow_][textCol_] == 0) {
          textCol_ = 0;
          ++textRow_;
        }
      }

      if (updateCount % 20 == 0) {
        Canvas.setPenColor(random(4), random(4), random(4));
        if (Keyboard.isKeyboardAvailable() && Mouse.isMouseAvailable())
          Canvas.drawText(45, 75, "Press [SPACE] or CLICK to Play");
        else if (Keyboard.isKeyboardAvailable())
          Canvas.drawText(80, 75, "Press [SPACE] to Play");
        else if (Mouse.isMouseAvailable())
          Canvas.drawText(105, 75, "Click to Play");
      }

      // handle keyboard or mouse (after two seconds)
      if (updateCount > 50) {
        if (Keyboard.isKeyboardAvailable() && Keyboard.isVKDown(fabgl::VK_SPACE))
          controller_ = 1;  // select keyboard as controller
        else if (Mouse.isMouseAvailable() && Mouse.getNextDelta(nullptr, 0) && Mouse.status().buttons.left)
          controller_ = 2;  // select mouse as controller
        starting_ = (controller_ > 0);  // start only when a controller has been selected
      }
    }
  }

  void collisionDetected(Sprite * spriteA, Sprite * spriteB, Point collisionPoint)
  {
  }

};


int IntroScene::controller_ = 0;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GameScene


struct GameScene : public Scene {

  enum SpriteType { TYPE_PLAYERFIRE, TYPE_ENEMIESFIRE, TYPE_ENEMY, TYPE_PLAYER, TYPE_SHIELD, TYPE_ENEMYMOTHER };

  struct SISprite : Sprite {
    SpriteType type;
    uint8_t    enemyPoints;
  };

  enum GameState { GAMESTATE_PLAYING, GAMESTATE_PLAYERKILLED, GAMESTATE_ENDGAME, GAMESTATE_GAMEOVER, GAMESTATE_LEVELCHANGING, GAMESTATE_LEVELCHANGED };

  static const int PLAYERSCOUNT       = 1;
  static const int SHIELDSCOUNT       = 4;
  static const int ROWENEMIESCOUNT    = 11;
  static const int PLAYERFIRECOUNT    = 1;
  static const int ENEMIESFIRECOUNT   = 1;
  static const int ENEMYMOTHERCOUNT   = 1;
  static const int SPRITESCOUNT       = PLAYERSCOUNT + SHIELDSCOUNT + 5 * ROWENEMIESCOUNT + PLAYERFIRECOUNT + ENEMIESFIRECOUNT + ENEMYMOTHERCOUNT;

  static const int ENEMIES_X_SPACE    = 16;
  static const int ENEMIES_Y_SPACE    = 10;
  static const int ENEMIES_START_X    = 0;
  static const int ENEMIES_START_Y    = 30;
  static const int ENEMIES_STEP_X     = 6;
  static const int ENEMIES_STEP_Y     = 8;

  static const int PLAYER_Y           = 170;

  static int lives_;
  static int score_;
  static int level_;
  static int hiScore_;

  SISprite * sprites_     = new SISprite[SPRITESCOUNT];
  SISprite * player_      = sprites_;
  SISprite * shields_     = player_ + PLAYERSCOUNT;
  SISprite * enemies_     = shields_ + SHIELDSCOUNT;
  SISprite * enemiesR1_   = enemies_;
  SISprite * enemiesR2_   = enemiesR1_ + ROWENEMIESCOUNT;
  SISprite * enemiesR3_   = enemiesR2_ + ROWENEMIESCOUNT;
  SISprite * enemiesR4_   = enemiesR3_ + ROWENEMIESCOUNT;
  SISprite * enemiesR5_   = enemiesR4_ + ROWENEMIESCOUNT;
  SISprite * playerFire_  = enemiesR5_ + ROWENEMIESCOUNT;
  SISprite * enemiesFire_ = playerFire_ + PLAYERFIRECOUNT;
  SISprite * enemyMother_ = enemiesFire_ + ENEMIESFIRECOUNT;

  int playerVelX_          = 0;  // used when controller is keyboard (0 = no move)
  int playerAbsX_          = -1; // used when controller is mouse (-1 = no move)
  int enemiesX_            = ENEMIES_START_X;
  int enemiesY_            = ENEMIES_START_Y;
  int enemiesDir_          = 1;
  int enemiesAlive_        = ROWENEMIESCOUNT * 5;
  SISprite * lastHitEnemy_ = nullptr;
  GameState gameState_     = GAMESTATE_PLAYING;

  bool updateScore_        = true;
  int64_t pauseStart_;

  Bitmap bmpShield[4] = { Bitmap(22, 16, shield_data, 1, RGB(0, 3, 0), true),
                          Bitmap(22, 16, shield_data, 1, RGB(0, 3, 0), true),
                          Bitmap(22, 16, shield_data, 1, RGB(0, 3, 0), true),
                          Bitmap(22, 16, shield_data, 1, RGB(0, 3, 0), true), };

  GameScene()
    : Scene(SPRITESCOUNT)
  {
  }

  ~GameScene()
  {
    delete [] sprites_;
  }

  void initEnemy(Sprite * sprite, int points)
  {
    SISprite * s = (SISprite*) sprite;
    s->addBitmap(&bmpEnemyExplosion);
    s->type = TYPE_ENEMY;
    s->enemyPoints = points;
    addSprite(s);
  }

  void init()
  {
    // setup player
    player_->addBitmap(&bmpPlayer)->addBitmap(&bmpPlayerExplosion[0])->addBitmap(&bmpPlayerExplosion[1]);
    player_->moveTo(152, PLAYER_Y);
    player_->type = TYPE_PLAYER;
    addSprite(player_);
    // setup player fire
    playerFire_->addBitmap(&bmpPlayerFire);
    playerFire_->visible = false;
    playerFire_->type = TYPE_PLAYERFIRE;
    addSprite(playerFire_);
    // setup shields
    for (int i = 0; i < 4; ++i) {
      shields_[i].addBitmap(&bmpShield[i])->moveTo(35 + i * 75, 150);
      shields_[i].isStatic = true;
      shields_[i].type = TYPE_SHIELD;
      addSprite(&shields_[i]);
    }
    // setup enemies
    for (int i = 0; i < ROWENEMIESCOUNT; ++i) {
      initEnemy( enemiesR1_[i].addBitmap(&bmpEnemyA[0])->addBitmap(&bmpEnemyA[1]), 30 );
      initEnemy( enemiesR2_[i].addBitmap(&bmpEnemyB[0])->addBitmap(&bmpEnemyB[1]), 20 );
      initEnemy( enemiesR3_[i].addBitmap(&bmpEnemyB[0])->addBitmap(&bmpEnemyB[1]), 20 );
      initEnemy( enemiesR4_[i].addBitmap(&bmpEnemyC[0])->addBitmap(&bmpEnemyC[1]), 10 );
      initEnemy( enemiesR5_[i].addBitmap(&bmpEnemyC[0])->addBitmap(&bmpEnemyC[1]), 10 );
    }
    // setup enemies fire
    enemiesFire_->addBitmap(&bmpEnemiesFire[0])->addBitmap(&bmpEnemiesFire[1]);
    enemiesFire_->visible = false;
    enemiesFire_->type = TYPE_ENEMIESFIRE;
    addSprite(enemiesFire_);
    // setup enemy mother ship
    enemyMother_->addBitmap(&bmpEnemyD)->addBitmap(&bmpEnemyExplosionRed);
    enemyMother_->visible = false;
    enemyMother_->type = TYPE_ENEMYMOTHER;
    enemyMother_->enemyPoints = 100;
    enemyMother_->moveTo(getWidth(), ENEMIES_START_Y);
    addSprite(enemyMother_);

    VGAController.setSprites(sprites_, SPRITESCOUNT);

    Canvas.setBrushColor(Color::Black);
    Canvas.clear();

    Canvas.setPenColor(Color::Green);
    Canvas.drawLine(0, 180, 320, 180);

    //Canvas.setPenColor(Color::Yellow);
    //Canvas.drawRectangle(0, 0, getWidth() - 1, getHeight() - 1);

    Canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    Canvas.selectFont(Canvas.getPresetFontInfo(80, 33));
    Canvas.setPenColor(Color::White);
    Canvas.drawText(125, 20, "WE COME IN PEACE");
    Canvas.selectFont(Canvas.getPresetFontInfo(40, 14));
    Canvas.setPenColor(0, 3, 3);
    Canvas.drawText(2, 2, "SCORE");
    Canvas.setPenColor(0, 0, 3);
    Canvas.drawText(254, 2, "HI-SCORE");
    Canvas.setPenColor(3, 3, 3);
    Canvas.drawTextFmt(254, 181, "Level %02d", level_);

    if (IntroScene::controller_ == 2) {
      // setup mouse controller
      Mouse.setSampleRate(40);  // reduce number of samples from mouse to reduce delays
      Mouse.setupAbsolutePositioner(getWidth() - player_->getWidth(), 0, false, false, nullptr); // take advantage of mouse acceleration
    }

    showLives();
  }

  void drawScore()
  {
    Canvas.setPenColor(3, 3, 3);
    Canvas.drawTextFmt(2, 14, "%05d", score_);
    if (score_ > hiScore_)
      hiScore_ = score_;
    Canvas.setPenColor(3, 3, 3);
    Canvas.drawTextFmt(266, 14, "%05d", hiScore_);
  }

  void moveEnemy(SISprite * enemy, int x, int y)
  {
    if (enemy->visible) {
      enemy->moveTo(x, y);
      enemy->setFrame(enemy->getFrameIndex() ? 0 : 1);
      updateSprite(enemy);
      if (y >= PLAYER_Y) {
        // enemies reach earth!
        gameState_ = GAMESTATE_ENDGAME;
      }
    }
  }

  void gameOver()
  {
    // disable enemies drawing, so text can be over them
    for (int i = 0; i < ROWENEMIESCOUNT * 5; ++i)
      enemies_[i].allowDraw = false;
    // show game over
    Canvas.setPenColor(0, 3, 0);
    Canvas.setBrushColor(0, 0, 0);
    Canvas.fillRectangle(80, 60, 240, 130);
    Canvas.drawRectangle(80, 60, 240, 130);
    Canvas.setGlyphOptions(GlyphOptions().DoubleWidth(1));
    Canvas.setPenColor(3, 3, 3);
    Canvas.drawText(90, 80, "GAME OVER");
    Canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));
    Canvas.setPenColor(0, 3, 0);
    if (IntroScene::controller_ == 1)
      Canvas.drawText(110, 100, "Press [SPACE]");
    else if (IntroScene::controller_ == 2)
      Canvas.drawText(93, 100, "Click to continue");
    // change state
    gameState_ = GAMESTATE_GAMEOVER;
    level_ = 1;
    lives_ = 3;
    score_ = 0;
  }

  void levelChange()
  {
    ++level_;
    // show game over
    Canvas.setPenColor(0, 3, 0);
    Canvas.drawRectangle(80, 80, 240, 110);
    Canvas.setGlyphOptions(GlyphOptions().DoubleWidth(1));
    Canvas.drawTextFmt(105, 88, "LEVEL %d", level_);
    Canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));
    // change state
    gameState_  = GAMESTATE_LEVELCHANGED;
    pauseStart_ = esp_timer_get_time();
  }

  void update(int updateCount)
  {
    if (updateScore_) {
      updateScore_ = false;
      drawScore();
    }

    if (gameState_ == GAMESTATE_PLAYING || gameState_ == GAMESTATE_PLAYERKILLED) {

      // move enemies and shoot
      if ((updateCount % max(3, 21 - level_ * 2)) == 0) {
        // handle enemy explosion
        if (lastHitEnemy_) {
          lastHitEnemy_->visible = false;
          lastHitEnemy_ = nullptr;
        }
        // handle enemies movement
        enemiesX_ += enemiesDir_ * ENEMIES_STEP_X;
        for (int i = 0; i < ROWENEMIESCOUNT; ++i) {
          moveEnemy(&enemiesR1_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 0 * ENEMIES_Y_SPACE);
          moveEnemy(&enemiesR2_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 1 * ENEMIES_Y_SPACE);
          moveEnemy(&enemiesR3_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 2 * ENEMIES_Y_SPACE);
          moveEnemy(&enemiesR4_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 3 * ENEMIES_Y_SPACE);
          moveEnemy(&enemiesR5_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 4 * ENEMIES_Y_SPACE);
        }
        bool leftSide  = enemiesX_ <= 0;
        bool rightSide = enemiesX_ >= getWidth() - ROWENEMIESCOUNT * ENEMIES_X_SPACE;
        if (rightSide || leftSide) {
          if (enemiesDir_ == 0) {
            enemiesDir_ = leftSide ? 1 : -1;
          } else {
            enemiesDir_ = 0;
            enemiesY_ += ENEMIES_STEP_Y;
          }
        }
        // handle enemies fire generation
        if (!enemiesFire_->visible) {
          int shottingEnemy = random(enemiesAlive_);
          for (int i = 0, a = 0; i < ROWENEMIESCOUNT * 5; ++i) {
            if (enemies_[i].visible) {
              if (a == shottingEnemy) {
                enemiesFire_->x = enemies_[i].x + enemies_[i].getWidth() / 2;
                enemiesFire_->y = enemies_[i].y + enemies_[i].getHeight() / 2;
                enemiesFire_->visible = true;
                break;
              }
              ++a;
            }
          }
        }
      }

      if (gameState_ == GAMESTATE_PLAYERKILLED) {
        // animate player explosion or restart playing other lives
        if ((updateCount % 20) == 0) {
          if (player_->getFrameIndex() == 1)
            player_->setFrame(2);
          else {
            player_->setFrame(0);
            gameState_ = GAMESTATE_PLAYING;
          }
        }
      } else if (IntroScene::controller_ == 1 && playerVelX_ != 0) {
        // move player using Keyboard
        player_->x += playerVelX_;
        player_->x = iclamp(player_->x, 0, getWidth() - player_->getWidth());
        updateSprite(player_);
      } else if (IntroScene::controller_ == 2 && playerAbsX_ != -1) {
        // move player using Mouse
        player_->x = playerAbsX_;
        playerAbsX_ = -1;
        updateSprite(player_);
      }

      // move player fire
      if (playerFire_->visible) {
        playerFire_->y -= 3;
        if (playerFire_->y < ENEMIES_START_Y)
          playerFire_->visible = false;
        else
          updateSpriteAndDetectCollisions(playerFire_);
      }

      // move enemies fire
      if (enemiesFire_->visible) {
        enemiesFire_->y += 2;
        enemiesFire_->setFrame( enemiesFire_->getFrameIndex() ? 0 : 1 );
        if (enemiesFire_->y > PLAYER_Y + player_->getHeight())
          enemiesFire_->visible = false;
        else
          updateSpriteAndDetectCollisions(enemiesFire_);
      }

      // move enemy mother ship
      if (enemyMother_->visible && enemyMother_->getFrameIndex() == 0) {
        enemyMother_->x -= 1;
        if (enemyMother_->x < -enemyMother_->getWidth())
          enemyMother_->visible = false;
        else
          updateSprite(enemyMother_);
      }

      // start enemy mother ship
      if ((updateCount % 800) == 0) {
        enemyMother_->x = getWidth();
        enemyMother_->setFrame(0);
        enemyMother_->visible = true;
      }

      // handle fire and movement from controller
      if (IntroScene::controller_ == 1) {
        // KEYBOARD controller
        if (Keyboard.isVKDown(fabgl::VK_LEFT))
          playerVelX_ = -1;
        else if (Keyboard.isVKDown(fabgl::VK_RIGHT))
          playerVelX_ = +1;
        else
          playerVelX_ = 0;
        if (Keyboard.isVKDown(fabgl::VK_SPACE) && !playerFire_->visible)  // fire?
          playerFire_->moveTo(player_->x + 7, player_->y - 1)->visible = true;
      } else if (IntroScene::controller_ == 2) {
        // MOUSE controller
        if (Mouse.deltaAvailable()) {
          MouseDelta delta;
          Mouse.getNextDelta(&delta);
          Mouse.updateAbsolutePosition(&delta);
          playerAbsX_ = Mouse.status().X;
          if (delta.buttons.left && !playerFire_->visible)    // fire?
            playerFire_->moveTo(player_->x + 7, player_->y - 1)->visible = true;
        }
      }
    }

    if (gameState_ == GAMESTATE_ENDGAME)
      gameOver();

    if (gameState_ == GAMESTATE_LEVELCHANGING)
      levelChange();

    if (gameState_ == GAMESTATE_LEVELCHANGED && esp_timer_get_time() >= pauseStart_ + 2500000)
      stop(); // restart from next level

    if (gameState_ == GAMESTATE_GAMEOVER) {

      // animate player burning
      if ((updateCount % 20) == 0)
        player_->setFrame( player_->getFrameIndex() == 1 ? 2 : 1);

      // wait for SPACE or click from controller
      if ((IntroScene::controller_ == 1 && Keyboard.isVKDown(fabgl::VK_SPACE)) ||
          (IntroScene::controller_ == 2 && Mouse.getNextDelta(nullptr, 0) && Mouse.status().buttons.left))
        stop();

    }

    VGAController.refreshSprites();
  }

  void damageShield(SISprite * shield, Point collisionPoint)
  {
    uint8_t * data = (uint8_t*) shield->getFrame()->data;
    int x = collisionPoint.X - shield->x;
    int y = collisionPoint.Y - shield->y;
    for (int i = 0; i < 64; ++i) {
      int px = iclamp(x + random(-4, 5), 0, shield->getWidth() - 1);
      int py = iclamp(y + random(-4, 5), 0, shield->getHeight() - 1);
      *(data + px + shield->getWidth() * py) = 0;
    }
  }

  void showLives()
  {
    Canvas.fillRectangle(1, 181, 100, 195);
    Canvas.setPenColor(Color::White);
    Canvas.drawTextFmt(5, 181, "%d", lives_);
    for (int i = 0; i < lives_; ++i)
      Canvas.drawBitmap(15 + i * (bmpPlayer.width + 5), 183, &bmpPlayer);
  }

  void collisionDetected(Sprite * spriteA, Sprite * spriteB, Point collisionPoint)
  {
    SISprite * sA = (SISprite*) spriteA;
    SISprite * sB = (SISprite*) spriteB;
    if (!lastHitEnemy_ && sA->type == TYPE_PLAYERFIRE && sB->type == TYPE_ENEMY) {
      // player fire hits an enemy
      sA->visible = false;
      sB->setFrame(2);
      lastHitEnemy_ = sB;
      --enemiesAlive_;
      score_ += sB->enemyPoints;
      updateScore_ = true;
      if (enemiesAlive_ == 0)
        gameState_ = GAMESTATE_LEVELCHANGING;
    }
    if (sB->type == TYPE_SHIELD) {
      // something hits a shield
      sA->visible = false;
      damageShield(sB, collisionPoint);
      sB->allowDraw = true;
    }
    if (gameState_ == GAMESTATE_PLAYING && sA->type == TYPE_ENEMIESFIRE && sB->type == TYPE_PLAYER) {
      // enemies fire hits player
      --lives_;
      gameState_ = lives_ ? GAMESTATE_PLAYERKILLED : GAMESTATE_ENDGAME;
      player_->setFrame(1);
      showLives();
    }
    if (sB->type == TYPE_ENEMYMOTHER) {
      // player fire hits enemy mother ship
      sA->visible = false;
      sB->setFrame(1);
      lastHitEnemy_ = sB;
      score_ += sB->enemyPoints;
      updateScore_ = true;
    }
  }

};

int GameScene::hiScore_ = 0;
int GameScene::level_   = 1;
int GameScene::lives_   = 3;
int GameScene::score_   = 0;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void setup()
{
  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  VGAController.begin();
  VGAController.setResolution(VGA_320x200_75Hz);

  // adjust this to center screen in your monitor
  VGAController.moveScreen(20, -2);
}


void loop()
{
  if (GameScene::level_ == 1) {
    IntroScene introScene;
    introScene.start();
  }
  GameScene gameScene;
  gameScene.start();
}
