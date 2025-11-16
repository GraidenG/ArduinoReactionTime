#include <Arduino.h>
#include <LiquidCrystal.h>

// Button0 (left) = 18
// Button1 (middle) = 19
// Button2 (right) = 20
// Void button (right) =
// Start button (left) = 3
int BUTTONS[5] = {18, 19, 20, 2, 3};
volatile bool BUTTON_STATES[5] = {false, false, false, false, false};

volatile long BUTTON_PRESS_TIMES[5];

int LEDS[3] = {49, 51, 53};


int VOID_BUTTON = BUTTONS[3];
int START_BUTTON = BUTTONS[4];

int RUNNING_INDICATOR_LED = 0;

int ACTIVE_LED = 0; // which LED is active

long LED_TIMESTAMP = -1;

bool RUNNING = false;
bool PRACTICE = false;
long COUNTDOWN = -1;
int CS = 10; // SD card pin thing

bool continueRound = false;

int roundNumber = 0;

bool startButtonHeld = false;
bool voidButtonHeld = false;

bool bothHeld = false;
long heldBothStartTimestamp = 0;

long cancelFlashEndTime = -1;

bool CHOICE_MODE = false;

int MAX_ROUND = 3;

int TIMEOUT = 1500;

void buttonHandler(int button);
void detectButton(int button_index);
void start();
void countdownHandling();
void detectButton_1();
void detectButton_2();
void detectButton_3();
void setRandomLED();
void setLED(int led_index);
void setLEDTimestamp();
void setButtonState(int button, bool state);
bool getButtonState(int button);
void setButtonLastPressed(int button);
long getButtonLastPressed(int button);
void cancel();
void cancelHandling();

void setup() {
  Serial.begin(9600);

  // display the start screen/reset initial state
  // start button to begin test, countdown from 3 seconds, go blank
  // init random delay
  // random LED at start time
  // check if button pressed is right one and measure time

  pinMode(BUTTONS[0], INPUT_PULLUP);
  pinMode(BUTTONS[1], INPUT_PULLUP);
  pinMode(BUTTONS[2], INPUT_PULLUP);
  pinMode(VOID_BUTTON, INPUT_PULLUP);
  pinMode(START_BUTTON, INPUT_PULLUP);

  pinMode(LEDS[0], OUTPUT);
  pinMode(LEDS[1], OUTPUT);
  pinMode(LEDS[2], OUTPUT);



  attachInterrupt(digitalPinToInterrupt(BUTTONS[0]), []{buttonHandler(BUTTONS[0]);}, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTONS[1]), []{buttonHandler(BUTTONS[1]);}, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTONS[2]), []{buttonHandler(BUTTONS[2]);}, FALLING);
  attachInterrupt(digitalPinToInterrupt(VOID_BUTTON), []{buttonHandler(BUTTONS[3]);}, FALLING);
  attachInterrupt(digitalPinToInterrupt(START_BUTTON), []{buttonHandler(BUTTONS[4]);}, FALLING);

}

void buttonHandler(int button) {
  if (millis() - getButtonLastPressed(button) < 20) return;
  setButtonLastPressed(button);
  setButtonState(button, true);
}



void loop() {
  // check if start button is pressed
  if (getButtonState(START_BUTTON) && !RUNNING) {
    Serial.println("START BUTTON PRESSED");
    startButtonHeld = true;
    setButtonState(START_BUTTON, false);
  }

  if (getButtonState(VOID_BUTTON)) {
    Serial.println("VOID BUTTON PRESSED");
    setButtonState(VOID_BUTTON, false);
    voidButtonHeld = true;
  }

  // maybe overcomplicated. Goal is to detect when both are pressed, even if one is released and repressed (since that seems like anticipated behaviour).
  if ((startButtonHeld || voidButtonHeld) && (getButtonLastPressed(START_BUTTON) + 20 < millis() && getButtonLastPressed(VOID_BUTTON) + 20 < millis())) {
    if (digitalRead(START_BUTTON) == LOW && digitalRead(VOID_BUTTON) == LOW) {
      // if both are being held down 20ms after both
      bothHeld = true;
      startButtonHeld = false;
      voidButtonHeld = false;
      Serial.println("BOTH BUTTONS PRESSED");
    }
  }

  if (bothHeld && !RUNNING) {
    // run before the check b/c it could make this true by resetting the timestamp.
    if (millis() > heldBothStartTimestamp + 1000) {
      bothHeld = false;
      heldBothStartTimestamp = 0;

      PRACTICE = true;
      start();
      Serial.println("PRACTICE MODE");
    }

    if (digitalRead(START_BUTTON) != LOW || digitalRead(VOID_BUTTON) != LOW) {
      Serial.println("ONE BUTTONS RELEASED");
      bothHeld = false;
      heldBothStartTimestamp = 0;
    }
  }


  if (startButtonHeld && !RUNNING) {
    if (getButtonLastPressed(START_BUTTON) + 20 < millis() && digitalRead(START_BUTTON) != LOW ) {
      // no longer held (with 20 ms cooldown to protect from debounce)
      startButtonHeld = false;
    } else if (millis() > getButtonLastPressed(START_BUTTON) + 1000 && digitalRead(START_BUTTON) == LOW) {
      startButtonHeld = false; // reset
      start(); // start
    }
  }



  if (voidButtonHeld) {
    if (getButtonLastPressed(VOID_BUTTON) + 20 < millis() && digitalRead(VOID_BUTTON) != LOW) {
      voidButtonHeld = false; // reset with 20ms delay for debounce
    } else if (millis() > getButtonLastPressed(VOID_BUTTON) + 500 && digitalRead(VOID_BUTTON) == LOW && RUNNING) {
      // cancel current run after 500ms hold if the process is running
      voidButtonHeld = false; // reset
      cancel();
    } else if (millis() > getButtonLastPressed(VOID_BUTTON) + 2000 && digitalRead(VOID_BUTTON) == LOW && !RUNNING)
    {
      // todo: clear previous data entry
      voidButtonHeld = false; // reset
      Serial.println("RESET");
    }
  }


  // check if buttons are pressed
  for (int i = 0; i < 3; i++) {
    int button = BUTTONS[i];
    if (getButtonState(button) && ACTIVE_LED != 0) {
      detectButton(i);
      setButtonLastPressed(button);
      setButtonState(button, false);
    }
  }

  // put your main code here, to run repeatedly:
  digitalWrite(RUNNING_INDICATOR_LED, RUNNING);
  countdownHandling();
  cancelHandling();

  if ((long)millis() - LED_TIMESTAMP > 0 && !continueRound && ACTIVE_LED != 0 && COUNTDOWN == -1) {
    digitalWrite(ACTIVE_LED, HIGH);
  }

  if (continueRound) {
    digitalWrite(ACTIVE_LED, LOW);
    continueRound = false;
    Serial.print("round: ");
    Serial.println(roundNumber);

    if (roundNumber < 3) {
      setLEDTimestamp();

      if (CHOICE_MODE) {

        setRandomLED();
      } else {
        Serial.println("not choice mode, led = 1");
        setLED(1);
      }
    } else if (roundNumber == 3 && CHOICE_MODE) { // transition out of choice mode
      COUNTDOWN = millis() + 1000; // start countdown
      CHOICE_MODE = false;
      Serial.println("choice mode end");
      roundNumber = 0;
      setLED(1);
    } else {
      Serial.println("end of test");
      RUNNING = false;
      CHOICE_MODE = true;
      roundNumber = 0;
      ACTIVE_LED = 0;
    }
  } else if (millis() > LED_TIMESTAMP + TIMEOUT && COUNTDOWN == -1 && RUNNING) {
    digitalWrite(ACTIVE_LED, LOW);
    Serial.print("TIMEOUT: ");
    Serial.println(millis() - LED_TIMESTAMP);
    setLEDTimestamp();
    setRandomLED();
  }
}


void start() {
  COUNTDOWN = millis();
  RUNNING = true;
  randomSeed(millis());

  Serial.println("STARTING TEST");
  roundNumber = 0;
  continueRound = false;

  CHOICE_MODE = true;
}

void cancel()
{
  cancelFlashEndTime = millis() + 850;

  // reset state to default
  RUNNING = false;
  COUNTDOWN = -1;
  CHOICE_MODE = true;
  roundNumber = 0;
  ACTIVE_LED = 0;

  Serial.println("CANCELLED TEST");
}

void cancelHandling()
{
  if (cancelFlashEndTime == -1) return;


 // 500 ms on -> 250 ms off -> 250 on -> off
  if (millis() < cancelFlashEndTime - 450)
  {
    // on for first 450 ms
    digitalWrite(LEDS[0], HIGH);
    digitalWrite(LEDS[1], HIGH);
    digitalWrite(LEDS[2], HIGH);
  } else if (millis() < cancelFlashEndTime - 200) {
    // off for 450-650ms
    digitalWrite(LEDS[0], LOW);
    digitalWrite(LEDS[1], LOW);
    digitalWrite(LEDS[2], LOW);
  } else if (millis() < cancelFlashEndTime) {
    // flash back on for final 200 ms
    digitalWrite(LEDS[0], HIGH);
    digitalWrite(LEDS[1], HIGH);
    digitalWrite(LEDS[2], HIGH);
  } else {
    digitalWrite(LEDS[0], LOW);
    digitalWrite(LEDS[1], LOW);
    digitalWrite(LEDS[2], LOW);

    cancelFlashEndTime = -1;
  }
}

void countdownHandling() {
  long timeSinceCountdown = millis() - COUNTDOWN;

  if (COUNTDOWN < 0 || timeSinceCountdown < 0) return;

  if (timeSinceCountdown < 1000) {
    digitalWrite(LEDS[0], HIGH);
    digitalWrite(LEDS[1], HIGH);
    digitalWrite(LEDS[2], HIGH);
  } else if (timeSinceCountdown < 2000) {
    digitalWrite(LEDS[0], LOW);
    digitalWrite(LEDS[1], HIGH);
    digitalWrite(LEDS[2], HIGH);
  } else if (timeSinceCountdown < 3000) {
    digitalWrite(LEDS[0], LOW);
    digitalWrite(LEDS[1], LOW);
    digitalWrite(LEDS[2], HIGH);
  } else {
    digitalWrite(LEDS[0], LOW);
    digitalWrite(LEDS[1], LOW);
    digitalWrite(LEDS[2], LOW);

    COUNTDOWN = -1;
    setLEDTimestamp();
    Serial.println("countdown end");

    if (CHOICE_MODE) {
      setRandomLED();
    }
  }
}

void detectButton(int button_index) {
  if (ACTIVE_LED == 0 || LED_TIMESTAMP == -1) return; // bad input/debounce filtering

  long currentTime = millis();
  long timeDelta = currentTime - LED_TIMESTAMP;

  Serial.print("pressed button: " );
  Serial.println(button_index);

  if (((ACTIVE_LED == LEDS[button_index] && CHOICE_MODE) || !CHOICE_MODE)  && timeDelta > 100) {
    // correct button and more than 100 ms after the LED turned on
    continueRound = true;
    roundNumber++;

    Serial.print("Correct! Time: ");
    Serial.println(timeDelta);
    // record data
  } else if (ACTIVE_LED != LEDS[button_index] && CHOICE_MODE) {
    Serial.println("INCORRECT! Time: " );
    Serial.println(timeDelta);
    // wrong button
    // record incorrect + time
  } else if (timeDelta > 0 && timeDelta <= 100) {
    // too fast, don't record
    Serial.println("too fast");
    continueRound = true;

  } else {
    // early guess, do nothing
  }
}

void setButtonState(int button, bool state) {
  for (int i = 0; i < 5; i++) {
    if (button == BUTTONS[i]) BUTTON_STATES[i] = state;
  }
}

bool getButtonState(int button) {
  for (int i = 0; i < 5; i++) {
    if (button == BUTTONS[i]) return BUTTON_STATES[i];
  }

  return false;
}

void setButtonLastPressed(int button) {
  for (int i = 0; i < 5; i++) {
    if (button == BUTTONS[i]) BUTTON_PRESS_TIMES[i] = millis();
  }
}

long getButtonLastPressed(int button) {
  for (int i = 0; i < 5; i++) {
    if (button == BUTTONS[i]) return BUTTON_PRESS_TIMES[i];
  }

  return 0; // as if it's never been pressed before but for really weird edge case where it isn't found
}

void setRandomLED() {
  ACTIVE_LED = LEDS[(int)random(0,4)];
  Serial.print("Random LED: ");
  Serial.println(ACTIVE_LED);
}

void setLED(int led_index) {
  ACTIVE_LED = LEDS[led_index];
}

void setLEDTimestamp() {
  LED_TIMESTAMP = millis() + random(2000, 6000);
  Serial.print("LED Timestamp: ");
  Serial.println(LED_TIMESTAMP);
}
