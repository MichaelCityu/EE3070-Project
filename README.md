# EE3070-Project

This is a purchasing system with a complete design of the frontend interface for customer purchase assisted and summarized by a shopping cart and the backend data storage, visualization, and analysis.
The frontend purchase system comprises two primary entities: customers and merchandise. Each merchandise has a tag attached containing its unique information such as name, ID combination. Other information such as stock, price, number of purchases in a certain time in the past for every category of merchandise is constantly kept track. Each customer has a card containing his/her name, age, gender, and current balance that can be read and deducted when purchasing. We have a separate program to provide top-up service to customers.
The merchandise prices are dynamically adjusted using a particular algorithm based on customer throughputs in different time intervals.
The system effectively leverages the timers provided by multiple platforms to conduct specific tasks after some time has elapsed, such as the timeout of the current purchase, uploading of local data, and visualization of data after analysis.

We are motivated by noticing that inefficiency exists for both consumers and shopkeepers in the retail industry.
Without the aid of automation, queueing delay can be long, especially in densely populated areas. This will cause a waste of time for consumers and a decrease in revenue for shopkeepers. For this reason, we designed the fully automated checkout system. To further reduce the time, instead of letting the customer tap and pay for every product, we sum up all the products in a shopping cart whose details will be displayed on an OLED screen.
For shopkeepers, price adjustment is often inefficient (sometimes outdated) because of inefficient data collection and price updates, which are slow and laborious if conducted manually. Therefore, we include dynamic pricing in our design, where we automatically analyze and adjust the prices based on customer throughput.
What's more, it is difficult for shopkeepers to monitor and keep track of the data records through conventional approaches where data are kept locally and cannot be comprehensively analyzed in time. In our design, we upload the data to the cloud with the desired time intervals, shopkeepers can monitor it from the alteration of prices to the stock of products, or even the 
age distribution and gender ratio of the consumers can be monitored in real-time without even getting out of the office.
Noticing that many automated vending systems (for instance, vending machines driven by octopus paying systems) lack sufficient guidance for consumers, although their purchasing logics often differ, we display every step of the purchase OLED screen. What is unique is that our system has sufficient flexibility. Even if the customer does not read the instructions at all, he/she will still be able to pay smoothly with a couple of taps of cards.
The design itself has many highlights and innovations. For example, the system is straightforward and easy to use for customers without previous experiences, the shopping cart function is efficient and intuitive to use, and the back-end data analysis and visualization are exciting. Due to limited functionality in the given ThingSpeak cloud platform with a free license (where data visualization update is opted out), we moved to MATLAB and updated the visualization leveraging a built-in timer class.

Functions provided:
  Object (person) detection with ultrasonic sensor 
  RFID detection and recognition
  Checking of customer’s account
  Display of the current products in a shopping cart
  Dynamic adjustment of the product price
  Payment result display with LED indications
  Stock monitoring with reminding when out of stock
  Basic data visualization of price and stock of products with timestamps
  Insightful data analysis and visualization of age distribution and gender ratio of customers for each type of product
  
Distinct features: Fast and user-friendly process, protection on RFID information, practical shopping cart, cloud analysis and visualization, reusability of PICC tags, functional modules.

Modules involved: 
  Arduino mega 2560
  ESP8266 Wi-Fi module
  HC-SR04 Ultrasonic Sensor
  MFRC522 RFID modules
  SSD1306 OLED module
  RGB LEDs
  ThingSpeak Cloud Platform
  MATLAB
