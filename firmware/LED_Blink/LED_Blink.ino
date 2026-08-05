#define LED_PIN 18

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000);
  pinMode(LED_PIN, OUTPUT);

}

int state = LOW;

void loop() {
  // put your main code here, to run repeatedly:
  state = !state;
  digitalWrite(LED_PIN, state);
  delay(100);

}
