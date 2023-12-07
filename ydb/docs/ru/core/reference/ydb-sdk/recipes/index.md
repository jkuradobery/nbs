---
title: "Обзор рецептов кода с использованием {{ ydb-short-name }} SDK."
description: "В данном разделе содержатся рецепты кода на разных языках программирования для решения различных задач, часто встречающихся на практике, с использованием {{ ydb-short-name }} SDK."
---

# Рецепты кода

{% include [work in progress message](_includes/addition.md) %}

В данном разделе содержатся рецепты кода на разных языках программирования для решения различных задач, часто встречающихся на практике, с использованием {{ ydb-short-name }} SDK.

Содержание:
- [Обзор](index.md)
- [Инициализация драйвера](init.md)
- [Аутентификация](auth.md)
  - [С помощью токена](auth-access-token.md)
  - [Анонимная](auth-anonymous.md)
  - [Файл сервисного аккаунта](auth-service-account.md)
  - [Сервис метаданных](auth-metadata.md)
  - [С помощью переменных окружения](auth-env.md)
  - [С помощью логина и пароля](auth-static.md)
- [Балансировка](balancing.md)
  - [Равномерный случайный выбор](balancing-random-choice.md)
  - [Предпочитать ближайший дата-центр](balancing-prefer-local.md)
  - [Предпочитать зону доступности](balancing-prefer-location.md)
- [Выполнение повторных запросов](retry.md)
- [Установить размер пула сессий](session-pool-limit.md)
- [Вставка данных](upsert.md)
- [Пакетная вставка данных](bulk-upsert.md)
- [Диагностика проблем](debug.md)
  - [Включить логирование](debug-logs.md)
  - [Подключить метрики в Prometheus](debug-prometheus.md)
  - [Подключить трассировку в Jaeger](debug-jaeger.md)
