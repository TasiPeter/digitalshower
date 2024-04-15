//COMMUNICATION
char input_buffer[128] = {0};
unsigned char buffer_pointer = 0;

unsigned char rgb_alpha = 0;
unsigned char rgb_red = 0;
unsigned char rgb_green = 0;
unsigned char rgb_blue = 0;

unsigned char rgb_alpha_temp = 0;
unsigned char rgb_red_temp = 0;
unsigned char rgb_green_temp = 0;
unsigned char rgb_blue_temp = 0;

bool static_color = false;
bool rainbow_effect = false;
bool fill_effect = false;
bool star_effect = false;

int pixelnum = 10;

#define LED_PIN    9
#define LED_COUNT 100

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() { 
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(255);
  Serial.begin(115200);
}

void loop() {   
  read_serial();

  if(static_color) {
    write_rgb_color();
  }  
  else if(rainbow_effect) {
    strip.clear();
    rainbow(50);
  }  
  else if(fill_effect) {   
    colorWipe(40);
  } 
  else if(star_effect)
  {
    strip.clear();
    star(5);
  }
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
    
    //Serial.print(key_i);
    //Serial.print(":");
    //Serial.println(value_f);

    //Clear these variables even tho they are locally scoped. Weird.
    for(int i = 0; i < 10; i++) {
      key_c[i] = 0;
      value_c[i] = 0;
    }

    switch(key_i)     //Assign value_f to variables based on key_i
    {
      case 5: rgb_red = value_f;    static_color = true; rainbow_effect = false; fill_effect = false; star_effect = false; pixelnum = 10; break;
      case 6: rgb_green = value_f;  static_color = true; rainbow_effect = false; fill_effect = false; star_effect = false; pixelnum = 10;break;
      case 7: rgb_blue = value_f;   static_color = true; rainbow_effect = false; fill_effect = false; star_effect = false; pixelnum = 10;break;
      case 10: rainbow_effect = true;  static_color = false; fill_effect = false; star_effect = false; pixelnum = 10;break;
      case 11: if(fill_effect) {pixelnum++;} else {fill_effect = true;}     static_color = false; rainbow_effect = false; star_effect = false; break;
      case 12: star_effect = true;     static_color = false; rainbow_effect = false; fill_effect = false; pixelnum = 10; break;
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

void write_rgb_color() {
 for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, rgb_red, rgb_blue, rgb_green );         //  Set pixel's color (in RAM)
  }
  strip.show();   
  static_color = false;                       
}

void colorWipe(int wait) {

  
  int position = 0;

  while(true) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      if(i >= position && i <= (position + pixelnum)) {
        strip.setPixelColor(i, rgb_red, rgb_blue, rgb_green);  //  Set pixel's color (in RAM)
      }
      else if ((position + pixelnum) > strip.numPixels()) {
        int extra = (position + pixelnum) - strip.numPixels();
        if(i < extra) {
          strip.setPixelColor(i, rgb_red, rgb_blue, rgb_green);  //  Set pixel's color (in RAM)
        }
        else {
          strip.setPixelColor(i, 0,0,0);
        }
      }
      else {
        strip.setPixelColor(i, 0,0,0);  //  Set pixel's color (in RAM)
      }
    }

    position++;
    if(position >= strip.numPixels()) {
      position = 0;
    }

    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
    read_serial();
    if(!fill_effect) {
      return;
    }
  }  
}

void rainbow(int wait) {
  
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {    
    strip.rainbow(firstPixelHue);   
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
    read_serial();
    if(!rainbow_effect) {
      return;
    }
  }
}

void star(int wait) {  
  long counter = 0;
  const int amount = 15;

  int position[amount];
  int value[amount];
  bool stage[amount];

  for(int i = 0; i < amount; i++) {
    position[i] = random(0,100);  
    value[i] = random(0,255);
    stage[i] = random(0,1);
  }

  while(true) {

    for(int i = 0; i < amount; i++) {
      strip.setPixelColor(position[i], value[i], value[i], value[i]);    
      if(stage[i]) { value[i]--; }
      if(!stage[i]) { value[i]++; }
      if(value[i] == 255) { stage[i] = !stage[i]; }
      if(value[i] == 0) { 
        strip.setPixelColor(position[i], 0, 0, 0);   
        stage[i] = !stage[i]; 
        position[i] = random(0,100);  
      }
    }

    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
    read_serial();
    if (!star_effect) {
      return;
    }
  }
}