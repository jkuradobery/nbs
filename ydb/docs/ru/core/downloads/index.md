# Загрузки

## {{ ydb-short-name }} CLI

[{{ ydb-short-name }} CLI](../reference/ydb-cli/index.md) — утилита командной строки для работы с базами данных YDB.

{% list tabs %}

- Linux

  {% include notitle [Linux](_includes/ydb-cli/linux.md) %}

- macOS (Intel)

  {% include notitle [macIntel](_includes/ydb-cli/darwin_amd64.md) %}

- macOS (M1 arm)

  {% include notitle [macM1](_includes/ydb-cli/darwin_arm64.md) %}

- Windows

  {% include notitle [Windows](_includes/ydb-cli/windows.md) %}


{% endlist %}

## {{ ydb-short-name }} Server

{{ ydb-short-name }} Server - сборка для запуска узла [кластера YDB](../concepts/databases.md#cluster).

{% list tabs %}

- Linux

  {% include notitle [linux](_includes/server/linux.md) %}

- Docker

  {% include notitle [docker](_includes/server/docker.md) %}

- Исходный код

  {% include notitle [docker](_includes/server/source_code.md) %}


{% endlist %}
