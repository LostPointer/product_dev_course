#pragma once

#include <stddef.h>

#include "esp_err.h"

/** Статус Wi‑Fi STA (клиент, подключение к роутеру). */
struct WiFiStaStatus {
  bool configured{false};
  bool connected{false};
  int last_disconnect_reason{0};  // см. wifi_err_reason_t (0 = нет данных)
  int rssi{0};                    // RSSI текущей сети (0 если неизвестно)
  char ssid[33]{};                // SSID (ASCII, '\0'-terminated)
  char ip[16]{};                  // IPv4 (точка) или пусто
};

/** Элемент результата Wi‑Fi scan (STA). */
struct WiFiScanNetwork {
  int rssi{0};
  int channel{0};
  int authmode{0};   // wifi_auth_mode_t (numeric)
  char ssid[33]{};   // '\0'-terminated
};

/**
 * Инициализация Wi-Fi Access Point
 * Также поднимает интерфейс STA (AP+STA), чтобы можно было подключаться к
 * внешним сетям, не выключая точку доступа.
 * @return ESP_OK при успехе, иначе код ошибки
 */
esp_err_t WiFiApInit(void);

/**
 * Получить SSID текущей точки доступа (softAP)
 * @param ssid_str буфер для SSID
 * @param len размер буфера
 * @return ESP_OK при успехе
 */
esp_err_t WiFiApGetSsid(char* ssid_str, size_t len);

/**
 * Получить IP адрес точки доступа
 * @param ip_str буфер для строки IP
 * @param len размер буфера
 * @return ESP_OK при успехе
 */
esp_err_t WiFiApGetIp(char* ip_str, size_t len);

/**
 * Подключить STA к внешней Wi‑Fi сети (роутеру).
 * По умолчанию параметры сохраняются в NVS (если save=true) и автоподключаются
 * после перезагрузки.
 */
esp_err_t WiFiStaConnect(const char* ssid, const char* password, bool save);

/**
 * Отключить STA от внешней сети.
 * @param forget если true — также удалить сохранённые креды из NVS
 */
esp_err_t WiFiStaDisconnect(bool forget);

/**
 * Получить статус STA (потокобезопасно).
 */
esp_err_t WiFiStaGetStatus(WiFiStaStatus* out_status);

/**
 * Сканировать доступные Wi‑Fi сети (из STA).
 * Блокирующая операция: может занять 1–4 секунды.
 *
 * @param out_networks буфер результатов
 * @param inout_count на входе — размер out_networks, на выходе — кол-во записей
 */
esp_err_t WiFiStaScan(WiFiScanNetwork* out_networks, size_t* inout_count);
