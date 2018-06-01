#include <TimeLord.h>
#include <DS3231.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 4                //Change this number to the correct pin
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
// Init the DS3231 clock using the harware interface
DS3231 rtc(SDA,SCL);

//Variable Declarations
int up = 0; //bool, if 1, voltage between in1 and in2 raises door
int doorState = 1; //bool, 1=open, 0=closed
int fail = 0; //bool, if 1 a function has failed
int topPin = A7;
int bottomPin = A14;
int enA = 9; 
int in1 = 6;
int in2 = 7;
int enB = 10;
int in3 = 3;
int in4 = 5;
int solenoidPin = 8;                  //Change this number to the correct pin
float Celcius=0;
float Fahrenheit=0;
// To find Lat and Lon, go to google maps and hover over a point.
float const LONGITUDE = -121.74;
float const LATITUDE = 38.54;
// tell TimeLord what timezone your RTC is synchronized to. You can ignore DST
int UTC_Timezone = -8;
//Initialize arrays to hold times
char todaysDate[8] = {0,0,0,0,0,0,0,0}; 
char todaysTime[5] = {0,0,0,0,0}; 
TimeLord Davis; //Declare a city object

//Function Declarations
int initializeDoor(int enA, int in1, int in2, int enB, int in3, int in4, int topPin, int bottomPin);
int raiseDoor(int up, int enA, int in1, int in2, int enB, int in3, int in4, int topPin, int bottomPin, int doorState, int fail);
int closeDoor(int up, int enA, int in1, int in2, int enB, int in3, int in4, int topPin, int bottomPin, int doorState, int fail);

void setup() {
  // put your setup code here, to run once:
  delay(1000); //wait a second for the program to start
  Serial.begin(115200);
  //Initialize the rtc object
  rtc.begin();
  //Set the time if it hasn't been set already
  rtc.setTime(10, 46, 0);     // Set the time to Hr:Min:Sec (24hr format)
  rtc.setDate(25, 5, 2018);   // Set the date (day, month, year)
  Davis.TimeZone(UTC_Timezone * 60); 
  Davis.Position(LATITUDE, LONGITUDE); // tell TimeLord where in the world we are
  Davis.DstRules(3,2,11,1, 60); //Daylight savings starts second sunday in march thru first
  
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  pinMode(topPin, INPUT);
  pinMode(bottomPin, INPUT);
  pinMode(solenoidPin, OUTPUT);    //Sets the solenoid as an output

  up = initializeDoor(enA, in1, in2, enB, in3, in4, topPin, bottomPin);
  if (up == 1){
    doorState = 1;
  }//if
  else{
    doorState = 0;
  }//else
  Serial.println("The door is now in the ");
  Serial.print(up); 
  Serial.print(" position");
  delay(5000); //give enough time for the pin to discharge 
}//setup()


void loop(){
  memcpy(todaysTime, rtc.getTimeStr(FORMAT_SHORT), sizeof(todaysTime)); //fills todaysTime
  memcpy(todaysDate, rtc.getDateStr(FORMAT_SHORT), sizeof(todaysDate)); //fills todaysDate
  //Convert date and time to integer variables
    char cHour[3]  = {todaysTime[0], todaysTime[1], '\0'};
    char cMin[3]   = {todaysTime[3], todaysTime[4], '\0'};
    char cDay[3]   = {todaysDate[0], todaysDate[1], '\0'};
    char cMonth[3] = {todaysDate[3], todaysDate[4], '\0'};
    char cYear[3]  = {todaysDate[6], todaysDate[7], '\0'};
    int Hour = atoi(cHour);
    int Min = atoi(cMin);
    int Day = atoi(cDay);
    int Month = atoi(cMonth);
    int Year = atoi(cYear); 
    //Calculate Sunset and Sunrise
    byte today[] = {0, Min, Hour, Day, Month, 18}; // store today's date in an array {sec, min, hour, day, month, year (last 2 digits of year)}
    byte sunriseTime[] = {0, 0, 0, today[3],today[4],today[5]}; //initialize to be 8:00
    byte sunsetTime[] = {0,0,0,today[3],today[4],today[5]}; //initialize to size of today
    Davis.SunRise(sunriseTime); //changes sunriseTime to the true value
    Davis.SunSet(sunsetTime); //same as above
    Davis.DST(sunriseTime); //updates the sunrise array for daylight savings time
    Davis.DST(sunsetTime); //same for sunset array
    
  if(Hour < sunriseTime[2] && doorState == 1 && fail == 0){//Before Sunrise and door is open
    Serial.println("Before sunrise and door is open");
    doorState = closeDoor(up, enA, in1, in2, enB, in3, in4, topPin, bottomPin, doorState, fail);
    if (doorState == 1){//door state should be 0, something went wrong
      fail = 1;
    }
    else{
    }
    Serial.println("Door now closed!");
  }
  else if(Hour > sunriseTime[2] && Hour < (sunsetTime[2]+1) && doorState == 0 && fail == 0){//After Sunrise and door is closed
    Serial.println("After sunrise and door is closed");   
    doorState = raiseDoor(up, enA, in1, in2, enB, in3, in4, topPin, bottomPin, doorState, fail);
    if (doorState == 0){//door state should be 1, something went wrong
      fail = 1;
    }
    else{
    }    
    Serial.println("Door now open!");
  }
  else if (Hour > (sunsetTime[2]+1) && doorState == 1 && fail == 0){ //After Sunset  and door is open
    Serial.println("After sunset and door is open");
    doorState = closeDoor(up, enA, in1, in2, enB, in3, in4, topPin, bottomPin, doorState, fail);
    if (doorState == 1){//door state should be 0, something went wrong
      fail = 1;
    }
    else{
    }    
  Serial.println("Door now closed!");  
}//else if
    else{
      Serial.println("Do nothing!");
    }//Do nothing
    delay(10000);
}//loop()




int initializeDoor(int enA, int in1, int in2, int enB, int in3, int in4, int topPin, int bottomPin){
  //Function turns the motor in a direction until hitting a stop
  int up; //Boolean that tracks what direction the motor turns
  int topValue = 0; //stores the analog to digital reading for pin voltage
  int bottomValue = 0;
  Serial.println("Turning the motor!");
  while(topValue<900 && bottomValue < 900){
    //turn the motor until it hits a switch
    digitalWrite(in3, LOW);
    digitalWrite(in4, LOW);
    digitalWrite(enB, LOW);
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(enA, HIGH);
    topValue = analogRead(topPin);
    bottomValue = analogRead(bottomPin);
  }
  //cut off the motor
  digitalWrite(in1, LOW);
  digitalWrite(enA, LOW);
  //Store the result
  if(topValue >= 900){
    //voltage between in1 and in2 raised the motor
    up = 1;
  }
  else{
    up = 0; 
    //voltage difference between in1 and in2 raised the motor
  }
  //reverse the motor for .1 seconds
  digitalWrite(in3, HIGH);
  digitalWrite(enB, HIGH);
  delay(100); //pause for .1 se
  digitalWrite(in3, LOW);
  digitalWrite(enB, LOW);
  delay (5000); //Wait 5 seconds for the pins to discharge
  return up; 
}//initializeDoor

int raiseDoor(int up, int enA, int in1, int in2, int enB, int in3, int in4, int topPin, int bottomPin, int doorState, int fail){
  if (fail == 1){
    return doorState;
  }
  else{
  Serial.println("Raising the door!"); 
  int topValue = 0; //stores the analog to digital reading for pin voltage
  int bottomValue = 0;
  if (up == 1){
    while(topValue<= 900 && bottomValue <=900){
    //turn the motor until it hits a switch
    digitalWrite(in3, LOW);
    digitalWrite(in4, LOW);
    digitalWrite(enB, LOW);
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(enA, HIGH);
    //check if pins have been pressed
    topValue = analogRead(topPin);
    bottomValue = analogRead(bottomPin);
  }//while
  //stop and reverse the motor for .1 seconds
  digitalWrite(in1, LOW);
  digitalWrite(enA, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(enB, HIGH);
  delay(100);
  digitalWrite(in3, LOW);
  digitalWrite(enB, LOW);
  }//if up==1
  else{ //up==0
    while(topValue<= 900 && bottomValue <=900){
    //turn the motor until it hits a switch
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    digitalWrite(enB, HIGH);
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    digitalWrite(enA, LOW);
    //check if pins have been pressed
    topValue = analogRead(topPin);
    bottomValue = analogRead(bottomPin);    
  }//while
  //stop and reverse the motor for 1 second
  digitalWrite(in3, LOW);
  digitalWrite(enB, LOW);
  digitalWrite(in1, HIGH);
  digitalWrite(enA, HIGH);
  delay(100); 
  digitalWrite(in1, LOW);
  digitalWrite(enA, LOW);
  }//else
  if (topValue == 1){
    doorState = 1;
    Serial.println("The Door is now open!");
  }
  else{
    doorState = 0; // the door closed instead of opening
  }
  return doorState;
  }
}//raiseDoor





int closeDoor(int up, int enA, int in1, int in2, int enB, int in3, int in4, int topPin, int bottomPin, int doorState, int fail){
  if (fail == 1){
    return doorState;
  }
  else{
  int topValue = 0; //stores the analog to digital reading for pin voltage
  int bottomValue = 0;  
  if (up == 1){
    while(topValue<=900 && bottomValue<=900){
    //turn the motor until it hits a switch
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    digitalWrite(enB, HIGH);
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    digitalWrite(enA, LOW);
    //check if pins have been pressed
    topValue = analogRead(topPin);
    bottomValue = analogRead(bottomPin);   
  }//while
  //stop and reverse the motor for .1 seconds
  digitalWrite(in3, LOW);
  digitalWrite(enB, LOW);
  digitalWrite(in1, HIGH);
  digitalWrite(enA, HIGH);
  delay(100);
  digitalWrite(in1, LOW);
  digitalWrite(enA, LOW);
  }//if
  else{
    while(topValue<=900 && bottomValue<=900){
    //turn the motor until it hits a switch
    digitalWrite(in3, LOW);
    digitalWrite(in4, LOW);
    digitalWrite(enB, LOW);
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(enA, HIGH);
    //check if pins have been pressed
    topValue = analogRead(topPin);
    bottomValue = analogRead(bottomPin); 
  }//while
  //stop and reverse the motor for .1 seconds
  digitalWrite(in1, LOW);
  digitalWrite(enA, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(enB, HIGH);
  delay(100); 
  digitalWrite(in3, LOW);
  digitalWrite(enB, LOW);
  }//else
  if (bottomValue>900){
   doorState = 0; //the door is now closed 
  Serial.println("The door is now closed!");   
  }
  else{
    doorState = 1; //the door opened (this is bad)
  }
  return doorState;
  }
    
  // Acquire temperature data
  sensors.requestTemperatures(); 
  Celcius=sensors.getTempCByIndex(0);
  Fahrenheit=sensors.toFahrenheit(Celcius);
  // Prints the temperatures that the sensor reads
  Serial.print(" C  ");
  Serial.print(Celcius);
  Serial.print(" F  ");
  Serial.println(Fahrenheit);

  // Uses temperature data to determine whether or not to turn on solenoid
  if (Fahrenheit > 70) {    // Set desired critical temperature
    digitalWrite(solenoidPin,HIGH);
  }
  else {
    digitalWrite(solenoidPin,LOW);
  }
}//closeDoor
