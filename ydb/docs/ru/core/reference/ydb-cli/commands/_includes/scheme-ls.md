# Список объектов

Команда `scheme ls` позволяет получить список объектов в базе данных:

```bash
{{ ydb-cli }} [connection options] scheme ls [path] [-lR]
```

{% include [conn_options_ref.md](conn_options_ref.md) %}

При запуске без параметров выводится перечень имен объектов в корневой директории базы данных в сжатом формате.

Параметром `path` можно задать [директорию](../dir.md), для которой нужно вывести перечень объектов.

Для команды доступны следующие опции:
- `-l` : Полная информация об атрибутах каждого объекта
- `-R` : Рекурсивный обход всех поддиректорий

**Примеры**

{% include [example_db1.md](../../_includes/example_db1.md) %}

- Получение объектов в корневой директории базы данных в сжатом формате

```bash
{{ ydb-cli }} --profile db1 scheme ls
```

- Получение объектов во всех директориях базы данных в сжатом формате

```bash
{{ ydb-cli }} --profile db1 scheme ls -R
```

- Получение объектов в заданной директории базы данных в сжатом формате

```bash
{{ ydb-cli }} --profile db1 scheme ls dir1
{{ ydb-cli }} --profile db1 scheme ls dir1/dir2
```

- Получение объектов во всех поддиректориях заданной директории базы данных в сжатом формате

```bash
{{ ydb-cli }} --profile db1 scheme ls dir1 -R
{{ ydb-cli }} --profile db1 scheme ls dir1/dir2 -R
```

- Получение полной информации по объектам в корневой директории базы данных

```bash
{{ ydb-cli }} --profile db1 scheme ls -l
```

- Получение полной информации по объектам в заданной директории базы данных

```bash
{{ ydb-cli }} --profile db1 scheme ls dir1 -l
{{ ydb-cli }} --profile db1 scheme ls dir2/dir3 -l
```

- Получение полной информации по объектам во всех директориях базы данных

```bash
{{ ydb-cli }} --profile db1 scheme ls -lR
```

- Получение полной информации по объектам во всех поддиректориях заданной директории базы данных

```bash
{{ ydb-cli }} --profile db1 scheme ls dir1 -lR
{{ ydb-cli }} --profile db1 scheme ls dir2/dir3 -lR
```

