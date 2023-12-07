# Резервное копирование и восстановление

Резервное копирование применяется для защиты от потери данных, позволяя восстановить их из резервной копии.

{{ ydb-short-name }} предоставляет несколько решений для выполнения резервного копирования и восстановления:

* Резервное копирование в файлы и восстановление с помощью {{ ydb-short-name }} CLI.
* Резервное копирование в S3-совместимое хранилище и восстановление с помощью {{ ydb-short-name }} CLI.

{% include [_includes/backup_and_recovery/options_overlay.md](_includes/backup_and_recovery/options_overlay.md) %}

## {{ ydb-short-name }} CLI {#cli}

### Файлы {#files}

Для выполнения резервного копирования в файлы применяется команда `ydb tools dump`. Перейдите [по ссылке](../reference/ydb-cli/export_import/tools_dump.md) в справочник по {{ ydb-short-name }} CLI для получения информации о данной команде.

Для выполнения восстановления из файловой резервной копии применяется команда `ydb tools restore`. Перейдите [по ссылке](../reference/ydb-cli/export_import/tools_restore.md) в справочник по {{ ydb-short-name }} CLI для получения информации о данной команде.

### S3-совместимое хранилище {#s3}

Для выполнения резервного копирования в S3-совместимое хранилище (например, [AWS S3](https://docs.aws.amazon.com/AmazonS3/latest/dev/Introduction.html))  применяется команда `ydb export s3`. Перейдите [по ссылке](../reference/ydb-cli/export_import/s3_export.md) в справочник по {{ ydb-short-name }} CLI для получения информации о данной команде.

Для выполнения восстановления из резервной копии, созданной в S3-совместимом хранилище применяется команда `ydb import s3`. Перейдите [по ссылке](../reference/ydb-cli/export_import/s3_import.md) в справочник по {{ ydb-short-name }} CLI для получения информации о данной команде.

{% include [_includes/backup_and_recovery/cli_overlay.md](_includes/backup_and_recovery/cli_overlay.md) %}

{% include [_includes/backup_and_recovery/others_overlay.md](_includes/backup_and_recovery/others_overlay.md) %}
