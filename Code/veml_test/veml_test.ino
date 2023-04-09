/* VEML7700 Auto Lux Example
 *
 * This example sketch demonstrates reading lux using the automatic
 * method which adjusts gain and integration time as needed to obtain
 * a good reading. A non-linear correction is also applied if needed.
 *
 * See Vishy App Note "Designing the VEML7700 Into an Application"
 * Vishay Document Number: 84323, Fig. 24 Flow Chart
 */

#include "Adafruit_VEML7700.h"

Adafruit_VEML7700 veml = Adafruit_VEML7700();

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("Adafruit VEML7700 Auto Lux Test");

  if (!veml.begin()) {
    Serial.println("Sensor not found");
    while (1);
  }
  Serial.println("Sensor found");
}

void loop() {
  // to read lux using automatic method, specify VEML_LUX_AUTO
  float lux = veml.readLux(VEML_LUX_AUTO);

// typical lux values: (from Vishay App Note: 84323, https://www.vishay.com/docs/84323/designingveml7700.pdf)
// 0.002 lux - moonless, overcast night sky (starlight)
// 0.27 - 1.0 lux - moonlight (full moon)
// 3.4 lux - dark limit of civil twilight under a clear sky
// 50 - 100 lux - typical family living room lighting
// 100 lux - very dark overcast day
// 320 - 500 lux - office lighting
// 400 lux - sunrise or sunset on a clear day
// 1000 lux - overcast day; typical TV studio lighting
// 10,000 - 25,000 lux - full daylight (not direct sun)
// 32,000 - 100,000 lux - direct sunlight

  Serial.println("------------------------------------");
  Serial.print("Lux = "); Serial.println(lux);
  Serial.println("Settings used for reading:");
  Serial.print(F("Gain: "));
  switch (veml.getGain()) {
    case VEML7700_GAIN_1: Serial.println("1"); break;
    case VEML7700_GAIN_2: Serial.println("2"); break;
    case VEML7700_GAIN_1_4: Serial.println("1/4"); break;
    case VEML7700_GAIN_1_8: Serial.println("1/8"); break;
  }
  Serial.print(F("Integration Time (ms): "));
  switch (veml.getIntegrationTime()) {
    case VEML7700_IT_25MS: Serial.println("25"); break;
    case VEML7700_IT_50MS: Serial.println("50"); break;
    case VEML7700_IT_100MS: Serial.println("100"); break;
    case VEML7700_IT_200MS: Serial.println("200"); break;
    case VEML7700_IT_400MS: Serial.println("400"); break;
    case VEML7700_IT_800MS: Serial.println("800"); break;
  }
  Serial.print("Low threshold: "); Serial.println(veml.getLowThreshold());
  Serial.print("High threshold: "); Serial.println(veml.getHighThreshold());
  Serial.print("Integration time: "); Serial.println(veml.getIntegrationTime());
  Serial.print("Gain: "); Serial.println(veml.getGain());

  delay(1000);
}