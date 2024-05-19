#ifndef RadioModule_h
#define RadioModule_h
#include "si4432.h"
#include <string>

class RadioModule {
    Si4432 *radio; // CS, SDN, IRQ
public:
    RadioModule(uint8_t ChipSelectPin, uint8_t SleepPin, uint8_t IterruptPin)
    {
        radio = new Si4432(ChipSelectPin, SleepPin, IterruptPin);
    }

    void init(SPIClass *spi = &SPI)
    {
        pinMode(LED_BUILTIN, OUTPUT);

        int counter = 0;
        while (radio->init(&SPI) == false)
        { // В случае неудачи будем мигать и перезагружаться
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(500);

            counter++;

            if (counter > 10)
            {
                NVIC_SystemReset();
            }
        }

        radio->setBaudRate(70);
        radio->setFrequency(433);
        radio->readAll();

        radio->startListening();
    }

    // Тип данных длины 64 байта, где первый байт - конфигурация, 62 байта - сообщение
    // и 1 байт контрольной суммы
    union message
    {
        struct
        {
            uint8_t config_bits;      // биты конфигурации и цифровая подпись
            uint8_t message_byte[62]; // сообщение 66 символов
            uint8_t _CRC;             // хеш-функция от первых 7 бит
        };
        uint8_t bytes[64];
    };

private:
    // Функция вычисления хеша, используется контрольная
    // сумма, материал: https://habr.com/ru/articles/278171/
    // Так же осуществляется проверка равенства
    bool calculate_CRC(message &mess)
    {
        uint8_t last_CRC = mess._CRC;
        // uint16_t _CRC = 0;

        for (uint8_t i = 0; i < 63; ++i)
        {
            uint16_t val = static_cast<uint8_t>(mess.bytes[i] * 44111);
            mess._CRC = mess._CRC + val;
        }

        bool check_equal = (mess._CRC == last_CRC);
        //    bool check_equal = true;
        return !check_equal;
    }

public:
    /* Функция отправки сообщения произвольной длины. Аргументы
    ----> req_guarant - Требуется ли подтверждения получения
    ----> message_len - Длина сообщения
    ----> message_text - Само сообщение
    ----> return = 0 - нет ошибок (req_guarant == false or success) */
    bool ssend(bool req_guarant, const uint16_t message_len, const char *message_text)
    {
        uint8_t iterations = (message_len + 61) / 62; // округляем вверх

        for (uint8_t i = 0; i < iterations; ++i)
        {
            uint8_t config = 0; // FIXME: настройка конфигурационных битов
            if (iterations == 1)
            { // Простое сообщение,      всего один пакет: 11000000
                config = 0 | (3 << 6);
            }
            else if (i == 0)
            { // Сообщение составное,        первый пакет: 10000000
                config = 0 | (2 << 6);
            }
            else if (i < iterations - 1)
            { // Сообщение составное, промежуточный пакет: 00000000
                config = 0 | (0 << 6);
            }
            else if (i == iterations - 1)
            { // Сообщение составное,     последний пакет: 01000000
                config = 0 | (1 << 6);
            }
            else
            {
                // Ошибка
            }
            if (req_guarant)
            { // Требуется подтверждение о доставке (важное сообщение)
                config = config | (1 << 5);
            }

            message packet;
            packet.config_bits = config;
            memcpy(packet.message_byte, &message_text[i * 62], sizeof(uint8_t) * 62);
            packet._CRC = 0;
            calculate_CRC(packet);

            bool sent = radio->sendPacket(64, packet.bytes);
        }
        radio->startListening();
        return 0; // TODO: Проверка получения req_guarant
    }

    bool ssend(const std::string txMessage, const bool req_guarant = false) {
        uint8_t iterations = (txMessage.length() + 61) / 62; // округляем вверх

        for (uint8_t i = 0; i < iterations; ++i) {
            uint8_t config = 0; // FIXME: настройка конфигурационных битов
            if (iterations == 1)
            { // Простое сообщение,      всего один пакет: 11000000
                config = 0 | (3 << 6);
            }
            else if (i == 0)
            { // Сообщение составное,        первый пакет: 10000000
                config = 0 | (2 << 6);
            }
            else if (i < iterations - 1)
            { // Сообщение составное, промежуточный пакет: 00000000
                config = 0 | (0 << 6);
            }
            else if (i == iterations - 1)
            { // Сообщение составное,     последний пакет: 01000000
                config = 0 | (1 << 6);
            }
            else
            {
                // Ошибка
            }
            if (req_guarant)
            { // Требуется подтверждение о доставке (важное сообщение)
                config = config | (1 << 5);
            }

            message packet;
            packet.config_bits = config;
            memcpy(packet.message_byte, &(txMessage.c_str()[i * 62]), sizeof(uint8_t) * 62);
            packet._CRC = 0;
            calculate_CRC(packet);

            bool sent = radio->sendPacket(64, packet.bytes);
        }
        radio->startListening();
        return 0; // TODO: Проверка получения req_guarant
    }

    bool srecv(uint16_t &text_len, char text[])
    {
        bool recv;
        uint8_t rxLen;
        text_len = 0;
        bool not_last_massage = true; // Переменная-индикатор является ли полученный пакет последним

        uint16_t time_begin_recv = millis() % 1000;

        while (not_last_massage)
        { // Пока есть что получать и сообщение не последнее
            message packet = {};

            radio->getPacketReceived(&rxLen, packet.bytes); // Получаем сообщение

            radio->startListening();

            memcpy(&text[text_len], &packet.message_byte, sizeof(uint8_t) * 62);

            // std::copy(text[text_len], text[text_len + rxLen], packet.message_byte); // Копируем
            text_len += rxLen - 2;
            // not_last_massage = ((packet.config_bits<<1)>>7) == 0;
            if ((packet.config_bits >> 6) == 2 ||
                (packet.config_bits >> 6) == 0)
            { // Сообщение не последнее
                not_last_massage = true;
            }
            else
            {
                not_last_massage = false;
            }
            while (!radio->isPacketReceived() && not_last_massage)
                ; // TODO: сделать тайм-аут изменяемым
        }
        return 0; // TODO: Проверка целостности
    }

    bool srecv(std::string &rxMessage)
    {
        bool recv;
        uint8_t rxLen;
        bool not_last_massage = true; // Переменная-индикатор является ли полученный пакет последним

        uint16_t time_begin_recv = millis() % 1000;

        while (not_last_massage)
        { // Пока есть что получать и сообщение не последнее
            message packet = {};

            radio->getPacketReceived(&rxLen, packet.bytes); // Получаем сообщение

            radio->startListening();
            
            char text[62];
            memcpy(&text, &packet.message_byte, sizeof(uint8_t) * 62);
            rxMessage+=text;

            if ((packet.config_bits >> 6) == 2 ||
                (packet.config_bits >> 6) == 0)
            { // Сообщение не последнее
                not_last_massage = true;
            }
            else
            {
                not_last_massage = false;
            }
            while (!radio->isPacketReceived() && not_last_massage)
                ; // TODO: сделать тайм-аут изменяемым
        }
        return 0; // TODO: Проверка целостности
    }

    bool isPacketReceived(){
        return radio->isPacketReceived();
    }

    ~RadioModule(){
        delete radio;
    }
};
#endif