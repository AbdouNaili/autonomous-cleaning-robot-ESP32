# Autonomous Floor-Cleaning Robot

A small robot that covers a room by itself and avoids obstacles. It runs on an ESP32-S3.

- It drives in a **zigzag** pattern (line by line) to cover the floor.
- **3 ultrasonic sensors** look in front, front-left and front-right.
- A **small obstacle** in front: the robot goes around it and comes back to its line.
- A **wall** in front: the robot turns into the next line.
- It builds a simple **grid map** of free and blocked cells and prints it in the Serial Monitor.

---

## 1. Get the code

```bash
git clone https://github.com/YOUR-USERNAME/autonomous-cleaning-robot.git
cd autonomous-cleaning-robot
```

Or click **Code → Download ZIP** on GitHub.

```
autonomous-cleaning-robot/
├── firmware/cleaning_robot/cleaning_robot.ino
└── README.md
```

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

Some ESP32-S3 boards (with "R8" in the name, like N16R8) use pins 35, 36 and 37 for their memory. On those boards, move IN3 and IN4 to free pins and change `RIGHT_IN3` and `RIGHT_IN4` in the code.

---

## 4. Upload

1. Open `firmware/cleaning_robot/cleaning_robot.ino` in the Arduino IDE.
2. Choose **Tools → Board → ESP32S3 Dev Module** and your port.
3. Upload, then open the Serial Monitor at **115200** baud.

**Test it on a stand first, wheels off the floor.** If a wheel turns the wrong way, swap its two IN wires (or its two motor wires).

---

## 5. Adjust for your robot

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

3. Updates the map and prints it:

```
Grid (R=robot, #=occupied, .=free, ?=unknown):
? ? ? ? ?
? ? # ? ?
. . . ? ?
. ? . ? ?
R ? . ? ?
```

Before each step forward the robot checks the front sensor again. If something appears during a manoeuvre, it stops and decides again.

**Going around an obstacle** (example on the right):

```
turn right → 1 cell → turn left → 2 cells → turn left → 1 cell → turn right
```

The robot ends 2 cells ahead, on its original line, facing the same direction.

**Zigzag:** going north, the robot turns into the line on its right. Going south, it turns left. The lines always move in the same direction across the room.

---

## Limits

- **The robot has no wheel encoders.** Its position is estimated from the time the motors run, so the map drifts. Battery level and floor type change the real distance and angle.
- The map is small (5 × 5 cells, about 1 m × 1 m). Outside it, the robot keeps moving but the map is not updated.
- Ultrasonic sensors can miss soft, thin or angled objects.
- There is no cleaning motor in the code, only the navigation.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Always `UNKNOWN` | A sensor does not answer: check its wiring and 5 V supply |
| Distances are always 0 or wrong | Check the ECHO voltage divider and the pin numbers |
| The robot turns too much or too little | Change `TURN_MS` |
| The robot does not move | Remove the ENA/ENB jumpers on the L298N and check the battery |
| Compile error on `ledcAttachChannel` | Update the ESP32 boards package to version 3.x |

---

© 2026. All rights reserved. You may read the code, but do not copy or reuse it without permission.
