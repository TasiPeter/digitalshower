#include <OneWire.h>
#include <DallasTemperature.h>
#include <Arduino.h>
#include "AsyncTimer.h"

//IO DEFINITION
#define temp_valve_open 2
#define temp_valve_close 3
#define press_valve_open 4
#define press_valve_close 5
#define shower_solenoid 6
#define rainer_solenoid 7

#define scl 13  
#define sda 12  
#define cs_1 10
#define cs_2 11

#define TRUE 1
#define FALSE 0

//INTERNAL VARIABLES
float set_temp = 0.0;
float actual_temp = 0.0;

float temp_error_treshold = 1.0;
float temp_proportional = 1.0;

unsigned char set_press = 1;
unsigned char actual_press = 0;

bool shower_on = false;
bool rainer_on = false;
bool run_warmup = false;

float valve_pos_limit_max = 90.0;
float valve_pos_limit_min = 0.0;

float valve_pos_backlash = 13.0;

float valve_pos_temp = 1.23;
float valve_pos_temp_setpoint = 10.0;
float valve_pos_temp_offset = -20;
unsigned char temp_valve_move_direction = 0; //0: do not move, 1: decrement 2: increment
unsigned char temp_valve_move_direction_last = 0;    //Store the last direction to compensate for backlash

float valve_pos_press = 1.23;
float valve_pos_press_setpoint = 90;
float valve_pos_press_offset = -45;
unsigned char press_valve_move_direction = 0; //0: do not move, 1: decrement 2: increment
unsigned char press_valve_move_direction_last = 0;    //Store the last direction to compensate for backlash

//COMMUNICATION
char input_buffer[128] = {0};
unsigned char buffer_pointer = 0;
char output_buffer[128] = {0};
bool general_interrogation = true;

//TIMING
double write_serial_time;
double temp_reading_time;
double encoder_reading_time;
AsyncTimer temp_valve_position_timer;
AsyncTimer temp_settle_timer;
AsyncTimer temp_valve_moving_timer;
AsyncTimer press_valve_position_timer;
AsyncTimer press_settle_timer;
AsyncTimer press_valve_moving_timer;

//DS18B20 TEMP SENSOR
#define ONE_WIRE_BUS 8
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature temp_sensor(&oneWire);

void setup() {
  //Start serial port
  Serial.begin(115200);

  //Start up temp sensor
  temp_sensor.begin();
  temp_sensor.setWaitForConversion(false);
  temp_sensor.requestTemperatures();
  delay(50);
  set_temp = temp_sensor.getTempCByIndex(0);  //Initialize the set temp with the actual temp. So at startup, the temp control loop will chill.

  pinMode(temp_valve_open, OUTPUT);
  pinMode(temp_valve_close, OUTPUT);

  pinMode(press_valve_open, OUTPUT);
  pinMode(press_valve_close, OUTPUT);

  pinMode(shower_solenoid, OUTPUT);
  pinMode(rainer_solenoid, OUTPUT);

  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT);
  pinMode(cs_1, OUTPUT);
  pinMode(cs_2, OUTPUT);
  digitalWrite(cs_1, HIGH);  
  digitalWrite(cs_2, HIGH);  
}

void loop() {  
  read_serial();
  read_temp();  
  read_encoders();
  temp_control_loop();  
  press_control_loop();    

  if(run_warmup) {    
    shower_on = true;
    rainer_on = true;
    set_press = 10;    
    set_temp = 32.0;

    if(actual_temp >= set_temp - 1.0 && actual_temp <= set_temp + 1.0 ) {
      run_warmup = false;  
      set_press = 5;  
      rainer_on = false;
      general_interrogation = true;    
    }
  }

  update_temp_valve_position();  
  update_press_valve_position(); 
  write_serial(); 
     
  temp_valve_position_timer.update();
  temp_settle_timer.update();  
  temp_valve_moving_timer.update();
  press_valve_position_timer.update();
  press_settle_timer.update();
  press_valve_moving_timer.update();

  digitalWrite(shower_solenoid, shower_on);
  digitalWrite(rainer_solenoid, rainer_on);  
}

void read_serial() {
  //Read serial port
  while(Serial.available() > 0){
    input_buffer[buffer_pointer] = Serial.read();   //Read one byte, then store it in the buffer
    buffer_pointer++;   
    if(buffer_pointer >= 128) { clear_buffer();}    //If the buffer is filled, clear it.
  }

  if(input_buffer[0] == '#' && input_buffer[buffer_pointer - 1] == '\n') {     //If the first character is # and the last is newline, thats a package
    
    /*
    package structure: #KEY:VAL/n
    eg: [#][1][0][:][3][5][.][5][0][/n]
    means: KEY=10, VAL = 35.50
    */

    char key_c[10];
    char value_c[10];

    for(int i = 0; i < buffer_pointer; i++)   //Loop trough the buffer
    {
      if(input_buffer[i] == ':')              //When we find the semicolon
      {
        for(int j = 1; j < i; j++)            //Copy the part before the semicolon to the key_c
        {
          key_c[j-1] = input_buffer[j];
          //Serial.print(input_buffer[j]);
        }
        int l = 0;
        for(int k = i + 1; k < buffer_pointer; k++)   //And copy the part after the semicolon to value_c
        {
          value_c[l] = input_buffer[k];
          l++;
        }
        break;
      }      
    }

    int key_i = atoi(key_c);        //Convert them to int and float
    float value_f = atof(value_c);

    //Clear these variables even tho they are locally scoped. Weird.
    for(int i = 0; i < 10; i++) {
      key_c[i] = 0;
      value_c[i] = 0;
    }

    switch(key_i)     //Assign value_f to variables based on key_i
    {
      case 1: set_temp = value_f; break;
      case 2: set_press = value_f; break;
      case 3: shower_on = value_f; break;
      case 4: rainer_on = value_f; break;      
      case 8: run_warmup = value_f; general_interrogation = true; break;
      case 9: general_interrogation = true; break;
    }
    
    clear_buffer();     //Clear input buffer
  }
}

void clear_buffer() {
  while(buffer_pointer) {
    input_buffer[buffer_pointer] = 0;
    buffer_pointer--;
  }
}

void read_temp() {
  //Read the sensor every 500ms
  if((millis() - 500) > temp_reading_time) {
    temp_reading_time = millis(); 

    actual_temp = temp_sensor.getTempCByIndex(0);   //Sensor reading is stored in global variable
    temp_sensor.requestTemperatures();              //Request a new one
  }  
}

void write_serial() {
  //Send serial data as JSON string every 500ms
  if((millis() - 500) > write_serial_time) {
    write_serial_time = millis();

    //Always send the actual temp
    Serial.print("{\"actual_temp\":");
    Serial.print(actual_temp);
    
    Serial.print("\"set_temp\":");
    Serial.print(set_temp);
    Serial.print("\"valve_pos_temp\":");
    Serial.print(valve_pos_temp);
    Serial.print("\"valve_pos_temp_setpoint\":");
    Serial.print(valve_pos_temp_setpoint);
    Serial.print("\"temp_valve_move_direction\":");
    Serial.print(temp_valve_move_direction);
    Serial.print("\"temp_valve_move_direction_last\":");
    Serial.print(temp_valve_move_direction_last);
 
    if(general_interrogation) {
      Serial.print(",\"set_temp\":");
      Serial.print(set_temp);
      Serial.print(",\"set_press\":");
      Serial.print(set_press);
      Serial.print(",\"shower_on\":");
      Serial.print(shower_on);
      Serial.print(",\"rainer_on\":");
      Serial.print(rainer_on);    
      Serial.print(",\"run_warmup\":");
      Serial.print(run_warmup);         
      general_interrogation = false; 
    }
    
    Serial.println("}"); 
   }
}

void temp_control_loop() {
  //If the valve is moving, do not command new position.
  if(temp_valve_move_direction) {
    return;
  }
  //If the moving timer is not running, so moving command is not active.
  if(!temp_valve_moving_timer.running) {
    //Check the temperature error. Positive values: need to raise temp. Negative values: lower the temp
    double temp_error = set_temp - actual_temp; 
    //If the error is less than a given amount, we are satisfied.
    if(abs(temp_error) < temp_error_treshold) {
      return;
    }
    //Based on error, open, or close the valve.
    valve_pos_temp_setpoint = valve_pos_temp_setpoint + (temp_error * temp_proportional);
    //If the new setpoint is outside of valid range, change it to be within range
    if(valve_pos_temp_setpoint > valve_pos_limit_max) {
      valve_pos_temp_setpoint = valve_pos_limit_max;
    }
    if(valve_pos_temp_setpoint < valve_pos_limit_min) {
      valve_pos_temp_setpoint = valve_pos_limit_min;
    }
    //After setting new valve position command, wait for the effect
    temp_valve_moving_timer.start(1000);
  }

  //If the timer is finished
  if(temp_valve_moving_timer.on_tick()) {
    //Stop timer, so in the next cycle, we can restart the cycle (running is not true)
    temp_valve_moving_timer.stop();
  }
  
  //Valve position command is proportional to the temp error
}

void update_temp_valve_position() {
  bool fast_move = false;
  const float delta_pos = abs(valve_pos_temp - valve_pos_temp_setpoint);
  
  if((valve_pos_temp - valve_pos_temp_setpoint) > 0 ) {
    if(delta_pos > 180) {
      temp_valve_move_direction = 2;
    }    
    else {
      temp_valve_move_direction = 1; 
    }
  }
  else {
    if(delta_pos > 180) {   
      temp_valve_move_direction = 1;   
    }  
    else {
      temp_valve_move_direction = 2;
    }
  }

  if(delta_pos > 180) { //If the difference between the two is larger than 180 degrees, then we calculate the difference differently.
    if(360 - (delta_pos) > 5) { //In this case we calculate the difference by (360 - |current - setpoint|)
      fast_move = true;
    }
    else if(360 - (delta_pos) < 0.25) {
      temp_valve_move_direction = 0;
    }

  }
  else {
    if(delta_pos > 5) { //In this case we calculate the difference by (|current - setpoint|)
      fast_move = true;
    }
    else if(delta_pos < 0.25) { //In this case we calculate the difference by (|current - setpoint|)
      temp_valve_move_direction = 0;
    }
  }

  if(temp_valve_move_direction == 1 || temp_valve_move_direction == 2) {    //If the direction is a move command
    if(temp_valve_move_direction != temp_valve_move_direction_last) {       //If we changed direction
      temp_valve_move_direction_last = temp_valve_move_direction;           //Save the current direction

      if(temp_valve_move_direction == 1) {                                   //If we decrement, substract the backlash
        valve_pos_temp_setpoint = valve_pos_temp_setpoint - valve_pos_backlash;
      }
      if(temp_valve_move_direction == 2) {                                   //If increment, add it
        valve_pos_temp_setpoint = valve_pos_temp_setpoint + valve_pos_backlash;
      }
      
    }
  }

  if(temp_valve_move_direction == 2 && fast_move){
    digitalWrite(temp_valve_open, TRUE);
    digitalWrite(temp_valve_close, FALSE);
  }
  else if(temp_valve_move_direction == 1 && fast_move) {
    digitalWrite(temp_valve_open, FALSE);
    digitalWrite(temp_valve_close, TRUE);
  }

  if(temp_valve_move_direction == 2 && !fast_move && !temp_valve_position_timer.running && !temp_settle_timer.running){
    digitalWrite(temp_valve_open, TRUE);
    digitalWrite(temp_valve_close, FALSE);
    temp_valve_position_timer.start(50);
  }
  else if(temp_valve_move_direction == 1 && !fast_move && !temp_valve_position_timer.running && !temp_settle_timer.running) {
    digitalWrite(temp_valve_open, FALSE);
    digitalWrite(temp_valve_close, TRUE);
    temp_valve_position_timer.start(50);
  }

  if(temp_valve_move_direction == 0) {
    digitalWrite(temp_valve_open, FALSE);
    digitalWrite(temp_valve_close, FALSE);
  }

  if(temp_valve_position_timer.on_tick()) {
    digitalWrite(temp_valve_open, FALSE);
    digitalWrite(temp_valve_close, FALSE);
    temp_valve_position_timer.stop();
    temp_settle_timer.start(100);
  }

  if(temp_settle_timer.on_tick()) {
    temp_settle_timer.stop();
  }
}

void press_control_loop() {
  valve_pos_press_setpoint = set_press * 9;

  if(valve_pos_press_setpoint > valve_pos_limit_max) {
    valve_pos_press_setpoint = valve_pos_limit_max;
  }
  if(valve_pos_press_setpoint < valve_pos_limit_min) {
    valve_pos_press_setpoint = valve_pos_limit_min;
  }
}

void update_press_valve_position() {
  bool fast_move = false;
  const float delta_pos = abs(valve_pos_press - valve_pos_press_setpoint);
  
  if((valve_pos_press - valve_pos_press_setpoint) > 0 ) {
    if(delta_pos > 180) {
      press_valve_move_direction = 2;
    }    
    else {
      press_valve_move_direction = 1; 
    }
  }
  else {
    if(delta_pos > 180) {   
      press_valve_move_direction = 1;   
    }  
    else {
      press_valve_move_direction = 2;
    }
  }

  if(delta_pos > 180) { //If the difference between the two is larger than 180 degrees, then we calculate the difference differently.
    if(360 - (delta_pos) > 5) { //In this case we calculate the difference by (360 - |current - setpoint|)
      fast_move = true;
    }
    else if(360 - (delta_pos) < 0.25) {
      press_valve_move_direction = 0;
    }

  }
  else {
    if(delta_pos > 5) { //In this case we calculate the difference by (|current - setpoint|)
      fast_move = true;
    }
    else if(delta_pos < 0.25) { //In this case we calculate the difference by (|current - setpoint|)
      press_valve_move_direction = 0;
    }
  }

  if(press_valve_move_direction == 1 || press_valve_move_direction == 2) {    //If the direction is a move command
    if(press_valve_move_direction != press_valve_move_direction_last) {       //If we changed direction
      press_valve_move_direction_last = press_valve_move_direction;           //Save the current direction

      if(press_valve_move_direction == 1) {                                   //If we decrement, substract the backlash
        valve_pos_press_setpoint = valve_pos_press_setpoint - valve_pos_backlash;
      }
      if(press_valve_move_direction == 2) {                                   //If increment, add it
        valve_pos_press_setpoint = valve_pos_press_setpoint + valve_pos_backlash;
      }
      
    }
  }

  if(press_valve_move_direction == 2 && fast_move){
    digitalWrite(press_valve_open, TRUE);
    digitalWrite(press_valve_close, FALSE);
  }
  else if(press_valve_move_direction == 1 && fast_move) {
    digitalWrite(press_valve_open, FALSE);
    digitalWrite(press_valve_close, TRUE);
  }

  if(press_valve_move_direction == 2 && !fast_move && !press_valve_position_timer.running && !press_settle_timer.running){
    digitalWrite(press_valve_open, TRUE);
    digitalWrite(press_valve_close, FALSE);
    press_valve_position_timer.start(50);
  }
  else if(press_valve_move_direction == 1 && !fast_move && !press_valve_position_timer.running && !press_settle_timer.running) {
    digitalWrite(press_valve_open, FALSE);
    digitalWrite(press_valve_close, TRUE);
    press_valve_position_timer.start(50);
  }

  if(press_valve_move_direction == 0) {
    digitalWrite(press_valve_open, FALSE);
    digitalWrite(press_valve_close, FALSE);
  }

  if(press_valve_position_timer.on_tick()) {
    digitalWrite(press_valve_open, FALSE);
    digitalWrite(press_valve_close, FALSE);
    press_valve_position_timer.stop();
    press_settle_timer.start(100);
  }

  if(press_settle_timer.on_tick()) {
    press_settle_timer.stop();
  }
}

void read_encoders() {
  
  if((millis() - 100) > encoder_reading_time) {
    encoder_reading_time = millis(); 

    unsigned int raw_input = read_encoder(1);
    valve_pos_temp = ((raw_input / 16384.0) * 360.0) + valve_pos_temp_offset;

    raw_input = read_encoder(2);
    valve_pos_press = ((raw_input / 16384.0) * 360.0) + valve_pos_press_offset;
  }
}

uint16_t read_encoder(unsigned char device_id) {
  // Wait for the SSI device to initiate the frame sync
  if(device_id == 1) { 
    digitalWrite(cs_1, LOW);
  }
  else if(device_id == 2) { 
    digitalWrite(cs_2, LOW);
  }
  else
  { return 0;}

  // Read the SSI data bits MSB first
  uint16_t response = 0;
  for (int i = 13; i >= 0; i--) {
    digitalWrite(scl, HIGH);
    __asm__("nop\n\t");//delayMicroseconds(1);
    response |= digitalRead(sda) << i;
    digitalWrite(scl, LOW);
    __asm__("nop\n\t");//delayMicroseconds(1);
  }
 
  if(device_id == 1) { 
    digitalWrite(cs_1, HIGH);
  }
  else if(device_id == 2) { 
    digitalWrite(cs_2, HIGH);
  }

  return response;  
}

