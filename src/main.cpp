#define BLYNK_TEMPLATE_ID "TMPL608Q0deFC"
#define BLYNK_TEMPLATE_NAME "SmartFan IoT"
#define BLYNK_AUTH_TOKEN "Fi0skjq3gZonnrph6HVEt6qQF4ffYIoF"

#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "esp_system.h"

#include <Wire.h>
#include <RTClib.h>
#include <PZEM004Tv30.h>
#include <time.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

char ssid[] = "FIKRI";
char pass[] = "fikri2486";

#define RELAY_PIN 18
#define SOUND_PIN 34

#define RELAY_ON HIGH
#define RELAY_OFF LOW

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);

RTC_DS3231 rtc;

HardwareSerial PZEMSerial(2);

PZEM004Tv30 pzem(
    PZEMSerial,
    16,
    17
);

float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;
float frequency = 0;
float pf = 0;

#define THRESHOLD 80
#define DEBOUNCE_TIME 300

unsigned long lastClap = 0;

int soundLevel = 0;

bool relayState = false;

String fanStatus = "OFF";

//==================================================
// EEPROM DATA
//==================================================

#define EEPROM_MAGIC 0x55AA

struct SavedData
{
    uint16_t magic;

    uint32_t remainingSeconds;

    uint32_t dailyQuotaSeconds;

    uint32_t todayOperationSeconds;

    uint16_t todayClapCount;
};

#define EEPROM_START_ADDR 0

SavedData savedData;

// Timer sesi yang sedang berjalan
uint32_t remainingSeconds = 0;

// Kuota harian (8 jam)
uint32_t dailyQuotaSeconds = 8UL * 60UL * 60UL;

// Statistik
uint16_t todayClapCount = 0;
uint32_t todayOperationSeconds = 0;

// Reset harian
uint8_t lastDay = 0;

// Tick timer
unsigned long lastSecondTick = 0;

void writeEEPROM(uint16_t address,uint8_t data);

uint8_t readEEPROM(uint16_t address);

void writeEEPROMBlock(uint16_t address,
                      const uint8_t *data,
                      uint16_t length);

void readEEPROMBlock(uint16_t address,
                     uint8_t *data,
                     uint16_t length);

BlynkTimer timer;

// ================= OLED AUTO PAGE =================
uint8_t oledPage = 0;

unsigned long lastPageChange = 0;

const uint32_t PAGE_INTERVAL = 3000;   // 3 detik
// ==================================================

BLYNK_CONNECTED()
{
    Blynk.syncVirtual(V6);

    Blynk.virtualWrite(V6, relayState ? 1 : 0);
    Blynk.virtualWrite(V10, fanStatus);
}

void relayOn();
void relayOff(String status);

void relayOff(String status)
{
    relayState = false;

    digitalWrite(RELAY_PIN, RELAY_OFF);

    fanStatus = status;

    Serial.println("Relay OFF");

    Blynk.virtualWrite(V10, fanStatus);
}

void drawHomePage();
void drawPowerPage();
void drawPowerQualityPage();
void drawStatisticPage();
void drawSystemPage();
void drawClockPage();
void updateOLEDPage();
void saveState();
void loadState();

void relayOn()
{
    if (remainingSeconds == 0)
        return;

    if (relayState)
        return;

    relayState = true;

    digitalWrite(RELAY_PIN, RELAY_ON);

    fanStatus = "AKTIF";

    Blynk.virtualWrite(V10, fanStatus);

    Serial.println("Relay ON");
}

void addOneHour()
{
    if(dailyQuotaSeconds < 3600)
    {
        fanStatus = "LIMIT HARIAN";
        relayOff("LIMIT HARIAN");
        return;
    }

    if(remainingSeconds >= 8UL * 3600UL)
        return;

    remainingSeconds += 3600;

    if(remainingSeconds > 8UL * 3600UL)
        remainingSeconds = 8UL * 3600UL;

    dailyQuotaSeconds -= 3600;

    todayClapCount++;

    saveState();

    relayOn();

    Serial.println("======================");
    Serial.println("+1 JAM DITAMBAHKAN");
    Serial.print("Timer : ");
    Serial.println(remainingSeconds);
    Serial.print("Kuota : ");
    Serial.println(dailyQuotaSeconds);
    Serial.println("======================");
}

void drawHomePage()
{
    display.clearDisplay();

    display.setTextColor(SSD1306_WHITE);

    //----------------------------------
    // Judul
    //----------------------------------

    display.setTextSize(1);
    display.setCursor(18,0);
    display.print("SMART FAN IoT");

    display.drawLine(0,10,127,10,SSD1306_WHITE);

    //----------------------------------
    // Status
    //----------------------------------

    display.setCursor(0,16);
    display.print("Status : ");

    display.print(fanStatus);

    //----------------------------------
    // Timer
    //----------------------------------

    uint32_t h = remainingSeconds / 3600;
    uint32_t m = (remainingSeconds % 3600) / 60;
    uint32_t s = remainingSeconds % 60;

    display.setCursor(0,30);
    display.printf("Timer  : %02lu:%02lu:%02lu",
                   h,m,s);

    //----------------------------------
    // Kuota
    //----------------------------------

    uint32_t quotaHour =
        dailyQuotaSeconds / 3600;

    display.setCursor(0,45);
    display.printf("Kuota  : %lu / 8 Jam",
                   quotaHour);

    //----------------------------------
    // Relay
    //----------------------------------

    display.setCursor(0,56);

    display.print("Relay : ");

    display.print(
        relayState ?
        "ON":"OFF");

    display.display();
}

void drawPowerPage()
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    //==========================
    // Header
    //==========================
    display.setCursor(8, 0);
    display.print("MONITOR LISTRIK");

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    //==========================
    // Voltage
    //==========================
    display.setCursor(0, 16);
    display.printf("Voltage : %6.1f V", voltage);

    //==========================
    // Current
    //==========================
    display.setCursor(0, 28);
    display.printf("Current : %6.2f A", current);

    //==========================
    // Power
    //==========================
    display.setCursor(0, 40);
    display.printf("Power   : %6.1f W", power);

    //==========================
    // Energy
    //==========================
    display.setCursor(0, 52);
    display.printf("Energy  : %6.3f kWh", energy);

    display.display();
}

void drawPowerQualityPage()
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    //-------------------------
    // Header
    //-------------------------
    display.setCursor(8,0);
    display.print("POWER QUALITY");

    display.drawLine(0,10,127,10,SSD1306_WHITE);

    //-------------------------
    // Frequency
    //-------------------------
    display.setCursor(0,16);
    display.printf("Freq : %.1f Hz", frequency);

    //-------------------------
    // Power Factor
    //-------------------------
    display.setCursor(0,28);
    display.printf("PF   : %.2f", pf);

    //-------------------------
    // Status Beban
    //-------------------------
    display.setCursor(0,40);

    if(power > 5)
        display.print("Load : NORMAL");
    else
        display.print("Load : IDLE");

    //-------------------------
    // Status PZEM
    //-------------------------
    display.setCursor(0,52);

    if(!isnan(voltage))
        display.print("PZEM : CONNECT");
    else
        display.print("PZEM : ERROR");

    display.display();
}

void drawStatisticPage()
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    //-------------------------
    // Header
    //-------------------------
    display.setCursor(12,0);
    display.print("STATISTIK HARI INI");

    display.drawLine(0,10,127,10,SSD1306_WHITE);

    //-------------------------
    // Tepukan
    //-------------------------
    display.setCursor(0,16);
    display.printf("Tepukan : %u x", todayClapCount);

    //-------------------------
    // Waktu Operasi
    //-------------------------
    uint32_t opHour = todayOperationSeconds / 3600;
    uint32_t opMin  = (todayOperationSeconds % 3600) / 60;

    display.setCursor(0,30);
    display.printf("Operasi : %02lu:%02lu",
                   opHour,
                   opMin);

    //-------------------------
    // Sisa Kuota
    //-------------------------
    uint32_t qHour = dailyQuotaSeconds / 3600;
    uint32_t qMin  = (dailyQuotaSeconds % 3600) / 60;

    display.setCursor(0,44);
    display.printf("Sisa    : %02lu:%02lu",
                   qHour,
                   qMin);

    //-------------------------
    // Persentase Kuota
    //-------------------------
    uint8_t percent =
        (dailyQuotaSeconds * 100UL) /
        (8UL * 3600UL);

    display.setCursor(0,56);
    display.printf("Kuota   : %u%%", percent);

    display.display();
}

void drawSystemPage()
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    //-------------------------
    // Header
    //-------------------------
    display.setCursor(12,0);
    display.print("STATUS SISTEM");

    display.drawLine(0,10,127,10,SSD1306_WHITE);

    //-------------------------
    // WiFi
    //-------------------------
    display.setCursor(0,16);

    if(WiFi.status() == WL_CONNECTED)
        display.print("WiFi  : ONLINE");
    else
        display.print("WiFi  : OFFLINE");

    //-------------------------
    // Blynk
    //-------------------------
    display.setCursor(0,28);

    if(Blynk.connected())
        display.print("Blynk : ONLINE");
    else
        display.print("Blynk : OFFLINE");

    //-------------------------
    // RTC
    //-------------------------
    display.setCursor(0,40);

    if(rtc.begin())
        display.print("RTC   : READY");
    else
        display.print("RTC   : ERROR");

    //-------------------------
    // Heap
    //-------------------------
    display.setCursor(0,52);

    display.print("Heap  : ");
    display.print(ESP.getFreeHeap() / 1024);
    display.print(" KB");

    display.display();
}

void drawClockPage()
{
    DateTime now = rtc.now();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    //-------------------------
    // Header
    //-------------------------
    display.setCursor(15,0);
    display.print("TANGGAL & JAM");

    display.drawLine(0,10,127,10,SSD1306_WHITE);

    //-------------------------
    // Tanggal
    //-------------------------
    display.setCursor(20,20);
    display.printf("%02d/%02d/%04d",
                   now.day(),
                   now.month(),
                   now.year());

    //-------------------------
    // Jam
    //-------------------------
    display.setTextSize(2);
    display.setCursor(18,38);
    display.printf("%02d:%02d:%02d",
                   now.hour(),
                   now.minute(),
                   now.second());

    display.display();
}

void drawOLED()
{
    switch(oledPage)
    {
        case 0:
            drawHomePage();
            break;

        case 1:
            drawPowerPage();
            break;

        case 2:
            drawPowerQualityPage();
            break;

        case 3:
            drawStatisticPage();
            break;

        case 4:
            drawSystemPage();
            break;

        case 5:
            drawClockPage();
            break;
    }
}

BLYNK_WRITE(V6)
{
    if(param.asInt() == 1)
    {
        addOneHour();
    }
}

void checkClap()
{
    int peak = 0;

    for (int i = 0; i < 25; i++)
    {
        int sample = analogRead(SOUND_PIN);

        if (sample > peak)
            peak = sample;
    }

    soundLevel = peak;

    unsigned long now = millis();

    if (peak > THRESHOLD &&
        now - lastClap > DEBOUNCE_TIME)
    {
        addOneHour();

        Blynk.virtualWrite(V8, todayClapCount);

        lastClap = now;
    }
}

void sendData()
{
    char runtimeStr[20];

    uint32_t h = remainingSeconds / 3600;
    uint32_t m = (remainingSeconds % 3600) / 60;
    uint32_t s = remainingSeconds % 60;

    sprintf(
        runtimeStr,
        "%02lu:%02lu:%02lu",
        h,
        m,
        s);

    Blynk.virtualWrite(
        V9,
        runtimeStr);

    if(!isnan(voltage))
        Blynk.virtualWrite(V0, voltage);

    if(!isnan(current))
        Blynk.virtualWrite(V1, current);

    if(!isnan(power))
        Blynk.virtualWrite(V2, power);

    if(!isnan(energy))
        Blynk.virtualWrite(V3, energy);

    if(!isnan(frequency))
        Blynk.virtualWrite(V4, frequency);

    if(!isnan(pf))
        Blynk.virtualWrite(V5, pf);

    Blynk.virtualWrite(
        V7,
        soundLevel);
    
    Blynk.virtualWrite(
        V8,
        todayClapCount);

if(dailyQuotaSeconds==0)
{
    fanStatus="LIMIT HARIAN";
}
else
{
    fanStatus =
    relayState ?
    "AKTIF":"SIAGA";
}

    Blynk.virtualWrite(
        V10,
        fanStatus);

Serial.print("Heap : ");
Serial.println(ESP.getFreeHeap());

char quotaStr[20];

sprintf(quotaStr,
"%02lu:%02lu:%02lu",
dailyQuotaSeconds/3600,
(dailyQuotaSeconds%3600)/60,
dailyQuotaSeconds%60);

Blynk.virtualWrite(V11, quotaStr);

char todayRuntimeStr[20];

sprintf(todayRuntimeStr,
"%02lu:%02lu:%02lu",
todayOperationSeconds/3600,
(todayOperationSeconds%3600)/60,
todayOperationSeconds%60);

Blynk.virtualWrite(V12, todayRuntimeStr);
}

void readPZEM()
{
    float v = pzem.voltage();
    float c = pzem.current();
    float p = pzem.power();
    float e = pzem.energy();
    float f = pzem.frequency();
    float pff = pzem.pf();

    if(!isnan(v)) voltage = v;
    if(!isnan(c)) current = c;
    if(!isnan(p)) power = p;
    if(!isnan(e)) energy = e;
    if(!isnan(f)) frequency = f;
    if(!isnan(pff)) pf = pff;
}

void updateDisplay()
{
    readPZEM();

    updateOLEDPage();

    drawOLED();
}

void updateOLEDPage()
{
    if(millis() - lastPageChange >= PAGE_INTERVAL)
    {
        lastPageChange = millis();

        oledPage = (oledPage + 1) % 6;
    }
}

void updateTimer()
{
    DateTime now = rtc.now();

    // Reset harian
    if(lastDay != now.day())
    {
        lastDay = now.day();

        dailyQuotaSeconds = 8UL * 3600UL;

        todayClapCount = 0;

        todayOperationSeconds = 0;

        Serial.println("Kuota Harian Reset");
    }

    if(relayState)
    {
        if(millis()-lastSecondTick>=1000)
        {
            lastSecondTick = millis();

            if(remainingSeconds>0)
            {
                remainingSeconds--;

                todayOperationSeconds++;
            }

            if(remainingSeconds==0)
            {
                relayOff("SESI SELESAI");
            }
        }
    }
}

void syncRTCWithNTP()
{
    Serial.println("Sinkronisasi waktu NTP...");

    configTime(
        7 * 3600,   // WIB (GMT+7)
        0,
        "pool.ntp.org",
        "time.nist.gov");

    struct tm timeinfo;

    int retry = 0;

    while(!getLocalTime(&timeinfo) && retry < 10)
    {
        Serial.print(".");
        delay(500);
        retry++;
    }

    if(retry >= 10)
    {
        Serial.println("\nGagal mendapatkan waktu NTP");
        return;
    }

    rtc.adjust(DateTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec));

    Serial.println("\nRTC berhasil disinkronkan");

    Serial.printf(
        "%02d/%02d/%04d %02d:%02d:%02d\n",
        timeinfo.tm_mday,
        timeinfo.tm_mon + 1,
        timeinfo.tm_year + 1900,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);
}

void printResetReason()
{
    esp_reset_reason_t reason = esp_reset_reason();

    Serial.print("Reset Reason : ");

    switch(reason)
    {
        case ESP_RST_POWERON:
            Serial.println("Power ON");
            break;

        case ESP_RST_SW:
            Serial.println("Software Reset");
            break;

        case ESP_RST_PANIC:
            Serial.println("Guru Meditation");
            break;

        case ESP_RST_INT_WDT:
            Serial.println("Interrupt Watchdog");
            break;

        case ESP_RST_TASK_WDT:
            Serial.println("Task Watchdog");
            break;

        case ESP_RST_WDT:
            Serial.println("Watchdog");
            break;

        case ESP_RST_BROWNOUT:
            Serial.println("Brownout");
            break;

        default:
            Serial.println("Unknown");
            break;
    }
}

#define EEPROM_ADDR 0x57

void writeEEPROM(uint16_t address, uint8_t data)
{
    Wire.beginTransmission(EEPROM_ADDR);

    Wire.write(address >> 8);
    Wire.write(address & 0xFF);

    Wire.write(data);

    Wire.endTransmission();

    delay(5);
}

uint8_t readEEPROM(uint16_t address)
{
    Wire.beginTransmission(EEPROM_ADDR);

    Wire.write(address >> 8);
    Wire.write(address & 0xFF);

    Wire.endTransmission();

    Wire.requestFrom(EEPROM_ADDR,1);

    if(Wire.available())
        return Wire.read();

    return 0;
}

void writeEEPROMBlock(uint16_t address,
                      const uint8_t *data,
                      uint16_t length)
{
    for(uint16_t i=0;i<length;i++)
    {
        writeEEPROM(address+i,data[i]);
    }
}

void readEEPROMBlock(uint16_t address,
                     uint8_t *data,
                     uint16_t length)
{
    for(uint16_t i=0;i<length;i++)
    {
        data[i]=readEEPROM(address+i);
    }
}

void saveState()
{
    savedData.magic = EEPROM_MAGIC;

    savedData.remainingSeconds = remainingSeconds;

    savedData.dailyQuotaSeconds = dailyQuotaSeconds;

    savedData.todayOperationSeconds = todayOperationSeconds;

    savedData.todayClapCount = todayClapCount;

    writeEEPROMBlock(
        EEPROM_START_ADDR,
        (uint8_t*)&savedData,
        sizeof(SavedData));

    Serial.println("======================");
    Serial.println("EEPROM SAVE");
    Serial.println("======================");

    Serial.print("Timer : ");
    Serial.println(savedData.remainingSeconds);

    Serial.print("Quota : ");
    Serial.println(savedData.dailyQuotaSeconds);

    Serial.print("Runtime : ");
    Serial.println(savedData.todayOperationSeconds);

    Serial.print("Clap : ");
    Serial.println(savedData.todayClapCount);

    Serial.println("======================");
}

void loadState()
{
    readEEPROMBlock(
        EEPROM_START_ADDR,
        (uint8_t*)&savedData,
        sizeof(SavedData));

    // Cek apakah data valid
    if(savedData.magic != EEPROM_MAGIC)
    {
        Serial.println("======================");
        Serial.println("EEPROM BELUM TERISI");
        Serial.println("Gunakan Data Default");
        Serial.println("======================");

        return;
    }

    // Restore data
    remainingSeconds = savedData.remainingSeconds;

    dailyQuotaSeconds = savedData.dailyQuotaSeconds;

    todayOperationSeconds = savedData.todayOperationSeconds;

    todayClapCount = savedData.todayClapCount;

    // Tentukan status relay
    if(remainingSeconds > 0)
    {
        relayOn();
    }
    else
    {
        relayOff("SIAGA");
    }

    Serial.println("======================");
    Serial.println("EEPROM LOAD");
    Serial.println("======================");

    Serial.print("Timer : ");
    Serial.println(remainingSeconds);

    Serial.print("Quota : ");
    Serial.println(dailyQuotaSeconds);

    Serial.print("Runtime : ");
    Serial.println(todayOperationSeconds);

    Serial.print("Clap : ");
    Serial.println(todayClapCount);

    Serial.println("======================");
}

void setup()
{
    Serial.begin(115200);

    delay(500);

    printResetReason();

    pinMode(
    RELAY_PIN,
    OUTPUT);

    relayState = false;
    fanStatus = "OFF";
    digitalWrite(RELAY_PIN, RELAY_OFF);

    analogReadResolution(12);

    Wire.begin(
        21,
        22);

    if(!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDR))
    {
        while(true);
    }

    if(!rtc.begin())
{
    Serial.println("RTC Tidak Terdeteksi");
    while(true);
}
    lastDay = rtc.now().day();

    PZEMSerial.begin(
        9600,
        SERIAL_8N1,
        16,
        17);

    Blynk.begin(
        BLYNK_AUTH_TOKEN,
        ssid,
        pass);

     syncRTCWithNTP();
     loadState();
     saveState();
     readPZEM();
     drawOLED();

    timer.setInterval(
        10000L,
        sendData);
    
    timer.setInterval(
        1000L,
        updateDisplay);  
    
    timer.setInterval(
        1000L,
        updateTimer);

Serial.println("===========================");
Serial.println("SMART FAN IoT READY");
Serial.println("===========================");
}

void loop()
{
    Blynk.run();
    timer.run();
    checkClap();
}