// arduino_mega_json_avg.ino
#include <ArduinoJson.h>
#include <math.h>

const int PIN_OUT = 7;        // Arduino → Lichee (сигнал "начал")
const int PIN_IN  = 19;       // Lichee → Arduino (ответный импульс)

const int GROUP_SIZE = 100;       // Количество импульсов в группе
const int NUM_GROUPS = 10;        // Количество групп
const int TOTAL_MEASUREMENTS = GROUP_SIZE * NUM_GROUPS;
const int SKIP_FIRST = 2;         // Пропустить первые измерения в каждой группе

volatile bool active = false;
const unsigned int DELAY_MICROSECONDS = 1000;
const unsigned int MIN_DELAY = DELAY_MICROSECONDS / 2;

// Массивы для статистики
float groupAvgResponse[NUM_GROUPS];     // Средние значения по группам
float groupRMSResponse[NUM_GROUPS];     // СКО по группам
float groupAvgPeriod[NUM_GROUPS];       // Средние периоды по группам
float groupRMSPeriod[NUM_GROUPS];       // СКО периодов по группам

// Временные массивы для текущей группы
volatile unsigned long currentGroupResponse[GROUP_SIZE+SKIP_FIRST];
volatile unsigned long currentGroupPeriod[GROUP_SIZE+SKIP_FIRST];

volatile int groupIndex = 0;            // Текущая группа (0-9)
volatile int measurementInGroup = 0;    // Измерение в текущей группе
volatile unsigned long previousTime = 0;
volatile unsigned long t_start = 0;

// Для синхронизации
unsigned long sessionId = 0;
bool sendDataFlag = false;
bool collectingData = false;

void setup() {
  Serial.begin(115200);
    
  pinMode(PIN_OUT, OUTPUT);
  pinMode(PIN_IN, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(PIN_IN), onResponse, FALLING);
  interrupts();
  
  // Генерируем уникальный ID сессии
  randomSeed(analogRead(0));
  sessionId = random(100000, 999999);
}

void loop() {
  // Проверяем команды от ПК
  checkSerialCommands();


   //Serial.print("\nGroup index is "); Serial.print(groupIndex);

  // Если флаг установлен, отправляем данные
  if (sendDataFlag && groupIndex == NUM_GROUPS) {
    sendAveragedJsonData();
    sendDataFlag = false;
    collectingData = false;
  }
  
  // Отправляем импульсы, если собираем данные
  if (collectingData && groupIndex < NUM_GROUPS) {
    // Посылаем импульс на Lichee
    t_start = micros();
    active = !active;
    digitalWrite(PIN_OUT, active);
    delayMicroseconds(DELAY_MICROSECONDS);
    
    // Если группа завершена, обрабатываем статистику
    if (measurementInGroup >= GROUP_SIZE) {
      processGroupStatistics();
      groupIndex++;      
      measurementInGroup = 0;
      
      if (groupIndex == NUM_GROUPS) {
        Serial.println(F("{\"status\":\"data_ready\",\"message\":\"All groups collected\"}"));
        collectingData = false;
      }
    }
  }
}

void onResponse() {
  if (collectingData && groupIndex < NUM_GROUPS && measurementInGroup < GROUP_SIZE) {
    unsigned long currentTime = micros();
    
    // Защита от двойного срабатывания
    if ((currentTime - t_start) > 0 && (currentTime - previousTime) > MIN_DELAY) {
      currentGroupResponse[measurementInGroup] = currentTime - t_start;
      currentGroupPeriod[measurementInGroup] = currentTime - previousTime;
      previousTime = currentTime;
      measurementInGroup++;
    }
  }
}

void processGroupStatistics() {
  noInterrupts();
  
  unsigned long sumResponse = 0;
  unsigned long sumPeriod = 0;
  
  // Пропускаем первые SKIP_FIRST измерений в группе
  int validMeasurements = 0;
  for (int i = SKIP_FIRST; i < GROUP_SIZE+SKIP_FIRST; i++) {
    sumResponse += currentGroupResponse[i];
    sumPeriod += currentGroupPeriod[i];
    validMeasurements++;
  }
  
  // Среднее арифметическое
  float avgResponse = (float)sumResponse / validMeasurements;
  float avgPeriod = (float)sumPeriod / validMeasurements;
  
  // СКО (среднеквадратичное отклонение)
  float sumSqResponse = 0;
  float sumSqPeriod = 0;
  
  for (int i = SKIP_FIRST; i < GROUP_SIZE+SKIP_FIRST; i++) {
    float diffResponse = (float)currentGroupResponse[i] - avgResponse;
    float diffPeriod = (float)currentGroupPeriod[i] - avgPeriod;
    sumSqResponse += diffResponse * diffResponse;
    sumSqPeriod += diffPeriod * diffPeriod;
  }
  
  float rmsResponse = sqrt(sumSqResponse / validMeasurements);
  float rmsPeriod = sqrt(sumSqPeriod / validMeasurements);
  
  // Сохраняем статистику группы
  groupAvgResponse[groupIndex] = avgResponse;
  groupRMSResponse[groupIndex] = rmsResponse;
  groupAvgPeriod[groupIndex] = avgPeriod;
  groupRMSPeriod[groupIndex] = rmsPeriod;
  
  interrupts();
}

void checkSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "START") {
      // Начинаем сбор данных
      groupIndex = 0;
      measurementInGroup = 0;
      previousTime = 0;
      sendDataFlag = false;
      collectingData = true;
      Serial.println(F("{\"status\":\"started\",\"message\":\"Measurement started (10 groups of 100 pulses)\"}"));
    }
    else if (command == "SEND") {
      // Запрашиваем отправку данных
      if (groupIndex == NUM_GROUPS) {
        sendDataFlag = true;
        Serial.println(F("{\"status\":\"sending\",\"message\":\"Preparing to send averaged data\"}"));
      } else {
        Serial.println(F("{\"status\":\"error\",\"message\":\"Data not ready yet\"}"));
      }
    }
    else if (command == "STATUS") {
      // Отправляем текущий статус
      StaticJsonDocument<200> doc;
      doc["status"] = "status_report";
      doc["groups_collected"] = groupIndex;
      doc["measurements_in_current_group"] = measurementInGroup;
      doc["session_id"] = sessionId;
      serializeJson(doc, Serial);
      Serial.println();
    }
    else if (command == "RESET") {
      // Сброс измерений
      groupIndex = 0;
      measurementInGroup = 0;
      previousTime = 0;
      sendDataFlag = false;
      collectingData = false;
      Serial.println(F("{\"status\":\"reset\",\"message\":\"Measurements reset\"}"));
    }
    else {
      Serial.print(F("{\"status\":\"error\",\"message\":\"Unknown command: "));
      Serial.print(command);
      Serial.println(F("\"}"));
    }
  }
}

void sendAveragedJsonData() {
  noInterrupts();
  
  // Создаем JSON документ
  DynamicJsonDocument doc(4096);
  
  // Базовая информация
  doc["session_id"] = sessionId;
  doc["groups_count"] = NUM_GROUPS;
  doc["measurements_per_group"] = GROUP_SIZE;
  doc["total_measurements"] = NUM_GROUPS * (GROUP_SIZE);
  doc["timestamp"] = millis();
  doc["device"] = "Arduino Mega";
  
  // Массивы со статистикой по группам
  JsonArray avgResponseArray = doc.createNestedArray("avg_response_us");
  JsonArray rmsResponseArray = doc.createNestedArray("rms_response_us");
  JsonArray avgPeriodArray = doc.createNestedArray("avg_period_us");
  JsonArray rmsPeriodArray = doc.createNestedArray("rms_period_us");
  
  // Общая статистика по всем группам
  float totalAvgResponse = 0;
  float totalRMSEesponse = 0;
  float totalAvgPeriod = 0;
  float totalRMSPeriod = 0;
  
  // Заполняем массивы и считаем общую статистику
  for (int i = 0; i < NUM_GROUPS; i++) {
    avgResponseArray.add(groupAvgResponse[i]);
    rmsResponseArray.add(groupRMSResponse[i]);
    avgPeriodArray.add(groupAvgPeriod[i]);
    rmsPeriodArray.add(groupRMSPeriod[i]);
    
    totalAvgResponse += groupAvgResponse[i];
    totalRMSEesponse += groupRMSResponse[i];
    totalAvgPeriod += groupAvgPeriod[i];
    totalRMSPeriod += groupRMSPeriod[i];
  }
  
  // Средние значения по всем группам
  totalAvgResponse /= NUM_GROUPS;
  totalRMSEesponse /= NUM_GROUPS;
  totalAvgPeriod /= NUM_GROUPS;
  totalRMSPeriod /= NUM_GROUPS;
  
  // Статистика
  JsonObject stats = doc.createNestedObject("statistics");
  
  JsonObject responseStats = stats.createNestedObject("response_time");
  responseStats["overall_avg_us"] = totalAvgResponse;
  responseStats["overall_rms_us"] = totalRMSEesponse;
  
  JsonObject periodStats = stats.createNestedObject("period");
  periodStats["overall_avg_us"] = totalAvgPeriod;
  periodStats["overall_rms_us"] = totalRMSPeriod;
  
  // Информация о параметрах
  JsonObject params = doc.createNestedObject("parameters");
  params["delay_between_pulses_us"] = DELAY_MICROSECONDS;
  params["groups"] = NUM_GROUPS;
  params["measurements_per_group"] = GROUP_SIZE;
  params["skip_first_per_group"] = SKIP_FIRST;
  
  // Сериализуем и отправляем
  serializeJson(doc, Serial);
  Serial.println();
  
  // Сбрасываем для новых измерений
  groupIndex = 0;
  measurementInGroup = 0;
  
  interrupts();
  
  Serial.println(F("{\"status\":\"sent\",\"message\":\"Averaged data sent successfully\"}"));
}