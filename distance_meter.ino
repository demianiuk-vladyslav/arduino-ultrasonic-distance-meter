#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <EEPROM.h>
// Визначення пінів (блок ініціалізації пінів згідно з логікою)
#define TRIGGER_PIN 9
#define ECHO_PIN 10
#define MAX_DISTANCE 400
#define BUTTON_PIN 2
#define RED_LED 5
#define YELLOW_LED 4
#define GREEN_LED 3
#define BUZZER_PIN 6
// Ініціалізація LCD (20x4 з адресою 0x3F)
LiquidCrystal_I2C lcd(0x3F, 20, 4);
// Ініціалізація RTC (модуль реального часу)
RTC_DS1307 rtc;
// Структура для виміру (відстань + дата/час)
struct Measurement {
int distance;
uint8_t day;
uint8_t month;
uint16_t year;
uint8_t hour;
uint8_t minute;
uint8_t second; };
// Масив для трьох попередніх вимірів
Measurement history[3];
// Адреса в EEPROM для збереження історії (розмір: 3 * sizeof(Measurement) ≈ 3*10 = 30 байт)
const int EEPROM_ADDRESS = 0;
// Стан кнопки та змінні для обробки натискань (початкові змінні)
int lastButtonState = HIGH;
unsigned long buttonPressStart = 0;
bool measuring = false;
// Прототипи функцій (щоб уникнути помилок компіляції через порядок визначення)
void displayOffState(DateTime now);
void performMeasurement(DateTime now);
int measureDistance();
void startMeasurement();
void stopMeasurement();
void saveMeasurement(DateTime now);
void printTwoDigits(int value);
// Функція setup() - блок ініціалізації (налаштування портів, змінних, бібліотек)
void setup() {
// 1. Ініціалізація пінів світлодіодів, ультрасоніка, кнопки, п’єзо-зумера
pinMode(TRIGGER_PIN, OUTPUT);
pinMode(ECHO_PIN, INPUT);
pinMode(BUTTON_PIN, INPUT_PULLUP);
pinMode(RED_LED, OUTPUT);
pinMode(YELLOW_LED, OUTPUT);
pinMode(GREEN_LED, OUTPUT);
pinMode(BUZZER_PIN, OUTPUT);
// 2. Ініціалізація бібліотек та пристроїв
Serial.begin(9600);
Serial.println("\n=== Hello, World! ===");
lcd.init();
lcd.backlight();
Serial.println("LCD I2C is initialized (adress 0x3F)");
// Ініціалізація RTC
if (!rtc.begin()) {
Serial.println("Could'nt find RTC");
while (1) delay(10);
} else {
Serial.println("RTC DS1307 is found and initialized");
}
if (!rtc.isrunning()) {
// Встановити час компіляції, якщо RTC не запущено
Serial.println("RTC isn't runnig → setting compilation time");
rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
} else {
Serial.print("RTC is running. Current time: ");
DateTime now = rtc.now();
Serial.print(now.year()); Serial.print("/");
Serial.print(now.month()); Serial.print("/");
Serial.print(now.day()); Serial.print(" ");
Serial.print(now.hour()); Serial.print(":");
Serial.print(now.minute()); Serial.print(":");
Serial.println(now.second());
}
// 3. Задання початкових змінних та зчитування історії з EEPROM
EEPROM.get(EEPROM_ADDRESS, history);
Serial.println("Read measurements history from EEPROM:");
for (int i = 0; i < 3; i++) {
Serial.print(" #"); Serial.print(i+1); Serial.print(" ");
if (history[i].distance <= 0 || history[i].distance >
MAX_DISTANCE) {
Serial.println("[empty / not valid]");
} else {
Serial.print(history[i].distance);
Serial.print(" cm ");
Serial.print(history[i].day); Serial.print(".");
Serial.print(history[i].month); Serial.print(".");
Serial.print(history[i].year); Serial.print(" ");
Serial.print(history[i].hour); Serial.print(":");
Serial.print(history[i].minute);Serial.print(":");
Serial.println(history[i].second);
}
}
// Початковий екран
lcd.setCursor(0, 0);
lcd.print("Distance Meter");
delay(2000);
lcd.clear();
displayOffState(rtc.now());
}
// Основний нескінченний цикл loop() - основний цикл роботи програми
void loop() {
DateTime now = rtc.now();
// 4. Перевірка натиснення кнопки
int buttonState = digitalRead(BUTTON_PIN);
// Виявлення початку натискання (з антидребезгом)
if (buttonState == LOW && lastButtonState == HIGH) {
delay(50); // Антидребезг
if (digitalRead(BUTTON_PIN) == LOW) {
buttonPressStart = millis();
}
}
// Виявлення відпускання кнопки (з антидребезгом)
if (buttonState == HIGH && lastButtonState == LOW) {
delay(50); // Антидребезг
if (digitalRead(BUTTON_PIN) == HIGH) {
unsigned long pressDuration = millis() -
buttonPressStart;
// 12. Перевірка натискання кнопки (коротке натискання для перемикання режиму)
if (pressDuration < 2000) {
if (!measuring) {
// Якщо не в режимі вимірювання, почати (перейти до блоку вимірювання)
startMeasurement();
} else {
// Якщо в режимі, зупинити (повернутися до OFF)
stopMeasurement();
}
}
}
}
if (measuring) {
// 11. Перевірка затискання кнопки (утримання 2 секунди для збереження)
if (buttonState == LOW && millis() - buttonPressStart >=
2000) {
saveMeasurement(now);
while (digitalRead(BUTTON_PIN) == LOW) {
delay(10); // Чекаємо відпускання кнопки
}
}
// 5-10. Виконання блоку вимірювання, виведення та сигналізації
performMeasurement(now);
} else {
// 4a. Якщо кнопка не натиснута, вивести OFF, дату, час
displayOffState(now);
}
lastButtonState = buttonState;
delay(100); // Затримка для циклу
}
// 5. Вимірювання відстані ультразвуковим датчиком
int measureDistance() {
digitalWrite(TRIGGER_PIN, LOW);
delayMicroseconds(2);
digitalWrite(TRIGGER_PIN, HIGH);
delayMicroseconds(10);
digitalWrite(TRIGGER_PIN, LOW);
long duration = pulseIn(ECHO_PIN, HIGH);
int distance = duration * 0.034 / 2;
if (distance > MAX_DISTANCE || distance <= 0) {
return MAX_DISTANCE;
}
return distance;
}
// Початок режиму вимірювання (з очищенням дисплея згідно з логікою)
void startMeasurement() {
measuring = true;
lcd.clear(); // Очищення LCD при початку
}
// Зупинка режиму вимірювання (12a: очищення, вивід OFF, повернення до перевірки)
void stopMeasurement() {
measuring = false;
lcd.clear(); // Очищення LCD
digitalWrite(RED_LED, LOW);
digitalWrite(YELLOW_LED, LOW);
digitalWrite(GREEN_LED, LOW);
noTone(BUZZER_PIN);
}
// 5-10. Блок вимірювання, виведення відстані, часу, історії та сигналізації
void performMeasurement(DateTime now) {
// 12b. Очищення LCD перед новим виміром (якщо продовжуємо цикл)
lcd.clear();
// 5. Вимірювання відстані
int distance = measureDistance();
// 6. Виведення виміряної відстані
lcd.setCursor(0, 0);
lcd.print("D:");
lcd.print(distance);
lcd.print("cm ");
// 7. Виведення поточного часу
printTwoDigits(now.hour());
lcd.print(":");
printTwoDigits(now.minute());
lcd.print(":");
printTwoDigits(now.second());
// 8. Зчитування трьох попередніх вимірів (з масиву, завантаженого з пам'яті)
// (Оптимізовано: зчитано в setup, оновлюється тільки при збереженні)
// 9. Виведення трьох попередніх вимірів, часу та дати
bool hasData = false;
for (int i = 0; i < 3; i++) {
if (history[i].distance > 0) {
hasData = true;
lcd.setCursor(0, i + 1);
lcd.print(i + 1);
lcd.print(":");
lcd.print(history[i].distance);
lcd.print("cm ");
printTwoDigits(history[i].hour);
lcd.print(":");
printTwoDigits(history[i].minute);
lcd.print(" ");
printTwoDigits(history[i].day);
lcd.print("/");
printTwoDigits(history[i].month);
}
}
if (!hasData) {
lcd.setCursor(0, 2);
lcd.print("No saved data");
}
// 10. Підпрограма сигналізації
digitalWrite(RED_LED, LOW);
digitalWrite(YELLOW_LED, LOW);
digitalWrite(GREEN_LED, LOW);
noTone(BUZZER_PIN);
// 10a. Перевірка, чи відстань більша за 50 см
if (distance >= 50) {
digitalWrite(GREEN_LED, HIGH);
} else if (distance < 20) {
// 10b. Якщо менша за 20 см, червоний LED та зумер
digitalWrite(RED_LED, HIGH);
tone(BUZZER_PIN, 500);
} else {
// 10c. Жовтий LED
digitalWrite(YELLOW_LED, HIGH);
}
}
// 11a. Збереження виміру в пам'ять (зсув історії)
void saveMeasurement(DateTime now) {
int distance = measureDistance();
// Зсув історії: останній стає першим
for (int i = 2; i > 0; i--) {
history[i] = history[i - 1];
}
history[0] = {distance, (uint8_t)now.day(),
(uint8_t)now.month(), (uint16_t)now.year(),
(uint8_t)now.hour(), (uint8_t)now.minute(),
(uint8_t)now.second()};
// Збереження в EEPROM
EEPROM.put(EEPROM_ADDRESS, history);
Serial.print("Saved to EEPROM: ");
Serial.print(distance);
Serial.print(" cm ");
Serial.print(now.day()); Serial.print(".");
Serial.print(now.month()); Serial.print(".");
Serial.print(now.year()); Serial.print(" ");
Serial.print(now.hour()); Serial.print(":");
Serial.print(now.minute());Serial.print(":");
Serial.print(now.second());
Serial.println();
Serial.println("Read measurements history from EEPROM:");
for (int i = 0; i < 3; i++) {
Serial.print(" #"); Serial.print(i+1); Serial.print("");
if (history[i].distance <= 0 || history[i].distance >
MAX_DISTANCE) {
Serial.println("[empty / not valid]");
} else {
Serial.print(history[i].distance);
Serial.print(" cm ");
Serial.print(history[i].day); Serial.print(".");
Serial.print(history[i].month); Serial.print(".");
Serial.print(history[i].year); Serial.print(" ");
Serial.print(history[i].hour); Serial.print(":");
Serial.print(history[i].minute);Serial.print(":");
Serial.println(history[i].second);
}
}
}
// 4a. Екран у стані OFF (виведення дати, часу та повідомлення)
void displayOffState(DateTime now) {
lcd.setCursor(0, 0);
lcd.print("OFF "); // Очищення рядка
lcd.setCursor(0, 1);
lcd.print("Date: ");
printTwoDigits(now.day());
lcd.print("/");
printTwoDigits(now.month());
lcd.print("/");
lcd.print(now.year());
lcd.setCursor(0, 2);
lcd.print("Time: ");
printTwoDigits(now.hour());
lcd.print(":");
printTwoDigits(now.minute());
lcd.print(":");
printTwoDigits(now.second());
lcd.setCursor(0, 3);
lcd.print("Press to measure ");
}
// Допоміжна функція для виведення двозначних чисел
void printTwoDigits(int value) {
if (value < 10) lcd.print("0");
lcd.print(value);
}
