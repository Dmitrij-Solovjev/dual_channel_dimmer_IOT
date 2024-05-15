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
#define SDN PA2  // Режим сна закорочен на землю (всегда выкл)

#define BUTTON_1 PB5
#define BUTTON_2 PB4
#define BUTTON_3 PB3
#define BUTTON_4 PA15

#define DIMM_1 PB13
#define DIMM_2 PB12

#define LED_BUTTON_2 PB0
#define LED_BUTTON_1 PB1

#define DEVICE_ID 1
const std::string device_config = "{\"id\":1,\"name\":\"Dimmer\",\"description\":\"Да,тосамоенашеустройство\",\"attributes\":[{\"name\":\"mode_1\",\"description\":\"Состояниепервогоканала\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":true,\"readable\":true},{\"name\":\"mode_2\",\"description\":\"Состояниевторогоканала\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":true,\"readable\":true},{\"name\":\"brightness_1\",\"description\":\"Яркостьпервогоканала\",\"show_attribute\":true,\"var_type\":\"int\",\"upper_limit\":100,\"lower_limit\":1,\"writable\":true,\"readable\":true},{\"name\":\"brightness_2\",\"description\":\"Яркостьвторогоканала\",\"show_attribute\":true,\"var_type\":\"int\",\"upper_limit\":100,\"lower_limit\":1,\"writable\":true,\"readable\":true},{\"name\":\"button_single_1\",\"description\":\"Одинарноенажатиепервойкнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true},{\"name\":\"button_double_1\",\"description\":\"Двойноенажатиепервойкнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true},{\"name\":\"button_long_1\",\"description\":\"Долгоенажатиепервойкнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true},{\"name\":\"button_single_2\",\"description\":\"Одинарноенажатиевторойкнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true},{\"name\":\"button_double_2\",\"description\":\"Двойноенажатиевторойкнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true},{\"name\":\"button_long_2\",\"description\":\"Долгоенажатиевторойкнопки\",\"show_attribute\":true,\"var_type\":\"bool\",\"upper_limit\":true,\"lower_limit\":false,\"writable\":false,\"readable\":true}]}";

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
task_manager My_tasks(&My_timer, 500);
CTinyJS *js = new CTinyJS();

static Button button_up(BUTTON_1, &My_tasks, up, button_position::off);

#if 0
void js_print(CScriptVar *v, void *userdata)
{
  Serial.printf("> %s\n", v->getParameter("text")->getString().c_str());
}
void js_delay(CScriptVar *v, void *userdata)
{
  delay(atoi(v->getParameter("time")->getString().c_str()));
}
#endif

// Считать значение, установить пин, установить переменную в JS
void js_mode(CScriptVar *v, void *userdata) {
  auto [var, pin] = *((std::pair<std::string, uint32_t> *) userdata);
  bool status = atoi(v->getParameter("status")->getString().c_str());
  digitalWrite(pin, status);

  std::string state = "id" + std::to_string(DEVICE_ID) + "." + var +"=" + (digitalRead(pin) ? "true" : "false") + ";";
  js->execute(state.c_str());
}


// сам исполнитель
void js_timer_execute(void *args)
{
  //Serial.println("Hello!!!");
  std::string *s_line = (std::string *)args;
  Serial.println(s_line->c_str());
  js->execute(s_line->c_str());
}

inline void sub_timer(uint32_t time, const std::string function)
{
  std::function<void(void *)> *ulala = new std::function<void(void *)>(js_timer_execute);
  std::string *function_pointer = new std::string(function);
  My_tasks.add_task(std::make_pair(ulala, function_pointer), millis() + time);
}

void js_timer(CScriptVar *v, void *userdata)
{
  uint32_t time = atoi(v->getParameter("time")->getString().c_str());
  const std::string function = v->getParameter("body")->getString();
  Serial.print(time);
  Serial.print(" --> ");
  Serial.println(function.c_str());
  std::function<void(void *)> *ulala = new std::function<void(void *)>(js_timer_execute);
  std::string *function_pointer = new std::string(function);
  My_tasks.add_task(std::make_pair(ulala, function_pointer), millis() + time);
}


void setup()
{
  SPI.setMOSI(MOSI); // TODO возможно не нужно
  SPI.setMISO(MISO);
  SPI.setSCLK(SCLK);

  SPI.begin();

  pinMode(LED_BUTTON_2, OUTPUT);
  pinMode(DIMM_1, OUTPUT);
  pinMode(DIMM_2, OUTPUT);

  auto button_single_1_topic = "id" + std::to_string(DEVICE_ID) + ".button_single_1";
  auto button_double_1_topic = "id" + std::to_string(DEVICE_ID) + ".button_double_1";
  auto button_long_1_topic = "id" + std::to_string(DEVICE_ID) + ".button_long_1";

  events_and_commands[button_single_1_topic] = "led_2(!(id1.led_2));";
  events_and_commands[button_double_1_topic] = "mode_1(!(id1.mode_1));";
  //events_and_commands[button_long_1_topic] = "mode_2(!(id1.mode_2));";

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

//  js->addNative("function print(text)", &js_print, 0);

  js->addNative("function mode_1(status)", &js_mode, (void *) new std::pair<std::string, int> ("mode_1", DIMM_1));
  js->addNative("function mode_2(status)", &js_mode, (void *) new std::pair<std::string, int> ("mode_2", DIMM_2));
  js->addNative("function led_2(status)", &js_mode, (void *) new std::pair<std::string, int> ("led_2", LED_BUTTON_2));
  //js->addNative("function led2(status)", &js_led2, 0);
  

  Serial.println("Test 3: ");

  js->addNative("function timer(body, time)", &js_timer, 0);
  js->execute("id1 = new Object(); id1.led_2 =false; id1.mode_1 = false; id1.mode_2 = false; id1.button_single_1 = false; id1.button_double_1 = false; id1.button_long_1 = false;");
}

void loop() {
  char textBuf[1000] = {};
  bool recv = Rmodile.isPacketReceived();
  
  if (recv) {
    uint16_t textLen = 0;

    Rmodile.srecv(textLen, textBuf);
  
    JsonDocument recv_message;
    deserializeJson(recv_message, textBuf);

    String type = recv_message["type"];
    // Исполнение соббытия
    if (type == "event")
    {
      auto message = recv_message["message"];
      std::string event = message["event"];
      std::string text = message["text"];


      auto foo = events_and_commands.find(event);
      if (foo != std::end(events_and_commands)) { // Найден в списке на исполнение
        js->execute(text);
        js->execute(events_and_commands[event]);
      }
    }

    // Поступил запрос на конфиг
    if (type == "request")
    {
      auto message = recv_message["message"];
      uint32_t id = message["id"];
      if (id == DEVICE_ID)
      {
        Rmodile.ssend(false, device_config.length(), device_config.c_str());
      }
    }

    // Пришла настройка, изменение реакции на событие
    if (type == "config")
    {
      auto message = recv_message["message"];
      uint32_t id = message["id"];
      std::string event = message["event"];
      std::string text = message["text"];
      
      if (id == DEVICE_ID)
      {
        //digitalWrite(LED_BUTTON_2, !digitalRead(LED_BUTTON_2));
        events_and_commands[event] = text;
      }
    }
  }
  //digitalWrite(DIMM_1, !digitalRead(DIMM_1));
  //digitalWrite(DIMM_2, !digitalRead(DIMM_2));
  //delay(500);
  
}
//{"type":"event","message":{"event":"id1.button_long_1","text": "id1.button_double_1 = true"}}