# 🎮 ESP8266 OLED Retro Arcade Console

A **multi‑game retro arcade console** built using **ESP8266 + 128×64 OLED**, featuring **12 classic mini‑games**, sound effects, pause system, and a clean menu UI.

> Built for learning and fun.

---

## ✨ Features

* 📟 128×64 SSD1306 OLED display
* 🎮 **12 playable games**
* 🔊 Buzzer sound effects + mute toggle
* ⏸️ Long‑press pause system
* 🧠 Modular game engine (easy to add games)
* 🎛️ Button‑based controls
* ⚡ Optimized for ESP8266 memory limits

---

## 🕹️ Games Included

1. Flappy Bird
2. Snake
3. Brick Breaker
4. Pong
5. Knife Thrower
6. Mario Runner
7. Stack Builder
8. Doodle Jump
9. Dodge Pixels
10. Tunnel Runner
11. Tetris
12. Helicopter

---

## 🧰 Hardware Required

| Component                  | Quantity  |
| -------------------------- | --------- |
| ESP8266 (NodeMCU / ESP‑12) | 1         |
| 0.96" OLED (SSD1306, I2C)  | 1         |
| Push Buttons               | 5         |
| Active / Passive Buzzer    | 1         |
| Breadboard + Jumper Wires  | As needed |

---

## 🔌 Pin Connections (ESP8266)

| Function | ESP8266 Pin | Label |
| -------- | ----------- | ----- |
| UP       | GPIO12      | D6    |
| DOWN     | GPIO13      | D7    |
| LEFT     | GPIO14      | D5    |
| RIGHT    | GPIO3       | RX    |
| SELECT   | GPIO1       | TX    |
| Buzzer   | GPIO2       | D4    |
| OLED SDA | GPIO4       | D2    |
| OLED SCL | GPIO5       | D1    |

> ⚠️ RX/TX are reused as buttons — **Serial Monitor must be disconnected** while playing.

---

## 🧑‍💻 Software Setup

### 1️⃣ Install Arduino IDE

Download from:
👉 [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)

---

### 2️⃣ Add ESP8266 Board Package

1. Open **Arduino IDE**
2. Go to **File → Preferences**
3. In **Additional Board Manager URLs**, paste:

```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

4. Click **OK**

---

### 3️⃣ Install ESP8266 Boards

1. Go to **Tools → Board → Boards Manager**
2. Search for **ESP8266**
3. Install **"esp8266 by ESP8266 Community"**

---

### 4️⃣ Select Board & Port

* **Board**: NodeMCU 1.0 (ESP‑12E Module)
* **Upload Speed**: 921600 (or 115200 if unstable)
* **CPU Frequency**: 80 MHz
* **Flash Size**: 4MB
* **Port**: COMx

---

### 5️⃣ Install Required Libraries

Go to **Sketch → Include Library → Manage Libraries** and install:

* `Adafruit GFX Library`
* `Adafruit SSD1306`

---

## 🚀 Uploading the Code

1. Open `retroconsole8266.ino`
2. Connect ESP8266 via USB
3. Click **Upload**
4. Disconnect Serial Monitor (important)
5. Power on and enjoy 🎉

---

## 🎮 Controls

| Button              | Action                    |
| ------------------- | ------------------------- |
| UP / DOWN           | Navigate / Move           |
| LEFT / RIGHT        | Move / Toggle mute (menu) |
| SELECT              | Confirm / Jump / Action   |
| SELECT (Long Press) | Pause Game                |

---

## 🔇 Sound Control

* Toggle **Mute/Unmute** using **LEFT or RIGHT** in the menu
* Sound auto‑disables when muted

---

## 🧠 Code Structure

```
retroconsole8266.ino
├── InputSystem
├── SoundSystem
├── GameEngine
├── Individual Game Namespaces
│   ├── Flappy
│   ├── Snake
│   ├── Tetris
│   ├── Mario
│   └── ...
```

Each game has:

* `init()`
* `update(score, gameOver)`
* `draw()`

---

## 🛠️ Known Limitations

* No EEPROM high‑score saving (yet)
* RX/TX buttons disable Serial Monitor
* OLED resolution limits graphics

---

## 🌱 Future Improvements

* EEPROM / Flash high score saving
* Sprite‑based graphics
* Battery + power management
* External controller support

---

## 📜 License

MIT License — free to use, modify, and share.

---

## 🙌 Credits

Designed & Developed by **Prajjwal**
ESP8266 • Arduino • Adafruit Libraries

If you like this project, ⭐ it on GitHub 
