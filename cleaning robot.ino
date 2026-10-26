/*
 * Autonomous floor-cleaning robot (ESP32-S3)
 *
 * The robot covers the floor in a zigzag (boustrophedon) pattern:
 *   - path clear              -> move forward one cell
 *   - small obstacle in front -> go around it and come back to the same line
 *   - wall in front           -> turn into the next line (U-turn)
 *
 * Sensors : 3 x HC-SR04 ultrasonic (front, front-left, front-right)
 * Motors  : 2 DC motors with an L298N driver
 * Map     : small occupancy grid, printed in the Serial Monitor (115200 baud)
 *
 * The position in the grid is estimated from the commands only (no encoders),
 * so the map drifts over time.
 *
 * Needs the ESP32 Arduino core version 3.x.
 */

// ---------------------------------------------------------------- pins

// Ultrasonic sensors
const int TRIG_FRONT = 5;
const int ECHO_FRONT = 18;
const int TRIG_LEFT = 6;
const int ECHO_LEFT = 7;
const int TRIG_RIGHT = 8;
const int ECHO_RIGHT = 9;

// L298N motor driver
const int LEFT_IN1 = 47;
const int LEFT_IN2 = 48;
const int LEFT_EN = 21;   // PWM (ENA)
const int RIGHT_IN3 = 36;
const int RIGHT_IN4 = 37;
const int RIGHT_EN = 40;  // PWM (ENB)

// ---------------------------------------------------------------- settings

// Motors
const int PWM_FREQ = 30000;
const int PWM_RES = 8;        // 8 bits: speed 0..255
const int PWM_CH_LEFT = 0;
const int PWM_CH_RIGHT = 1;
const int SPEED = 180;
const unsigned long TURN_MS = 200;   // time for a 90 degree turn (adjust on your floor)
const unsigned long CELL_MS = 1000;  // time to drive one cell forward (adjust)

// Sensors
const float SOUND_SPEED_CM_PER_US = 0.034f;
const unsigned long PULSE_TIMEOUT_US = 40000UL;  // HC-SR04 answers within 38 ms even with no obstacle
const int MEDIAN_SAMPLES = 10;
const int SAMPLE_DELAY_MS = 10;
const int SENSOR_GAP_MS = 50;       // pause between two sensors to avoid echo interference

// Obstacle detection
const float SAFE_DISTANCE_CM = 14.0f;
const float SIDE_MARGIN_CM = 10.0f;  // a side is "clear" above SAFE_DISTANCE + margin

// Grid
const int GRID_ROWS = 5;
const int GRID_COLS = 5;          // one cell = the distance driven in CELL_MS (about 20 cm)

// ---------------------------------------------------------------- state

enum Cell { CELL_FREE = 0, CELL_OCCUPIED = 1, CELL_UNKNOWN = 2 };
enum ObstacleClass { FREE_SPACE, AVOIDABLE, WALL_LIKE, UNKNOWN };
enum Direction { NORTH, EAST, SOUTH, WEST };

uint8_t grid[GRID_ROWS][GRID_COLS];
int robotRow = 0;  // start in the bottom-left cell, facing north
int robotCol = 0;
Direction robotDir = NORTH;

// ---------------------------------------------------------------- function list

float readDistance(int trigPin, int echoPin);
float measureOnce(int trigPin, int echoPin);
ObstacleClass classify(float front, float left, float right);
const char *className(ObstacleClass c);

bool stepForward();
void goAround(bool viaRight);
void changeLine(bool toRight);
void turn(bool right);

void motorsStop();
void motorsForward(int speed);
void motorsSpin(bool right, int speed);
void setMotor(int in1, int in2, int en, int speed);

bool cellInFront(int &row, int &col);
void markInFront(Cell value);
void moveInGrid();
void printGrid();

// ================================================================ setup / loop

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-S3 robot starting...");

  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_IN3, OUTPUT);
  pinMode(RIGHT_IN4, OUTPUT);
  ledcAttachChannel(LEFT_EN, PWM_FREQ, PWM_RES, PWM_CH_LEFT);
  ledcAttachChannel(RIGHT_EN, PWM_FREQ, PWM_RES, PWM_CH_RIGHT);
  motorsStop();

  pinMode(TRIG_FRONT, OUTPUT);
  pinMode(ECHO_FRONT, INPUT);
  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);
  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);

  for (int r = 0; r < GRID_ROWS; r++) {
    for (int c = 0; c < GRID_COLS; c++) {
      grid[r][c] = CELL_UNKNOWN;
    }
  }
  grid[robotRow][robotCol] = CELL_FREE;

  Serial.println("Setup complete.");
}

void loop() {
  Serial.println("=== New sensing cycle ===");

  float front = readDistance(TRIG_FRONT, ECHO_FRONT);
  delay(SENSOR_GAP_MS);
  float left = readDistance(TRIG_LEFT, ECHO_LEFT);
  delay(SENSOR_GAP_MS);
  float right = readDistance(TRIG_RIGHT, ECHO_RIGHT);

  Serial.printf("Front: %.1f cm  Left: %.1f cm  Right: %.1f cm\n", front, left, right);

  ObstacleClass cls = classify(front, left, right);
  Serial.printf("Class: %s\n", className(cls));
  delay(500);

  // The zigzag moves to the right when going north, and to the left when going south,
  // so the lines always progress in the same direction.
  bool preferRight = (robotDir == NORTH || robotDir == EAST);

  switch (cls) {
    case FREE_SPACE:
      stepForward();
      break;

    case AVOIDABLE: {
      markInFront(CELL_OCCUPIED);
      bool leftClear = left > SAFE_DISTANCE_CM + SIDE_MARGIN_CM;
      bool rightClear = right > SAFE_DISTANCE_CM + SIDE_MARGIN_CM;
      bool viaRight;
      if (leftClear != rightClear) {
        viaRight = rightClear;      // only one side is clear: use it
      } else if (leftClear) {
        viaRight = preferRight;     // both clear: follow the zigzag side
      } else {
        viaRight = right >= left;   // both narrow: use the larger gap
      }
      goAround(viaRight);
      break;
    }

    case WALL_LIKE:
      markInFront(CELL_OCCUPIED);
      changeLine(preferRight);
      break;

    case UNKNOWN:
      Serial.println("Sensor problem -> stop");
      motorsStop();
      delay(1000);
      break;
  }

  printGrid();
}

// ================================================================ sensors

// Median of several measurements in cm. Returns -1 if the sensor never answered.
float readDistance(int trigPin, int echoPin) {
  float values[MEDIAN_SAMPLES];
  int count = 0;

  for (int i = 0; i < MEDIAN_SAMPLES; i++) {
    float d = measureOnce(trigPin, echoPin);
    if (d > 0) {
      values[count++] = d;  // ignore failed measurements
    }
    delay(SAMPLE_DELAY_MS);
  }

  if (count == 0) {
    return -1;
  }

  // sort (small array)
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (values[j] < values[i]) {
        float t = values[i];
        values[i] = values[j];
        values[j] = t;
      }
    }
  }

  if (count % 2 == 0) {
    return (values[count / 2 - 1] + values[count / 2]) / 2.0f;
  }
  return values[count / 2];
}

// One measurement in cm, 0 if there is no echo.
float measureOnce(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, PULSE_TIMEOUT_US);
  return duration * SOUND_SPEED_CM_PER_US / 2.0f;
}

/*
 * Rule-based sensor fusion
 *   FREE_SPACE : front is clear
 *   AVOIDABLE  : obstacle in front, but at least one side has room to go around
 *   WALL_LIKE  : obstacle in front and both sides blocked
 *   UNKNOWN    : a sensor does not answer
 */
ObstacleClass classify(float front, float left, float right) {
  if (front < 0 || left < 0 || right < 0) return UNKNOWN;
  if (front > SAFE_DISTANCE_CM) return FREE_SPACE;
  if (left > SAFE_DISTANCE_CM || right > SAFE_DISTANCE_CM) return AVOIDABLE;
  return WALL_LIKE;
}

const char *className(ObstacleClass c) {
  switch (c) {
    case FREE_SPACE: return "FREE_SPACE";
    case AVOIDABLE: return "AVOIDABLE";
    case WALL_LIKE: return "WALL_LIKE";
    default: return "UNKNOWN";
  }
}

// ================================================================ movements

// Checks the front sensor, then drives one cell. Returns false if the way is blocked.
bool stepForward() {
  float front = readDistance(TRIG_FRONT, ECHO_FRONT);
  if (front < 0 || front <= SAFE_DISTANCE_CM) {
    Serial.println("Blocked, step cancelled");
    if (front >= 0) {
      markInFront(CELL_OCCUPIED);
    }
    return false;
  }

  motorsForward(SPEED);
  delay(CELL_MS);
  motorsStop();
  moveInGrid();
  return true;
}

// Goes around an obstacle and comes back to the same line, with the same heading:
//   side, 1 cell, back, 2 cells, back, 1 cell, turn to the original heading.
// If something blocks the way, the manoeuvre stops and the next cycle decides again.
void goAround(bool viaRight) {
  Serial.printf("Go around on the %s\n", viaRight ? "RIGHT" : "LEFT");

  turn(viaRight);
  if (!stepForward()) return;

  turn(!viaRight);
  if (!stepForward()) return;
  if (!stepForward()) return;

  turn(!viaRight);
  if (!stepForward()) return;

  turn(viaRight);  // face the original direction again
}

// U-turn into the next line of the zigzag: turn, 1 cell, turn the same way.
// If the next line is blocked too, just finish the 180 degree turn.
void changeLine(bool toRight) {
  Serial.printf("Change line to the %s\n", toRight ? "RIGHT" : "LEFT");

  turn(toRight);
  stepForward();
  turn(toRight);
}

// Turns 90 degrees and updates the heading.
void turn(bool right) {
  motorsSpin(right, SPEED);
  delay(TURN_MS);
  motorsStop();

  robotDir = (Direction)((robotDir + (right ? 1 : 3)) % 4);
  Serial.printf("Turned %s\n", right ? "RIGHT" : "LEFT");
}

// ================================================================ motors (L298N)

void motorsStop() {
  setMotor(LEFT_IN1, LEFT_IN2, LEFT_EN, 0);
  setMotor(RIGHT_IN3, RIGHT_IN4, RIGHT_EN, 0);
}

void motorsForward(int speed) {
  setMotor(LEFT_IN1, LEFT_IN2, LEFT_EN, speed);
  setMotor(RIGHT_IN3, RIGHT_IN4, RIGHT_EN, speed);
}

// Spins in place: right = left wheel forward, right wheel backward.
void motorsSpin(bool right, int speed) {
  setMotor(LEFT_IN1, LEFT_IN2, LEFT_EN, right ? speed : -speed);
  setMotor(RIGHT_IN3, RIGHT_IN4, RIGHT_EN, right ? -speed : speed);
}

// speed > 0 forward, < 0 backward, 0 stop. If a wheel turns the wrong way, swap its IN pins.
void setMotor(int in1, int in2, int en, int speed) {
  digitalWrite(in1, speed > 0 ? HIGH : LOW);
  digitalWrite(in2, speed < 0 ? HIGH : LOW);
  ledcWrite(en, constrain(abs(speed), 0, 255));
}

// ================================================================ grid

// Cell in front of the robot. Returns false if it is outside the grid.
bool cellInFront(int &row, int &col) {
  row = robotRow;
  col = robotCol;
  if (robotDir == NORTH) row++;
  else if (robotDir == SOUTH) row--;
  else if (robotDir == EAST) col++;
  else col--;
  return row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS;
}

void markInFront(Cell value) {
  int row, col;
  if (cellInFront(row, col)) {
    grid[row][col] = value;
  }
}

// Updates the position after driving one cell forward.
void moveInGrid() {
  int row, col;
  if (!cellInFront(row, col)) {
    Serial.println("Robot left the grid area (position not updated)");
    return;
  }
  robotRow = row;
  robotCol = col;
  grid[row][col] = CELL_FREE;
  Serial.printf("Robot at [%d,%d]\n", robotRow, robotCol);
}

// R = robot, # = occupied, . = free, ? = unknown. Top line = farthest row.
void printGrid() {
  Serial.println("Grid (R=robot, #=occupied, .=free, ?=unknown):");
  for (int r = GRID_ROWS - 1; r >= 0; r--) {
    for (int c = 0; c < GRID_COLS; c++) {
      if (r == robotRow && c == robotCol) Serial.print("R ");
      else if (grid[r][c] == CELL_OCCUPIED) Serial.print("# ");
      else if (grid[r][c] == CELL_FREE) Serial.print(". ");
      else Serial.print("? ");
    }
    Serial.println();
  }
}
