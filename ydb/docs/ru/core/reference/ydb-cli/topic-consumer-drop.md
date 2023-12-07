# Удаление читателя топика

С помощью команды `topic consumer drop` вы можете удалить [добавленного ранее](topic-consumer-add.md) читателя.

Общий вид команды:

```bash
{{ ydb-cli }} [global options...] topic consumer drop [options...] <topic-path>
```

* `global options` — [глобальные параметры](commands/global-options.md).
* `options` — [параметры подкоманды](#options).
* `topic-path` — путь топика.

Посмотрите описание команды удаления читателя:

```bash
{{ ydb-cli }} topic consumer drop --help
```

## Параметры подкоманды {#options}

Имя | Описание
---|---
`--consumer-name VAL` | Имя читателя, которого нужно удалить.

## Примеры {#examples}

{% include [ydb-cli-profile](../../_includes/ydb-cli-profile.md) %}

Удалите [созданного ранее](#consumer-add) читателя с именем `my-consumer` для топика `my-topic`:

```bash
{{ ydb-cli }} -p db1 topic consumer drop \
  --consumer-name my-consumer \
  my-topic
```
