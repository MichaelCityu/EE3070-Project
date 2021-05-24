
/*
EE3070 Project
Group: L04 G03
Members:
   LIU Yiyang  (55668377)
   Man Kin Hei (55712135)
   Qiang Dong  (55668248)
*/

#include "WiFiEsp.h"
#include "ThingSpeak.h"
#include "Timer.h"
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/pgmspace.h>

/*Pin configurations*/
const int led1RedPin = 2;   //LED1: payment status
const int led1GreenPin = 3;
const int led2RedPin = 4;   //LED2: Stock status
const int led2GreenPin = 5;   
const int led3RedPin = 6;   //LED3: Data uploading status
const int led3GreenPin = 7;
const int SS_PIN = 53;      //Digital pin 53
const int RST_PIN = 10;    //Digital pin 10
const int trigPin = 22;     //trig pin of ultrasonic
const int echoPin = 23;     //echo pin of ultrasonic

//OLED settings
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/*Wifi settings*/
char ssid[] = "EE3070_P1800_1";
char pass[] = "EE3070P1800";
WiFiEspClient client;
Timer t;

/*RFID settings*/
MFRC522 rfid;
MFRC522::MIFARE_Key default_key;
byte balance_block = 1;
byte sexAge_block = 8;
byte name_block = 4, name_block_size = 48;

/*ThingSpeak settings*/
unsigned long channelID = 1293952;  //ThingSpeak channel "EE3070"
char readKey[] = "CQSQB032ST9Y78RV";
char writeKey[] = "KPZ5AH5SZB8H7HEZ";

float authenticate_keyA_get_balance(byte, MFRC522::MIFARE_Key*);
void  authenticate_keyA_update_balance(byte, MFRC522::MIFARE_Key*, float*);
String authenticate_keyA_getName(byte, MFRC522::MIFARE_Key*, byte);
void authenticate_keyA_get_sexAge(byte, MFRC522::MIFARE_Key*, char*, int*);

template <class T1, class T2>
class Pair
{
public:
    T1 first;
    T2 second;
    Pair() {
        T1 t;
        first = t;
        second = 0;
    }

    Pair(T1* a, T2* b) {
        first = *a;
        second = *b;
    }
};

class Product
{
public:
    int stock;
    String name;
    float price;
    float throughput;
    float prevThroughput;
    byte* id;
    int productID;
    String sexSum;
    String ageSum;

    Product() {
        stock = 0;
        name = "";
        price = 0;
        throughput = 0;
        productID = 0;
        sexSum = " ";
        ageSum = " ";
    }

    Product(int s, String n, float p, int pid) { //pid a.k.a. sexAge
        stock = s;
        name = n;
        price = p;
        throughput = 0;
        prevThroughput = 0;
        productID = pid;
        sexSum = " ";
        ageSum = " ";
    }

    void update() {
        throughput = prevThroughput * 0.8 + throughput * 0.2;

        if (prevThroughput != 0)
            price = price * (float)(throughput / prevThroughput);

        price = round(price * 100.0) / 100.0;
        prevThroughput = throughput;
        throughput = 0;
    }
};

class Customer
{
public:
    String name;
    float balance;
    int age;
    char sex;

    Customer() {
        name = authenticate_keyA_getName(name_block, &default_key, name_block_size);
        balance = authenticate_keyA_get_balance(balance_block, &default_key);
        authenticate_keyA_get_sexAge(sexAge_block, &default_key, &sex, &age);
    };

    void deductBalance(float price) {
        balance -= price;
        authenticate_keyA_update_balance(balance_block, &default_key, &balance);
    }
};

class Cart
{
public:
    int num;
    float price;
    int numCat;
    Product list[40];
    Pair<Product, int> result[5];
    int result_size;

    Cart() {
        num = 0;
        price = 0;
        numCat = 0;
        result_size = 0;
    }

    void clear() {
        num = 0;
        price = 0;
        numCat = 0;
        result_size = 0;
    }

    void add(Product& p) {
        list[num] = p;
        num++;
        price += p.price;
        p.stock -= 1;
        p.throughput += 1;
        Serial.println("Product added: " + list[num - 1].name);
    }

    void reduce(Product& p) {
        for (int i = 0; i < num; i++) {
            if (list[i].name == p.name) {
                for (int j = i; j < num - 1; j++) {
                    list[j] = list[j + 1];
                }
            }
        }
        num--;
        price -= p.price;
        p.stock += 1;
        p.throughput -= 1;
    }

    void sumCart() {
        bool rp;
        Serial.println("sumCart loop start");
        rp = false;
        //j is index of compare opject
        for (int j = 0; j < result_size && rp == false; j++) {
            if (list[num - 1].productID == result[j].first.productID) {
                // case1: repeated
                result[j].second++;
                rp = true;
            }
        }
        if (rp == false) {
            // case2: never repeated;
            result[result_size].first = list[num - 1];
            result[result_size].second = 1;
            result_size++;
        }

    }


};


//Global variables
Cart cart;
Product cola(100, "cola", 5.50, 1);
Product cake(100, "cake", 10.50, 2);
unsigned long time = 0;

void setup() {
    pinMode(led1GreenPin, OUTPUT);
    pinMode(led1RedPin, OUTPUT);
    pinMode(led2RedPin, OUTPUT);
    pinMode(led2GreenPin, OUTPUT);
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    Serial.begin(115200);    //Serial port with PC
    Serial1.begin(115200);   //Wi-Fi setup
    SPI.begin();

    rfid.PCD_Init();
    defaultKeyInit(default_key);

    // Init the OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0X3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        //for (;;); // Don't proceed, loop forever
    }
    display.display();
    delay(500); // Pause for 0.5 seconds
    display.clearDisplay();

    WiFi.init(&Serial1);
    ThingSpeak.begin(client);

    t.every(120000, record, (void*)0);  //Record the customer throughput in the last minite

    if (WiFi.status() != WL_CONNECTED) {
        Serial.print("Connecting to SSID: ");
        Serial.println(ssid);
        while (WiFi.status() != WL_CONNECTED) {
            WiFi.begin(ssid, pass);
            Serial.print(".");
            delay(5000);
        }
        Serial.println("\nConnected");
    }

    digitalWrite(led3RedPin, LOW);
    digitalWrite(led3GreenPin, HIGH);
}

void loop() {

    t.update();

    if (millis() - time > 60000) {
        cart.clear();
        time = millis();
    }
    if (cart.num == 0){
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);

        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);

        float duration = pulseIn(echoPin, HIGH);  //microseconds
        float d = duration * 340 / 20000;

        if (d > 50)
            welcomeInterface();
        else
            instructions();
    }

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

        if (isProduct(&default_key) == 0 && cart.price == 0) { //Tap before reading good: check balance
            Serial.println("Customer");
            Customer customer;
            //OLED DISPLAY
            checkBal_Name(customer);                                             //Display customer and balance
            delay(3000);
            empty_cart();                                                        //Display empty cart
            delay(2000);
            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();
            return;
        }

        if (isProduct(&default_key) == 1) {
            time = millis();

            String name = authenticate_keyA_getName(name_block, &default_key, name_block_size).substring(0, 4);

            if (name == String("Cake")) {
                cart.add(cake);
            }
            else if (name == String("Cola")) {
                cart.add(cola);
            }
            cart.sumCart();
            disp(cart.result, cart.result_size);

            delay(2000);
        }
        //Customer checkout
        else if (isProduct(&default_key) == 0) {
            Customer* customer = new Customer;

            disp(cart.result, cart.result_size);

            if (customer->balance >= cart.price) {
                digitalWrite(led1RedPin, LOW);
                digitalWrite(led1GreenPin, HIGH);
                customer->deductBalance(cart.price); //a)success payment b)insufficient balance
                Serial.println("New balance: " + String(authenticate_keyA_get_balance(balance_block, &default_key)));
                successPay();
                for (int i = 0; i < 20 && cart.list[i].productID != 0; i++) {
                    if (cart.list[i].productID == 1) {
                        cake.ageSum += " " + String(customer->age);
                        cake.sexSum += String(customer->sex);
                    }
                    else {
                        cola.ageSum += " " + String(customer->age);
                        cola.sexSum += String(customer->sex);
                    }
                }
                cart.clear();
                digitalWrite(led1GreenPin, LOW);
            }
            else{
                digitalWrite(led1RedPin, HIGH);
                digitalWrite(led1GreenPin, LOW);
                insufBal(); 
                digitalWrite(led1RedPin, LOW);
            }

            checkBal_Name(*customer);
            delay(2000);
            delete customer;
        }
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
    }

    if (cake.stock < 5 || cola.stock < 5) {
        digitalWrite(led2RedPin, HIGH);
        digitalWrite(led2GreenPin, HIGH);
    }
    else {
        digitalWrite(led2RedPin, LOW);
        digitalWrite(led2GreenPin, HIGH);
    }
}

//Function to update price and clear throughput and uploading data every 20s
void record(void* context) {
    digitalWrite(led3RedPin, HIGH);
    dispUpdate();
    cake.update();
    cola.update();
    
    ThingSpeak.setField(1, cake.price);
    ThingSpeak.setField(2, cake.stock);
    ThingSpeak.setField(3, cake.sexSum);
    ThingSpeak.setField(4, cake.ageSum);
    ThingSpeak.setField(5, cola.price);
    ThingSpeak.setField(6, cola.stock);
    ThingSpeak.setField(7, cola.sexSum);
    ThingSpeak.setField(8, cola.ageSum);

    int x = ThingSpeak.writeFields(channelID, writeKey);
    if (x == 200) {
        Serial.println("Channel update successful.");
    }
    else {
        Serial.println("Problem updating channel. HTTP error code " + String(x));
    }

    if (cart.num != 0) {
        disp(cart.result, cart.result_size);
    }
    digitalWrite(led3RedPin, LOW);
    digitalWrite(led3GreenPin, HIGH);
}

//--------- Below are the supportted functions ---------------------------
//Display Functions
void generalDisplay(char* msg) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(msg);
    display.display();
}

void dispUpdate() {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(5, 15);

    display.println("System updating ...");
    display.setCursor(0, 37);

    display.println("Please wait before it");
    display.setCursor(0, 45);
    display.println("is done :)");
    display.display();
}

void welcomeInterface() {
    display.clearDisplay();
    display.setCursor(20, 0);
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.print("Welcome!");
    display.setCursor(0, 18);
    display.setTextSize(1);
    display.println("Don't hesitate to use");
    display.drawRoundRect(2, 30, 124, 13, 2, WHITE);
    display.setCursor(0, 33);
    display.println(" Instant Pay System ");
    // Draw bitmap on the screen
    static const uint8_t PROGMEM image_market[] = {
      0xff, 0xff, 0x7f, 0xff, 0xff, 0xef, 0xff, 0xfc,
      0xff, 0xff, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xfc,
      0xff, 0xff, 0xbf, 0xff, 0xff, 0xef, 0xff, 0xfc,
      0xff, 0xff, 0xbf, 0xff, 0xff, 0xef, 0xff, 0xfc,
      0xff, 0xd7, 0xac, 0x21, 0x55, 0x68, 0x0f, 0xfc,
      0xe0, 0x10, 0x22, 0xa9, 0x15, 0xe3, 0xf0, 0x1c,
      0xdf, 0xff, 0xbf, 0xff, 0xff, 0xef, 0xff, 0xdc,
      0xe0, 0x00, 0x3f, 0xff, 0xff, 0xe0, 0x00, 0x3c,
      0xef, 0xff, 0xf4, 0xc2, 0x15, 0xff, 0xff, 0xfc,
      0xec, 0x3f, 0xf0, 0x02, 0x10, 0xff, 0x1f, 0xfc,
      0xed, 0x3f, 0xf0, 0x02, 0x10, 0xff, 0x5f, 0xfc,
      0xec, 0x38, 0x70, 0x02, 0x10, 0xe3, 0x5f, 0xfc,
      0xee, 0x30, 0x70, 0x02, 0x10, 0x55, 0x50, 0xfc,
      0xef, 0xf0, 0x70, 0x02, 0x10, 0x5d, 0xf6, 0xfc,
      0xef, 0xf8, 0x70, 0x02, 0x10, 0x43, 0xf4, 0xfc,
      0xef, 0xff, 0xf0, 0x02, 0x10, 0xff, 0xff, 0xfc,
      0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c,
      0xdf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc,
      0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc
    };
    display.drawBitmap(33, 46, image_market, 62, 20, 1);
    display.display();
}

void empty_cart() {
    display.clearDisplay();
    // Draw bitmap on the screen
    static const uint8_t PROGMEM image_empty_cart[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x7c, 0x7f, 0xff, 0xff, 0xff, 0x00,
    0x00, 0x3c, 0x7f, 0xff, 0xff, 0xfe, 0x00,
    0x00, 0x3c, 0x00, 0x00, 0x00, 0x3e, 0x00,
    0x00, 0x3e, 0x00, 0x00, 0x00, 0x3e, 0x00,
    0x00, 0x3e, 0x00, 0x00, 0x00, 0x3e, 0x00,
    0x00, 0x1f, 0x00, 0x00, 0x00, 0x3e, 0x00,
    0x00, 0x1f, 0x00, 0x00, 0x00, 0x3e, 0x00,
    0x00, 0x1f, 0x00, 0x00, 0x00, 0x7c, 0x00,
    0x00, 0x0f, 0x80, 0x00, 0x00, 0x7c, 0x00,
    0x00, 0x0f, 0x80, 0x00, 0x00, 0x7c, 0x00,
    0x00, 0x0f, 0xc0, 0x00, 0x00, 0xfc, 0x00,
    0x00, 0x0f, 0xc0, 0x00, 0x00, 0xfc, 0x00,
    0x00, 0x07, 0xe0, 0x00, 0x00, 0xf8, 0x00,
    0x00, 0x07, 0xff, 0xff, 0xff, 0xf8, 0x00,
    0x00, 0x07, 0xff, 0xff, 0xff, 0xf8, 0x00,
    0x00, 0x07, 0xff, 0xff, 0xff, 0xf8, 0x00,
    0x00, 0x03, 0xff, 0xff, 0xff, 0xf8, 0x00,
    0x00, 0x03, 0xff, 0xff, 0xff, 0xf8, 0x00,
    0x00, 0x03, 0xf8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xfc, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xfc, 0x00, 0x00, 0x20, 0x00,
    0x00, 0x01, 0xff, 0xff, 0xff, 0xf0, 0x00,
    0x00, 0x00, 0xff, 0xff, 0xff, 0xe0, 0x00,
    0x00, 0x00, 0xff, 0xff, 0xff, 0xe0, 0x00,
    0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x7f, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7f, 0x00, 0x03, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0xc0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0xc0, 0x00,
    0x00, 0x00, 0x0f, 0x80, 0x07, 0xe0, 0x00,
    0x00, 0x00, 0x1f, 0xc0, 0x07, 0xc0, 0x00,
    0x00, 0x00, 0x3f, 0xc0, 0x03, 0x80, 0x00,
    0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    display.drawBitmap(30, 19, image_empty_cart, 49, 45, 1);
    display.setCursor(0, 0);
    display.setTextColor(WHITE);
    display.print("No items in your cartcurrently");
    display.drawCircle(83, 25, 9, WHITE);
    display.setCursor(81, 22);
    display.print("0");
    display.display();

}

void disp(Pair<Product, int>* pairList, int size) { //disp(cart.sumCart())   //main program: Cart x;
    display.clearDisplay();
    display.setTextColor(WHITE);
    int i = 0;
    int x = 0;
    float total = 0.00;
    while (i < size) {
        if (x > 48) {
            display.clearDisplay();
            x = 0;
        }

        display.setCursor(0, x);
        display.print(pairList[i].first.name);
        display.setCursor(65, x);
        display.print("x" + String(pairList[i].second));
        display.setCursor(95, x);
        display.println(String(pairList[i].first.price));
        x += 8;
        total += pairList[i].second * pairList[i].first.price;
        i++;
    };

    display.drawLine(0, 55, 128, 55, WHITE);
    display.setCursor(0, 57);
    display.print("Total");
    display.setCursor(70, 57);
    display.print("$ " + String(total));
    display.display();
}

void instructions() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextColor(WHITE);
    display.println("Instructions: ");
    display.drawLine(0, 9, 128, 9, WHITE);
    display.setCursor(0, 13);
    display.println("1.Tap product card toadd it into the cart");

    static const uint8_t PROGMEM image_arrow[] = {
    0x03, 0xc0,
    0x03, 0xc0,
    0x03, 0xc0,
    0x03, 0xc0,
    0x03, 0xc0,
    0x03, 0xc0,
    0x37, 0xec,
    0x3f, 0xfc,
    0x1f, 0xf8,
    0x0f, 0xf0,
    0x0f, 0xf0,
    0x07, 0xe0,
    0x03, 0xc0,
    0x01, 0x80,
    0x00, 0x00
    };
    display.drawBitmap(50, 30, image_arrow, 15, 15, 1);
    display.setCursor(0, 45);
    display.println("2.Tap your own card  to pay :)");
    display.display();
    //display.startscrollright(0x00, 0x00); 
}

void checkBal_Name(Customer cust) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(40, 0);
    display.setTextColor(WHITE);
    display.println(cust.name);
    display.setCursor(0, 13);
    display.println("Hi, your current balance is:");
    display.setTextSize(2);
    display.setCursor(48, 38);
    display.print("$");
    int integer = (cust.balance * 100) / 100;
    display.print(integer);
    display.setTextSize(1);
    display.print(".");
    int deci = (cust.balance - integer) * 100;
    display.print(deci);
    display.drawLine(49, 55, 128, 55, WHITE);
    display.drawLine(49, 59, 128, 59, WHITE);
    static const uint8_t PROGMEM image_wallet[] = {
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x60, 0x00, 0x00,
      0x00, 0x00, 0xf8, 0x00, 0x00,
      0x00, 0x01, 0xf8, 0x00, 0x00,
      0x00, 0x03, 0xf0, 0x00, 0x00,
      0x00, 0x07, 0xe3, 0x00, 0x00,
      0x00, 0x0f, 0xc7, 0x80, 0x00,
      0x00, 0x1f, 0x8f, 0xc0, 0x00,
      0x00, 0x3f, 0x1f, 0xe0, 0x00,
      0x00, 0x7e, 0x3f, 0xf0, 0x00,
      0x1f, 0xfc, 0x7f, 0xfe, 0x00,
      0x38, 0xf8, 0xff, 0xfb, 0x00,
      0x31, 0xf1, 0xff, 0xfb, 0x00,
      0x23, 0xe3, 0xff, 0xf3, 0x00,
      0x27, 0xc3, 0xff, 0xe0, 0x00,
      0x30, 0x00, 0x00, 0x00, 0x00,
      0x38, 0x00, 0x00, 0x00, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0x80, 0x00,
      0x3f, 0xff, 0xff, 0x3f, 0x80,
      0x3f, 0xff, 0xff, 0x7f, 0xc0,
      0x3f, 0xff, 0xfe, 0x7c, 0xc0,
      0x3f, 0xff, 0xff, 0x78, 0xc0,
      0x3f, 0xff, 0xfe, 0x7f, 0xc0,
      0x3f, 0xff, 0xff, 0x7f, 0x80,
      0x3f, 0xff, 0xff, 0x00, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xff, 0x00,
      0x3f, 0xff, 0xff, 0xfe, 0x00,
      0x0f, 0xff, 0xff, 0xfc, 0x00
    };
    display.drawBitmap(3, 25, image_wallet, 36, 40, 1);
    display.display();
}

void successPay() {
    display.clearDisplay();
    display.setCursor(10, 50);
    display.setTextColor(WHITE);
    display.println("Payment successful!");

    static const uint8_t PROGMEM image_success[] = {
    0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00,
    0x00, 0x01, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x07, 0xff, 0xff, 0xc0, 0x00,
    0x00, 0x1f, 0xff, 0xff, 0xf0, 0x00,
    0x00, 0x3f, 0xff, 0xff, 0xf8, 0x00,
    0x00, 0x7f, 0xff, 0xff, 0xfc, 0x00,
    0x00, 0xff, 0xff, 0xff, 0xfe, 0x00,
    0x01, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x03, 0xff, 0xff, 0xff, 0xef, 0x80,
    0x07, 0xff, 0xff, 0xff, 0xc7, 0xc0,
    0x0f, 0xff, 0xff, 0xff, 0x83, 0xc0,
    0x0f, 0xff, 0xff, 0xff, 0x01, 0xe0,
    0x0f, 0xff, 0xff, 0xfe, 0x00, 0xe0,
    0x1f, 0xff, 0xff, 0xfc, 0x00, 0x70,
    0x1f, 0xff, 0xff, 0xf8, 0x00, 0xf0,
    0x3f, 0xff, 0xff, 0xf0, 0x01, 0xf8,
    0x3f, 0xff, 0xff, 0xe0, 0x03, 0xf8,
    0x3f, 0xff, 0xff, 0xc0, 0x07, 0xf8,
    0x3f, 0xff, 0xff, 0x80, 0x0f, 0xf8,
    0x3f, 0xef, 0xff, 0x00, 0x1f, 0xfc,
    0x7f, 0xc7, 0xfe, 0x00, 0x3f, 0xfc,
    0x7f, 0x83, 0xfc, 0x00, 0x7f, 0xfc,
    0x7f, 0x01, 0xf8, 0x00, 0xff, 0xfc,
    0x7e, 0x00, 0xf0, 0x01, 0xff, 0xfc,
    0x7c, 0x00, 0x60, 0x03, 0xff, 0xfc,
    0x3c, 0x00, 0x00, 0x07, 0xff, 0xfc,
    0x3f, 0x00, 0x00, 0x0f, 0xff, 0xf8,
    0x3f, 0x80, 0x00, 0x1f, 0xff, 0xf8,
    0x3f, 0xc0, 0x00, 0x3f, 0xff, 0xf8,
    0x1f, 0xc0, 0x00, 0x3f, 0xff, 0xf8,
    0x1f, 0xe0, 0x00, 0xff, 0xff, 0xf0,
    0x1f, 0xf8, 0x01, 0xff, 0xff, 0xf0,
    0x0f, 0xfc, 0x03, 0xff, 0xff, 0xe0,
    0x0f, 0xfc, 0x07, 0xff, 0xff, 0xe0,
    0x07, 0xfe, 0x07, 0xff, 0xff, 0xc0,
    0x07, 0xff, 0x1f, 0xff, 0xff, 0xc0,
    0x03, 0xff, 0xbf, 0xff, 0xff, 0x80,
    0x01, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x00, 0xff, 0xff, 0xff, 0xfe, 0x00,
    0x00, 0x7f, 0xff, 0xff, 0xfc, 0x00,
    0x00, 0x3f, 0xff, 0xff, 0xf8, 0x00,
    0x00, 0x0f, 0xff, 0xff, 0xf0, 0x00,
    0x00, 0x03, 0xff, 0xff, 0x80, 0x00,
    0x00, 0x00, 0xff, 0xfe, 0x00, 0x00,
    0x00, 0x00, 0x0f, 0xe0, 0x00, 0x00
    };
    display.drawBitmap(40, 0, image_success, 47, 45, 1);
    display.display();
    delay(2000);
}

void insufBal() {
    display.clearDisplay();
    display.setCursor(4, 45);
    display.setTextColor(WHITE);
    display.println("Your Payment Failed!");
    display.setCursor(3, 53);
    display.drawRect(2, 52, 124, 11, WHITE);
    display.println("Insufficient Balance");

    static const uint8_t PROGMEM image_success[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xff, 0xe0, 0x00, 0x00,
    0x00, 0x0f, 0xff, 0xfc, 0x00, 0x00,
    0x00, 0x3f, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x7f, 0xff, 0xff, 0x80, 0x00,
    0x00, 0xff, 0xff, 0xff, 0xe0, 0x00,
    0x01, 0xff, 0xff, 0xff, 0xf0, 0x00,
    0x03, 0xff, 0xff, 0xff, 0xf8, 0x00,
    0x07, 0xff, 0xff, 0xff, 0xfc, 0x00,
    0x0f, 0xff, 0xff, 0xff, 0xfc, 0x00,
    0x1f, 0xff, 0xff, 0xff, 0xfe, 0x00,
    0x1f, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x3f, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x3f, 0xff, 0xf1, 0xff, 0xff, 0x80,
    0x3f, 0xff, 0xe0, 0xff, 0xff, 0x80,
    0x7f, 0xff, 0xe0, 0xff, 0xff, 0x80,
    0x7f, 0xff, 0xe1, 0xff, 0xff, 0x80,
    0x7f, 0xff, 0xf1, 0xff, 0xff, 0xc0,
    0x7f, 0xff, 0xf1, 0xff, 0xff, 0xc0,
    0x7f, 0xff, 0xf1, 0xff, 0xff, 0xc0,
    0xff, 0xff, 0xf3, 0xff, 0xff, 0xc0,
    0xff, 0xff, 0xf3, 0xff, 0xff, 0xc0,
    0xff, 0xff, 0xfb, 0xff, 0xff, 0xc0,
    0x7f, 0xff, 0xfb, 0xff, 0xff, 0xc0,
    0x7f, 0xff, 0xfb, 0xff, 0xff, 0xc0,
    0x7f, 0xff, 0xfb, 0xff, 0xff, 0xc0,
    0x7f, 0xff, 0xfb, 0xff, 0xff, 0x80,
    0x7f, 0xff, 0xff, 0xff, 0xff, 0x80,
    0x3f, 0xff, 0xe1, 0xff, 0xff, 0x80,
    0x3f, 0xff, 0xe1, 0xff, 0xff, 0x80,
    0x3f, 0xff, 0xe1, 0xff, 0xff, 0x00,
    0x1f, 0xff, 0xf1, 0xff, 0xff, 0x00,
    0x1f, 0xff, 0xff, 0xff, 0xfe, 0x00,
    0x0f, 0xff, 0xff, 0xff, 0xfc, 0x00,
    0x07, 0xff, 0xff, 0xff, 0xfc, 0x00,
    0x03, 0xff, 0xff, 0xff, 0xf8, 0x00,
    0x01, 0xff, 0xff, 0xff, 0xf0, 0x00,
    0x00, 0xff, 0xff, 0xff, 0xe0, 0x00,
    0x00, 0x7f, 0xff, 0xff, 0x80, 0x00,
    0x00, 0x3f, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x0f, 0xff, 0xfc, 0x00, 0x00,
    0x00, 0x01, 0xff, 0xe0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    display.drawBitmap(40, 0, image_success, 43, 44, 1);
    display.display();
    delay(2000);
}

void defaultKeyInit(MFRC522::MIFARE_Key& key) {
    key.keyByte[0] = 0xAA;
    key.keyByte[1] = 0xBB;
    key.keyByte[2] = 0xCC;
    key.keyByte[3] = 0xDD;
    key.keyByte[4] = 0xEE;
    key.keyByte[5] = 0xFF;
}

float authenticate_keyA_get_balance(byte bal_blockAddr, MFRC522::MIFARE_Key* k) {
    byte balHex[3] = {};
    MFRC522::StatusCode s = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bal_blockAddr, k, &(rfid.uid));
    if (s != MFRC522::STATUS_OK) {
        Serial.print(F("Authentication failed: "));
        Serial.println(rfid.GetStatusCodeName(s));
        return -1;
    }
    else {
        byte bal_buffer[18];
        byte bal_buff_len = 18;
        byte balance_size = 3;

        s = rfid.MIFARE_Read(bal_blockAddr, bal_buffer, &bal_buff_len);
        if(s != MFRC522::STATUS_OK) {
            Serial.print(F("Reading failed: "));
            Serial.println(rfid.GetStatusCodeName(s));
            return -1;
        }
        else
        {
            for (byte i = 0; i < balance_size; i++) {
                balHex[i] = bal_buffer[i];
            }
        }
    }
    return hexToDec(balHex);
}

void authenticate_keyA_update_balance(byte bal_blockAddr, MFRC522::MIFARE_Key* k, float* bal) {
    byte bal_buffer[18];
    byte bal_buff_len = 18;
    byte balance_size = 3;
    unsigned long int_part = trunc(*bal);
    unsigned long dec_part = (*bal) * 100 - int_part * 100;
    byte balHex[] = { (byte)(int_part / 256),(byte)(int_part % 256),(byte)dec_part };

    for (byte i = 0; i < balance_size; i++) {
        bal_buffer[i] = balHex[i];
    }

        MFRC522::StatusCode s = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bal_blockAddr, k, &(rfid.uid));
        if (s != MFRC522::STATUS_OK) {
            Serial.print(F("Authentication failed: "));
            Serial.println(rfid.GetStatusCodeName(s));
            return;
        }
        else {
            s = (MFRC522::StatusCode)rfid.MIFARE_Write(bal_blockAddr, bal_buffer, bal_buff_len - 2);
            if (s != MFRC522::STATUS_OK) {
                Serial.print(F("MIFARE_Write() failed: "));
                Serial.println(rfid.GetStatusCodeName(s));
                return;
            }
        }
}

String authenticate_keyA_getName(byte name_block, MFRC522::MIFARE_Key* k, byte unameSize) {
    byte buffer1[18];
    byte len = 18;
    MFRC522::StatusCode status;
    bool done_copy = false;
    String uname = "";
    for (byte i = 0; i < 3; i++) {
        //------------------------------------------- GET NAME
        status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, name_block + i, k, &(rfid.uid)); //line 834 of MFRC522.cpp file
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("Authentication failed: "));
            Serial.println(rfid.GetStatusCodeName(status));
            return "";
        }
        else 
        {
            status = rfid.MIFARE_Read(name_block + i, buffer1, &len);
            if (status != MFRC522::STATUS_OK) {
                Serial.print(F("Reading failed: "));
                Serial.println(rfid.GetStatusCodeName(status));
                return "";
            }
            else 
            {
                for (byte j = 0; j < 16 && !done_copy; j++) {
                    if (buffer1[i] != 0x13)
                        uname = uname + String((char)buffer1[j]);
                    else 
                        done_copy = true;
                }
                    
            }
        }
    }
    return uname;
}

void authenticate_keyA_get_sexAge(byte sexAge_block, MFRC522::MIFARE_Key* k, char* sex, int* age) {
    byte buffer[18];
    byte len = 18;
    MFRC522::StatusCode status;
    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, sexAge_block, k, &(rfid.uid));
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Authentication failed: "));
        Serial.println(rfid.GetStatusCodeName(status));
        return;
    }
    else {
        status = rfid.MIFARE_Read(sexAge_block, buffer, &len);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("Reading failed: "));
            Serial.println(rfid.GetStatusCodeName(status)); 
            return;
        }
        else {
            *sex = buffer[0];

            int tmp = 0;

            for (int i = 1; i < 4; i++)
                tmp = tmp * 10 + buffer[i] - 48;

            *age = tmp;
        }
    }
}

void printHex(byte* buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
    Serial.println();
}

float hexToDec(byte* buffer) {  //balance are in 3 bytes: 2bytes integer part+ 1 byte decimal part // for display purpose //also application for price
    float bal = 0;
    unsigned long int_part = 0;
    unsigned long dec_part = 0;
    int_part = buffer[0] << 8 | buffer[1];
    dec_part = buffer[2];
    bal = int_part + (float)dec_part / (float)100;
    return bal;
}

byte* decToHex(float bal_Dex) { //update a byte array with length of 18 //for calculation // also ,applicable to price
    unsigned long int_part = trunc(bal_Dex);
    unsigned long dec_part = bal_Dex * 100 - int_part * 100;
    byte buffer[] = { (byte)(int_part / 256),(byte)(int_part % 256),(byte)dec_part };
    return buffer;
}


// code below has not been tested yet
int isProduct(MFRC522::MIFARE_Key* k) {
    byte buffer[18];
    byte len = 18;
    MFRC522::StatusCode status;

    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, sexAge_block, k, &(rfid.uid));
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Authentication failed: "));
        Serial.println(rfid.GetStatusCodeName(status));
        return -1;
    }

    status = rfid.MIFARE_Read(sexAge_block, buffer, &len);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Reading failed: "));
        Serial.println(rfid.GetStatusCodeName(status));
        return -1;
    }

    if ((char)buffer[0] == 'P')
        return 1;
    else
        return 0;
}