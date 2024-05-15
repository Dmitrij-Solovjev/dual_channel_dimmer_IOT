#ifndef task_manager_h
#define task_manager_h

#include <Arduino.h>

typedef std::pair<uint32_t, std::pair<std::function<void(void *)>*, void *>> my_pair;
#include "priority_queue.h"


// компоратор для приоритетной очереди. Осуществляет сортировку в соответсвии с первым элементом (временем).
// Учитывает переполнение, переполненные элементы заносятся в конец. 
// const auto cmp = [](std::pair<uint32_t, std::function<void()> *> left, std::pair<uint32_t, std::function<void()> *> right) {
/*const static inline bool cmp(const my_pair &left, const my_pair &right) {

  // Возможно 4 случая (1 - переполнен, 0 - нет):
  // (0, 0), (0, 1), (1, 0), (1 , 1)
  const uint32_t now_time = millis();
  bool overflow_1 = false, overflow_2 = false;

  if (now_time - left.first  < left.first  - now_time) overflow_1 = true;
  if (now_time - right.first < right.first - now_time) overflow_2 = true;
  
  if (overflow_1 ^ overflow_2) {
    // случай (1, 0), (0, 1)
    return overflow_1 < overflow_2;
  } else {
    // случай (0, 0), (1, 1)
    return left.first < right.first;
  }

};*/

/*############################################################################################################*/
/*############################################################################################################*/
/*############################################################################################################*/
class task_manager {
  HardwareTimer *timer = nullptr; //(TIM9);
  static StaticPriorityQueue <my_pair, 100> tasks; //{cmp};

public:
  /*############################################################################################################*/
  task_manager(HardwareTimer *task_timer, uint16_t timer_frequency=1000) {   //  1000 Гц
    //timer = HardwareTimer(timer_instance);
    timer = task_timer;
    timer->attachInterrupt(OnTimerInterrupt);
	  Setup_Timer(timer_frequency);
  }

  /*############################################################################################################*/
  // Смотрит очередь на выполнение и выполняет задачи
  static void OnTimerInterrupt() {
//    	digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    if (tasks.empty()) return;

    uint32_t now_time = millis();
    
    // Пока есть пропущенные или невыполненные задачи и не прозошло переполнение
    while (!tasks.empty() && (tasks.top().first < now_time || ((now_time - tasks.top().first) < (tasks.top().first - now_time)))) {
      //Serial.print(millis());
      //Serial.print(" : ");
      //Serial.println(tasks.top().first);
      //Serial.println("I am execute!");
      auto a = *tasks.top().second.first;
      auto b = tasks.top().second.second;
      a(b);
      delete tasks.top().second.first;
      delete tasks.top().second.second;
      
//    	digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      tasks.pop();
    }
  }

  /*############################################################################################################*/
  void Setup_Timer(uint16_t frequency) {
    timer->setPrescaleFactor(2560 * 21 / frequency);   // Set prescaler to 2560*21/1 => timer frequency = 168MHz/(2564*21)*frequency = 3125 Hz * frequency (from prediv'd by 1 clocksource of 168 MHz)
		timer->setOverflow(3125);						              // Set overflow to 3125 => timer frequency = 3125Hz * frequency / 3125 = frequency Hz
		timer->resume();								                  // Make register changes take effect
    //timer->refresh();
	}

  /*############################################################################################################*/
  void add_task(std::pair<std::function<void(void *)>*, void *> executable_function_and_args, uint32_t timer_time_ms){
//    auto a = *executable_function_and_args.first;
//    auto b = executable_function_and_args.second;
//    a(b);
    tasks.push(std::make_pair(timer_time_ms, executable_function_and_args));
  }

  void add_task(std::function<void()>* executable_function, uint32_t timer_time_ms){

    std::function<void(void *)> lambda = [executable_function](void * arg) { (*executable_function)(); };
    std::function<void(void *)> *ulala = new std::function<void(void *)> (lambda);
    std::string *function_pointer = new std::string("");
    
    tasks.push(std::make_pair(timer_time_ms, std::make_pair(ulala, function_pointer)));
  }
};

StaticPriorityQueue <my_pair, 100> task_manager::tasks;

#endif