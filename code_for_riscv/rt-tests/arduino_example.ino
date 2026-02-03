#include <ArduinoJson.h>

const int PIN_OUT = 7;        // Arduino → Lichee (сигнал "начал")
const int PIN_IN  = 20;       // Lichee → Arduino (ответный импульс)

const int GROUP_SIZE = 500;       // Количество импульсов в группе
const int NUM_GROUPS = 50;        // Количество групп

volatile bool active = false;
const unsigned int DELAY_MICROSECONDS = 300;

// Массивы для статистики
float groupAvgLatency[NUM_GROUPS];     // Средняя задержка по группам
float groupMinLatency[NUM_GROUPS];     // Минимальная задержка по группам
float groupMaxLatency[NUM_GROUPS];     // Максимальная задержка по группам
float groupAvgJitter[NUM_GROUPS];      // ТОЛЬКО СРЕДНИЙ джиттер по группам

// Временные массивы для текущей группы
volatile unsigned long currentGroupLatency[GROUP_SIZE];
volatile unsigned long currentGroupJitter[GROUP_SIZE];

volatile uint8_t groupIndex = 0;            // Текущая группа (0-9)
volatile int measurementInGroup = 0;    // Измерение в текущей группе
volatile unsigned long t_start = 0;

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
    measurementInGroup++;
  }
}

void processGroupStatistics() {
  if (GROUP_SIZE == 0) return;
  
  // Для latency считаем min, max и среднее
  unsigned long minLatency = currentGroupLatency[0];
  unsigned long maxLatency = currentGroupLatency[0];
  unsigned long sumLatency = currentGroupLatency[0];
  
  // Для jitter считаем только среднее
  unsigned long sumJitter = currentGroupJitter[0];
  
  // Обрабатываем остальные измерения
  for (int i = 1; i < GROUP_SIZE; i++) {
    unsigned long latency = currentGroupLatency[i];
    
    // Обновляем min/max для latency
    if (latency < minLatency) minLatency = latency;
    if (latency > maxLatency) maxLatency = latency;
    
    sumLatency += latency;
    sumJitter += currentGroupJitter[i];
  }
  
  // Сохраняем статистику группы
  groupAvgLatency[groupIndex] = (float)sumLatency / GROUP_SIZE;
  groupMinLatency[groupIndex] = (float)minLatency;
  groupMaxLatency[groupIndex] = (float)maxLatency;
  groupAvgJitter[groupIndex] = (float)sumJitter / (GROUP_SIZE-1);
}

void checkSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "START") {
      // Начинаем сбор данных
      groupIndex = 0;
      measurementInGroup = 0;
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
  DynamicJsonDocument doc(3072);
  
  // Базовая информация
  doc["session_id"] = sessionId;
  doc["groups_count"] = NUM_GROUPS;
  doc["measurements_per_group"] = GROUP_SIZE;
  doc["total_measurements"] = NUM_GROUPS * GROUP_SIZE;
  doc["timestamp"] = millis();
  doc["device"] = "Arduino Mega";
  
  // Массивы со статистикой по группам
  JsonArray avgLatencyArray = doc.createNestedArray("avg_latency_us");
  JsonArray minLatencyArray = doc.createNestedArray("min_latency_us");
  JsonArray maxLatencyArray = doc.createNestedArray("max_latency_us");
  JsonArray avgJitterArray = doc.createNestedArray("avg_jitter_us");
  
  // Общая статистика по всем группам
  float totalAvgLatency = 0;
  float totalMinLatency = groupMinLatency[0];
  float totalMaxLatency = groupMaxLatency[0];
  float totalAvgJitter = 0;
  
  // Заполняем массивы и считаем общую статистику
  for (int i = 0; i < NUM_GROUPS; i++) {
    avgLatencyArray.add(groupAvgLatency[i]);
    minLatencyArray.add(groupMinLatency[i]);
    maxLatencyArray.add(groupMaxLatency[i]);
    avgJitterArray.add(groupAvgJitter[i]);
    
    totalAvgLatency += groupAvgLatency[i];
    totalAvgJitter += groupAvgJitter[i];
    
    // Обновляем общие минимумы и максимумы для latency
    if (groupMinLatency[i] < totalMinLatency) totalMinLatency = groupMinLatency[i];
    if (groupMaxLatency[i] > totalMaxLatency) totalMaxLatency = groupMaxLatency[i];
  }
  
  // Средние значения по всем группам
  totalAvgLatency /= NUM_GROUPS;
  totalAvgJitter /= NUM_GROUPS;
  
  // Статистика
  JsonObject stats = doc.createNestedObject("statistics");
  
  JsonObject latencyStats = stats.createNestedObject("latency");
  latencyStats["overall_avg_us"] = totalAvgLatency;
  latencyStats["overall_min_us"] = totalMinLatency;
  latencyStats["overall_max_us"] = totalMaxLatency;
  latencyStats["variation_us"] = totalMaxLatency - totalMinLatency;
  
  JsonObject jitterStats = stats.createNestedObject("jitter");
  jitterStats["overall_avg_us"] = totalAvgJitter;
  
  // Информация о параметрах
  JsonObject params = doc.createNestedObject("parameters");
  params["delay_between_pulses_us"] = DELAY_MICROSECONDS;
  params["groups"] = NUM_GROUPS;
  params["measurements_per_group"] = GROUP_SIZE;

  // Сериализуем и отправляем
  serializeJson(doc, Serial);
  Serial.println();
}