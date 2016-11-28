
// Gear Position Settings
int gearPosA;                    // side to side position of gear shift 0-1023
int gearPosB;                    // front to back postion of gear shift 0-1023
int gearPinA = 0;                // analog input pin used to determine side to side position of gear shift
int gearPinB = 1;                // analog input pin used to determine front to back position of gear shift
int gear;                        // gear transmission
int gRatio [] = {0,13841,8197,5390,3934,3132}; // Final gear ratios for transmission (gear ration*final drive ratio*1000), 

// Brake Indicator Switch
int brake;                       // brake pedal
int brakepin = 4;                // digital input pin for brake postion

// Clutch Indicator Switch
int clutch;                      // clutch pedal
int clutchpin = 5;               // digital input pin for clutch position

// Vehicle Speed Settings
int speedPin = 3;                // digital input pin used to determine speed of vehicle
float speed_trigger = 4.725;     // 40cm/pulse based in OEM)
unsigned long high_time_speed;
unsigned long low_time_speed;
unsigned int tire_revs_per_mile = 786;     // tire revolutins/mile for 275/40/17
unsigned int tire_oem_revs_per_mile = 852; // tire revolutins/mile for 205/60/14
float period_time_speed;
float freq_speed;
int vSpeed = 0;                  // vehicle speed

// Engine RPM Settings
int rpmPin = 2;                  // digital input pin used to determine engine speed
int rpm_trigger = 3;             // rpm signal has 3 periods per revolution
unsigned long high_time_rpm;
unsigned long low_time_rpm;
float period_time_rpm;
float freq_rpm;
int RPM = 0;                     // engine speed in RPM
int tRPM = 0;                    // targeted RPM to match engine to transmission speed
// volatile int rpmcount = 0;       // counts engine rotations
 
// Throttle Blip Request
int tBlip = 0;                    // variable used to request throttle blip output
float VPA_slope = 0.000097;       // Slope used to calculate tVPA from RPM
float VPA_intercept = 1.026;      // Y-intercept used to calculate tVPA from target RPM

// DAC Settings:
float VPA = 0.8 / 5 * 65536;      // targeted VPA output voltage on DAC channel 1, ranges from 0.8 to 3.75V NOTE: before diode compensation
float VPA2 = 1.6 /5 * 65536;      // targeted VPA2 output voltage on DAC channel 1, ranges from 1.6 to 4.55V NOTE: before diode compensation
int dac_A_input_register = 0x18;  // Select DAC and input register - write to and update DAC channel A
int dac_B_input_register = 0x19;  // Select DAC and input register - write to and update DAC channel B
int dac_AB_input_register = 0x1F; // Select DAC and input register - write to and update DAC channel A+B
float VPA_idle = 0.8;             // Throttle idle voltage 
float VPA_max = 1.8;              // Max voltage to input to VPA -> ECU
float tVPA = VPA_idle;            // Target throttle voltage for blip - will be calculated based on tRPM lookup (initally set fort idle)
float Vcc = 5.0;                  // Configures the Vcc input into the DAC
float Rout = 0.33;                // DAC Output voltage drop accross the diode(s)
#include <Wire.h>                 // link to i2c arduino library
#define Addr 0x0E                 // AD5667 I2C address is 0x0E(14)

// General Settings
// unsigned long lastmillis = 0;

void setup() {                        // put your setup code here, to run once:
                      
  
  Wire.begin();                       // Initialise I2C communication as Master
  Serial.begin(57600);                 //start serial monitor

  // Set VPA and VPA2 DAC output to 0

  Wire.beginTransmission(Addr);       // Start I2C transmission
  Wire.write(dac_AB_input_register);  // Select DAC and input register - write to and udpate dac channel A & B
  Wire.write(0x00);                   // Write data = 0x00 first byte
  Wire.write(0x00);                   // Write data = 0x00 second byte
  Wire.endTransmission();             // Stop I2C transmission
  
  pinMode(brakepin, INPUT);
  pinMode(clutchpin, INPUT);
  pinMode(speedPin, INPUT);

}

void loop() {                           // put your main code here, to run repeatedly:

  // Measures gear position                                  
  gearPosA = analogRead(gearPinA);      //read output from linear pot A
  gearPosB = analogRead(gearPinB);      //read output from linear pot B
  brake = digitalRead(brakepin);
  clutch = digitalRead(clutchpin);

  // Measures vehicle speed
  high_time_speed = pulseIn(speedPin,HIGH);
  low_time_speed = pulseIn(speedPin,LOW);

  period_time_speed = (high_time_speed + low_time_speed) / 1000.0;
  freq_speed = 1000.0/period_time_speed;
  vSpeed = freq_speed * 60 * 60 / speed_trigger / tire_revs_per_mile;   // calculates in MPH

  //Measures engine RPM
  high_time_rpm = pulseIn(rpmPin,HIGH);
  low_time_rpm = pulseIn(rpmPin,LOW);

  period_time_rpm = (high_time_rpm + low_time_rpm) / 1000.0;
  freq_rpm = 1000.0/period_time_rpm;
  RPM = freq_rpm * 60 / rpm_trigger;

  if (gearPosB >= 100 && gearPosB <= 850){
    gear = 0;                      // Neutral
  }
  if (gearPosA < 390 && gearPosB > 800){
    gear = 1;                      // 1st gear
  }
  if (gearPosA < 390 && gearPosB < 100){
    gear = 2;                      // 2nd gear
  }
  if (gearPosA >= 390 && gearPosA <= 750 && gearPosB > 800){
    gear = 3;                      // 3rd gear
  }
  if (gearPosA >= 390 && gearPosA <= 750 && gearPosB < 100){
    gear = 4;                      // 4th gear
  }
  if (gearPosA > 750 && gearPosB > 800){
    gear = 5;                      // 5th gear
  }
  if (gearPosA > 750 && gearPosB < 100){
    gear = 6;                      // Reverse
  }
  
  tRPM = vSpeed * gRatio[gear] / 1000 / 60 / tire_oem_revs_per_mile * (tire_revs_per_mile / tire_oem_revs_per_mile);  // target rpm = vehicle speed * gear ratio

  if (brake == 1 && clutch == 1 && vSpeed >= 25 && tRPM >> RPM && (gear == 2 || gear == 3 || gear == 4))
  {
    tBlip = 1;                                         // request for throttle blip
    tVPA = tRPM * VPA_slope + VPA_intercept;           // Target throttle voltage
    
    if (tVPA > VPA_max)                                // Ensures calculated, target VPA voltage does not exceed max VPA voltage (e.g. incorrect speed measurement or downshift to incorrect gear)
    {
      tVPA = VPA_max;
    }
    
    VPA = tVPA + Rout;                                 // Sets target output voltage for VPA (DAC channel A) plus voltage drop across diodes
    unsigned long VPAdac = VPA * 65536 / Vcc;          // Converts voltage to 16 bit based
    int dac_A_byte_1 = VPAdac / 256;                   // Calculates value for 1st 8bit frame to send to DAC
    int dac_A_byte_2 = VPAdac - (dac_A_byte_1 * 256);  // Calculates value for 2nd 8bit frame to send to DAC

    Wire.beginTransmission(Addr);                      // Start I2C transmission
    Wire.write(dac_A_input_register);                  // Select DAC and input register
    Wire.write(dac_A_byte_1);                          // Write data first byte
    Wire.write(dac_A_byte_2);                          // Write data second byte
    Wire.endTransmission();                            // Stop I2C transmission

    VPA2 = VPA + 0.87;                                  // Sets target output voltage for VP2 (DAC channel B) as 0.8V above VPA
    unsigned long VPA2dac = VPA2 * 65536 / Vcc;        // Converts voltage to 16 bit
    int dac_B_byte_1 = VPA2dac / 256;                  // Calculates value for 1st 8bit frame to send to DAC
    int dac_B_byte_2 = VPA2dac - (dac_B_byte_1 * 256); // Calculates value for 2nd 8bit frame to send to DAC

    Wire.beginTransmission(Addr);                      // Start I2C transmission
    Wire.write(dac_B_input_register);                  // Select DAC and input register - write to and udpate DAC channel B
    Wire.write(dac_B_byte_1);                          // Write data first byte
    Wire.write(dac_B_byte_2);                          // Write data second byte
    Wire.endTransmission();                            // Stop I2C transmission
  }
  
  else
  {
    tBlip = 0;                                         // ensures request for throttle blip stays low
    
    // Returns throttle back to idle state
    VPA = VPA_idle + Rout;                             // Sets target output voltage for VPA (DAC channel A) plus voltage drop across diodes
    unsigned long VPAdac = VPA * 65536 / Vcc;          // Converts voltage to 16 bit based
    int dac_A_byte_1 = VPAdac / 256;                   // Calculates value for 1st 8bit frame to send to DAC
    int dac_A_byte_2 = VPAdac - (dac_A_byte_1 * 256);  // Calculates value for 2nd 8bit frame to send to DAC

    Wire.beginTransmission(Addr);                      // Start I2C transmission
    Wire.write(dac_A_input_register);                  // Select DAC and input register
    Wire.write(dac_A_byte_1);                          // Write data first byte
    Wire.write(dac_A_byte_2);                          // Write data second byte
    Wire.endTransmission();                            // Stop I2C transmission

    VPA2 = VPA + 0.87;                                 // Sets target output voltage for VP2 (DAC channel B) as 0.8V above VPA
    unsigned long VPA2dac = VPA2 * 65536 / Vcc;        // Converts voltage to 16 bit
    int dac_B_byte_1 = VPA2dac / 256;                  // Calculates value for 1st 8bit frame to send to DAC
    int dac_B_byte_2 = VPA2dac - (dac_B_byte_1 * 256); // Calculates value for 2nd 8bit frame to send to DAC

    Wire.beginTransmission(Addr);                      // Start I2C transmission
    Wire.write(dac_B_input_register);                  // Select DAC and input register - write to and udpate DAC channel B
    Wire.write(dac_B_byte_1);                          // Write data first byte
    Wire.write(dac_B_byte_2);                          // Write data second byte
    Wire.endTransmission();                            // Stop I2C transmission
  }
  // float voltageOut = (((dac_A_byte_1 * 256.0) + (dac_A_byte_2)) / 65536.0) * Vcc - Rout;   // Convert the data, Vref = 5 V
  // float voltageOut2 = (((dac_B_byte_1 * 256.0) + (dac_B_byte_2)) / 65536.0) * Vcc - Rout;   // Convert the data, Vref = 5 V


  if (Serial.available())
  {
    // Serial.print("gear ");
    // Serial.println(gear);
    // Serial.print("brake ");
    // Serial.println(brake);
    // Serial.print("clutch ");
    // Serial.println(clutch);
    // Serial.print("request throttle blip? ");
    // Serial.println(tBlip);
    // Serial.print("Target engine RPM ");
    // Serial.println(tRPM);
    // Serial.print("DAC Output Voltage : ");
    // Serial.print("DAC 1 out: ");
    // Serial.println(VPAdac);
    // Serial.print("DAC 1 1st byte: ");
    // Serial.println(dac_A_byte_1);
    // Serial.print("DAC 1 2nd byte: ");
    // Serial.println(dac_A_byte_2);
    // Serial.print("DAC 1 Voltage out: ");
    // Serial.print(voltageOut);
    // Serial.println(" V");
    // Serial.print(voltageOut2);
    // Serial.println(" V");
    // Serial.print("gearPosA ");
    // Serial.println(gearPosA);
    // Serial.print("gearPosB ");
    // Serial.println(gearPosB);
    Serial.print("RPM ");
    Serial.println(RPM);
    // Serial.print("Target RPM ");
    // Serial.println(tRPM);
    Serial.print("Vehicle Speed (MPH) ");
    Serial.println(vSpeed);
    // Serial.print("speedfreq: ");
    // Serial.println(freq_speed);
    // Serial.print("highspeed");
    // Serial.println(high_time_speed);
    // Serial.print("lowspeed");
    // Serial.println(low_time_speed);
    // delay(500);
  }
}
