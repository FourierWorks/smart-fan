#define OPTO_PIN 25

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000);
  pinMode(OPTO_PIN, OUTPUT);

}

int state = LOW;

void loop() {
  // put your main code here, to run repeatedly:
  state = !state;
  digitalWrite(OPTO_PIN, state);
  Serial.println(state ? "OPTO ON  - node should be ~0.2V" : "OPTO OFF - node should be ~5V");
  delay(10000);
}
