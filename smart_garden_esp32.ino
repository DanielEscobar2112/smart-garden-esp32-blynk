/***************************************
 * SMART GARDEN ESP32 BLYNK IoT
 * Board  : ESP32 DEVKIT
 * Input  : DHT11, DS18B20, SOIL MOISTURE
 * Output : BLYNK LCD & WEB DASHBOARD
 *
 * NOTE: WiFi and Blynk credentials live in a separate file,
 *       "Secrets.h", which is NOT committed to the repository.
 *       Copy "Secrets.h.example" to "Secrets.h" and fill in
 *       your own values before uploading to the board.
 ****************************************/

#include "Secrets.h"   // BLYNK_TEMPLATE_ID, BLYNK_TEMPLATE_NAME, BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#define DHTPIN 5
#define DHTTYPE DHT11
#define ONE_WIRE_BUS 4
#define pump 14

DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Inisialisasi Fitur Timer Blynk
BlynkTimer timer;

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;

const int AirValue = 2620;
const int WaterValue = 1180;
int soilMoistureValue = 0;
int soilmoist = 0;
int humi, temp, fp, sistem;
int temp_lingkungan;
int buttonState;

// ============================================================
// AMBANG KONTROL POMPA (silakan sesuaikan sesuai kebutuhan)
// ============================================================
// -- Kelembapan tanah (%) --
int SP_KERING        = 40;  // tanah kering   -> pompa ON
int SP_KERING_STRES  = 50;  // saat panas/udara kering -> mulai menyiram lebih awal
int SP_BASAH         = 60;  // tanah lembap   -> pompa OFF
int SP_BANJIR        = 70;  // batas atas     -> paksa OFF (cegah genangan)

// -- Suhu lingkungan / udara (DHT11, derajat C) --
int SUHU_LING_DINGIN = 15;  // < 15 C  -> blokir pompa
int SUHU_LING_PANAS  = 32;  // > 32 C  -> kondisi stres panas

// -- Suhu tanah (DS18B20, derajat C) --
int SUHU_TANAH_DINGIN = 18; // < 18 C  -> blokir pompa
int SUHU_TANAH_PANAS  = 30; // >= 30 C -> kondisi stres panas

// -- Kelembapan udara (DHT11, %) --
int HUMI_KERING       = 40; // < 40 %  -> udara kering (stres)

//=============================
void read_DHT11(){
  humi = dht.readHumidity();
  temp_lingkungan = dht.readTemperature();

  if (isnan(humi) || isnan(temp_lingkungan)) {
    Serial.println("DHT11 tidak terbaca... !");
    return;
  }
  else {
    lcd.setCursor(4, 1);
    lcd.print(humi);
    lcd.print("%");
  }
}

//===========
void read_SoilMoist(){
  soilMoistureValue = analogRead(A6);
  soilmoist = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
  if(soilmoist >= 100)  soilmoist = 100;
  else if(soilmoist <= 0) soilmoist = 0;

  Serial.print("Soil Moisture :");
  Serial.print(soilmoist);
  Serial.println("%");

  lcd.setCursor(4, 0);
  if(soilmoist < 10) lcd.print(" ");
  lcd.print(soilmoist);
  lcd.print("%");
}

//---------------------
void read_DS1820(){
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(0);

  lcd.setCursor(12, 0);
  if(temp < 10 && temp >= 0) lcd.print(" ");
  lcd.print(temp);
  lcd.print((char)223);
  lcd.print("C");
}

//=================================
BLYNK_WRITE(V3){
  buttonState = param.asInt();
  if(buttonState == HIGH){
    sistem = 1;
    fp = 0;
  }
  else if(buttonState == LOW){
    sistem = 0;
  }
}

//==========================================
BLYNK_WRITE(V4){
  buttonState = param.asInt();
  if(sistem == 0){
    if(buttonState == LOW){
      digitalWrite(pump, HIGH);
      lcd.setCursor(12, 1);
      lcd.print("OFF");
    }
    else if(buttonState == HIGH){
      digitalWrite(pump, LOW);
      lcd.setCursor(12, 1);
      lcd.print("ON ");
    }
  }
}

//============================================================
// FUNGSI UTAMA EKSEKUSI SISTEM (DIPANGGIL SETIAP 2 DETIK OLEH TIMER)
//============================================================
void jalankanSistemUtama() {
  // 1. Baca semua sensor
  read_DHT11();
  read_SoilMoist();
  read_DS1820();

  // 2. Kirim data ke Blynk Cloud secara berkala (Aman dari pembatasan data)
  Blynk.virtualWrite(V0, temp);            // Suhu tanah (DS18B20)
  Blynk.virtualWrite(V1, soilmoist);       // Kelembapan tanah
  Blynk.virtualWrite(V2, humi);            // Kelembapan udara
  Blynk.virtualWrite(V5, temp_lingkungan); // Suhu lingkungan (DHT11)

  // 3. Logika otomatisasi (hanya saat mode AUTO aktif)
  if (sistem == 1) {

    // --- Evaluasi kondisi lingkungan ---
    // Catatan konvensi: pompa AKTIF-LOW -> LOW = nyala, HIGH = mati
    bool dinginEkstrem = (temp_lingkungan < SUHU_LING_DINGIN) ||
                         (temp < SUHU_TANAH_DINGIN);

    bool tanahBanjir   = (soilmoist >= SP_BANJIR);

    // Stres = panas atau udara kering -> tanaman menguap lebih cepat,
    // jadi sistem menyiram lebih awal (ambang nyala dinaikkan).
    bool stresPanas    = (temp_lingkungan > SUHU_LING_PANAS) ||
                         (temp >= SUHU_TANAH_PANAS) ||
                         (humi < HUMI_KERING);

    int ambangNyala = stresPanas ? SP_KERING_STRES : SP_KERING;

    // --- PRIORITAS 1 & 2: PROTEKSI (selalu paksa OFF) ---
    if (dinginEkstrem || tanahBanjir) {
      digitalWrite(pump, HIGH);   // OFF
      fp = 0;
      lcd.setCursor(12, 1);
      lcd.print(dinginEkstrem ? "BLK" : "OFF");

      if (dinginEkstrem)
        Serial.println("PROTEKSI: Suhu terlalu dingin -> Pompa DIBLOKIR (BLK)");
      else
        Serial.println("PROTEKSI: Tanah jenuh (>=70%) -> Pompa OFF");
    }
    // --- PRIORITAS 3: PENYIRAMAN NORMAL (histeresis kelembapan tanah) ---
    else if ((soilmoist < ambangNyala) && (fp == 0)) {
      digitalWrite(pump, LOW);    // ON
      fp = 1;
      lcd.setCursor(12, 1);
      lcd.print("ON ");
      Serial.print("Pompa ON  (tanah ");
      Serial.print(soilmoist);
      Serial.print("% < ambang ");
      Serial.print(ambangNyala);
      Serial.println("%)");
    }
    else if ((soilmoist >= SP_BASAH) && (fp == 1)) {
      digitalWrite(pump, HIGH);   // OFF
      fp = 0;
      lcd.setCursor(12, 1);
      lcd.print("OFF");
      Serial.println("Pompa OFF (tanah sudah lembap >=60%)");
    }
    // else: pertahankan kondisi terakhir (zona histeresis / dead band)
  }
}

//==============================
void setup()
{
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("= Smart Garden =");
  lcd.setCursor(0, 1);
  lcd.print("    IoT ESP32   ");

  pinMode(pump, OUTPUT);
  digitalWrite(pump, HIGH);
  dht.begin();
  sensors.begin();
  delay(2000);

  Blynk.begin(auth, ssid, pass);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mst=     T=     ");
  lcd.setCursor(0, 1);
  lcd.print("Hum=     P=OFF  ");

  // ATUR TIMER: Menjalankan fungsi sistem setiap 2000 milidetik (2 detik)
  timer.setInterval(2000L, jalankanSistemUtama);
}

//=============================
void loop()
{
  Blynk.run();
  timer.run(); // Menjalankan pengatur waktu Blynk tanpa menghentikan koneksi
}
