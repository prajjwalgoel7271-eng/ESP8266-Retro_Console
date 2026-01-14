#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -------------------------------------------------------------------------
// Hardware Definitions
// -------------------------------------------------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C

// Pin Definitions
#define PIN_UP      12 // D6
#define PIN_DOWN    13 // D7
#define PIN_LEFT    14 // D5
#define PIN_RIGHT   3  // RX
#define PIN_SELECT  1  // TX
#define PIN_BUZZER  2  // D4

// -------------------------------------------------------------------------
// Global Objects
// -------------------------------------------------------------------------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------------------------------------------------------------
// Input System
// -------------------------------------------------------------------------
class InputSystem {
private:
    struct ButtonState {
        uint8_t pin;
        bool pressed;
        bool justPressed;
        unsigned long pressTime;
        bool longPressHandled;
    };

    ButtonState buttons[5];
    const unsigned long DEBOUNCE_MS = 20; // Simple debounce via polling interval
    const unsigned long LONG_PRESS_MS = 600;

public:
    enum ButtonID { UP = 0, DOWN, LEFT, RIGHT, SELECT };

    void init() {
        // RX/TX pins need special handling if used as GPIO
        pinMode(PIN_UP, INPUT_PULLUP);
        pinMode(PIN_DOWN, INPUT_PULLUP);
        pinMode(PIN_LEFT, INPUT_PULLUP);
        pinMode(PIN_RIGHT, FUNCTION_3); // RX as GPIO
        pinMode(PIN_RIGHT, INPUT_PULLUP);
        pinMode(PIN_SELECT, FUNCTION_3); // TX as GPIO
        pinMode(PIN_SELECT, INPUT_PULLUP);

        buttons[UP] = {PIN_UP, false, false, 0, false};
        buttons[DOWN] = {PIN_DOWN, false, false, 0, false};
        buttons[LEFT] = {PIN_LEFT, false, false, 0, false};
        buttons[RIGHT] = {PIN_RIGHT, false, false, 0, false};
        buttons[SELECT] = {PIN_SELECT, false, false, 0, false};
    }

    void update() {
        for (int i = 0; i < 5; i++) {
            bool reading = (digitalRead(buttons[i].pin) == LOW); // Active LOW
            
            buttons[i].justPressed = false;

            if (reading && !buttons[i].pressed) {
                buttons[i].pressed = true;
                buttons[i].justPressed = true;
                buttons[i].pressTime = millis();
                buttons[i].longPressHandled = false;
            } else if (!reading && buttons[i].pressed) {
                buttons[i].pressed = false;
            }
        }
    }

    bool isPressed(ButtonID id) { return buttons[id].pressed; }
    bool isJustPressed(ButtonID id) { return buttons[id].justPressed; }
    
    // Returns true ONCE when long press threshold is reached
    bool isLongPressed(ButtonID id) {
        if (buttons[id].pressed && !buttons[id].longPressHandled) {
            if (millis() - buttons[id].pressTime > LONG_PRESS_MS) {
                buttons[id].longPressHandled = true;
                return true;
            }
        }
        return false;
    }
};

InputSystem input;

// -------------------------------------------------------------------------
// Sound System
// -------------------------------------------------------------------------
class SoundSystem {
private:
    bool muted = false;
    unsigned long noteEndTime = 0;

public:
    void init() {
        pinMode(PIN_BUZZER, OUTPUT);
        digitalWrite(PIN_BUZZER, LOW);
    }

    void playTone(unsigned int frequency, unsigned long duration) {
        if (muted) return;
        tone(PIN_BUZZER, frequency, duration);
        noteEndTime = millis() + duration;
    }

    void stop() {
        noTone(PIN_BUZZER);
    }

    void toggleMute() {
        muted = !muted;
        if (muted) stop();
    }

    bool isMuted() { return muted; }

    void update() {
        if (millis() > noteEndTime) {
            // Ensure silence if needed, though tone(duration) handles it mostly
        }
    }
    
    // SFX Presets
    void sfxClick() { playTone(1000, 20); }
    void sfxSelect() { playTone(1500, 50); }
    void sfxScore() { playTone(2000, 100); }
    void sfxCollision() { playTone(150, 300); }
    void sfxGameOver() { 
        // Simple sequence handled in game logic or here if blocking is ok (it's not)
        // For now just a long tone
        playTone(100, 1000); 
    }
};

SoundSystem sound;

// -------------------------------------------------------------------------
// Game Implementations
// -------------------------------------------------------------------------

// --- Flappy Bird ---
namespace Flappy {
    float birdY, birdVy;
    const float GRAVITY = 0.25;
    const float JUMP_STR = -2.5;
    const int BIRD_X = 20;
    const int PIPE_W = 10;
    const int PIPE_GAP = 24;
    
    struct Pipe { float x; int gapY; bool active; bool passed; };
    Pipe pipes[2];

    void init() {
        birdY = SCREEN_HEIGHT / 2;
        birdVy = 0;
        pipes[0] = {SCREEN_WIDTH, 32, true, false};
        pipes[1] = {SCREEN_WIDTH + SCREEN_WIDTH/2 + PIPE_W, 32, true, false};
    }

    void update(unsigned long &score, bool &gameOver) {
        // Physics
        birdVy += GRAVITY;
        birdY += birdVy;

        // Jump
        if (input.isJustPressed(InputSystem::SELECT) || input.isJustPressed(InputSystem::UP)) {
            birdVy = JUMP_STR;
            sound.sfxClick(); // Wing sound
        }

        // Floor/Ceiling collision
        if (birdY < 0 || birdY > SCREEN_HEIGHT - 8) { // -8 for HUD/Ground
            sound.sfxCollision();
            gameOver = true;
        }

        // Pipe Logic
        for (int i = 0; i < 2; i++) {
            pipes[i].x -= 1.5; // Speed
            
            // Reset Pipe
            if (pipes[i].x < -PIPE_W) {
                pipes[i].x = SCREEN_WIDTH;
                pipes[i].gapY = random(10, SCREEN_HEIGHT - 10 - PIPE_GAP);
                pipes[i].passed = false;
            }

            // Collision
            if (pipes[i].x < BIRD_X + 4 && pipes[i].x + PIPE_W > BIRD_X) {
                if (birdY < pipes[i].gapY || birdY + 4 > pipes[i].gapY + PIPE_GAP) {
                    sound.sfxCollision();
                    gameOver = true;
                }
            }

            // Score
            if (!pipes[i].passed && pipes[i].x < BIRD_X) {
                score++;
                pipes[i].passed = true;
                sound.sfxScore();
            }
        }
    }

    void draw() {
        // Bird
        display.fillRect(BIRD_X, (int)birdY, 4, 4, SSD1306_WHITE);
        
        // Pipes
        for (int i = 0; i < 2; i++) {
            display.fillRect((int)pipes[i].x, 0, PIPE_W, pipes[i].gapY, SSD1306_WHITE);
            display.fillRect((int)pipes[i].x, pipes[i].gapY + PIPE_GAP, PIPE_W, SCREEN_HEIGHT - (pipes[i].gapY + PIPE_GAP), SSD1306_WHITE);
        }
    }
}

// --- Snake ---
namespace Snake {
    const int GRID_SIZE = 4;
    const int MAX_LEN = 128;
    struct Point { int x, y; };
    Point body[MAX_LEN];
    int length;
    Point dir;
    Point food;
    unsigned long lastMove;
    int speedMs;

    void spawnFood() {
        bool onBody;
        do {
            onBody = false;
            food.x = random(0, SCREEN_WIDTH / GRID_SIZE);
            food.y = random(2, SCREEN_HEIGHT / GRID_SIZE); // Reserve top for HUD
            for(int i=0; i<length; i++) {
                if(body[i].x == food.x && body[i].y == food.y) onBody = true;
            }
        } while(onBody);
    }

    void init() {
        length = 3;
        body[0] = {10, 10};
        body[1] = {9, 10};
        body[2] = {8, 10};
        dir = {1, 0};
        spawnFood();
        lastMove = millis();
        speedMs = 150;
    }

    void update(unsigned long &score, bool &gameOver) {
        // Input
        if (input.isJustPressed(InputSystem::UP) && dir.y == 0) dir = {0, -1};
        else if (input.isJustPressed(InputSystem::DOWN) && dir.y == 0) dir = {0, 1};
        else if (input.isJustPressed(InputSystem::LEFT) && dir.x == 0) dir = {-1, 0};
        else if (input.isJustPressed(InputSystem::RIGHT) && dir.x == 0) dir = {1, 0};

        if (millis() - lastMove > speedMs) {
            lastMove = millis();
            
            // Move Body
            for (int i = length - 1; i > 0; i--) {
                body[i] = body[i-1];
            }
            body[0].x += dir.x;
            body[0].y += dir.y;

            // Wall Collision
            if (body[0].x < 0 || body[0].x >= SCREEN_WIDTH/GRID_SIZE || 
                body[0].y < 2 || body[0].y >= SCREEN_HEIGHT/GRID_SIZE) { // Top 8px reserved
                sound.sfxCollision();
                gameOver = true;
                return;
            }

            // Self Collision
            for (int i = 1; i < length; i++) {
                if (body[0].x == body[i].x && body[0].y == body[i].y) {
                    sound.sfxCollision();
                    gameOver = true;
                    return;
                }
            }

            // Eat Food
            if (body[0].x == food.x && body[0].y == food.y) {
                score++;
                sound.sfxScore();
                if (length < MAX_LEN) length++;
                spawnFood();
                if (speedMs > 50) speedMs -= 2;
            }
        }
    }

    void draw() {
        // Food
        display.fillRect(food.x * GRID_SIZE + 1, food.y * GRID_SIZE + 1, GRID_SIZE - 2, GRID_SIZE - 2, SSD1306_WHITE); // Smaller food
        
        // Body
        for (int i = 0; i < length; i++) {
            display.fillRect(body[i].x * GRID_SIZE, body[i].y * GRID_SIZE, GRID_SIZE - 1, GRID_SIZE - 1, SSD1306_WHITE);
        }
    }
}

// --- Pong ---
namespace Pong {
    float ballX, ballY, ballVx, ballVy;
    float paddleY, aiY;
    const int PADDLE_H = 12;
    const int PADDLE_W = 3;
    
    void init() {
        ballX = SCREEN_WIDTH / 2;
        ballY = SCREEN_HEIGHT / 2;
        ballVx = 2.0;
        ballVy = 1.5;
        paddleY = SCREEN_HEIGHT / 2 - PADDLE_H / 2;
        aiY = paddleY;
    }

    void update(unsigned long &score, bool &gameOver) {
        // Player Paddle
        if (input.isPressed(InputSystem::UP)) paddleY -= 2;
        if (input.isPressed(InputSystem::DOWN)) paddleY += 2;
        
        // Clamp Player
        if (paddleY < 8) paddleY = 8;
        if (paddleY > SCREEN_HEIGHT - PADDLE_H) paddleY = SCREEN_HEIGHT - PADDLE_H;

        // AI Paddle
        if (aiY + PADDLE_H/2 < ballY) aiY += 1.5;
        if (aiY + PADDLE_H/2 > ballY) aiY -= 1.5;
        // Clamp AI
        if (aiY < 8) aiY = 8;
        if (aiY > SCREEN_HEIGHT - PADDLE_H) aiY = SCREEN_HEIGHT - PADDLE_H;

        // Ball Move
        ballX += ballVx;
        ballY += ballVy;

        // Wall Bounce (Top/Bottom)
        if (ballY <= 8 || ballY >= SCREEN_HEIGHT - 2) {
            ballVy = -ballVy;
            sound.sfxClick();
        }

        // Paddle Collision
        // Player (Left)
        if (ballX <= PADDLE_W + 2 && ballY >= paddleY && ballY <= paddleY + PADDLE_H) {
            ballVx = -ballVx * 1.1; // Speed up
            ballX = PADDLE_W + 2;
            sound.sfxClick();
        }
        // AI (Right)
        if (ballX >= SCREEN_WIDTH - PADDLE_W - 4 && ballY >= aiY && ballY <= aiY + PADDLE_H) {
            ballVx = -ballVx;
            ballX = SCREEN_WIDTH - PADDLE_W - 4;
            sound.sfxClick();
        }

        // Score / Game Over
        if (ballX < 0) {
            sound.sfxCollision();
            gameOver = true; // Player lost
        }
        if (ballX > SCREEN_WIDTH) {
            score++;
            sound.sfxScore();
            // Reset ball
            ballX = PADDLE_W + 10;
            ballY = paddleY + PADDLE_H/2;
            ballVx = 2.0;
            ballVy = 1.5;
        }
    }

    void draw() {
        // Net
        for(int i=8; i<SCREEN_HEIGHT; i+=4) display.drawPixel(SCREEN_WIDTH/2, i, SSD1306_WHITE);
        
        // Paddles
        display.fillRect(2, (int)paddleY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
        display.fillRect(SCREEN_WIDTH - 2 - PADDLE_W, (int)aiY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
        
        // Ball
        display.fillRect((int)ballX, (int)ballY, 3, 3, SSD1306_WHITE);
    }
}

// --- Brick Breaker ---
namespace Brick {
    float ballX, ballY, ballVx, ballVy;
    float paddleX;
    const int PADDLE_W = 20;
    const int PADDLE_H = 3;
    const int BRICK_W = 10;
    const int BRICK_H = 6;
    const int COLS = 12;
    const int ROWS = 8;
    bool bricks[ROWS][COLS];
    unsigned long lastRowTime;
    int speedMs;

    void spawnRow() {
        // Shift down
        for(int y=ROWS-1; y>0; y--) {
            for(int x=0; x<COLS; x++) {
                bricks[y][x] = bricks[y-1][x];
            }
        }
        // New row
        for(int x=0; x<COLS; x++) {
            bricks[0][x] = random(0, 10) > 3; // 70% chance of brick
        }
    }

    void init() {
        paddleX = SCREEN_WIDTH / 2 - PADDLE_W / 2;
        ballX = SCREEN_WIDTH / 2;
        ballY = SCREEN_HEIGHT - 10;
        ballVx = 1.5;
        ballVy = -1.5;
        speedMs = 5000; // New row every 5s
        lastRowTime = millis();
        
        // Clear bricks
        for(int y=0; y<ROWS; y++) 
            for(int x=0; x<COLS; x++) 
                bricks[y][x] = false;
        
        spawnRow();
        spawnRow();
        spawnRow();
    }

    void update(unsigned long &score, bool &gameOver) {
        // Paddle
        if (input.isPressed(InputSystem::LEFT)) paddleX -= 3;
        if (input.isPressed(InputSystem::RIGHT)) paddleX += 3;
        if (paddleX < 0) paddleX = 0;
        if (paddleX > SCREEN_WIDTH - PADDLE_W) paddleX = SCREEN_WIDTH - PADDLE_W;

        // Ball Move
        ballX += ballVx;
        ballY += ballVy;

        // Walls
        if (ballX <= 0 || ballX >= SCREEN_WIDTH) {
            ballVx = -ballVx;
            sound.sfxClick();
        }
        if (ballY <= 8) { // Top HUD
            ballVy = -ballVy;
            sound.sfxClick();
        }
        if (ballY > SCREEN_HEIGHT) {
            sound.sfxCollision();
            gameOver = true;
        }

        // Paddle Collision
        if (ballY >= SCREEN_HEIGHT - PADDLE_H - 2 && ballY < SCREEN_HEIGHT - 2) {
            if (ballX >= paddleX && ballX <= paddleX + PADDLE_W) {
                ballVy = -abs(ballVy); // Always go up
                // Angle change based on hit position
                float hitPos = (ballX - paddleX) / PADDLE_W;
                ballVx = (hitPos - 0.5) * 4.0; 
                sound.sfxClick();
            }
        }

        // Brick Collision
        int bx = ballX / BRICK_W; // Approx col
        int by = (ballY - 8) / BRICK_H; // Approx row (offset by HUD)
        
        if (by >= 0 && by < ROWS && bx >= 0 && bx < COLS) {
            if (bricks[by][bx]) {
                bricks[by][bx] = false;
                ballVy = -ballVy;
                score++;
                sound.sfxScore();
            }
        }

        // Spawn new rows
        if (millis() - lastRowTime > speedMs) {
            lastRowTime = millis();
            // Check if bottom row has bricks before shifting (Game Over)
            for(int x=0; x<COLS; x++) {
                if(bricks[ROWS-1][x]) {
                    gameOver = true;
                    sound.sfxCollision();
                }
            }
            spawnRow();
            if (speedMs > 1000) speedMs -= 50;
        }
    }

    void draw() {
        // Paddle
        display.fillRect((int)paddleX, SCREEN_HEIGHT - PADDLE_H, PADDLE_W, PADDLE_H, SSD1306_WHITE);
        
        // Ball
        display.fillRect((int)ballX, (int)ballY, 2, 2, SSD1306_WHITE);

        // Bricks
        for(int y=0; y<ROWS; y++) {
            for(int x=0; x<COLS; x++) {
                if (bricks[y][x]) {
                    display.fillRect(x * BRICK_W + 1, y * BRICK_H + 8 + 1, BRICK_W - 2, BRICK_H - 2, SSD1306_WHITE);
                }
            }
        }
    }
}

// --- Knife Thrower ---
namespace Knife {
    float angle; // Target rotation
    float knifeY;
    bool knifeFlying;
    const int RADIUS = 16;
    const int CENTER_X = SCREEN_WIDTH / 2;
    const int CENTER_Y = SCREEN_HEIGHT / 2 - 5;
    
    struct StuckKnife { float angle; };
    StuckKnife knives[20];
    int knifeCount;
    int knivesToWin;
    int level;

    void init() {
        angle = 0;
        knifeY = SCREEN_HEIGHT - 10;
        knifeFlying = false;
        knifeCount = 0;
        knivesToWin = 6;
        level = 1;
    }

    void update(unsigned long &score, bool &gameOver) {
        // Rotate Target
        angle += 0.05 + (level * 0.01);
        if (angle > 6.28) angle -= 6.28;

        // Throw
        if (!knifeFlying && input.isJustPressed(InputSystem::SELECT)) {
            knifeFlying = true;
            sound.sfxClick();
        }

        if (knifeFlying) {
            knifeY -= 4;
            if (knifeY <= CENTER_Y + RADIUS) {
                // Hit Logic
                // Check collision with existing knives
                // Current target angle is 'angle'. 
                // A knife hits at PI/2 (bottom) relative to current rotation?
                // Actually, the target rotates. The knife hits at the bottom of the circle.
                // The angle on the target that is currently at the bottom is:
                float hitAngle = angle + 1.57; // +90 deg
                if (hitAngle > 6.28) hitAngle -= 6.28;

                bool collision = false;
                for(int i=0; i<knifeCount; i++) {
                    if (abs(knives[i].angle - hitAngle) < 0.2) collision = true; // Too close
                }

                if (collision) {
                    sound.sfxCollision();
                    gameOver = true;
                } else {
                    if (knifeCount < 20) {
                        knives[knifeCount++] = {hitAngle};
                        score++;
                        sound.sfxScore();
                        
                        // Level Up
                        if (knifeCount >= knivesToWin) {
                            knifeCount = 0;
                            level++;
                            knivesToWin += 2;
                            if (knivesToWin > 15) knivesToWin = 15;
                            // Bonus score
                            score += 10;
                        }
                    }
                }
                
                // Reset knife
                knifeFlying = false;
                knifeY = SCREEN_HEIGHT - 10;
            }
        }
    }

    void draw() {
        // Target Circle
        display.drawCircle(CENTER_X, CENTER_Y, RADIUS, SSD1306_WHITE);
        
        // Stuck Knives
        for(int i=0; i<knifeCount; i++) {
            // Calculate position based on current target rotation
            float a = knives[i].angle - angle;
            int kx = CENTER_X + cos(a) * RADIUS;
            int ky = CENTER_Y + sin(a) * RADIUS;
            int kx2 = CENTER_X + cos(a) * (RADIUS + 8);
            int ky2 = CENTER_Y + sin(a) * (RADIUS + 8);
            display.drawLine(kx, ky, kx2, ky2, SSD1306_WHITE);
        }

        // Player Knife
        display.drawLine(CENTER_X, (int)knifeY, CENTER_X, (int)knifeY + 8, SSD1306_WHITE);
        display.drawLine(CENTER_X - 2, (int)knifeY + 8, CENTER_X + 2, (int)knifeY + 8, SSD1306_WHITE); // Handle
    }
}

// --- Dodge Pixels ---
namespace Dodge {
    struct Obstacle { float x, y, vy; bool active; };
    Obstacle obs[10];
    float playerX, playerY;
    float spawnRate;
    unsigned long lastSpawn;

    void init() {
        playerX = SCREEN_WIDTH / 2;
        playerY = SCREEN_HEIGHT - 10;
        spawnRate = 1000;
        lastSpawn = millis();
        for(int i=0; i<10; i++) obs[i].active = false;
    }

    void update(unsigned long &score, bool &gameOver) {
        // Move Player
        if (input.isPressed(InputSystem::LEFT)) playerX -= 2;
        if (input.isPressed(InputSystem::RIGHT)) playerX += 2;
        if (input.isPressed(InputSystem::UP)) playerY -= 2;
        if (input.isPressed(InputSystem::DOWN)) playerY += 2;
        
        // Clamp
        if (playerX < 0) playerX = 0;
        if (playerX > SCREEN_WIDTH - 4) playerX = SCREEN_WIDTH - 4;
        if (playerY < 8) playerY = 8;
        if (playerY > SCREEN_HEIGHT - 4) playerY = SCREEN_HEIGHT - 4;

        // Spawn
        if (millis() - lastSpawn > spawnRate) {
            lastSpawn = millis();
            for(int i=0; i<10; i++) {
                if (!obs[i].active) {
                    obs[i] = { (float)random(0, SCREEN_WIDTH-4), 8, (float)random(10, 30)/10.0, true };
                    break;
                }
            }
            if (spawnRate > 200) spawnRate -= 10;
        }

        // Update Obstacles
        for(int i=0; i<10; i++) {
            if (obs[i].active) {
                obs[i].y += obs[i].vy;
                
                // Collision
                if (obs[i].x < playerX + 4 && obs[i].x + 4 > playerX &&
                    obs[i].y < playerY + 4 && obs[i].y + 4 > playerY) {
                    sound.sfxCollision();
                    gameOver = true;
                }

                // Remove
                if (obs[i].y > SCREEN_HEIGHT) {
                    obs[i].active = false;
                    score++;
                    sound.sfxScore();
                }
            }
        }
    }

    void draw() {
        // Player
        display.fillRect((int)playerX, (int)playerY, 4, 4, SSD1306_WHITE);
        
        // Obstacles
        for(int i=0; i<10; i++) {
            if (obs[i].active) {
                display.drawRect((int)obs[i].x, (int)obs[i].y, 4, 4, SSD1306_WHITE);
            }
        }
    }
}

// --- Mario Runner ---
namespace Mario {
    float playerY, playerVy;
    const float GRAVITY = 0.3;
    const float JUMP = -3.5;
    const int PLAYER_X = 20;
    const int COLS = 17; // 128 / 8 + 1
    uint8_t ground[COLS];
    
    struct Entity { float x, y; int type; bool active; }; // 0=Coin, 1=Enemy
    Entity entities[5];
    
    float speed;
    float offset; // Sub-pixel scroll

    void init() {
        playerY = 32;
        playerVy = 0;
        speed = 1.5;
        offset = 0;
        for(int i=0; i<COLS; i++) ground[i] = 50; // Flat ground initially
        for(int i=0; i<5; i++) entities[i].active = false;
    }

    void update(unsigned long &score, bool &gameOver) {
        // Jump
        if (input.isJustPressed(InputSystem::SELECT) || input.isJustPressed(InputSystem::UP)) {
            // Check if grounded (simple check: y >= ground height at player x)
            int col = PLAYER_X / 8;
            if (playerY >= ground[col] - 8 - 2) {
                playerVy = JUMP;
                sound.sfxClick();
            }
        }

        // Physics
        playerVy += GRAVITY;
        playerY += playerVy;

        // Ground Collision
        int col = PLAYER_X / 8;
        if (playerY > ground[col] - 8) {
            playerY = ground[col] - 8;
            playerVy = 0;
        }
        
        // Pitfall
        if (playerY > SCREEN_HEIGHT) {
            sound.sfxCollision();
            gameOver = true;
        }

        // Scroll
        offset += speed;
        if (offset >= 8) {
            offset -= 8;
            // Shift Ground
            for(int i=0; i<COLS-1; i++) ground[i] = ground[i+1];
            
            // Generate new ground
            int prev = ground[COLS-2];
            if (random(0, 10) < 2) ground[COLS-1] = SCREEN_HEIGHT + 20; // Pit
            else {
                int h = prev + random(-4, 5);
                if (h < 32) h = 32;
                if (h > 60) h = 60;
                ground[COLS-1] = h;
            }

            // Spawn Entities
            if (ground[COLS-1] < SCREEN_HEIGHT) { // If not pit
                if (random(0, 10) < 3) { // 30% chance
                    for(int i=0; i<5; i++) {
                        if (!entities[i].active) {
                            entities[i] = { (float)(COLS-1)*8, (float)ground[COLS-1] - 8, random(0, 2), true };
                            if (entities[i].type == 0) entities[i].y -= 16; // Coin higher
                            break;
                        }
                    }
                }
            }
            
            score++;
        }

        // Update Entities
        for(int i=0; i<5; i++) {
            if (entities[i].active) {
                entities[i].x -= speed;
                
                // Collision
                if (entities[i].x < PLAYER_X + 8 && entities[i].x + 8 > PLAYER_X &&
                    entities[i].y < playerY + 8 && entities[i].y + 8 > playerY) {
                    
                    if (entities[i].type == 0) { // Coin
                        score += 10;
                        sound.sfxScore();
                        entities[i].active = false;
                    } else { // Enemy
                        sound.sfxCollision();
                        gameOver = true;
                    }
                }

                if (entities[i].x < -8) entities[i].active = false;
            }
        }
    }

    void draw() {
        // Player
        display.fillRect(PLAYER_X, (int)playerY, 8, 8, SSD1306_WHITE);
        
        // Ground
        for(int i=0; i<COLS; i++) {
            if (ground[i] < SCREEN_HEIGHT) {
                display.fillRect(i*8 - (int)offset, ground[i], 8, SCREEN_HEIGHT - ground[i], SSD1306_WHITE);
            }
        }

        // Entities
        for(int i=0; i<5; i++) {
            if (entities[i].active) {
                if (entities[i].type == 0) // Coin
                    display.drawCircle((int)entities[i].x, (int)entities[i].y, 3, SSD1306_WHITE);
                else // Enemy
                    display.fillRect((int)entities[i].x, (int)entities[i].y, 6, 6, SSD1306_WHITE); // Small box
            }
        }
    }
}

// --- Helicopter ---
namespace Heli {
    float y, vy;
    const float GRAVITY = 0.15;
    const float THRUST = -0.3;
    const int COLS = 17;
    uint8_t roof[COLS];
    uint8_t floor[COLS];
    float offset;
    float speed;

    void init() {
        y = 32;
        vy = 0;
        speed = 2.0;
        offset = 0;
        for(int i=0; i<COLS; i++) {
            roof[i] = 10;
            floor[i] = 54;
        }
    }

    void update(unsigned long &score, bool &gameOver) {
        // Input
        if (input.isPressed(InputSystem::SELECT) || input.isPressed(InputSystem::UP)) {
            vy += THRUST;
            // sound.sfxClick(); // Too noisy for continuous
        }
        vy += GRAVITY;
        y += vy;

        // Scroll
        offset += speed;
        if (offset >= 8) {
            offset -= 8;
            score++;
            
            // Shift
            for(int i=0; i<COLS-1; i++) {
                roof[i] = roof[i+1];
                floor[i] = floor[i+1];
            }
            
            // Generate
            int r = roof[COLS-2] + random(-2, 3);
            int f = floor[COLS-2] + random(-2, 3);
            
            // Constraints
            if (r < 8) r = 8; // HUD
            if (f > 63) f = 63;
            if (f - r < 20) { // Minimum gap
                if (random(0, 2) == 0) r -= 2; else f += 2;
            }
            
            roof[COLS-1] = r;
            floor[COLS-1] = f;
        }

        // Collision
        int col = 20 / 8; // Player X is fixed at 20
        // Interpolate collision? Or just check current col
        // Simple check
        if (y < roof[col] || y + 6 > floor[col]) {
            sound.sfxCollision();
            gameOver = true;
        }
    }

    void draw() {
        // Player
        display.fillRect(20, (int)y, 8, 6, SSD1306_WHITE);
        
        // Cave
        for(int i=0; i<COLS; i++) {
            int x = i*8 - (int)offset;
            display.fillRect(x, 0, 8, roof[i], SSD1306_WHITE);
            display.fillRect(x, floor[i], 8, SCREEN_HEIGHT - floor[i], SSD1306_WHITE);
        }
    }
}

// --- Tunnel Runner ---
namespace Tunnel {
    const int ROWS = 16; // 64 / 4
    uint8_t leftWall[ROWS];
    uint8_t rightWall[ROWS];
    float playerX;
    int speedMs;
    unsigned long lastMove;

    void init() {
        playerX = SCREEN_WIDTH / 2;
        speedMs = 100;
        lastMove = millis();
        for(int i=0; i<ROWS; i++) {
            leftWall[i] = 20;
            rightWall[i] = SCREEN_WIDTH - 20;
        }
    }

    void update(unsigned long &score, bool &gameOver) {
        // Input
        if (input.isPressed(InputSystem::LEFT)) playerX -= 2;
        if (input.isPressed(InputSystem::RIGHT)) playerX += 2;

        if (millis() - lastMove > speedMs) {
            lastMove = millis();
            score++;
            
            // Shift Down
            for(int i=ROWS-1; i>0; i--) {
                leftWall[i] = leftWall[i-1];
                rightWall[i] = rightWall[i-1];
            }
            
            // Generate Top
            int l = leftWall[1] + random(-4, 5);
            int r = rightWall[1] + random(-4, 5);
            
            if (l < 0) l = 0;
            if (r > SCREEN_WIDTH) r = SCREEN_WIDTH;
            if (r - l < 20) { // Min gap
                l -= 2; r += 2;
            }
            
            leftWall[0] = l;
            rightWall[0] = r;
            
            if (speedMs > 20) speedMs -= 1;
        }

        // Collision (Player is at bottom)
        int pRow = ROWS - 2;
        if (playerX < leftWall[pRow] || playerX + 4 > rightWall[pRow]) {
            sound.sfxCollision();
            gameOver = true;
        }
    }

    void draw() {
        // Player
        display.fillRect((int)playerX, SCREEN_HEIGHT - 10, 4, 4, SSD1306_WHITE);
        
        // Walls
        for(int i=0; i<ROWS; i++) {
            int y = i * 4 + 8; // Offset by HUD
            display.fillRect(0, y, leftWall[i], 4, SSD1306_WHITE);
            display.fillRect(rightWall[i], y, SCREEN_WIDTH - rightWall[i], 4, SSD1306_WHITE);
        }
    }
}

// --- Doodle Jump ---
namespace Doodle {
    float playerX, playerY, playerVy;
    const float GRAVITY = 0.2;
    const float JUMP = -4.0;
    struct Platform { float x, y; bool active; };
    Platform plats[10];
    float cameraY;
    float maxScoreY;

    void init() {
        playerX = SCREEN_WIDTH / 2;
        playerY = SCREEN_HEIGHT / 2;
        playerVy = 0;
        cameraY = 0;
        maxScoreY = 0;
        
        // Initial platforms
        for(int i=0; i<10; i++) {
            plats[i] = { (float)random(0, SCREEN_WIDTH-10), (float)(SCREEN_HEIGHT - i * 15), true };
        }
    }

    void update(unsigned long &score, bool &gameOver) {
        // Input
        if (input.isPressed(InputSystem::LEFT)) playerX -= 2;
        if (input.isPressed(InputSystem::RIGHT)) playerX += 2;
        
        // Wrap X
        if (playerX < -4) playerX = SCREEN_WIDTH;
        if (playerX > SCREEN_WIDTH) playerX = -4;

        // Physics
        playerVy += GRAVITY;
        playerY += playerVy;

        // Camera Follow
        if (playerY < SCREEN_HEIGHT / 2) {
            float diff = (SCREEN_HEIGHT / 2) - playerY;
            playerY = SCREEN_HEIGHT / 2;
            cameraY += diff;
            maxScoreY += diff;
            score = maxScoreY / 10;
            
            // Shift Platforms
            for(int i=0; i<10; i++) {
                plats[i].y += diff;
                if (plats[i].y > SCREEN_HEIGHT) {
                    plats[i].y = random(-10, 0);
                    plats[i].x = random(0, SCREEN_WIDTH-10);
                }
            }
        }

        // Collision (only falling)
        if (playerVy > 0) {
            for(int i=0; i<10; i++) {
                if (playerX + 4 > plats[i].x && playerX < plats[i].x + 10 &&
                    playerY + 8 > plats[i].y && playerY + 8 < plats[i].y + 8) {
                    playerVy = JUMP;
                    sound.sfxClick();
                }
            }
        }

        // Fall
        if (playerY > SCREEN_HEIGHT) {
            sound.sfxCollision();
            gameOver = true;
        }
    }

    void draw() {
        // Player
        display.fillRect((int)playerX, (int)playerY, 4, 8, SSD1306_WHITE);
        
        // Platforms
        for(int i=0; i<10; i++) {
            display.fillRect((int)plats[i].x, (int)plats[i].y, 10, 2, SSD1306_WHITE);
        }
    }
}

// --- Stack Builder ---
namespace Stack {
    float currentX, currentW;
    float speed;
    bool movingRight;
    float prevX, prevW;
    int stackHeight;
    
    void init() {
        currentW = 60;
        currentX = 0;
        speed = 1.5;
        movingRight = true;
        prevX = (SCREEN_WIDTH - currentW) / 2;
        prevW = currentW;
        stackHeight = 0;
    }

    void update(unsigned long &score, bool &gameOver) {
        // Move
        if (movingRight) {
            currentX += speed;
            if (currentX + currentW >= SCREEN_WIDTH) movingRight = false;
        } else {
            currentX -= speed;
            if (currentX <= 0) movingRight = true;
        }

        // Drop
        if (input.isJustPressed(InputSystem::SELECT)) {
            sound.sfxClick();
            
            // Calculate Overlap
            float left = max(currentX, prevX);
            float right = min(currentX + currentW, prevX + prevW);
            float overlap = right - left;
            
            if (overlap <= 0) {
                sound.sfxCollision();
                gameOver = true;
            } else {
                score++;
                sound.sfxScore();
                
                // Update State
                prevX = left;
                prevW = overlap;
                currentW = overlap;
                currentX = 0;
                movingRight = true;
                stackHeight++;
                speed += 0.2;
            }
        }
    }

    void draw() {
        // Previous Block (Base)
        display.fillRect((int)prevX, SCREEN_HEIGHT - 10, (int)prevW, 10, SSD1306_WHITE);
        
        // Current Block
        int y = SCREEN_HEIGHT - 20; // Always draw above base
        display.fillRect((int)currentX, y, (int)currentW, 10, SSD1306_WHITE);
        
        // Visual stack effect (lines below)
        for(int i=1; i<min(stackHeight, 5); i++) {
            display.drawRect((int)prevX, SCREEN_HEIGHT - 10 + (i*2), (int)prevW, 2, SSD1306_WHITE);
        }
    }
}

// --- Tetris ---
namespace Tetris {
    const int COLS = 10;
    const int ROWS = 20;
    const int CELL = 3; // Small cells to fit
    const int OFF_X = (SCREEN_WIDTH - (COLS * CELL)) / 2;
    const int OFF_Y = 4; // Below HUD
    
    uint16_t grid[ROWS]; // 10 bits used
    
    struct Piece { int x, y, type, rot; };
    Piece curr;
    unsigned long lastDrop;
    int dropSpeed;
    
    // Shapes (4x4 bitmasks, 4 rotations each)
    // Simplified: Just defining offsets manually or using a compact representation is better.
    // Let's use a function to get blocks for a piece/rot.
    
    void getBlocks(int type, int rot, int &x1, int &y1, int &x2, int &y2, int &x3, int &y3, int &x4, int &y4) {
        // 0=I, 1=O, 2=T, 3=S, 4=Z, 5=J, 6=L
        // Relative coords
        switch(type) {
            case 0: // I
                if(rot%2==0) { x1=0; y1=1; x2=1; y2=1; x3=2; y3=1; x4=3; y4=1; }
                else         { x1=1; y1=0; x2=1; y2=1; x3=1; y3=2; x4=1; y4=3; }
                break;
            case 1: // O
                x1=1; y1=0; x2=2; y2=0; x3=1; y3=1; x4=2; y4=1;
                break;
            case 2: // T
                if(rot==0) { x1=1; y1=0; x2=0; y2=1; x3=1; y3=1; x4=2; y4=1; }
                if(rot==1) { x1=1; y1=0; x2=1; y2=1; x3=2; y3=1; x4=1; y4=2; }
                if(rot==2) { x1=0; y1=1; x2=1; y2=1; x3=2; y3=1; x4=1; y4=2; }
                if(rot==3) { x1=1; y1=0; x2=0; y2=1; x3=1; y3=1; x4=1; y4=2; }
                break;
            // Simplified others to T for brevity if needed, but let's try to add all
            case 3: // S
                if(rot%2==0) { x1=1; y1=0; x2=2; y2=0; x3=0; y3=1; x4=1; y4=1; }
                else         { x1=1; y1=0; x2=1; y2=1; x3=2; y3=1; x4=2; y4=2; }
                break;
            case 4: // Z
                if(rot%2==0) { x1=0; y1=0; x2=1; y2=0; x3=1; y3=1; x4=2; y4=1; }
                else         { x1=2; y1=0; x2=1; y2=1; x3=2; y3=1; x4=1; y4=2; }
                break;
            case 5: // J
                if(rot==0) { x1=0; y1=0; x2=0; y2=1; x3=1; y3=1; x4=2; y4=1; }
                if(rot==1) { x1=1; y1=0; x2=2; y2=0; x3=1; y3=1; x4=1; y4=2; }
                if(rot==2) { x1=0; y1=1; x2=1; y2=1; x3=2; y3=1; x4=2; y4=2; }
                if(rot==3) { x1=1; y1=0; x2=1; y2=1; x3=0; y3=2; x4=1; y4=2; }
                break;
            case 6: // L
                if(rot==0) { x1=2; y1=0; x2=0; y2=1; x3=1; y3=1; x4=2; y4=1; }
                if(rot==1) { x1=1; y1=0; x2=1; y2=1; x3=1; y3=2; x4=2; y4=2; }
                if(rot==2) { x1=0; y1=1; x2=1; y2=1; x3=2; y3=1; x4=0; y4=2; }
                if(rot==3) { x1=0; y1=0; x2=1; y2=0; x3=1; y3=1; x4=1; y4=2; }
                break;
        }
    }

    bool checkCollision(int px, int py, int rot) {
        int x[4], y[4];
        getBlocks(curr.type, rot, x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);
        for(int i=0; i<4; i++) {
            int nx = px + x[i];
            int ny = py + y[i];
            if (nx < 0 || nx >= COLS || ny >= ROWS) return true;
            if (ny >= 0 && (grid[ny] & (1 << nx))) return true;
        }
        return false;
    }

    void placePiece() {
        int x[4], y[4];
        getBlocks(curr.type, curr.rot, x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);
        for(int i=0; i<4; i++) {
            int ny = curr.y + y[i];
            int nx = curr.x + x[i];
            if (ny >= 0) grid[ny] |= (1 << nx);
        }
    }

    void spawnPiece() {
        curr.type = random(0, 7);
        curr.rot = 0;
        curr.x = 3;
        curr.y = -2;
    }

    void init() {
        for(int i=0; i<ROWS; i++) grid[i] = 0;
        spawnPiece();
        lastDrop = millis();
        dropSpeed = 500;
    }

    void update(unsigned long &score, bool &gameOver) {
        // Rotation
        if (input.isJustPressed(InputSystem::SELECT)) {
            int nextRot = (curr.rot + 1) % 4;
            if (!checkCollision(curr.x, curr.y, nextRot)) {
                curr.rot = nextRot;
                sound.sfxClick();
            }
        }
        // Move
        if (input.isJustPressed(InputSystem::LEFT)) {
            if (!checkCollision(curr.x - 1, curr.y, curr.rot)) curr.x--;
        }
        if (input.isJustPressed(InputSystem::RIGHT)) {
            if (!checkCollision(curr.x + 1, curr.y, curr.rot)) curr.x++;
        }
        // Drop
        bool forceDrop = input.isPressed(InputSystem::DOWN);
        if (forceDrop || millis() - lastDrop > dropSpeed) {
            lastDrop = millis();
            if (!checkCollision(curr.x, curr.y + 1, curr.rot)) {
                curr.y++;
            } else {
                // Lock
                if (curr.y < 0) {
                    gameOver = true;
                    sound.sfxCollision();
                } else {
                    placePiece();
                    sound.sfxClick();
                    
                    // Clear Lines
                    for(int y=0; y<ROWS; y++) {
                        if (grid[y] == 1023) { // 1111111111
                            // Shift down
                            for(int k=y; k>0; k--) grid[k] = grid[k-1];
                            grid[0] = 0;
                            score += 10;
                            sound.sfxScore();
                            dropSpeed -= 10;
                        }
                    }
                    spawnPiece();
                }
            }
        }
    }

    void draw() {
        // Grid
        display.drawRect(OFF_X - 1, OFF_Y - 1, COLS * CELL + 2, ROWS * CELL + 2, SSD1306_WHITE);
        
        for(int y=0; y<ROWS; y++) {
            for(int x=0; x<COLS; x++) {
                if (grid[y] & (1 << x)) {
                    display.fillRect(OFF_X + x*CELL, OFF_Y + y*CELL, CELL-1, CELL-1, SSD1306_WHITE);
                }
            }
        }
        
        // Current Piece
        int x[4], y[4];
        getBlocks(curr.type, curr.rot, x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);
        for(int i=0; i<4; i++) {
            int nx = curr.x + x[i];
            int ny = curr.y + y[i];
            if (ny >= 0) {
                display.fillRect(OFF_X + nx*CELL, OFF_Y + ny*CELL, CELL-1, CELL-1, SSD1306_WHITE);
            }
        }
    }
}

// -------------------------------------------------------------------------
// Game Engine & State Machine
// -------------------------------------------------------------------------
enum GameState { STATE_MENU, STATE_PLAYING, STATE_PAUSED };
enum GameType { 
    GAME_FLAPPY, 
    GAME_SNAKE, 
    GAME_BRICK, 
    GAME_PONG,
    GAME_KNIFE,
    GAME_MARIO,
    GAME_STACK,
    GAME_DOODLE,
    GAME_DODGE,
    GAME_TUNNEL,
    GAME_TETRIS,
    GAME_HELI,
    GAME_COUNT 
};

const char* gameTitles[] = {
    "Flappy Bird", "Snake", "Brick Breaker", "Pong",
    "Knife Thrower", "Mario Runner", "Stack Builder", "Doodle Jump",
    "Dodge Pixels", "Tunnel Run", "Tetris", "Helicopter"
};

class GameEngine {
public:
    GameState currentState = STATE_MENU;
    GameType selectedGame = GAME_FLAPPY;
    int menuIndex = 0;
    
    // Shared Game Variables
    unsigned long score = 0;
    unsigned long highScore = 0;
    int difficulty = 1;

    void init() {
        if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
            for(;;); // Don't proceed, loop forever
        }
        display.clearDisplay();
        display.display();
        
        // Default high scores could be loaded from EEPROM here
    }

    void update() {
        // Global Pause Toggle
        if (currentState == STATE_PLAYING && input.isLongPressed(InputSystem::SELECT)) {
            currentState = STATE_PAUSED;
            sound.sfxSelect();
            return;
        }

        switch (currentState) {
            case STATE_MENU:    updateMenu(); break;
            case STATE_PLAYING: updateGame(); break;
            case STATE_PAUSED:  updatePause(); break;
        }
    }

    void draw() {
        display.clearDisplay();
        
        switch (currentState) {
            case STATE_MENU:    drawMenu(); break;
            case STATE_PLAYING: 
                drawGame(); 
                drawHUD();
                break;
            case STATE_PAUSED:  
                drawGame(); // Draw game behind overlay
                drawPause(); 
                break;
        }
        
        display.display();
    }

private:
    void updateMenu() {
        if (input.isJustPressed(InputSystem::UP)) {
            menuIndex--;
            if (menuIndex < 0) menuIndex = GAME_COUNT - 1;
            sound.sfxClick();
        }
        if (input.isJustPressed(InputSystem::DOWN)) {
            menuIndex++;
            if (menuIndex >= GAME_COUNT) menuIndex = 0;
            sound.sfxClick();
        }
        if (input.isJustPressed(InputSystem::SELECT)) {
            sound.sfxSelect();
            selectedGame = (GameType)menuIndex;
            startGame(selectedGame);
        }
        // Sound Toggle in Menu
        if (input.isJustPressed(InputSystem::LEFT) || input.isJustPressed(InputSystem::RIGHT)) {
             sound.toggleMute();
             sound.sfxClick();
        }
    }

    void drawMenu() {
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        
        display.println(F("== RETRO CONSOLE =="));
        display.drawLine(0, 9, 128, 9, SSD1306_WHITE);
        
        int startY = 12;
        int itemsPerPage = 5;
        int pageStart = (menuIndex / itemsPerPage) * itemsPerPage;
        
        for (int i = 0; i < itemsPerPage; i++) {
            int idx = pageStart + i;
            if (idx >= GAME_COUNT) break;
            
            if (idx == menuIndex) display.print(F("> "));
            else display.print(F("  "));
            
            display.println(gameTitles[idx]);
        }

        // Footer
        display.setCursor(0, 56);
        display.print(F("Sound: "));
        display.print(sound.isMuted() ? F("OFF") : F("ON"));
    }

    void updatePause() {
        if (input.isJustPressed(InputSystem::SELECT)) {
            // Resume
            currentState = STATE_PLAYING;
            sound.sfxSelect();
        }
        if (input.isJustPressed(InputSystem::LEFT)) {
            // Exit
            currentState = STATE_MENU;
            sound.stop();
            sound.sfxSelect();
        }
        if (input.isJustPressed(InputSystem::RIGHT)) {
            sound.toggleMute();
            sound.sfxClick();
        }
    }

    void drawPause() {
        // Dim background (simulate by drawing alternating pixels or just a box)
        display.fillRect(10, 10, 108, 44, SSD1306_BLACK);
        display.drawRect(10, 10, 108, 44, SSD1306_WHITE);
        
        display.setCursor(40, 15);
        display.print(F("PAUSED"));
        
        display.setCursor(20, 28);
        display.print(F("SEL: Resume"));
        display.setCursor(20, 38);
        display.print(F("LFT: Exit"));
        display.setCursor(20, 48);
        display.print(F("RGT: Snd "));
        display.print(sound.isMuted() ? F("OFF") : F("ON"));
    }

    void drawHUD() {
        // Top bar
        display.fillRect(0, 0, 128, 8, SSD1306_BLACK); // Clear top area
        display.drawLine(0, 8, 128, 8, SSD1306_WHITE);
        display.setCursor(2, 0);
        display.print(F("Score:"));
        display.print(score);
    }

    void startGame(GameType game) {
        currentState = STATE_PLAYING;
        score = 0;
        difficulty = 1;
        
        switch(game) {
            case GAME_FLAPPY: Flappy::init(); break;
            case GAME_SNAKE: Snake::init(); break;
            case GAME_PONG: Pong::init(); break;
            case GAME_BRICK: Brick::init(); break;
            case GAME_KNIFE: Knife::init(); break;
            case GAME_DODGE: Dodge::init(); break;
            case GAME_MARIO: Mario::init(); break;
            case GAME_HELI: Heli::init(); break;
            case GAME_TUNNEL: Tunnel::init(); break;
            case GAME_DOODLE: Doodle::init(); break;
            case GAME_STACK: Stack::init(); break;
            case GAME_TETRIS: Tetris::init(); break;
            default: break;
        }
    }

    void updateGame() {
        bool gameOver = false;
        switch(selectedGame) {
            case GAME_FLAPPY: Flappy::update(score, gameOver); break;
            case GAME_SNAKE: Snake::update(score, gameOver); break;
            case GAME_PONG: Pong::update(score, gameOver); break;
            case GAME_BRICK: Brick::update(score, gameOver); break;
            case GAME_KNIFE: Knife::update(score, gameOver); break;
            case GAME_DODGE: Dodge::update(score, gameOver); break;
            case GAME_MARIO: Mario::update(score, gameOver); break;
            case GAME_HELI: Heli::update(score, gameOver); break;
            case GAME_TUNNEL: Tunnel::update(score, gameOver); break;
            case GAME_DOODLE: Doodle::update(score, gameOver); break;
            case GAME_STACK: Stack::update(score, gameOver); break;
            case GAME_TETRIS: Tetris::update(score, gameOver); break;
            default: break;
        }

        if (gameOver) {
            sound.sfxGameOver();
            currentState = STATE_MENU; // Or a GAME_OVER state
            delay(1000); // Brief pause
        }
    }

    void drawGame() {
        switch(selectedGame) {
            case GAME_FLAPPY: Flappy::draw(); break;
            case GAME_SNAKE: Snake::draw(); break;
            case GAME_PONG: Pong::draw(); break;
            case GAME_BRICK: Brick::draw(); break;
            case GAME_KNIFE: Knife::draw(); break;
            case GAME_DODGE: Dodge::draw(); break;
            case GAME_MARIO: Mario::draw(); break;
            case GAME_HELI: Heli::draw(); break;
            case GAME_TUNNEL: Tunnel::draw(); break;
            case GAME_DOODLE: Doodle::draw(); break;
            case GAME_STACK: Stack::draw(); break;
            case GAME_TETRIS: Tetris::draw(); break;
            default: break;
        }
    }
};

GameEngine engine;

// -------------------------------------------------------------------------
// Main Arduino Loop
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    randomSeed(micros()); // Basic seeding
    
    input.init();
    sound.init();
    engine.init();
}

void loop() {
    input.update();
    sound.update();
    engine.update();
    engine.draw();
}
