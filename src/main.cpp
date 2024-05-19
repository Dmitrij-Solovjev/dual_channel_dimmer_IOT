/***
 * Двухканальный диммер, трехпроводной (нуль, фаза, лампа)
 * ссылка на проект EasyEDA: https://u.easyeda.com/account/user/projects/index/detail?project=867664d336e24240b35f88530a9bd3b4
 */

#include <Arduino.h>
#include <SPI.h>
#include "RadioModule.h"
#include "task_manager.h"
#include "button.h"
#include <ArduinoJson.h>

#include "TinyJS.h"
#include "TinyJS_Functions.h"
#include <assert.h>
#include <stdio.h>
#include <map>

#define MOSI PA7
#define MISO PA6
#define SCLK PA5
#define CS PA4   // chip select
#define NIQR PA3 // прерывание при получении сообщения
#define SDN PA1  // Режим сна закорочен на землю (всегда выкл)

#define BUTTON_1 PB5
#define BUTTON_2 PB4
#define BUTTON_3 PB3
#define BUTTON_4 PA15

#define AC_SENSOR PA2 // Оптропара подключенная к пину Pin12 (PA2)

#define DIMM_1 PB13
#define DIMM_2 PB12

#define LED_BUTTON_2 PB0
#define LED_BUTTON_1 PB1

#define AC_SENSOR PA2

#define DEVICE_ID 2
const std::string device_config = "{\"id\":"+ std::to_string(DEVICE_ID) +",\"name\":\"Dimmer\",\"description\":\"Двухканальныйдиммер\",\"attributes\":[{\"name\":\"mode_1\",\"description\":\"Состояние первого канала\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":true,\"readable\":true},{\"name\":\"mode_2\",\"description\":\"Состояние второго канала\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":true,\"readable\":true},{\"name\":\"dimm_1\",\"description\":\"Яркость первого канала. Не используйте значение \\\"0\\\" для выключения, управляйте состоянием при помощи mode_x\",\"show_attribute\":true,\"var_type\":\"int\",\"upper_limit\":100,\"lower_limit\":1,\"writable\":true,\"readable\":true},{\"name\":\"dimm_2\",\"description\":\"Яркость второго канала. Не используйте значение \\\"0\\\" для выключения, управляйте состоянием при помощи mode_x\",\"show_attribute\":true,\"var_type\":\"int\",\"upper_limit\":100,\"lower_limit\":1,\"writable\":true,\"readable\":true},{\"name\":\"button_single_1\",\"description\":\"Одинарное нажатие первой кнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true},{\"name\":\"button_double_1\",\"description\":\"Двойное нажатие первой кнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true},{\"name\":\"button_long_1\",\"description\":\"Долгое нажатие первой кнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true}]}";

// Очередь в которой хранятся названия событий и команды к ним
std::map <std::string, std::string> events_and_commands;

const std::string on_event_send[3][2] = {
  {
  "{\"type\":\"event\",\"message\":{\"event\":\"id" + std::to_string(DEVICE_ID) + ".button_single_1\",\"text\": \"id" + std::to_string(DEVICE_ID) + ".button_single_1 = true\"}}",
  "{\"type\":\"event\",\"message\":{\"event\":\"id" + std::to_string(DEVICE_ID) + ".button_single_1\",\"text\": \"id" + std::to_string(DEVICE_ID) + ".button_single_1 = false\"}}"
  },
  {
  "{\"type\":\"event\",\"message\":{\"event\":\"id" + std::to_string(DEVICE_ID) + ".button_double_1\",\"text\": \"id" + std::to_string(DEVICE_ID) + ".button_double_1 = true\"}}",
  "{\"type\":\"event\",\"message\":{\"event\":\"id" + std::to_string(DEVICE_ID) + ".button_double_1\",\"text\": \"id" + std::to_string(DEVICE_ID) + ".button_double_1 = false\"}}"
  },
  {
  "{\"type\":\"event\",\"message\":{\"event\":\"id" + std::to_string(DEVICE_ID) + ".button_long_1\",\"text\": \"id" + std::to_string(DEVICE_ID) + ".button_long_1 = true\"}}",
  "{\"type\":\"event\",\"message\":{\"event\":\"id" + std::to_string(DEVICE_ID) + ".button_long_1\",\"text\": \"id" + std::to_string(DEVICE_ID) + ".button_long_1 = false\"}}"
  }};


RadioModule Rmodile(CS, SDN, NIQR);

HardwareTimer My_timer(TIM9);
task_manager My_tasks(&My_timer, 2560*21);
CTinyJS *js = new CTinyJS();

static Button button_up(BUTTON_1, &My_tasks, up, button_position::off);

uint8_t dimm_1_percent = 50; // Dimming level (0-100)  100 = MAX, 1 = MIN. Don't use 0 to turn off? use dimm_X_mode = false
uint8_t dimm_2_percent = 50; // Dimming level (0-100)  100 = MAX, 1 = MIN. Don't use 0 to turn off? use dimm_X_mode = false

bool dimm_1_mode = false;     // Dimming mode (false-true) true = ON, false = OFF
bool dimm_2_mode = true;     // Dimming mode (false-true) true = ON, false = OFF

// Считать значение, установить пин, установить переменную в JS
void js_mode(CScriptVar *v, void *userdata) {
  auto [var, pin] = *((std::pair<std::string, uint32_t> *) userdata);
  bool status = atoi(v->getParameter("status")->getString().c_str());
  if (pin == DIMM_1) {
    dimm_1_mode = status;
  } else if (pin == DIMM_2) {
    dimm_2_mode = status;
  } else {
//    Rmodile.ssend("I am here");
    digitalWrite(pin, status);
  }
  //201
  //200

  std::string state = "id" + std::to_string(DEVICE_ID) + "." + var +"=" + (status ? "true" : "false") + ";";
  js->execute(state.c_str());
}

void js_dimm(CScriptVar *v, void *userdata) {
  auto [var, pin] = *((std::pair<std::string, uint32_t> *) userdata);
  uint8_t percent = atoi(v->getParameter("status")->getString().c_str());
  if (pin == DIMM_1) {
    dimm_1_percent = percent;
  } else if (pin == DIMM_2) {
    dimm_2_percent = percent;
  } else {
    // Ошибка: нам больше нечего диммировать
  }

  std::string state = "id" + std::to_string(DEVICE_ID) + "." + var + "=" + std::to_string(percent) + ";";
  js->execute(state.c_str());
}

void js_timer(CScriptVar *v, void *userdata)
{
  uint32_t time = atoi(v->getParameter("time")->getString().c_str());
  const std::string function = v->getParameter("body")->getString();
  std::function<void(void *)> *ulala = new std::function<void(void *)>(
        [](void *args) {
          std::string *s_line = (std::string *)args;
          js->execute(s_line->c_str());
        });
  std::string *function_pointer = new std::string(function);
  My_tasks.add_task(std::make_pair(ulala, function_pointer), millis() + time);
}

// Прерывание по пину оптопары "AC_SENSOR" для отслеживания сетевой синусоиды.
// Вызывается когда напряжение в сети = 0, между вызовами около 7.2 мс.
void zero_crosss_int() {
  noInterrupts(); // Останавливаем другие прерывания, чтобы не порушить программу
  uint16_t dim_1_time = (72 * (100 - dimm_1_percent));
  uint16_t dim_2_time = (72 * (100 - dimm_2_percent));

  if (dimm_1_mode) {
    std::function<void()> ulala([]()
        {
          digitalWrite(DIMM_1, HIGH);
          delayMicroseconds(25);
          digitalWrite(DIMM_1, LOW);
        });
    
    
    My_tasks.add_task_microsecond(ulala, micros() + dim_1_time);
  }

  if (dimm_2_mode) {
    std::function<void()> ulala([]()
        {
          digitalWrite(DIMM_2, HIGH);
          delayMicroseconds(25);
          digitalWrite(DIMM_2, LOW);
        });

    My_tasks.add_task_microsecond(ulala, micros() + dim_2_time);
  }
  interrupts(); // Разрешаем снова
}

void setup() {
  SPI.setMOSI(MOSI); // TODO возможно не нужно
  SPI.setMISO(MISO);
  SPI.setSCLK(SCLK);

  SPI.begin();

  pinMode(LED_BUTTON_1, OUTPUT);
  pinMode(LED_BUTTON_2, OUTPUT);
  pinMode(DIMM_1, OUTPUT);
  pinMode(DIMM_2, OUTPUT);
  attachInterrupt(AC_SENSOR, zero_crosss_int, FALLING);

  auto button_single_1_topic = "id" + std::to_string(DEVICE_ID) + ".button_single_1";
  auto button_double_1_topic = "id" + std::to_string(DEVICE_ID) + ".button_double_1";
  auto button_long_1_topic = "id" + std::to_string(DEVICE_ID) + ".button_long_1";

  events_and_commands[button_single_1_topic] = "if (id"+ std::to_string(DEVICE_ID) + ".button_single_1){ led_1(!id"+ std::to_string(DEVICE_ID) + ".led_1); } "+
                                               "if (id"+ std::to_string(DEVICE_ID) + ".button_single_1){ led_2(!id"+ std::to_string(DEVICE_ID) + ".led_2); }";
  events_and_commands[button_double_1_topic] = "mode_1(!(id"+ std::to_string(DEVICE_ID) + ".mode_1));";
  events_and_commands[button_long_1_topic] = "if (id"+ std::to_string(DEVICE_ID) + ".button_long_1){ mode_2(!id"+ std::to_string(DEVICE_ID) + ".mode_2); }";

  Rmodile.init(&SPI);

  button_up.init(
      []() { button_up.Clarify_Status_Single(); }, // обработка одинарного нажатия
      []() { button_up.Clarify_Status_Double(); }, // обработка двойного   нажатия
      []() { button_up.Clarify_Status_Long(); }, // обработка повторного нажатия
      []() { button_up.Detect_Press_Type(); }, // обработка изменения статуса кнопки (отпустили/нажали) по хорошему Detect_Press -> Contact_Bounce_Checker -> Detect_Press_Type
      []() { button_up.Contact_Bounce_Checker(); }, // обработка дребезга контакта
      [button_single_1_topic]() {                                       // одиночное нажатие
        js->execute(button_single_1_topic + "=true;");
        js->execute(events_and_commands[button_single_1_topic]);
        js->execute(button_single_1_topic + "=false;");

        Rmodile.ssend(false, on_event_send[0][0].length(), on_event_send[0][0].c_str());
      },
      [button_double_1_topic]() {                                       // двойное нажатие
        std::string state;
        const std::string *text;
        if (digitalRead(button_up.pin) != button_up.resting_position) { // Кнопка зажата
          text = &on_event_send[1][0];
          state = "true";
        } else {                                                        // Кнопка отпущена
          text = &on_event_send[1][1];
          state = "false";
        }
        
        js->execute(button_double_1_topic + "=" + state + ";");
        js->execute(events_and_commands[button_double_1_topic]);


        Rmodile.ssend(false, text->length(), text->c_str());
      },
      [button_long_1_topic]() {
//        button_up.Repeat([](){
        std::string state;
        const std::string *text;

        if (digitalRead(button_up.pin) != button_up.resting_position) { // Кнопка зажата
          text = &on_event_send[2][0];
          state = "true";
        } else {                                                        // Кнопка отпущена
          text = &on_event_send[2][1];
          state = "false";
        }

        js->execute(button_long_1_topic + "=" + state + ";");
        js->execute(events_and_commands[button_long_1_topic]);

        Rmodile.ssend(false, text->length(), text->c_str());
//        }, delay_time::repeat_delay_time);
      }); // долгое нажатие

  js->addNative("function led_1(status)", &js_mode, (void *) new std::pair<std::string, int> ("led_1", LED_BUTTON_1));
  js->addNative("function led_2(status)", &js_mode, (void *) new std::pair<std::string, int> ("led_2", LED_BUTTON_2));

  js->addNative("function mode_1(status)", &js_mode, (void *) new std::pair<std::string, int> ("mode_1", DIMM_1));
  js->addNative("function mode_2(status)", &js_mode, (void *) new std::pair<std::string, int> ("mode_2", DIMM_2));

  js->addNative("function dimm_1(status)", &js_dimm, (void *) new std::pair<std::string, int> ("dimm_1", DIMM_1));
  js->addNative("function dimm_2(status)", &js_dimm, (void *) new std::pair<std::string, int> ("dimm_2", DIMM_2));

  js->addNative("function timer(body, time)", &js_timer, 0);

  js->execute("id"+ std::to_string(DEVICE_ID) +" = new Object();");
  js->execute("id"+ std::to_string(DEVICE_ID) +".led_1 = false;");
  js->execute("id"+ std::to_string(DEVICE_ID) +".led_2 = false;");
  js->execute("id"+ std::to_string(DEVICE_ID) +".mode_1 = false;");
  js->execute("id"+ std::to_string(DEVICE_ID) +".mode_2 = false;");
  js->execute("id"+ std::to_string(DEVICE_ID) +".dimm_1 = 100;");
  js->execute("id"+ std::to_string(DEVICE_ID) +".dimm_2 = 100;");
  js->execute("id"+ std::to_string(DEVICE_ID) +".button_single_1 = false;");
  js->execute("id"+ std::to_string(DEVICE_ID) +".button_double_1 = false;");
  js->execute("id"+ std::to_string(DEVICE_ID) +".button_long_1 = false;");
}

void loop() {
  
//  char textBuf[1000] = {};
  bool recv = Rmodile.isPacketReceived();
  
  if (recv) {
//    uint16_t textLen = 0;
    std::string textBuf;

//    Rmodile.srecv(textLen, textBuf);
    Rmodile.srecv(textBuf);
  
    JsonDocument recv_message;
    deserializeJson(recv_message, textBuf);

    String type = recv_message["type"];

    // Исполнение соббытия
    if (type == "event") {
      auto message = recv_message["message"];
      std::string event = message["event"];
      std::string text = message["text"];


      auto foo = events_and_commands.find(event);
      if (foo != std::end(events_and_commands)) { // Найден в списке на исполнение
        js->execute(text);
        js->execute(events_and_commands[event]);
      }
    } else 

    // Поступил запрос на конфиг
    if (type == "request") {
      auto message = recv_message["message"];
      uint32_t id = message["id"];
      if (id == DEVICE_ID)
      {
        Rmodile.ssend(false, device_config.length(), device_config.c_str());
      }
    } else

    // Пришла настройка, изменение реакции на событие
    if (type == "config") {
      auto message = recv_message["message"];
      uint32_t id = message["id"];
      std::string event = message["event"];
      std::string text = message["text"];
      
      if (id == DEVICE_ID)
        events_and_commands[event] = text;
    }
  }
  
}
#if 0
{"type":"event","message":{"event":"id2.button_double_1","text": "id2.button_double_1 = false"}}
{"type":"config","message":{"id":2,"event":"id2.dimm_2","text":""}}

{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(100);"}}

{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(80);"}}
{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(70);"}}
{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(50);"}}

{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(40);"}}
{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(30);"}}
{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(20);"}}
{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(10);"}}
{"type":"event","message":{"event":"id2.dimm_2","text":"dimm_2(0);"}}



{"type":"config","message":{"id":2,"event":"id2.button_long_1","text":"function t1(){ mode_2(!id2.mode_2); if (id2.button_long_1) {timer(t1, 500);} } if (id2.button_long_1){ timer(t1, 500); }"}}

#endif