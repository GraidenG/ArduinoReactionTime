#include <Arduino.h>
#include <LiquidCrystal.h>
#include <SD.h>
#include <SPI.h>
#include <avr/wdt.h>


// Button0 (left) = 18
// Button1 (middle) = 19
// Button2 (right) = 20
// Void button (right) = 2
// Start button (left) = 3
int BUTTONS[5] = {18, 19, 20, 2, 3};
volatile bool BUTTON_STATES[5] = {false, false, false, false, false};

volatile unsigned long BUTTON_PRESS_TIMES[5] = {0, 0, 0, 0 ,0};

int LEDS[3] = {43, 45, 47};

int VOID_BUTTON = BUTTONS[3];
int START_BUTTON = BUTTONS[4];

int RUNNING_INDICATOR_LED = 0;

int ACTIVE_LED = 0; // which LED is active

long LED_TIMESTAMP = -1;

bool RUNNING = false;
bool PRACTICE = false;
long COUNTDOWN_START = -1;
int CS = 53; // SD card pin thing

int userID = 0;

bool continueRound = false;

int roundNumber = 0;

bool startButtonHeld = false;
bool voidButtonHeld = false;
bool leftButtonHeld = false;
bool rightButtonHeld = false;

unsigned long lastIncorrectTime = 0;

long cancelFlashEndTime = -1;

bool CHOICE_MODE = true;

int MAX_ROUND = 3;

long *currentRoundTimes; // round
int currentRoundPresses = 0;

const char* fileName = "data.csv";

int TIMEOUT = 1000;

// VSS = GND
// VDD = 5V
// V0 = contrast (goes to GND through resistor)
// RW = read/write mode, write = 0 so no wire needed
// D0-D3 unused
// D4 - D7 data
// A = 5v behind resistor
// K = GND
const int rs = 23, en = 25, d4 = 27, d5 = 29, d6 = 31, d7 = 33;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


void buttonHandler(int button);
void detectButton(int button_index);
void startTest();
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
void practice();
void newUser();
void end();
void LCDShowStartScreen();
void LCDWriteCurrentTime(long time);
void LCDStartCountdown();
void LCDStartTest();
void LCDShowSummary();
void LCDShowError(const String& error);
void start();

class MenuItem {
  public:
    String name;
    int row;
    int position;
    void (*action)();
    bool selected = false;

    MenuItem(String name, int row, int position, void (*action)(), bool selected) {
      this->name = name;
      this->row = row;
      this->position = position;
      this->action = action;
      this->selected = selected;
    }

    void draw() {
      lcd.setCursor(position, row);
      lcd.print(name);
    }

};

MenuItem menuItems[] = {
  MenuItem("STRT", 0, 0, start, true),
  MenuItem("PRAC", 0, 6, practice, false),
  MenuItem("NEWUSR", 1, 0, newUser, false)
};

bool onMenu = true;

bool writeToFile(const char* path, const String& data) {
  File file = SD.open(path, FILE_WRITE);

  if (!file) {
    file.close();
    file = SD.open(path, FILE_WRITE);
    if (!file) {
      file.close();
      return false; // failed to open file twice
    }
  }

  file.println(data);

  file.close();

  return true;
}


void setup() {
  Serial.begin(9600);

  currentRoundTimes = new long[MAX_ROUND];

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

  attachInterrupt(digitalPinToInterrupt(BUTTONS[0]), []{buttonHandler(0);}, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTONS[1]), []{buttonHandler(1);}, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTONS[2]), []{buttonHandler(2);}, FALLING);
  attachInterrupt(digitalPinToInterrupt(VOID_BUTTON), []{buttonHandler(3);}, FALLING);
  attachInterrupt(digitalPinToInterrupt(START_BUTTON), []{buttonHandler(4);}, FALLING);

  lcd.begin(16, 2);


  pinMode(CS, OUTPUT);

   File file;
   if (!SD.begin(CS)) {
     Serial.print("Error init SD card!");
     LCDShowError("SD INIT ERROR");
     while(true); // wait for arduino restart
   }

   file = SD.open(fileName, FILE_WRITE);

   if (!file) {
     Serial.print("Error while creating/opening file");
     LCDShowError("SD CREATE ERROR");
     while(true); // wait for arduino restart
   }

  file.close();

  file = SD.open(fileName, FILE_READ);
  const int bufferSize = 128;
  char* charArray = new char[bufferSize];


  String line;
  while (file.available()) {
    unsigned int bytesRead = file.readBytesUntil('\n', charArray, bufferSize - 1);
    charArray[bytesRead] = '\0'; // null terminate, not sure if needed but can't hurt to not do strings wrong

    char* element = strtok(charArray, ",");

    int i = 0;
    while (element != NULL) {
      Serial.print(element);
      Serial.print(" ");

      // user ID stored in first column
      if (i == 0) {
        userID = atoi(element) + 1;
        break;
      }

      element = strtok(NULL, ",");
      i++;
    }
  }

  delete[] charArray;

  file.close();

  LCDShowStartScreen();
}

void buttonHandler(int index) {
  if (millis() - BUTTON_PRESS_TIMES[index] < 20) return; // for debounce protection
  BUTTON_PRESS_TIMES[index] = millis();
  BUTTON_STATES[index] = true;
}

void buttonPressChecks() {
  // check if start button is pressed
  if (getButtonState(START_BUTTON)) {
    Serial.println("START BUTTON PRESSED");
    startButtonHeld = true;
    setButtonState(START_BUTTON, false);
  }

  if (getButtonState(VOID_BUTTON)) {
    Serial.println("VOID BUTTON PRESSED");
    setButtonState(VOID_BUTTON, false);
    voidButtonHeld = true;
  }

  // Button 0 (leftmost) acting as a "left" button for the menu
  if (getButtonState(BUTTONS[0]) && onMenu && !RUNNING) {
    setButtonState(BUTTONS[0], false);
    leftButtonHeld = true;
    Serial.println("BUTTON 0 PRESSED");
  }

  // Button 2 (rightmost) acting as a "right" button for the menu
  if (getButtonState(BUTTONS[2]) && onMenu && !RUNNING) {
    Serial.println("BUTTON 2 PRESSED");
    setButtonState(BUTTONS[2], false);
    rightButtonHeld = true;
  }



}

void buttonHeldActions() {
    /*// maybe overcomplicated. Goal is to detect when both are pressed, even if one is released and repressed (since that seems like anticipated behaviour).
  if ((startButtonHeld || voidButtonHeld) && (getButtonLastPressed(START_BUTTON) + 20 < millis() && getButtonLastPressed(VOID_BUTTON) + 20 < millis())) {
    if (digitalRead(START_BUTTON) == LOW && digitalRead(VOID_BUTTON) == LOW) {
      // if both are being held down 20ms after both
      bothHeld = true;
      startButtonHeld = false;
      voidButtonHeld = false;
      heldBothStartTimestamp = millis();
      Serial.println("BOTH BUTTONS PRESSED");
    }
  }

  if (bothHeld && !RUNNING) {
    // run before the check b/c it could make this true by resetting the timestamp.
    if (millis() > heldBothStartTimestamp + 1000) {
      bothHeld = false;
      heldBothStartTimestamp = 0;

      practice();
      Serial.println("PRACTICE MODE");
    }

    if (digitalRead(START_BUTTON) != LOW || digitalRead(VOID_BUTTON) != LOW) {
      Serial.println("ONE BUTTONS RELEASED");
      bothHeld = false;
      heldBothStartTimestamp = 0;
    }
  }*/

  // the logic in these checks is a bit confusing, but it boils down to checking if the press is fake (i.e. caused by something like debounce but idk what), then it will be back to HIGH shortly and can be ignored. Otherwise there's a 40ms timer to hold down a button for (imperceptible) to help with debounce

  if (leftButtonHeld && !RUNNING && onMenu) {
    Serial.println("LEFT BUTTON HELD");
    if (getButtonLastPressed(BUTTONS[0]) + 20 < millis() && digitalRead(BUTTONS[0]) != LOW) {
      leftButtonHeld = false; // no longer held down, debounce
    } else if (millis() > getButtonLastPressed(BUTTONS[0]) + 40 && digitalRead(BUTTONS[0]) == LOW) {
      leftButtonHeld = false; // reset

      int size = sizeof(menuItems) / sizeof(MenuItem); // array size

      for (int i = 0; i < size; i++) {
        MenuItem &menuItem = menuItems[i];
        if (menuItem.selected) {
          menuItem.selected = false;

          // set cursor "highlight"
          if (i - 1 >= 0) {
            Serial.println("left");
            MenuItem &newMenuItem = menuItems[i - 1];

            Serial.print(" new selected name: ");
            Serial.println(newMenuItem.name);

            newMenuItem.selected = true;
            lcd.setCursor(newMenuItem.position, newMenuItem.row);
            break;
          } else {
            Serial.println("wrap from left");
            MenuItem &newMenuItem = menuItems[size - 1];

            Serial.print(" new selected name: ");
            Serial.println(newMenuItem.name);
            newMenuItem.selected = true;
            lcd.setCursor(newMenuItem.position, newMenuItem.row);
            break;
          }
        }  else if (i + 1 == size) {
          // fallback
          menuItem.selected = true;
          lcd.setCursor(menuItem.position, menuItem.row);
        }
      }
    }
  }


  if (rightButtonHeld && !RUNNING && onMenu) {
    if (getButtonLastPressed(BUTTONS[2]) + 20 < millis() && digitalRead(BUTTONS[2]) != LOW) {
      rightButtonHeld = false; // no longer held down, debounce
    } else if (millis() > getButtonLastPressed(BUTTONS[2]) + 40 && digitalRead(BUTTONS[2]) == LOW) {
      rightButtonHeld = false; // reset

      int size = sizeof(menuItems) / sizeof(MenuItem); // array size
      for (int i = 0; i < size; i++) {
        MenuItem &menuItem = menuItems[i];
        Serial.print("menu item: ");
        Serial.print(menuItem.name);
        Serial.print(" selected: ");
        Serial.println(menuItem.selected);


        if (menuItem.selected) {
          menuItem.selected = false;

          // set cursor highlight
          if (i + 1 < size) {
            Serial.println("right");
            MenuItem &newMenuItem = menuItems[i + 1];

            Serial.print(" new selected name: ");
            Serial.println(newMenuItem.name);
            newMenuItem.selected = true;
            lcd.setCursor(newMenuItem.position, newMenuItem.row);

            break;
          } else {
            Serial.println("wrap from right");
            MenuItem &newMenuItem = menuItems[0];
            Serial.print(" new selected name: ");
            Serial.println(newMenuItem.name);
            newMenuItem.selected = true;
            lcd.setCursor(newMenuItem.position, newMenuItem.row);
            break;
          }
        } else if (i + 1 == size) {
          // fallback
          menuItem.selected = true;
          lcd.setCursor(menuItem.position, menuItem.row);
        }
      }
    }
  }

  // acting as a confirmation button, not necessarily start
  if (startButtonHeld && onMenu) {
    if (getButtonLastPressed(START_BUTTON) + 20 <= millis() && digitalRead(START_BUTTON) != LOW ) {
      // no longer held (with 20 ms cooldown to protect from debounce)
      startButtonHeld = false;
    } else if (millis() > getButtonLastPressed(START_BUTTON) + 40 && digitalRead(START_BUTTON) == LOW) {
      startButtonHeld = false; // reset
      Serial.println("START BUTTON HELD");

      if (RUNNING && !PRACTICE) {
        // if we're on the menu and it is running, then it is the summary page
        // specifically in Choice Mode we want to start the new countdown to non-choice mode

        String csvFormattedData;

        csvFormattedData += String(userID) + ",";
        if (CHOICE_MODE) {
          csvFormattedData += "CHOICE,"; // choice mode
        } else {
          csvFormattedData += "SIMPLE,"; // simple mode
        }

        float accuracy = static_cast<float>(MAX_ROUND) / currentRoundPresses;
        csvFormattedData += String(accuracy) + ","; // accuracy

        for (int i = 0; i < MAX_ROUND; i++) {
          if (i < MAX_ROUND - 1) {
            csvFormattedData += String(currentRoundTimes[i]) + ",";
          } else {
            csvFormattedData += String(currentRoundTimes[i]);
          }
        }

        if (writeToFile(fileName, csvFormattedData)) {
          if (CHOICE_MODE) {
            CHOICE_MODE = false;
            startTest();
          } else {
            // if we're in non-choice mode then finished
            end();
          }
        } else {
          end();
          LCDShowError(" SD WRITE ERROR ");
        }

      } else if (RUNNING && PRACTICE) {
        if (CHOICE_MODE) {
          CHOICE_MODE = false;
          startTest();
        } else {
          // if we're in non-choice mode then finished
          end();
        }
      } else {
        for (MenuItem& menuItem : menuItems) {
          if (menuItem.action != nullptr && menuItem.selected) {
            Serial.print(menuItem.name);
            Serial.println(menuItem.selected);
            menuItem.action(); // e.g. start, practice, etc.
            menuItem.selected = false;
          }
        }
      }
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
      // original plan was to use this to remove the previous result but I think this will result in accidents. Instead we should mark one as incomplete on the questionaire answers.
    }
  }
}

void loop() {
  buttonPressChecks();
  buttonHeldActions();


  // check if buttons are pressed
  for (int i = 0; i < 3; i++) {
    int button = BUTTONS[i];
    if (getButtonState(button) && ACTIVE_LED != 0 && RUNNING) {
      detectButton(i);
      setButtonLastPressed(button);
      setButtonState(button, false);
    }
  }

  // put your main code here, to run repeatedly:
  digitalWrite(RUNNING_INDICATOR_LED, RUNNING);
  countdownHandling();
  cancelHandling();

  if (LED_TIMESTAMP > 0 && (long)millis() - LED_TIMESTAMP > 0 && !continueRound && ACTIVE_LED != 0 && COUNTDOWN_START == -1 && !onMenu) {
    digitalWrite(ACTIVE_LED, HIGH);
  }

  if (continueRound) {
    digitalWrite(ACTIVE_LED, LOW);
    continueRound = false;
    Serial.print("round: ");
    Serial.println(roundNumber);

    if (roundNumber < MAX_ROUND) {
      setLEDTimestamp();

      if (CHOICE_MODE) {
        setRandomLED();
      }
    } else if (roundNumber >= MAX_ROUND && CHOICE_MODE) { // transition out of choice mode
      Serial.println("choice mode end");
      // CHOICE_MODE Is set to false when the user confirms okay to move on
      LCDShowSummary();
    } else if (roundNumber >= MAX_ROUND) {
      Serial.println("end of test");
      LCDShowSummary();
    }
  } else if (LED_TIMESTAMP > 0 && millis() > LED_TIMESTAMP + TIMEOUT && COUNTDOWN_START == -1 && RUNNING && !onMenu) {
    digitalWrite(ACTIVE_LED, LOW);
    Serial.print("TIMEOUT");
    setLEDTimestamp();
    if (CHOICE_MODE) {
      setRandomLED();
    }
  }
}

void LCDShowError(const String& error) {
  lcd.clear();
  lcd.print(error);
  lcd.setCursor(0, 1);
  lcd.print("   RESTARTING   ");

  wdt_enable(WDTO_8S); // restart arduino in 8s
}

void LCDShowStartScreen() {
  lcd.clear();

  for (MenuItem menuItem : menuItems) {
    menuItem.selected = false;
    menuItem.draw();
  }

  menuItems[0].selected = true;

  lcd.setCursor(12,0);
  lcd.print(userID);

  lcd.setCursor(0, 0);
  lcd.blink();
}

void LCDWriteCurrentTime(long time) {
  lcd.setCursor(0, 1);
  lcd.print("    "); // clear out previous number fully
  lcd.setCursor(0, 1); // reset cursor

  if (time == -1) {
    lcd.print("FAST");
    return;
  }
  // print current
  lcd.print(time);

  // print average
  if (roundNumber > 0) {
    lcd.setCursor(6, 1);
    lcd.print("    "); // clear out previous number fully
    lcd.setCursor(6, 1); // reset cursor

    long sum = 0;

    for (int i = 0; i <= roundNumber; i++) {
      sum += currentRoundTimes[i];
    }

    lcd.print(sum / (roundNumber + 1));
  }
}

void LCDShowSummary() {
  onMenu = true;
  ACTIVE_LED = 0;
  LED_TIMESTAMP = -1;

  lcd.clear();

  lcd.print("BEST");

  lcd.setCursor(6, 0);
  lcd.print("AVG.");

  lcd.setCursor(12,0);
  lcd.print(userID);

  Serial.println("SUMMARY");
  Serial.println("Times: ");
  long sum = 0;
  long bestTime = 0;
  for (int i = 0; i < MAX_ROUND; i++) {
    Serial.println(currentRoundTimes[i]);
    sum += currentRoundTimes[i];
    if (currentRoundTimes[i] < bestTime || bestTime == 0) {
      bestTime = currentRoundTimes[i];
    }
  }

  lcd.setCursor(0, 1);
  lcd.print(bestTime);

  lcd.setCursor(6, 1);
  lcd.print(sum / MAX_ROUND);

  lcd.setCursor(12,1);
  lcd.print("OK");
  lcd.setCursor(12,1);
  lcd.blink();
}

void LCDStartCountdown() {
  lcd.clear();
  if (CHOICE_MODE) {
    lcd.print("  CHOICE  TEST  ");
  } else {
    lcd.print("  SIMPLE  TEST  ");
  }

  lcd.setCursor(0, 1);

  // literally just ensures it's centered for 1 and 2 digit numbers by hardcoding the strings. idk why I wrote this.
  if (MAX_ROUND < 10) {
    lcd.print("    ");
    lcd.print(MAX_ROUND);
    lcd.print(" ROUNDS    ");
  } else {
    lcd.print("   ");
    lcd.print(MAX_ROUND);
    lcd.print("  ROUNDS   ");
  }
}

void LCDStartTest() {
  lcd.clear();

  lcd.print("CUR.");

  lcd.setCursor(6, 0);
  lcd.print("AVG.");

  lcd.setCursor(12,0);
  lcd.print(userID);

  lcd.setCursor(12,1);
  if (PRACTICE) {
    lcd.print("PRAC");
  } else {
    lcd.print("TEST");
  }

  lcd.setCursor(0, 1);
  lcd.print("----");

  lcd.setCursor(6, 1);
  lcd.print("----");
}

void newUser() {
  userID++;
  lcd.setCursor(12,0);
  lcd.print(userID);

  // reset position
  menuItems[0].selected = true;
  lcd.setCursor(0, 0);
}

void practice() {
  PRACTICE = true;
  MAX_ROUND = 5;
  startTest();
}

void start() {
  PRACTICE = false;
  MAX_ROUND = 10;
  startTest();
}

void startTest() {
  RUNNING = true;
  randomSeed(millis());
  onMenu = false;
  lcd.noBlink();

  // reset state
  startButtonHeld = false;
  voidButtonHeld = false;
  rightButtonHeld = false;
  voidButtonHeld = false;

  Serial.println("STARTING TEST");
  roundNumber = 0;
  continueRound = false;

  delete[] currentRoundTimes;
  currentRoundTimes = new long[MAX_ROUND];
  currentRoundPresses = 0;

  COUNTDOWN_START = millis();
  LCDStartCountdown();
}

void cancel()
{
  cancelFlashEndTime = millis() + 850;

  // reset state to default
  COUNTDOWN_START = -1;
  end();

  Serial.println("CANCELLED TEST");
}

void end() {
  CHOICE_MODE = true;
  roundNumber = 0;
  ACTIVE_LED = 0;
  RUNNING = false;
  onMenu = true;

  LCDShowStartScreen();
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
  long timeSinceCountdown = millis() - COUNTDOWN_START;

  if (COUNTDOWN_START < 0 || timeSinceCountdown < 0) return;

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

    COUNTDOWN_START = -1;
    setLEDTimestamp();
    Serial.println("countdown end");

    if (CHOICE_MODE) {
      setRandomLED();
    } else {
      setLED(1);
    }

    LCDStartTest();
  }
}

void detectButton(int button_index) {
  if (ACTIVE_LED == 0 || LED_TIMESTAMP == -1) return; // bad input/debounce filtering
  if (roundNumber >= MAX_ROUND) return; // don't record after max rounds

  long currentTime = millis();
  long timeDelta = currentTime - LED_TIMESTAMP;

  Serial.print("pressed button: " );
  Serial.println(button_index);

  if (((ACTIVE_LED == LEDS[button_index] && CHOICE_MODE) || !CHOICE_MODE)  && timeDelta > 100) {
    // correct button and more than 100 ms after the LED turned on
    continueRound = true;

    Serial.print("Correct! Time: ");
    Serial.println(timeDelta);

    currentRoundTimes[roundNumber] = timeDelta;
    currentRoundPresses++;

    LCDWriteCurrentTime(timeDelta);

    roundNumber++; // used by LCDWriteTime so needs to be updated after
    // record data
  } else if (ACTIVE_LED != LEDS[button_index] && CHOICE_MODE && timeDelta > 100 && millis() - lastIncorrectTime > 20) {
    Serial.println("INCORRECT! Time: " );
    Serial.println(timeDelta);
    currentRoundPresses++;

    lastIncorrectTime = millis();
    // wrong button
    // record incorrect + time
  } else if (timeDelta > 0 && timeDelta <= 100) {
    // too fast, don't record
    Serial.println("too fast");
    continueRound = true;
    LCDWriteCurrentTime(-1);


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
  ACTIVE_LED = LEDS[(int)random(0,3)];
  Serial.print("Random LED: ");
  Serial.println(ACTIVE_LED);
}

void setLED(int led_index) {
  ACTIVE_LED = LEDS[led_index];
}

void setLEDTimestamp() {
  LED_TIMESTAMP = millis() + random(3000, 10000);
  Serial.print("LED Timestamp: ");
  Serial.println(LED_TIMESTAMP);
}
