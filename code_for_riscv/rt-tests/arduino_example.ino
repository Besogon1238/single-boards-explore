const int PIN_OUT = 7;        // Arduino → Lichee (сигнал "начал")
const int PIN_IN  = 19;       // Lichee → Arduino (ответный импульс)

const int SKIP_FIRST = 2;
const int MEASUREMENTS_COUNT = 102;

const unsigned int DELAY_MICROSECONDS = 50;
const unsigned int MIN_DELAY = DELAY_MICROSECONDS/2;  // для избавления от двойного срабатывания прерывания на пине

volatile unsigned long periodMeasurements[MEASUREMENTS_COUNT];
volatile int measurementIndex = 0;

volatile unsigned long t_start = 0;

void setup() {
  Serial.begin(9600);

  pinMode(PIN_OUT, OUTPUT);
  pinMode(PIN_IN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_IN), onResponse, RISING);
  interrupts();
}

void loop() {
  if (measurementIndex == MEASUREMENTS_COUNT) {
    
    noInterrupts();
  
    unsigned long sum = 0;
    for (int i = SKIP_FIRST; i < MEASUREMENTS_COUNT; i++)
      sum += periodMeasurements[i];

    unsigned long avg = sum / (MEASUREMENTS_COUNT - SKIP_FIRST);

    double rms = 0;
    for (int i = SKIP_FIRST; i < MEASUREMENTS_COUNT; i++)
      rms += (periodMeasurements[i] - avg) * (periodMeasurements[i] - avg);
    rms = sqrt(rms / (MEASUREMENTS_COUNT - SKIP_FIRST));

    Serial.print("AVG: "); Serial.print(avg); Serial.print(" us    ");
    Serial.print("RMS: "); Serial.println(rms);

    measurementIndex = 0;

    interrupts();
  }

  // посылаем импульс на Lichee
  t_start = micros();
  
  digitalWrite(PIN_OUT, HIGH);

  delayMicroseconds(DELAY_MICROSECONDS);
  
  digitalWrite(PIN_OUT, LOW);
    
  delayMicroseconds(DELAY_MICROSECONDS);
}

void onResponse() {
  if (measurementIndex < MEASUREMENTS_COUNT) {
    unsigned long now = micros();
    if((now-t_start) > MIN_DELAY)    // защита от двойного срабатывания прерывания
    {
      periodMeasurements[measurementIndex] = now - t_start;
      measurementIndex++;
    }
  }
}