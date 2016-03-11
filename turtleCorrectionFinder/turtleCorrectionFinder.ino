#include <Wire.h>
#include <L3G.h>
#include <ZumoMotors.h>
#include <Pushbutton.h>
#include <ZumoBuzzer.h>

#define W_OFFSET 4480
#define SAMPLING_TIME 11
#define TURN_SPEED 25
#define STRAIGHT_SPEED 200
#define TURN_ANGLE 90000
#define THRESHOLD 125
#define STRAIGHT_TH 125
#define STEP_LENGTH 500
#define CORRECTION_FACTOR 20
#define STRAIGHT_DURATION 500
#define COMMANDS_NUM 10

#define LDR_THRESHOLD 150

#define FRONT_LDR 4
#define BACK_LDR 0
#define SX_LDR 1
#define RX_LDR 3

#define LOW_TIME 600

#define LED_PIN 13

ZumoMotors motors;
ZumoBuzzer buzzer;
Pushbutton startButton(ZUMO_BUTTON);
Pushbutton stopButton(11);
L3G gyro;

int reading; // numerical value from gyro
float angularReading; // conversion from "reading" to angular Velocity
float currentAngle = 0; // integration over time
long currentOffset = 0; // to check if the defined offset is correct
int offset = W_OFFSET;
float absoluteAngle = 0; // absolute angular position relative to the starting point
float stepDeviation = 0;
long targetAngle; // ideal angular position to reach
byte commandList[COMMANDS_NUM];
unsigned int sensorTh[4];

unsigned long start; // check loops duration

void setup() {
  pinMode(LED_PIN, OUTPUT);
  
  Serial.begin (115200);
  Wire.begin();

  if (!gyro.init())
  {
    Serial.println("Failed to autodetect gyro type!");
    while (1);
  }

  gyro.enableDefault();

  setFullScale();
  
  checkOffset();
  
  delay(500);
}

void loop() {
  straight(10);
  
  absoluteAngle = targetAngle  = 0;

  startButton.waitForButton();

  delay(500);
}

void LDRcalibration2() {

  long int readingsH[4];
  long int readingsL[4];
  int frontReading;
  int i,cycle;
  /*        0
   *    1        2
   *         3
   */

  //find min reading for each sensor when lit
 for (i=0; i<4; i++) {
   readingsH[i] = 0;
   readingsL[i] = 0;   //set to zero
 }

 buzzer.playNote (NOTE_A(4), 125, 15);
 digitalWrite(LED_PIN, HIGH);  
 
 for ( cycle = 0; cycle < 100; cycle ++ ) {
  readingsH[0] += analogRead(FRONT_LDR);
  readingsH[1] += analogRead(SX_LDR);
  readingsH[2] += analogRead(RX_LDR);
  readingsH[3] += analogRead(BACK_LDR);  
  delay(50);
 }
 
 for ( i = 0; i < 4; i++ ) 
   readingsH[i] = readingsH[i] / 100;
   
 digitalWrite(LED_PIN, LOW);
 delay(500);
 
 for ( i = 0; i < 3; i++ ) {
  buzzer.playNote (NOTE_A(4), 125, 15); 
  delay(125);
  buzzer.playNote (SILENT_NOTE, 125, 15);
 }
 delay(100);
 buzzer.playNote (NOTE_A(5), 125, 15);
 
 digitalWrite(LED_PIN, HIGH); 
 delay(1000); 
 
 for ( cycle = 0; cycle < 100; cycle ++ ) {
  readingsL[0] += analogRead(FRONT_LDR);
  readingsL[1] += analogRead(SX_LDR);
  readingsL[2] += analogRead(RX_LDR);
  readingsL[3] += analogRead(BACK_LDR); 
  delay(50);
 }
 
 for ( i = 0; i < 4; i++ ) 
   readingsL[i] = readingsL[i] / 100;
 
 digitalWrite(LED_PIN, LOW);
 
  // evaluate the weighted average to get the threshold
  for ( i = 0; i < 4; i++ ) 
    sensorTh[i] = (int) (0.2*readingsH[i] + 0.8*readingsL[i]);

  for (int i=0; i<4; i++) {
    Serial.print(sensorTh[i]);
    Serial.print(" ");
  }

  delay(250);
  buzzer.playNote (NOTE_A(3), 125, 15); 
  
  return;
}


void buildList() {
  buzzer.playNote (NOTE_A(4), 125, 15); 
  delay(125);
  buzzer.playNote (SILENT_NOTE, 125, 15);
  delay(125); 
  buzzer.playNote (NOTE_A(4), 125, 15); 

  Serial.println("Waiting for commands...");

  int readings[4];
  bool lowValue[4]={0}; //sensor is currently low 
  bool prevLow[4]={0}; //sensor was low in the previous iteration
  int commands = 0;
  bool commandSet[4] = {0};
  unsigned long lowStart;

  while (commands <= COMMANDS_NUM && !startButton.isPressed() ) { //continue to read until the maximum number of commands is entered 
                                                                                                       //or the start button is pressed
    readings[0] = analogRead(FRONT_LDR);
    readings[1] = analogRead(SX_LDR);
    readings[2] = analogRead(RX_LDR);
    readings[3] = analogRead(BACK_LDR);
    
    for (int i=0; i<4; i++) {
      if (readings[i] < sensorTh[i]) {
        if (prevLow[i]==0) {
          lowValue[i] = 1; //the sensor's reading is below the set threshold  
          lowStart = millis();
          prevLow[i] = 1;
        }
        /* checks for the following cases: LDR was pressed before, a command was not already enqueued for this series of readings, the LDR was
         *  covered for a sufficient time and no other sensors are reading a value below threshold
         */
        else if (prevLow[i] == 1 && commandSet[i] == 0 && millis()-lowStart > LOW_TIME && noOtherLow(i, readings) ) { 
          commandList[commands++] = i;
          Serial.print(i);
          Serial.println(" SET");
          buzzer.playNote (NOTE_A(4), 125, 15); 
          commandSet[i] = 1;
        }
      }
      else {
        lowValue[i] = 0;
        prevLow[i] = 0;
        commandSet[i] = 0;
      }
    }
    delay(25);
  }
  while(startButton.isPressed()) { //wait for the user to realease the button
    delay(10); 
  }
  delay(500);
}

/* checks if other sensors other than the one given as argument are below threshold
 *  to avoid the insertion of multiple commands in the execution list
 */
bool noOtherLow(byte lowPin, int * sensorValues) { 
  for (int i=0; i<4; i++) {
    if (i != lowPin && sensorValues[i] < sensorTh[i]) {
      return 0;
    }
  }
  return 1;
}

void evaluateList (byte * list) {
  for (int i=0; i< COMMANDS_NUM; i++) {
    switch (list[i]) {
      case 0:
        straight (1);
        break;
      case 3:
        straight (-1);
        break;
      case 1:
        turn (1);
        break;
      case 2:
        turn (-1);
        break;
      default:
        return;
    }
  }
}

void straight (int totalSteps) {
  int dir = 1;
  unsigned long startStraight;
  unsigned int cycle = 0;
  int leftCorrection = 0, rightCorrection = 0;
  currentAngle = absoluteAngle;

  //Serial.print("START: ");
  //Serial.println(currentAngle);
  
    startStraight = millis();
    motors.setSpeeds( 0, 0);
    for (int steps = 0; steps < totalSteps; steps++) {
      while (millis() - startStraight < STRAIGHT_DURATION) {
        motors.setSpeeds( dir * (+STRAIGHT_SPEED + leftCorrection), dir * (+STRAIGHT_SPEED + rightCorrection) );
        
        for (int i=0; i<10; i++) {
          gyro.read();
          reading = (gyro.g.z >>8) <<8; // remove static noise
          angularReading = reading * 17.5 - offset; // compensate for residual noise (almost constant with time)
          currentAngle += angularReading * SAMPLING_TIME / 1000.0; // sampling time is in ms
          delay(10);
       }
        
        stepDeviation = currentAngle - targetAngle;
        
        cycle++;
      
        //Serial.print (stepDeviation);
        //Serial.print (" ");
        
        
        /*if (stepDeviation > 5*STRAIGHT_TH) {
          leftCorrection = stepDeviation / 10 - 30;
        }
        else if (stepDeviation > 2*STRAIGHT_TH) {
          leftCorrection = stepDeviation / 25 - 5;
        }
        else if (stepDeviation > STRAIGHT_TH) {
          leftCorrection = stepDeviation / 50;
        }*/
  
        if (stepDeviation > STRAIGHT_TH) {
          /*leftCorrection =  dir * (stepDeviation / 100 * stepDeviation / 200 + stepDeviation / 125); // quadratic formula based on the response of one Zumo
          if (leftCorrection < - 100 || leftCorrection > 100) {
              leftCorrection = dir * 100;
            }       */                                                                                                                                            // adjustments may be needed for the others
          leftCorrection += 10;
        }
        
        else if (stepDeviation < -STRAIGHT_TH) {
            /*leftCorrection = dir * stepDeviation / 50;
            if (leftCorrection < - 100 || leftCorrection > 100) {
              leftCorrection = - dir * 100;
            }*/
          leftCorrection -= 10;  //---------------------------bisogna controllare se quando la correzione da applicare è in verso opposto, è necessario ripartire da 0 o basta decrementare
        }
        
        Serial.print(" - L: ");
        Serial.println(leftCorrection);
      }
      currentAngle = 0; //--------------------------------ogni step è indipendente ai fini della deviazione (in fase di testing)
    }

    motors.setSpeeds( 0, 0);

    absoluteAngle = targetAngle+stepDeviation;

    Serial.print ("Deviation from straight: ");
    Serial.print(absoluteAngle-targetAngle);

  buzzer.playNote (NOTE_A(4), 125, 15);  
  //delay(250);
}

void turn(int dir) {
  currentAngle = absoluteAngle;

  if (dir == -1) { //right turn
    Serial.println("");
    Serial.println("RIGHT");
      
    targetAngle -= TURN_ANGLE;
    
    motors.setSpeeds (+4*TURN_SPEED, -4*TURN_SPEED);

      while (currentAngle > targetAngle + 5000) {
        gyro.read();
        reading = (gyro.g.z >>8)<<8; // remove static noise
        angularReading = reading * 17.5 - offset; // compensate for residual noise (almost constant with time)
        currentAngle += angularReading * SAMPLING_TIME / 1000; // sampling time is in ms
        //cycles++;
        delay(10);
      }
  
      motors.setSpeeds (+3*TURN_SPEED, -3*TURN_SPEED);
      while (currentAngle > targetAngle + THRESHOLD) {
        gyro.read();
        reading = (gyro.g.z >>8)<<8; // remove static noise
        angularReading = reading * 17.5 - offset; // compensate for residual noise (almost constant with time)
        currentAngle += angularReading * SAMPLING_TIME / 1000; // sampling time is in ms
        //cycles++;
        delay(10);
      }
    }

  else if (dir == +1) { //left turn
    Serial.println("");
    Serial.println("LEFT");

      
    targetAngle += TURN_ANGLE;
    
    motors.setSpeeds (- 4*TURN_SPEED, + 4*TURN_SPEED);

      while (currentAngle < targetAngle - 5000) {
        gyro.read();
        reading = (gyro.g.z >>8)<<8; // remove static noise
        angularReading = reading * 17.5 - offset; // compensate for residual noise (almost constant with time)
        currentAngle += angularReading * SAMPLING_TIME / 1000; // sampling time is in ms
        //cycles++;
        delay(10);
      }
      motors.setSpeeds (- 3*TURN_SPEED, + 3*TURN_SPEED);
      while (currentAngle < targetAngle - THRESHOLD) {
        gyro.read();
        reading = (gyro.g.z >>8)<<8; // remove static noise
        angularReading = reading * 17.5 - offset; // compensate for residual noise (almost constant with time)
        currentAngle += angularReading * SAMPLING_TIME / 1000; // sampling time is in ms
        //cycles++;
        delay(10);
      }
    }
  
  motors.setSpeeds(0,0);
  absoluteAngle = currentAngle;
  
  Serial.print(absoluteAngle);
  Serial.print(" ");
  Serial.print (targetAngle);
  Serial.print (" ");
  Serial.println (absoluteAngle - targetAngle);

  buzzer.playNote (NOTE_A(4), 125, 15);        
  delay(250); 
}

void setFullScale() {
  byte fsreg = gyro.readReg(0x23); // CTRL_REG4
  byte bitmask = 0b01 << 4; // set full scale to 500 dps
  fsreg = fsreg | bitmask;
  gyro.writeReg(0x23, fsreg);
  
  if (gyro.readReg(0x23) != bitmask) { // Check for errors
    Serial.println("Failed to set new FS!");
  }
}

void checkOffset() {
  int redefinedOffset = 0;
  
  for (int i=0; i<1000; i++) { // execute some readings when not moving
    gyro.read();
    reading = (gyro.g.z >>8) << 8;
    angularReading = reading * 17.5;
    currentOffset += angularReading;
    Serial.println(angularReading);
  }
  
  currentOffset /= 1000;

  if (currentOffset > 0) {
    while ( !(abs(currentOffset - redefinedOffset) < W_OFFSET/2) ) { // the only possible values that the offset can assume are multiple of 64*17.5
      redefinedOffset += W_OFFSET;                                                                                                                     // therefore the mean is not directly used, but the nearest multiple of 1120 is
    }
  }  
  else {
     while ( !(abs(currentOffset - redefinedOffset) < W_OFFSET/2) ) { 
      redefinedOffset -= W_OFFSET;                                                                                                                     
    } 
  }
  
    
  Serial.print("Current: ");
  Serial.print(currentOffset);
  Serial.print(" -> ");
  Serial.println(redefinedOffset);

  offset = redefinedOffset;
  Serial.println("Redefined");
}

