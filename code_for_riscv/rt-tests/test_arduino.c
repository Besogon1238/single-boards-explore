const int SKIP_FIRST = 2;
const int MEASUREMENTS_COUNT = 102;
volatile unsigned long periodMeasurements[MEASUREMENTS_COUNT];
volatile int measurementIndex = 0;
volatile unsigned long previousTime = 0;

void setup() {
  Serial.begin(9600);
  pinMode(18, INPUT);
  attachInterrupt(digitalPinToInterrupt(18), handleInterrupt, HIGH);
}

void loop() {
  if (measurementIndex == MEASUREMENTS_COUNT) {

    unsigned long totalSum = 0;
    for (int i = SKIP_FIRST; i < MEASUREMENTS_COUNT; i++) {
      totalSum += periodMeasurements[i];
    }

    unsigned long averagePeriod = totalSum / (MEASUREMENTS_COUNT - SKIP_FIRST);
    double rootMeanSquareDeviation = 0.0;

    for (int i = SKIP_FIRST; i < MEASUREMENTS_COUNT; i++) {
      Serial.print("Period [");
      Serial.print(i);
      Serial.print("] : ");
      Serial.print(periodMeasurements[i]);
      Serial.println(" us");

      rootMeanSquareDeviation += (averagePeriod - periodMeasurements[i]) * (averagePeriod - periodMeasurements[i]);
    }

    rootMeanSquareDeviation = sqrt(rootMeanSquareDeviation / (MEASUREMENTS_COUNT - SKIP_FIRST));

    Serial.print("Average Period: ");
    Serial.print(averagePeriod);
    Serial.print(" us, RMSD: ");
    Serial.print(rootMeanSquareDeviation);
    Serial.println(" us");
    Serial.println("");
    measurementIndex = 0;
    totalSum = 0;

  }
}

void a() {
  unsigned long currentTime = micros();
  periodMeasurements[measurementIndex] = currentTime - previousTime;
  previousTime = currentTime;
  measurementIndex++;
}