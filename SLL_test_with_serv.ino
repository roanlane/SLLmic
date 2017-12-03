#include <Servo.h> 
#include "rgb_lcd.h"
#include <SoftwareSerial.h>
#include <SLL.h> 
int serv_az = 9;
int serv_ze = 10;
int pin_array[4] = {0 ,1 ,2 ,3};
int* f_angle;
int* lenght;
int angle_transmit[2] = {0};
byte transmit_data[8] = {0};
bool is_master = true;
int distance_be_mic_arr = 50;

Servo myservo;
rgb_lcd lcd;
SLL sll(pin_array, 4, 128  );
SoftwareSerial mySerial(7, 8); // RX, TX

void setup() {
lcd.begin(16, 2);
Serial.begin(9600);
Serial.setTimeout(20);//устанавливается время ожидания поступлени символа 20мс.}
mySerial.begin(9600);
pinMode(2, INPUT);
pinMode(3, INPUT);
pinMode(4, INPUT);
pinMode(5, INPUT);
pinMode(6, INPUT);
}

void loop() {
 sll.Initialization(4,16,0,CROSS_CORRELATION, 1000, 6000);
 f_angle = sll.Find_angle(50);
 int* delta = sll.Get_delta();
 int lenght_nul[3] = {0};
 lenght = lenght_nul;
 if(!mySerial.available() && !is_master){
  mySerial.print(',');
  mySerial.print(f_angle[0]);
  mySerial.print(',');
  mySerial.print(f_angle[1]);
  delay(20);
 }
 else if(!mySerial.available() && is_master){
  delay(20); 
  clear_buffer();
  read_buffer();
  //int distance_between_mic = distance_be_mic();
  lenght = sll.Find_distance(angle_transmit[0],angle_transmit[1],50);
 }
 else{
  clear_buffer();
  read_buffer();
  //int distance_between_mic = distance_be_mic();
  lenght = sll.Find_distance(angle_transmit[0],angle_transmit[1],50);
 }
 Serial.print("delta");
 Serial.print(delta[0]);
 Serial.print(" ");
 Serial.println(delta[1]);
 Serial.print("angle");
 Serial.print(f_angle[0]);
 Serial.print(" ");
 Serial.println(f_angle[1]);
 Serial.print("angle_transmit");
 Serial.print(angle_transmit[0]);
 Serial.print(" ");
 Serial.println(angle_transmit[1]);
 for (int i =0; i<=59; i++){    
   Serial.print(sll.data_array[0][i]);
   Serial.print(' ');
}
 Serial.println(' ');                     
 for (int i =0; i<=59; i++){    
   Serial.print(sll.data_array[1][i]);
   Serial.print(' ');
 }
 Serial.println(' ');                      
 for (int i =0; i<=59; i++){    
   Serial.print(sll.data_array[2][i]);
   Serial.print(' ');
  }
 Serial.println(' ');
 //rotation_serv();
 lcd_print();
}

void clear_buffer(){
  if(mySerial.available()>8){
    while (mySerial.available()) mySerial.read();
  } 
}

void read_buffer(){
  int i = 0;
  int j = 0;
  int num_angle = 0;
  bool is_marker = false;
  angle_transmit[0] = 0;
  angle_transmit[1] = 0;  
  while(mySerial.available()){
    transmit_data[i] = mySerial.read();
    Serial.print(transmit_data[i]);
    Serial.print(" ");
    is_marker = false;
    if(transmit_data[i]==44){
       num_angle++;
       is_marker = true;
       j = 0;
    }
    if(num_angle>2){
      clear_buffer();
      angle_transmit[0] = 0;
      angle_transmit[1] = 0;
      break;
    }
    if(!is_marker){
        angle_transmit[num_angle-1] = 10*angle_transmit[num_angle-1]+transmit_data[i]- 48;
    }
    i++;
    j++;     
  } 
  i = 0;
  Serial.println(" "); 
}

void rotation_serv(){
  int data_1 = f_angle[0];
  int data_2 = f_angle[1];
  bool it_is_back = 0;
  if (data_1>=0&&data_1<=90)
      data_1 = 90-data_1;
      it_is_back = 0;
  if (data_1>90&&data_1<180){
      data_1 = 270-data_1;
      it_is_back = 1;
  }
  if (data_1>=180&&data_1<=270){
      data_1 = 270-data_1;
      it_is_back = 1;
  }
  if (data_1>270&&data_1<=360)
      data_1 = 450-data_1;
  if (data_1<10)
      data_1 = 10;
  if (data_1>170)
      data_1 = 170;
  if(!it_is_back){
      data_2 = 180-data_2;
  }  
  if (data_2<10)
      data_2 = 10;
  if (data_2>170)
      data_2 = 170;  
  
  myservo.attach(serv_az);
  myservo.write(data_1);
  delay(500);
  myservo.attach(serv_ze);
  myservo.write(data_2);
  delay(500);
}

void lcd_print(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("X");
  lcd.setCursor(2,0);
  //int num = (int)lenght[0];
  lcd.print(lenght[0]);
  lcd.setCursor(6,0);
  lcd.print("Y");
  lcd.setCursor(8,0);
  lcd.print(lenght[1]);
  lcd.setCursor(12,0);
  lcd.print("Z");
  lcd.setCursor(14,0);
  lcd.print(lenght[2]);
  lcd.setCursor(0,1);
  lcd.print("a");
  lcd.setCursor(1,1);
  lcd.print(f_angle[0]);
  lcd.setCursor(5,1);
  lcd.print(angle_transmit[0]);
  lcd.setCursor(8,1);
  lcd.print("q");
  lcd.setCursor(9,1);
  lcd.print(f_angle[1]);
  lcd.setCursor(13,1);
  lcd.print(angle_transmit[1]);
}

int distance_be_mic(){
  int distance_between_mic = 0;
  for(int i = 2;i<7;i++){
    if(digitalRead(i)){
      distance_between_mic = 10*i+10;
    }
  }
  Serial.print("distance_be_mic ");
  Serial.print(distance_between_mic);
  return distance_between_mic;
}

