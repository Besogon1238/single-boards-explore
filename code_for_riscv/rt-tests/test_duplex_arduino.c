const int PIN_OUT = 19;        // Arduino → Lichee (сигнал "начал")
const int PIN_IN  = 18;       // Lichee → Arduino (ответный импульс)

const int SKIP_FIRST = 2;
const int MEASUREMENTS_COUNT = 102;

volatile unsigned long periodMeasurements[MEASUREMENTS_COUNT];
volatile int measurementIndex = 0;

volatile unsigned long t_start = 0;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_OUT, OUTPUT);
  pinMode(PIN_IN, INPUT);

  attachInterrupt(digitalPinToInterrupt(PIN_IN), onResponse, RISING);
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
  delayMicroseconds(1);
  digitalWrite(PIN_OUT, LOW);

  delay(10); // период между тестами
}

void onResponse() {
  if (measurementIndex < MEASUREMENTS_COUNT) {
    unsigned long now = micros();
    periodMeasurements[measurementIndex] = now - t_start;
    measurementIndex++;
  }
}
