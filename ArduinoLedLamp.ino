/********************************************************************************************/
/*  Arduino Led Lamp - ver 1.0.0.0 beta                                                     */
/*  Copyright (c) 2015 UnitSoftware Giewon Grzegorz                                         */
/*  Wszelkie prawa zastrzeżone                                                              */
/*																							*/
/*  Poniższy kod programu można używać i rozpowszechniać za darmo pod warunkiem	zachowania  */
/*  informacji o licencji i autorstwie.														*/
/*																							*/
/*  Poniższy kod udostępniony jest bez żadnej gwarancji, używasz go na własne ryzyko.		*/
/*  Autor nie ponosi odpowiedzialności za szkody, utratę zysków, lub jakiekolwiek inne      */
/*  straty wynikłe w konsekwencji używania lub niemożności użycia poniższego kodu.			*/
/*																							*/
/*  Giewon Grzegorz, ggiewon@gmail.com														*/
/********************************************************************************************/
/* Kod powstał z pomocą kodów źródłowych AQma LED Control, których autorem jest Marcin Grunt*/
/*																							*/
/* Wszelkie prawa do AQma LED Control są zastrzeżone przez autora : Marcina Grunta.         */
/* Więcej informacji o AQma pod adresem: http://magu.pl/aqma-led-control                    */
/********************************************************************************************/

// Dla kodu kompilowanego pod MEGA należy odkomentować - aktualnie brak dodatkowej funkcjonalności pod MEGA
//#define MEGA

#include <TFT_ILI9341.h>
#include <SPI.h>
#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

/*********** Wersja ****************/
#define SOFTWARE_VERSION "AQma LED Control, ver 0.0.1" // Aktualnie wymagane do współpracy z oprogramowaniem AQma

/*********** Definicje pinów ****************/
#ifdef MEGA
// 1-wire communication
#define ONE_WIRE_PIN 8

// error led pinout
#define ERRORLED_PIN 7

// lcd led backlight
#define LCDLED_PIN 16

// PWM pinouts
#define PWM1_PIN 5
#define PWM2_PIN 6
#define PWM3_PIN 9

#define PWM_FAN_PIN 10
#else
// 1-wire communication
#define ONE_WIRE_PIN 8

// error led pinout
#define ERRORLED_PIN 7

// lcd led backlight
#define LCDLED_PIN 16

// PWM pinouts
#define PWM1_PIN 5
#define PWM2_PIN 6
#define PWM3_PIN 9

#define PWM_FAN_PIN 10
#endif

/*********** Definicje komend ****************/
#define CMD_TEMP_INFO				22
#define CMD_SET_MAX_RADIATOR_TEMP	23
#define CMD_GET_MAX_RADIATOR_TEMP	24
#define CMD_PWM_STATUS				25
#define CMD_SET_LCD_BACKLIGHT		27
#define CMD_PWM1					31
#define CMD_PWM2					32
#define CMD_PWM3					33
#define CMD_PWM4					34
#define CMD_PWM5					35
#define CMD_PWM6					36
#define CMD_SET_TIME				40
#define CMD_GET_CONFIG_DATA			66
#define CMD_GET_TIME				83
#define CMD_GET_VERSION				80

/*********** Definicje statusów kanałów PWM ****************/
#define CHANNEL_STATUS_OFF 0
#define CHANNEL_STATUS_SUNRIDE 1
#define CHANNEL_STATUS_SUNSET 2
#define CHANNEL_STATUS_ON 3
#define CHANNEL_STATUS_NIGHT 4

/*********** Definicje statusów diody ****************/
#define STATUSLED_ERROR 0
#define STATUSLED_OK 1

/*********** Konfiguracja - adresy w pamięci EEPROM ****************/
#define CONFIG_MAXRADIATORTEMP_ADDRESS 250
#define CONFIG_PWM_BEGIN_ADDRESS 282

/*********** PWM - definicje kolorów na wyświetlaczu LED ****************/
#define PWM1_COLOR TFT_BLUE
#define PWM2_COLOR TFT_WHITE
#define PWM3_COLOR TFT_DARKCYAN

#define FAN_COLOR TFT_DARKCYAN

/*********** Definicje elementów UI ****************/
#define UI_DISPLAY_POS_Y 40

#define UI_STATUS_SYMBOL_X_DELTA 30
#define UI_TEMP_POS_Y 140
#define UI_STATUS_SYMBOL_POS_X 300
#define UI_PWM_SYMBOL_POSX 220
#define UI_PWM_VALUE_POSX 240

/*********** Definicja wyświetlacza ****************/
TFT_ILI9341 tft = TFT_ILI9341();

/*********** Podświetlenie wyświetlacza ****************/
byte lcdBacklightStatus = HIGH; // Domyślnie jest włączone

/*********** Definicja komunikacji OneWire z sensorami temperatury ****************/
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

/*********** Adresy sensorów temperatury****************/
DeviceAddress radiatorSensor = { 0x28, 0x51, 0x2B, 0xBB, 0x03, 0x00, 0x00, 0x6E }; // Należy podać adres sensora na radiatorze
DeviceAddress waterSensor = { 0x28, 0x83, 0x91, 0xE2, 0x06, 0x00, 0x00, 0x48 };    // Należy podać adres sensora umieszczonego w wodzie

/*********** Info czy lampa jest przegrzana ****************/
volatile bool isOverheated = false;

/*********** Aktualny czas ****************/
tmElements_t actualTime;

/*********** Ostatnio wyświetlana minuta ****************/
volatile byte lastDisplayedMinute;

/*********** PWM wentylatora ****************/
byte pwmFanValue;

/*********** Ilość kanałów PWM ****************/
#define PWM_COUNT 3

/*********** PWM poszczególnych kanałów ****************/
byte pwmPin[PWM_COUNT] = { PWM1_PIN, PWM2_PIN, PWM3_PIN };

uint16_t pwmColors[PWM_COUNT] = { PWM1_COLOR, PWM2_COLOR, PWM3_COLOR };

bool pwmEnable[PWM_COUNT] = { 0, 0, 0 };

byte pwmStatus[PWM_COUNT] = { 0, 0, 0 };
byte pwmPrevStatus[PWM_COUNT] = { 0, 0, 0 };

byte pwmValue[PWM_COUNT] = { 0, 0, 0 };
byte pwmLastValue[PWM_COUNT] = { 0, 0, 0 };

byte pwmHStart[PWM_COUNT];
byte pwmMStart[PWM_COUNT];
byte pwmSStart[PWM_COUNT];

byte pwmHEnd[PWM_COUNT];
byte pwmMEnd[PWM_COUNT];
byte pwmSEnd[PWM_COUNT];

byte pwmMin[PWM_COUNT] = { 0, 0, 0 };
byte pwmMax[PWM_COUNT] = { 0, 0, 0 };

bool pwmNightLight[PWM_COUNT];

byte pwmSunrise[PWM_COUNT];
byte pwmSunset[PWM_COUNT];

/*********** Temperatura radiatora i wody ****************/
volatile float radiatorTemperature;
volatile float waterTemperature;

/*********** Max temperatura radiatora przy której lampa się wyłączy jako przegrzana ****************/
byte maxRadiatorTemp; 

/*********** Min temperatura przy której załączy się wentylator ****************/
#define FAN_MIN_TEMP_ON 40

// Odczyt konfiguracji z EEPROM'u
void ReadConfiguration()
{
	maxRadiatorTemp = EEPROM.read(CONFIG_MAXRADIATORTEMP_ADDRESS);

	for (byte i = 0; i < PWM_COUNT; i++)
	{
		int confFirstAddress = CONFIG_PWM_BEGIN_ADDRESS + (i * 30);

		pwmEnable[i] = EEPROM.read(confFirstAddress);
		pwmHStart[i] = EEPROM.read(confFirstAddress + 1);
		pwmMStart[i] = EEPROM.read(confFirstAddress + 2);
		pwmSStart[i] = EEPROM.read(confFirstAddress + 3);

		pwmHEnd[i] = EEPROM.read(confFirstAddress + 4);
		pwmMEnd[i] = EEPROM.read(confFirstAddress + 5);
		pwmSEnd[i] = EEPROM.read(confFirstAddress + 6);

		pwmMin[i] = EEPROM.read(confFirstAddress + 7) * 100 / 256;
		pwmMax[i] = EEPROM.read(confFirstAddress + 8) * 100 / 256;

		pwmSunrise[i] = EEPROM.read(confFirstAddress + 9);
		pwmSunset[i] = EEPROM.read(confFirstAddress + 10);

		pwmNightLight[i] = EEPROM.read(confFirstAddress + 11);
	}
}

// Metoda sprawdzająca czy sensory temperatury są podłączone
bool CheckTempSensors()
{
	bool result = true;

	result = result && sensors.isConnected(radiatorSensor);

	result = result && sensors.isConnected(waterSensor);

	return !result;
}

// Metoda odczytująca 
String ReadCommandFromSerial()
{
	char tmpChar = 0;

	String command = "";

	while (Serial.available() > 0)
	{
		tmpChar = Serial.read();		
		command.concat(tmpChar);
		delay(5);
	}

	return command;
}

// Metoda odczytująca temperaturę z sensorów
void ReadTemperatures()
{
	sensors.requestTemperatures();
	radiatorTemperature = sensors.getTempC(radiatorSensor);
	waterTemperature = sensors.getTempC(waterSensor);
}

// Metoda odpowiedzialna za przetworzenie komendy
boolean ProcessCommand(char commandData[64])
{
	unsigned int val[64];
	char *cmdVal;
	int i = 0;

	cmdVal = strtok(commandData, ",");

	while (cmdVal)
	{
		val[i++] = atoi(cmdVal);
		cmdVal = strtok(NULL, ",");
	}

	char response[24];

	if (val[0] == CMD_SET_LCD_BACKLIGHT)
	{
		if (val[1] == 1)
		{
			lcdBacklightStatus = HIGH;
		}
		else {
			lcdBacklightStatus = LOW;
		}
	}

	if (val[0] == CMD_PWM_STATUS)
	{
		sprintf(response, "*%i,%i,%i,%i,%i", val[0], pwmFanValue, pwmValue[0], pwmValue[1], pwmValue[2]);
		Serial.println(response);
	}

	if (val[0] == CMD_TEMP_INFO)
	{
		char radiatorTemperatureStr[7];
		dtostrf(radiatorTemperature, 2, 0, radiatorTemperatureStr);

		char waterTemperatureStr[7];
		dtostrf(waterTemperature, 2, 2, waterTemperatureStr);

		sprintf(response, "*%i,%s,%s", val[0], radiatorTemperatureStr, waterTemperatureStr);
		Serial.println(response);
	}

	if (val[0] == CMD_SET_MAX_RADIATOR_TEMP)
	{
		byte radiatorTemp = radiatorTemp = val[1];

		EEPROM.write(CONFIG_MAXRADIATORTEMP_ADDRESS, radiatorTemp);
	}

	if (val[0] == CMD_GET_MAX_RADIATOR_TEMP)
	{
		sprintf(response, "*%i,%i", val[0], maxRadiatorTemp);
		Serial.println(response);
	}

	if ((val[0] == CMD_PWM1) || (val[0] == CMD_PWM2) || (val[0] == CMD_PWM3) || (val[0] == CMD_PWM4) || (val[0] == CMD_PWM5) || (val[0] == CMD_PWM6))
	{ //          2         3            4        5          6          7         8        9          10        11       12         13
		byte ePwm1Status, ePwm1HOn, ePwm1MOn, ePwm1SOn, ePwm1HOff, ePwm1MOff, ePwm1SOff, ePwm1Min, ePwm1Max, ePwm1Sr, ePwm1Ss, ePwm1KeepLight;

		if (val[2] >= 0 && val[2] <= 1)  { ePwm1Status = val[2]; }
		else { return false; }          // ePwm1Status -    2
		if (val[3] >= 0 && val[3] <= 23) { ePwm1HOn = val[3]; }
		else { return false; }             // ePwm1HOn -       3
		if (val[4] >= 0 && val[4] <= 59) { ePwm1MOn = val[4]; }
		else { return false; }             // ePwm1MOn -       4
		if (val[5] >= 0 && val[5] <= 59) { ePwm1SOn = val[5]; }
		else { return false; }             // ePwm1SOn -       5
		if (val[6] >= 0 && val[6] <= 23) { ePwm1HOff = val[6]; }
		else { return false; }            // ePwm1HOff -      6
		if (val[7] >= 0 && val[7] <= 59) { ePwm1MOff = val[7]; }
		else { return false; }            // ePwm1MOff -      7
		if (val[8] >= 0 && val[8] <= 59) { ePwm1SOff = val[8]; }
		else { return false; }            // ePwm1SOff -      8
		if (val[9] >= 0 && val[9] <= 255) { ePwm1Min = val[9]; }
		else { return false; }            // ePwm1Min -       9
		if (val[10] >= 0 && val[10] <= 255) { ePwm1Max = val[10]; }
		else { return false; }         // ePwm1Max -       10
		if (val[11] >= 0 && val[11] <= 255) { ePwm1Sr = val[11]; }
		else { return false; }          // ePwm1Sr -        11
		if (val[12] >= 0 && val[12] <= 255) { ePwm1Ss = val[12]; }
		else { return false; }          // ePwm1Ss -        12
		if (val[13] >= 0 && val[13] <= 1)   { ePwm1KeepLight = val[13]; }
		else { return false; }   // ePwm1KeepLight - 13

		int confFirstAddress = CONFIG_PWM_BEGIN_ADDRESS + ((val[0] - 31) * 30);

		EEPROM.write(confFirstAddress, ePwm1Status);
		EEPROM.write(confFirstAddress + 1, ePwm1HOn);
		EEPROM.write(confFirstAddress + 2, ePwm1MOn);
		EEPROM.write(confFirstAddress + 3, ePwm1SOn);
		EEPROM.write(confFirstAddress + 4, ePwm1HOff);
		EEPROM.write(confFirstAddress + 5, ePwm1MOff);
		EEPROM.write(confFirstAddress + 6, ePwm1SOff);
		EEPROM.write(confFirstAddress + 7, ePwm1Min);
		EEPROM.write(confFirstAddress + 8, ePwm1Max);
		EEPROM.write(confFirstAddress + 9, ePwm1Sr);
		EEPROM.write(confFirstAddress + 10, ePwm1Ss);
		EEPROM.write(confFirstAddress + 11, ePwm1KeepLight);
	}

	if (val[0] == CMD_SET_TIME) {
		byte setHour, setMinute, setSecond, setYear, setMonth, setDay;

		if (val[1] >= 0 && val[1] <= 23) { setHour = val[1]; }
		else { return false; }          
		if (val[2] >= 0 && val[2] <= 59) { setMinute = val[2]; }
		else { return false; }         
		if (val[3] >= 0 && val[3] <= 59) { setSecond = val[3]; }
		else { return false; }         
		if (val[4] >= 15 && val[4] <= 99) { setYear = val[4] + 2000; }
		else { return false; }     
		if (val[5] >= 1 && val[5] <= 12) { setMonth = val[5]; }
		else { return false; }     
		if (val[6] >= 1 && val[6] <= 31) { setDay = val[6]; }
		else { return false; }     

		setTime(setHour, setMinute, setSecond, setDay, setMonth, setYear);

		RTC.set(now());

		return true;
	}

	if (val[0] == CMD_GET_CONFIG_DATA)
	{
		int eAddress = 0;
		Serial.write("*66,");

		while (eAddress <= 512)
		{
			Serial.print(EEPROM.read(eAddress));
			Serial.write(",");
			eAddress++;
		}
		Serial.write("#\n");
	}

	if (val[0] == CMD_GET_TIME)
	{
		char response[24];
		sprintf(response, "*83,%i,%i,%i,%i,%i,%i", actualTime.Hour, actualTime.Minute, actualTime.Second, tmYearToCalendar(actualTime.Year), actualTime.Month, actualTime.Day);
		Serial.println(response);
	}

	if (val[0] == CMD_GET_VERSION)
	{
		Serial.println(SOFTWARE_VERSION);
	}
}

// Metoda przeliczająca czas w standardowej formie na liczbę sekund
unsigned long CalculateSeconds(uint8_t hour, uint8_t minute, uint8_t second)
{
	return (long(hour) * 3600) + (long(minute) * 60) + long(second);
}

// Metoda wyliczająca wartość PWM dla wentylatora
byte CalculatePwmFanValue(float temp, float tempOff, float tempMax)
{
	if (temp < tempOff)
		return 0;

	byte value = (temp - tempOff) * 100 / (tempMax - tempOff);

	if (value > 100)
		value = 100;

	// Jeżeli wartość PWM jest poniżej 35 ustawiamy na 35 - poniżej 35 wentylator nie startuje (trzeba dobrać eksperymentalnie)
	if (value < 35)
		value = 35;

	return value;
}

// Metoda wyliczająca wartość PWM
byte CalculatePwmValue(unsigned long actualTime, unsigned long startSeconds, unsigned long endSeconds, byte minValue, byte maxValue, bool nightLight, byte sunriseValue, byte sunsetValue, bool pwmEnabled, byte *pwmSt, byte *pwmPrevSt)
{
	*pwmPrevSt = *pwmSt;

	if (!pwmEnabled)
	{
		*pwmSt = CHANNEL_STATUS_OFF;

		return 0;
	}

	if (startSeconds > endSeconds)
		endSeconds += 86400;

	if ((actualTime < startSeconds) || (actualTime > endSeconds))
	{
		if (nightLight)
		{
			*pwmSt = CHANNEL_STATUS_NIGHT;
			return minValue;
		}

		*pwmSt = CHANNEL_STATUS_OFF;

		return 0;
	}

	byte sunriseDelta = maxValue - minValue;

	float sunriseFactor = float(actualTime - startSeconds) / float(sunriseValue * 60);
	if (sunriseFactor > 1) sunriseFactor = 1;

	byte pwmSunriseValue = sunriseFactor * sunriseDelta + minValue;

	float sunsetFactor = float(endSeconds - actualTime) / float(sunsetValue * 60);
	if (sunsetFactor > 1) sunsetFactor = 1;

	byte pwmSunsetValue = maxValue - (1 - sunsetFactor) * sunriseDelta;

	if (pwmSunriseValue <= pwmSunsetValue)
	{
		*pwmSt = (pwmSunriseValue == maxValue ? CHANNEL_STATUS_ON : CHANNEL_STATUS_SUNRIDE);
		return pwmSunriseValue;
	}
	else
	{
		*pwmSt = CHANNEL_STATUS_SUNSET;
		return pwmSunsetValue;
	}
}

// Metoda ustawiająca wartości na odpowiednich pinach dla kanałów LED oraz wentylatora
void SetPwm(byte pwmValue[], byte pwmFanVal, byte p_pwmPrevValue[])
{
	byte pwmByteValue;

	for (int i = 0; i < PWM_COUNT; i++)
	{
		pwmByteValue = map(pwmValue[i], 0, 100, 0, 255);
		
		analogWrite(pwmPin[i], pwmByteValue);
	}

	analogWrite(PWM_FAN_PIN, map(pwmFanVal, 0, 100, 0, 255));
}

// Metoda ustawiająca stan diody
void SetStatusLed(bool isError)
{
	if (isError)
	{
		digitalWrite(ERRORLED_PIN, STATUSLED_ERROR);

		return;
	}

	// Jeżeli doszliśmy do tego miejsca, oznacza to, że wszystko ok
	digitalWrite(ERRORLED_PIN, STATUSLED_OK);
}

// Metoda wyświetlająca datę i czas na wyświetlaczu
void DisplayDateTime(tmElements_t time)
{
	char tmp[10];

	tft.setTextSize(2);
	// Display time and date
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	sprintf(tmp, "%02u:%02u", time.Hour, time.Minute);
	tft.drawString(tmp, 40, UI_DISPLAY_POS_Y + 15, 4);

	memset(tmp, 0, 10);
	tft.setTextSize(1);
	sprintf(tmp, "%02u-%02u-%02u", tmYearToCalendar(time.Year), time.Month, time.Day);
	tft.drawString(tmp, 40, UI_DISPLAY_POS_Y + 80, 4);
}

void SetTemperatureTextColor(float temp, float okValue, float warningValue)
{
	if (temp < okValue)
		tft.setTextColor(TFT_GREEN, TFT_BLACK);
	else if (temp < warningValue)
		tft.setTextColor(TFT_YELLOW, TFT_BLACK);
	else
		tft.setTextColor(TFT_RED, TFT_BLACK);
}

// Metoda wyświetlająca temperaturę
void DisplayTemp(byte posX, byte posY, byte tempValue, byte tempWarning, byte tempError, byte tempPrecision, char type)
{
	char tmpTemp[5];
	char tmp[10];

	memset(tmpTemp, 0, 5);
	memset(tmp, 0, 10);

	dtostrf(tempValue, 3, tempPrecision, tmpTemp);
	sprintf(tmp, "%c: %s 'C", type, tmpTemp);

	SetTemperatureTextColor(tempValue, tempWarning, tempError);
	tft.drawString(tmp, posX, posY, 4);
}

// Metoda wyświetlająca informacje o PWM wentylatora
void DisplayFanStatus(byte fanPwmValue, byte pos_y)
{
	tft.fillCircle(UI_PWM_SYMBOL_POSX, pos_y + 10, 4, FAN_COLOR);

	tft.drawLine(UI_PWM_SYMBOL_POSX - 8, pos_y + 2, UI_PWM_SYMBOL_POSX + 8, pos_y + 18, FAN_COLOR);
	tft.drawLine(UI_PWM_SYMBOL_POSX - 8, pos_y + 18, UI_PWM_SYMBOL_POSX + 8, pos_y + 2, FAN_COLOR);
	tft.drawLine(UI_PWM_SYMBOL_POSX - 10, pos_y + 10, UI_PWM_SYMBOL_POSX + 10, pos_y + 10, FAN_COLOR);
	tft.drawLine(UI_PWM_SYMBOL_POSX, pos_y, UI_PWM_SYMBOL_POSX, pos_y + 20, FAN_COLOR);

	tft.setTextColor(TFT_WHITE, TFT_BLACK);

	char tmp[10];
	memset(tmp, 0, 10);
	sprintf(tmp, "%02u %%", fanPwmValue);
	tft.drawString(tmp, UI_PWM_VALUE_POSX, pos_y, 4);
}

// Metoda wyświetlająca status jednego kanału LED
void DisplayPwmStatus(byte status, byte pwmNo, byte prevStatus)
{
	byte y = (pwmNo * UI_STATUS_SYMBOL_X_DELTA) + UI_DISPLAY_POS_Y + 10;

	// Jeżeli status kanału uległ zmianie, czyścimy miejsce wyświetlenia, tak aby nie zostały śmieci po poprzednim statusie
	if (prevStatus != status)
	{
		tft.fillRect(UI_STATUS_SYMBOL_POS_X - 1, y - 1, 12, 22, TFT_BLACK);
	}

	if (status == CHANNEL_STATUS_OFF)
	{
		//Niec nie trzeba robić
	}
	else if (status == CHANNEL_STATUS_SUNRIDE)
	{		
		tft.fillTriangle(UI_STATUS_SYMBOL_POS_X, y + 20, UI_STATUS_SYMBOL_POS_X + 10, y + 20, UI_STATUS_SYMBOL_POS_X + 5, y, TFT_GREEN); // Symbol wschodu słońca
	}
	else if (status == CHANNEL_STATUS_SUNSET)
	{
		tft.fillTriangle(UI_STATUS_SYMBOL_POS_X, y, UI_STATUS_SYMBOL_POS_X + 10, y, UI_STATUS_SYMBOL_POS_X + 5, y + 20, TFT_DARKGREY); // Symbol zachodu słońca
	}
	else if (status == CHANNEL_STATUS_ON)
	{
		tft.fillRect(UI_STATUS_SYMBOL_POS_X, y, 10, 20, TFT_YELLOW);
	}
	else if (status == CHANNEL_STATUS_NIGHT)
	{
		tft.fillRect(UI_STATUS_SYMBOL_POS_X, y, 10, 20, TFT_BLUE);
	}
}

// Metoda wyświetlająca info o jednym kanale PWM
void DisplaySinglePwm(byte pwmValue, bool p_pwmStatus, byte posY, uint16_t color)
{
	char tmp[10];

	// Display pwm
	tft.fillCircle(UI_PWM_SYMBOL_POSX, posY + 10, 10, color);

	tft.setTextColor(TFT_WHITE, TFT_BLACK);

	if (p_pwmStatus)
	{
		memset(tmp, 0, 10);
		sprintf(tmp, "%02u %%", pwmValue);
		tft.drawString(tmp, UI_PWM_VALUE_POSX, posY, 4);
	}
	else
		tft.fillRect(UI_PWM_VALUE_POSX, posY, 60, 20, TFT_BLACK);
}

// Metoda wyświetlająca dane na wyświetlaczu
void DisplayInfo(float radTemp, float waterTemp, byte p_pwmVal[], byte pwmFanValue, bool p_pwmStatus[], byte pwmSt[], byte pwmPrevSt[], uint16_t p_pwmColors[])
{
	for (byte i = 0; i < PWM_COUNT ; i++)
	{
		int pos_y = UI_DISPLAY_POS_Y + 10 + (i * 30);

		DisplaySinglePwm(p_pwmVal[i], p_pwmStatus[i], pos_y, p_pwmColors[i]);
		DisplayPwmStatus(pwmSt[i], i, pwmPrevSt[i]);
	}

	DisplayFanStatus(pwmFanValue, UI_DISPLAY_POS_Y + 10 + (PWM_COUNT * 30));

	DisplayTemp(50, UI_DISPLAY_POS_Y + UI_TEMP_POS_Y, radTemp, FAN_MIN_TEMP_ON, maxRadiatorTemp, 0, 'R');
	DisplayTemp(160, UI_DISPLAY_POS_Y + UI_TEMP_POS_Y, waterTemp, 26, 30, 0, 'W');
}

void setup()
{
	/*********** Ustawienie częstotliwości PWM dla poszczególnych wyprowadzeń *******************/
	/*********** D5 & D6 *******************/
	//TCCR0B = TCCR0B & B11111000 | B00000001;    // set timer 0 divisor to     1 for PWM frequency of 62500.00 Hz
	//TCCR0B = TCCR0B & B11111000 | B00000010;    // set timer 0 divisor to     8 for PWM frequency of  7812.50 Hz
	TCCR0B = TCCR0B & B11111000 | B00000011;    // set timer 0 divisor to    64 for PWM frequency of   976.56 Hz (The DEFAULT)
	//TCCR0B = TCCR0B & B11111000 | B00000100;    // set timer 0 divisor to   256 for PWM frequency of   244.14 Hz
	//TCCR0B = TCCR0B & B11111000 | B00000101;    // set timer 0 divisor to  1024 for PWM frequency of    61.04 Hz

	/*********** D9 & D10 *******************/
	TCCR1B = TCCR1B & B11111000 | B00000001;    // set timer 1 divisor to     1 for PWM frequency of 31372.55 Hz
	//TCCR1B = TCCR1B & B11111000 | B00000010;    // set timer 1 divisor to     8 for PWM frequency of  3921.16 Hz
	//TCCR1B = TCCR1B & B11111000 | B00000011;    // set timer 1 divisor to    64 for PWM frequency of   490.20 Hz (The DEFAULT)
	//TCCR1B = TCCR1B & B11111000 | B00000100;    // set timer 1 divisor to   256 for PWM frequency of   122.55 Hz
	//TCCR1B = TCCR1B & B11111000 | B00000101;    // set timer 1 divisor to  1024 for PWM frequency of    30.64 Hz

	/*********** D3 & D11 *******************/
	//TCCR2B = TCCR2B & B11111000 | B00000001;    // set timer 2 divisor to     1 for PWM frequency of 31372.55 Hz
	//TCCR2B = TCCR2B & B11111000 | B00000010;    // set timer 2 divisor to     8 for PWM frequency of  3921.16 Hz
	//TCCR2B = TCCR2B & B11111000 | B00000011;    // set timer 2 divisor to    32 for PWM frequency of   980.39 Hz
	TCCR2B = TCCR2B & B11111000 | B00000100;    // set timer 2 divisor to    64 for PWM frequency of   490.20 Hz (The DEFAULT)
	//TCCR2B = TCCR2B & B11111000 | B00000101;    // set timer 2 divisor to   128 for PWM frequency of   245.10 Hz
	//TCCR2B = TCCR2B & B11111000 | B00000110;    // set timer 2 divisor to   256 for PWM frequency of   122.55 Hz
	//TCCR2B = TCCR2B & B11111000 | B00000111;    // set timer 2 divisor to  1024 for PWM frequency of    30.64 Hz

	// Początkowo ustawiamy wszystkie kanały na 0
	analogWrite(PWM1_PIN, 0);
	analogWrite(PWM2_PIN, 0);
	analogWrite(PWM3_PIN, 0);

	// Ustawiamy piny dla podświetlenia LCD oraz diody błędu jako wyjścia
	pinMode(ERRORLED_PIN, OUTPUT);
	pinMode(LCDLED_PIN, OUTPUT);

	// Włączamy podświetlenie LCD
	digitalWrite(LCDLED_PIN, lcdBacklightStatus);

	// Set status led pin to OK
	digitalWrite(ERRORLED_PIN, STATUSLED_OK);

	Serial.begin(9600);

	// Inicjacja wyświetlacza
	tft.init();
	tft.setRotation(3);
	tft.fillScreen(TFT_BLACK);

	// Odczyt konfiguracji
	ReadConfiguration();
}

// Główna pętla
void loop()
{
	long unsigned currentMillis = millis();
  
	ReadConfiguration();

	bool isTimeError = !RTC.read(actualTime);

	bool isTempError = CheckTempSensors();

	ReadTemperatures();

	String command = ReadCommandFromSerial();

	if (command != "") {
		char commandOutputArray[64];

		memset(commandOutputArray, 0, 64);
		command.toCharArray(commandOutputArray, 64);

		if (!ProcessCommand(commandOutputArray))
		{
			Serial.print("666,Bledne dane\n");
		}
	}

	// Sprawdzamy czy lampa nie jest przegrzana
	isOverheated = radiatorTemperature > maxRadiatorTemp;

	// Jeżeli jest przegrzana ustawiamy wszystkie kanały światła na 0, a wentylator na 100
	if (isOverheated)
	{
		for (int i = 0; i < PWM_COUNT; i++)
		{
			pwmValue[i] = 0;
			pwmStatus[i] = CHANNEL_STATUS_OFF;
		}			

		pwmFanValue = 100;
	}
	else
	{
		for (int i = 0; i < PWM_COUNT; i++)
		{
			unsigned long actualSeconds = CalculateSeconds(actualTime.Hour, actualTime.Minute, actualTime.Second);
			unsigned long pwmStartSeconds = CalculateSeconds(pwmHStart[i], pwmMStart[i], pwmSStart[i]);
			unsigned long pwmEndSeconds = CalculateSeconds(pwmHEnd[i], pwmMEnd[i], pwmSEnd[i]);

			pwmValue[i] = CalculatePwmValue(actualSeconds, pwmStartSeconds, pwmEndSeconds, pwmMin[i], pwmMax[i], pwmNightLight[i], pwmSunrise[i], pwmSunset[i], pwmEnable[i], pwmStatus + i, pwmPrevStatus + i);
		}

		pwmFanValue = CalculatePwmFanValue(radiatorTemperature, FAN_MIN_TEMP_ON, maxRadiatorTemp);
	}

	SetPwm(pwmValue, pwmFanValue, pwmLastValue);

	SetStatusLed(isTimeError || isTempError || isOverheated);

	// Aktualizujemy czas tylko jeżeli zmieniła się minuta, bo wyświetlanie skalowanej czcionki jest wolne i powoduje mruganie 
	if (actualTime.Minute != lastDisplayedMinute)
	{
		lastDisplayedMinute = actualTime.Minute;
		DisplayDateTime(actualTime);
	}

	if (currentMillis % 1 == 0)
		DisplayInfo(radiatorTemperature, waterTemperature, pwmValue, pwmFanValue, pwmEnable, pwmStatus, pwmPrevStatus, pwmColors);
}
