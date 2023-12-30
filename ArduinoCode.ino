#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>
#include <I2CKeyPad.h>
#include <Servo.h> // Thêm thư viện Servo.h

const uint8_t Keypad_Address = 0x20;
I2CKeyPad keyPad(Keypad_Address);
char keymap[5] = {
  '1', '2', '3', 'A'
};

// Chân Trigger và Echo của cảm biến khoảng cách HC-SR04
const int TRIGGER_PIN = 4;
const int ECHO_PIN = 5;

// Chân DT và SCK của module HX711
const int LOADCELL_DT_PIN = 6;
const int LOADCELL_SCK_PIN = 7;

// Hệ số calib của load cell (đơn vị: kg)
const float LOADCELL_CALIB_FACTOR = 125000.0; // Ví dụ: load cell 40kg

// Chân điều khiển Servo
const int SERVO_PIN = 9;

// Khởi tạo đối tượng LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Địa chỉ I2C của LCD và kích thước 16x2

// Khởi tạo đối tượng HX711
HX711 loadcell;

// Khởi tạo đối tượng Servo
Servo servo;
int count = 0;

// Khởi tạo biến lưu trữ dữ liệu nhận từ Raspberry
volatile char receivedData;

void setup() {
  // Khởi tạo giao tiếp I2C
  Wire.begin();
  Serial.begin(9600);

  // Khởi tạo LCD
  lcd.begin();
  lcd.setCursor(12, 0);
  lcd.print("RN: ");
  lcd.setCursor(12, 1);
  lcd.print("BT: ");
  
  // Khởi tạo cảm biến khoảng cách HC-SR04
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Khởi tạo load cell
  loadcell.begin(LOADCELL_DT_PIN, LOADCELL_SCK_PIN);
  loadcell.set_scale(LOADCELL_CALIB_FACTOR); // Đặt hệ số calib
  loadcell.tare();

   // Khởi tạo chương trình ngắt nhận dữ liệu từ Raspberry
   //attachInterrupt(digitalPinToInterrupt(0), receiveData, FALLING); 

  // Khởi tạo Servo
  servo.attach(SERVO_PIN);
  delay(1000);

  // Khởi tạo KeyPad
  if (keyPad.begin() == false) {
      while (1);
            }
                keyPad.loadKeyMap(keymap);
  }
 bool buttonPressed = false; // Biến flag để kiểm tra trạng thái nút nhấn


void loop() {
  // Đo khoảng cách từ cảm biến khoảng cách HC-SR04
  float distance = measureDistance();
  
  // Hiển thị khoảng cách lên LCD
  lcd.setCursor(0, 1);
  lcd.print("D: ");
  lcd.print(distance);
  lcd.setCursor(7, 1);
  lcd.print(" cm");

  // Đọc giá trị từ load cell
  float weight = measureWeight();
  
  // Hiển thị trọng lượng lên LCD
  lcd.setCursor(0, 0);
  lcd.print("W: ");
  lcd.print(weight);
  lcd.setCursor(7, 0);
  lcd.print(" kg");
  
  delay(100);
  
  // Đọc ký tự từ ma trận phím
  if (keyPad.isPressed()) {
    if (!buttonPressed) {
      char ch = keyPad.getChar();
      int key = keyPad.getLastKey();
      lcd.setCursor(15, 1);
      lcd.print(ch);
      // Xử lí gửi Data
      switch(ch) {
        case '1':
          Serial.print(weight);
          Serial.print(",");
          Serial.print(distance);
          Serial.print(",");
          Serial.print("U");
          delay(600);
          for (int i = 0; i < 3; i++) {
              servo.write(80);
              delay(1500);  // Đợi 1 giây
              
              servo.write(150);
              delay(600);  // Đợi 1 giây
            }
            servo.write(80);
          break;
        case '2':
          Serial.print(weight);
          Serial.print(",");
          Serial.print(distance);
          Serial.print(",");
          Serial.print("M");
          delay(600);
          for (int i = 0; i < 3; i++) {
              servo.write(80);
              delay(1500);  // Đợi 1 giây
              
              servo.write(150);
              delay(600);  // Đợi 1 giây
            }
            servo.write(80);
          break;
        case '3':
          Serial.print(weight);
          Serial.print(",");
          Serial.print(distance);
          Serial.print(",");
          Serial.print("R");
          delay(600);
          for (int i = 0; i < 3; i++) {
              servo.write(80);
              delay(1500);  // Đợi 1 giây
              
              servo.write(150);
              delay(600);  // Đợi 1 giây
            }
            servo.write(80);
          break;
        case 'A':
          Serial.print(weight);
          Serial.print(",");
          Serial.print(distance);
          Serial.print(",");
          Serial.print("A");
          delay(600);
          for (int i = 0; i < 3; i++) {
              servo.write(80);
              delay(1500);  // Đợi 1 giây
              
              servo.write(150);
              delay(600);  // Đợi 1 giây
            }
            servo.write(80);
          break;
      }
      buttonPressed = true; // Đánh dấu rằng nút đã được nhấn
    }
  } else {
    buttonPressed = false; // Đặt lại biến flag khi nút không được nhấn
  }

  //
  receiveData();
}

float measureDistance() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  
  unsigned long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;
  distance = distance - 41.5;
  distance = abs(distance);

  return distance;
}

float measureWeight() {
  float weight = loadcell.get_units();
  weight = abs(weight);
  return weight;
}

void rotateServo(int angle) {
  servo.write(angle);
  delay(300); // Delay để đợi Servo hoàn thành xoay
}

void receiveData() {
  while (Serial.available()) {
    receivedData = Serial.read();  // Đọc dữ liệu từ giao tiếp serial

    // Hiển thị dữ liệu lên màn hình LCD
    lcd.setCursor(15, 0);
    lcd.print("  ");
    lcd.setCursor(15, 0);
    lcd.print(receivedData);
  }
}
