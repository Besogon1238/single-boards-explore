#include <ArduinoJson.h>
#include <math.h>

const int PIN_OUT = 7;        // Arduino → Lichee (сигнал "начал")
const int PIN_IN  = 20;       // Lichee → Arduino (ответный импульс)

const int GROUP_SIZE = 50;       // Количество импульсов в группе
const int NUM_GROUPS = 20;        // Количество групп
const int TOTAL_MEASUREMENTS = GROUP_SIZE * NUM_GROUPS;

volatile bool active = false;
const unsigned int DELAY_MICROSECONDS = 10000;

// Массивы для статистики
float groupAvgLatency[NUM_GROUPS];     // Средняя задержка по группам
float groupRMSLatency[NUM_GROUPS];     // СКО задержки по группам
float groupAvgJitter[NUM_GROUPS];      // Средний джиттер по группам
float groupRMSJitter[NUM_GROUPS];      // СКО джиттера по группам

// Временные массивы для текущей группы
volatile unsigned long currentGroupLatency[GROUP_SIZE];
volatile unsigned long currentGroupJitter[GROUP_SIZE];
volatile unsigned long lastResponseTime = 0;  // Для расчета джиттера

volatile int groupIndex = 0;            // Текущая группа (0-9)
volatile int measurementInGroup = 0;    // Измерение в текущей группе
volatile unsigned long previousTime = 0;
volatile unsigned long t_start = 0;
volatile int totalFallings = 0;

// Для синхронизации
volatile unsigned long sessionId = 0;
volatile bool sendDataFlag = false;
volatile bool collectingData = false;

// Для прямого доступа к портам
volatile uint8_t* portOutputRegisterPin7;
uint8_t pin7Mask;
volatile uint8_t* portInputRegisterPin20;
uint8_t pin20Mask;

void setup() {
  Serial.begin(115200);
  
  // Инициализация прямого доступа к портам для PIN_OUT (пин 7)
  pinMode(PIN_OUT, OUTPUT);
  portOutputRegisterPin7 = portOutputRegister(digitalPinToPort(PIN_OUT));
  pin7Mask = digitalPinToBitMask(PIN_OUT);
  
  // Инициализация прямого доступа к портам для PIN_IN (пин 20)
  pinMode(PIN_IN, INPUT);
  portInputRegisterPin20 = portInputRegister(digitalPinToPort(PIN_IN));
  pin20Mask = digitalPinToBitMask(PIN_IN);
  
  attachInterrupt(digitalPinToInterrupt(PIN_IN), onResponse, FALLING);
  interrupts();
  
  // Генерируем уникальный ID сессии
  randomSeed(analogRead(0));
  sessionId = random(100000, 999999);
}

void loop() {
  // Проверяем команды от ПК
  checkSerialCommands();

  // Если флаг установлен, отправляем данные
  if (sendDataFlag && (groupIndex == NUM_GROUPS)) {
    sendAveragedJsonData();
    sendDataFlag = false;
    collectingData = false;
  }
  
  // Отправляем импульсы, если собираем данные
  if (collectingData && (groupIndex < NUM_GROUPS)) {
    // Посылаем импульс на Lichee
    t_start = micros();
    active = !active;
    if (active) {
      *portOutputRegisterPin7 |= pin7Mask;   // Установить HIGH
    } else {
      *portOutputRegisterPin7 &= ~pin7Mask;  // Установить LOW
    }
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
    
    // Рассчитываем задержку (latency)
    unsigned long latency = currentTime - t_start;
    
    // Рассчитываем джиттер (разница между текущей и предыдущей задержкой)
    unsigned long jitter = 0;
    if (measurementInGroup > 0) {
      jitter = abs((long)(latency - currentGroupLatency[measurementInGroup-1]));
    }
  
    currentGroupLatency[measurementInGroup] = latency;
    currentGroupJitter[measurementInGroup] = jitter;
    previousTime = currentTime;
    measurementInGroup++;

    //Serial.println(latency);
    
  }
}

void processGroupStatistics() {
  unsigned long sumLatency = 0;
  unsigned long sumJitter = 0;
  
  int validMeasurements = 0;
  for (int i = 0; i < GROUP_SIZE; i++) {
    sumLatency += currentGroupLatency[i];
    sumJitter += currentGroupJitter[i];
    validMeasurements++;
  }
  
  float avgLatency = (float)sumLatency / validMeasurements;
  float avgJitter = (float)sumJitter / validMeasurements;
  
  // СКО (RMS)
  float sumSqLatency = 0;
  float sumSqJitter = 0;
  
  for (int i = 0; i < GROUP_SIZE; i++) {
    float diffLatency = (float)currentGroupLatency[i] - avgLatency;
    float diffJitter = (float)currentGroupJitter[i] - avgJitter;
    sumSqLatency += diffLatency * diffLatency;
    sumSqJitter += diffJitter * diffJitter;
  }
  
  float rmsLatency = sqrt(sumSqLatency / validMeasurements);
  float rmsJitter = sqrt(sumSqJitter / validMeasurements);
  
  // Сохраняем статистику группы
  groupAvgLatency[groupIndex] = avgLatency;
  groupRMSLatency[groupIndex] = rmsLatency;
  groupAvgJitter[groupIndex] = avgJitter;
  groupRMSJitter[groupIndex] = rmsJitter;
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
      
      Serial.println(F("{\"status\":\"started\",\"message\":\"Measurement started 20 groups 50 pulses\"}"));
    }
    else if (command == "SEND") {
      // Запрашиваем отправку данных
      if (groupIndex == NUM_GROUPS) {
        Serial.println(F("{\"status\":\"sending\",\"message\":\"Preparing to send averaged data\"}"));
        sendDataFlag = true;
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
  // Создаем JSON документ
  DynamicJsonDocument doc(4096);
  
  // Базовая информация
  doc["session_id"] = sessionId;
  doc["groups_count"] = NUM_GROUPS;
  doc["measurements_per_group"] = GROUP_SIZE;
  doc["total_measurements"] = NUM_GROUPS * GROUP_SIZE;
  doc["timestamp"] = millis();
  doc["device"] = "Arduino Mega";
  
  // Массивы со статистикой по группам
  JsonArray avgLatencyArray = doc.createNestedArray("avg_latency_us");
  JsonArray rmsLatencyArray = doc.createNestedArray("rms_latency_us");
  JsonArray avgJitterArray = doc.createNestedArray("avg_jitter_us");
  JsonArray rmsJitterArray = doc.createNestedArray("rms_jitter_us");
  
  // Общая статистика по всем группам
  float totalAvgLatency = 0;
  float totalRMSLatency = 0;
  float totalAvgJitter = 0;
  float totalRMSJitter = 0;
  
  // Заполняем массивы и считаем общую статистику
  for (int i = 0; i < NUM_GROUPS; i++) {
    avgLatencyArray.add(groupAvgLatency[i]);
    rmsLatencyArray.add(groupRMSLatency[i]);
    avgJitterArray.add(groupAvgJitter[i]);
    rmsJitterArray.add(groupRMSJitter[i]);
    
    totalAvgLatency += groupAvgLatency[i];
    totalRMSLatency += groupRMSLatency[i];
    totalAvgJitter += groupAvgJitter[i];
    totalRMSJitter += groupRMSJitter[i];
  }
  
  // Средние значения по всем группам
  totalAvgLatency /= NUM_GROUPS;
  totalRMSLatency /= NUM_GROUPS;
  totalAvgJitter /= NUM_GROUPS;
  totalRMSJitter /= NUM_GROUPS;
  
  // Статистика
  JsonObject stats = doc.createNestedObject("statistics");
  
  JsonObject latencyStats = stats.createNestedObject("latency");
  latencyStats["overall_avg_us"] = totalAvgLatency;
  latencyStats["overall_rms_us"] = totalRMSLatency;
  latencyStats["min_latency_us"] = findMinValue(groupAvgLatency, NUM_GROUPS);
  latencyStats["max_latency_us"] = findMaxValue(groupAvgLatency, NUM_GROUPS);
  
  JsonObject jitterStats = stats.createNestedObject("jitter");
  jitterStats["overall_avg_us"] = totalAvgJitter;
  jitterStats["overall_rms_us"] = totalRMSJitter;
  jitterStats["min_jitter_us"] = findMinValue(groupAvgJitter, NUM_GROUPS);
  jitterStats["max_jitter_us"] = findMaxValue(groupAvgJitter, NUM_GROUPS);
  
  // Информация о параметрах
  JsonObject params = doc.createNestedObject("parameters");
  params["delay_between_pulses_us"] = DELAY_MICROSECONDS;
  params["groups"] = NUM_GROUPS;
  params["measurements_per_group"] = GROUP_SIZE;

  doc["latency_variation_us"] = findMaxValue(groupAvgLatency, NUM_GROUPS) - 
                                 findMinValue(groupAvgLatency, NUM_GROUPS);
  doc["jitter_variation_us"] = findMaxValue(groupAvgJitter, NUM_GROUPS) - 
                               findMinValue(groupAvgJitter, NUM_GROUPS);
  
  // Сериализуем и отправляем
  serializeJson(doc, Serial);
  Serial.println();
}

float findMinValue(float arr[], int size) {
  float minVal = arr[0];
  for (int i = 1; i < size; i++) {
    if (arr[i] < minVal) minVal = arr[i];
  }
  return minVal;
}

float findMaxValue(float arr[], int size) {
  float maxVal = arr[0];
  for (int i = 1; i < size; i++) {
    if (arr[i] > maxVal) maxVal = arr[i];
  }
  return maxVal;
}
