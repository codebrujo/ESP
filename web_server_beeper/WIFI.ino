int connectToWiFi(int netIndex) {
  byte tries = 11;
  Serial.print("SSID ");
  switch (netIndex) {
    case 1:
      Serial.print(ssid);
      WiFi.begin(ssid, pass);
      delay(1000);
      while (--tries && WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
      }
      break;
    case 2:
      Serial.print(ssid2);
      WiFi.begin(ssid2, pass2);
      delay(1000);
      while (--tries && WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
      }
      break;
    case 3:
      Serial.print(ssid3);
      WiFi.begin(ssid3, pass3);
      delay(1000);
      while (--tries && WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
      }
      break;
    case 4:
      Serial.print(ssid4);
      WiFi.begin(ssid4, pass4);
      delay(1000);
      while (--tries && WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
      }
      break;
    case 5:
      Serial.print(ssid5);
      WiFi.begin(ssid5, pass5);
      delay(1000);
      while (--tries && WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
      }
      break;
    case 6:
      Serial.print(ssid6);
      WiFi.begin(ssid6, pass6);
      delay(1000);
      while (--tries && WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
      }
      break;
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    return 1;
  } else {
    return 0;
  }
}

void getConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  //params auth, ssid, pass to be defined in settings.h
  int attempts = COUNT_OF_AVAILABLE_NETWORKS;
  Serial.println("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  int isConnected = connectToWiFi(PREFERABLE_NETWORK);
  if (isConnected) {
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    attempts = 0;
  }
  while (attempts > 0) {
    isConnected = connectToWiFi(attempts);
    if (isConnected) {
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      break;
    }
    attempts--;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot connect to WiFi networks");
    Serial.println("Starting AP");
    StartAPMode();
  }
}

// void WIFIinit() {
//   // Попытка подключения к точке доступа
//   WiFi.mode(WIFI_STA);
//   byte tries = 11;
//   WiFi.begin(_ssid.c_str(), _password.c_str());
//   // Делаем проверку подключения до тех пор пока счетчик tries
//   // не станет равен нулю или не получим подключение
//   while (--tries && WiFi.status() != WL_CONNECTED) {
//     Serial.print(".");
//     delay(1000);
//   }
//   if (WiFi.status() != WL_CONNECTED) {
//     // Если не удалось подключиться запускаем в режиме AP
//     Serial.println("");
//     Serial.println("WiFi up AP");
//     StartAPMode();
//   } else {
//     // Иначе удалось подключиться отправляем сообщение
//     // о подключении и выводим адрес IP
//     Serial.println("");
//     Serial.println("WiFi connected");
//     Serial.println("IP address: ");
//     Serial.println(WiFi.localIP());
//   }
// }

bool StartAPMode() {  // Отключаем WIFI
  WiFi.disconnect();
  // Меняем режим на режим точки доступа
  WiFi.mode(WIFI_AP);
  // Задаем настройки сети
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  // Включаем WIFI в режиме точки доступа с именем и паролем
  // хронящихся в переменных _ssidAP _passwordAP
  WiFi.softAP(_ssidAP.c_str(), _passwordAP.c_str());
  return true;
}
