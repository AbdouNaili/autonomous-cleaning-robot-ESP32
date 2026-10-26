# Autonomous Floor-Cleaning Robot

A small robot that covers a room by itself and avoids obstacles. It runs on an ESP32-S3.

- It drives in a **zigzag** pattern (line by line) to cover the floor.
- **3 ultrasonic sensors** look in front, front-left and front-right.
- A **small obstacle** in front: the robot goes around it and comes back to its line.
- A **wall** in front: the robot turns into the next line.
- It builds a simple **grid map** of free and blocked cells and prints it in the Serial Monitor.
<img width="657" height="606" alt="ROBOT" src="https://github.com/user-attachments/assets/d538efc7-05ff-47ac-9bf7-b44d23bf4728" />

---

## 1. Get the code

```bash
git clone https://github.com/YOUR-USERNAME/autonomous-cleaning-robot.git
cd autonomous-cleaning-robot
```

Or click **Code → Download ZIP** on GitHub.
---

## 2. What you need

**Hardware**

- ESP32-S3 board
- 3 × HC-SR04 ultrasonic sensors
- L298N motor driver
- 2 DC motors with wheels (4-wheel chassis, the motors on each side wired together)
- 3 × Li-ion 18650 cells in series (about 12 V)
- Resistors for the sensor echo pins (see the note below)

**Software**

- Arduino IDE 2
- ESP32 boards by Espressif, **version 3.x** (*Tools → Board → Boards Manager*, search "esp32")

No extra library is needed.

---

## 3. Wiring

**Ultrasonic sensors**

| Sensor | TRIG | ECHO |
|---|---|---|
| Front | 5 | 18 |
| Front-left | 6 | 7 |
| Front-right | 8 | 9 |

The HC-SR04 ECHO pin outputs 5 V, but the ESP32-S3 pins accept only 3.3 V. Put a voltage divider on each ECHO wire, for example 1 kΩ from ECHO to the ESP32 pin and 2 kΩ from the ESP32 pin to GND.

**L298N**

| L298N | ESP32-S3 |
|---|---|
| IN1 | 47 |
| IN2 | 48 |
| ENA | 21 |
| IN3 | 36 |
| IN4 | 37 |
| ENB | 40 |
| GND | GND |

- Remove the jumpers on ENA and ENB, otherwise the speed control does not work.
- The battery goes to the L298N **12V** input. With the 5V jumper in place, the L298N **5V** pin can power the ESP32 (5V pin).

---

## 4. Adjust for your robot

The values at the top of the code depend on your motors, battery and floor:

| Setting | Default | What it does |
|---|---|---|
| `SPEED` | 180 | Motor speed (0 to 255) |
| `TURN_MS` | 200 | Time to turn 90°. Increase it if the robot turns less than 90° |
| `CELL_MS` | 1000 | Time to drive forward one cell (about 20 cm) |
| `SAFE_DISTANCE_CM` | 14 | Below this distance in front, there is an obstacle |
| `SIDE_MARGIN_CM` | 10 | Extra space needed on a side to go around |
| `MEDIAN_SAMPLES` | 10 | Measurements per sensor (the median is used) |
| `GRID_ROWS`, `GRID_COLS` | 5 × 5 | Size of the map |

---

## How it works

Every cycle the robot:

1. Measures each sensor several times and keeps the median value (this removes bad readings).
2. Classifies the situation:

| Class | Rule | Action |
|---|---|---|
| `FREE_SPACE` | front > 14 cm | Drive one cell forward |
| `AVOIDABLE` | front blocked, at least one side > 14 cm | Go around the obstacle on the free side, then come back to the line |
| `WALL_LIKE` | front and both sides blocked | Turn into the next line of the zigzag |
| `UNKNOWN` | a sensor does not answer | Stop |

Before each step forward the robot checks the front sensor again. If something appears during a manoeuvre, it stops and decides again.
---

© 2026. All rights reserved. You may read the code, but do not copy or reuse it without permission.
