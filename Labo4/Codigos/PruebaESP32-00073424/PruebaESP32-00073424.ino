void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println("======================================");
  Serial.println("  FRANCISCO JAVIER MARTINEZ DONADO");
  Serial.println("               00073424 ");
  Serial.println("=====================================");
  Serial.println();

}

void loop() {
  
  Serial.print("ESP32 funcionando correctamente - Timepo activa: ");
  Serial.print(millis()/1000.0);
  Serial.println(" segundos");

  delay(2000);

}
