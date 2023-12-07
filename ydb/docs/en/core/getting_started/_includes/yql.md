# YQL: Getting started

## Introduction {#intro}

YQL is a {{ ydb-short-name }} query language, a dialect of SQL. Specifics of its syntax let you use it when executing queries on clusters.

For more information about the YQL syntax, see the [YQL reference](../../yql/reference/index.md).

The examples below demonstrate how to get started with YQL and assume that the steps described will be completed sequentially: the queries in the [Working with data](#dml) section access data in the tables created in the [Working with a data schema](#ddl) section. Follow the steps one by one so that the examples copied through the clipboard are executed successfully.

The {{ ydb-short-name }} YQL basic interface accepts a script that may consist of multiple commands and not a single command as input.

## YQL query execution tools {#tools}

In {{ ydb-short-name }}, you can make YQL queries to a database using:

{% include [yql/ui_prompt.md](yql/ui_prompt.md) %}

* [{{ ydb-short-name }} CLI](#cli)

* [{{ ydb-short-name }} SDK](../sdk.md)

{% include [yql/ui_execute.md](yql/ui_execute.md) %}

### {{ ydb-short-name }} CLI {#cli}

To enable script execution using the {{ ydb-short-name }} CLI, do the following:

* [Install the CLI](../cli.md#install).
* Define and check [DB connection parameters](../cli#scheme-ls).
* [Create a `db1` profile](../cli.md#profile) configured to connect to your database.

Save the text of the scripts below to a file. Name it `script.yql` to be able to run the statements given in the examples by simply copying them through the clipboard. Next, run `{{ ydb-cli }} yql` indicating the use of the `db1` profile and reading the script from the `script.yql` file:

```bash
{{ ydb-cli }} --profile db1 yql -f script.yql
```

## Working with a data schema {#ddl}

### Creating tables {#create-table}

A table with the specified columns is created [using the YQL `CREATE TABLE` command](../../yql/reference/syntax/create_table.md). Make sure the primary key is defined in the table. Column data types are described in [YQL data types](../../yql/reference/types/index.md).

All columns are optional by default and can contain `NULL`. You can specify a `NOT NULL` limit for the columns that are part of the primary key. {{ ydb-short-name }} does not support `FOREIGN KEY` limits.

Create series directory tables named `series`, `seasons`, and `episodes` by running the following script:

```sql
CREATE TABLE series (
    series_id Uint64 NOT NULL,
    title Utf8,
    series_info Utf8,
    release_date Date,
    PRIMARY KEY (series_id)
);

CREATE TABLE seasons (
    series_id Uint64,
    season_id Uint64,
    title Utf8,
    first_aired Date,
    last_aired Date,
    PRIMARY KEY (series_id, season_id)
);

CREATE TABLE episodes (
    series_id Uint64,
    season_id Uint64,
    episode_id Uint64,
    title Utf8,
    air_date Date,
    PRIMARY KEY (series_id, season_id, episode_id)
);
```

For a description of everything you can do when working with tables, review the relevant sections of the YQL documentation:

* [CREATE TABLE](../../yql/reference/syntax/create_table.md): Create a table and define its initial parameters.
* [ALTER TABLE](../../yql/reference/syntax/alter_table.md): Modify a table's column structure and parameters.
* [DROP TABLE](../../yql/reference/syntax/drop_table.md): Delete a table.

To execute the script via the {{ ydb-short-name }} CLI, follow the instructions given under [Executing YQL queries in the {{ ydb-short-name }} CLI](#cli) in this article.

### Getting a list of existing DB tables {#scheme-ls}

Check that the tables are actually created in the database.

{% include [yql/ui_scheme_ls.md](yql/ui_scheme_ls.md) %}

To get a list of existing DB tables via the {{ ydb-short-name }} CLI, make sure that the prerequisites under [Executing YQL scripts in the {{ ydb-short-name }} CLI](#cli) are complete and run the [`scheme ls` command](../cli.md#ping):

```bash
{{ ydb-cli }} --profile db1 scheme ls
```

## Operations with data {#dml}

Commands for running YQL queries and scripts in the YDB CLI and the web interface run in Autocommit mode meaning that a transaction is committed automatically  after it is completed.

### UPSERT: Adding data {#upsert}

The most efficient way to add data to {{ ydb-short-name }} is through the [`UPSERT`](../../yql/reference/syntax/upsert_into.md) statement. It inserts new data by primary keys regardless of whether data by these keys previously existed in the table. As a result, unlike regular `INSERT` and `UPDATE`, it does not require a data pre-fetch on the server to verify that a key is unique. When working with {{ ydb-short-name }}, always consider `UPSERT` as the main way to add data and only use other statements when absolutely necessary.

All statements that write data to {{ ydb-short-name }} support working with both subqueries and multiple entries passed directly in a query.

Let's add data to the previously created tables:

```yql
UPSERT INTO series (series_id, title, release_date, series_info)
VALUES
    (
        1,
        "IT Crowd",
        Date("2006-02-03"),
        "The IT Crowd is a British sitcom produced by Channel 4, written by Graham Linehan, produced by Ash Atalla and starring Chris O'Dowd, Richard Ayoade, Katherine Parkinson, and Matt Berry."),
    (
        2,
        "Silicon Valley",
        Date("2014-04-06"),
        "Silicon Valley is an American comedy television series created by Mike Judge, John Altschuler and Dave Krinsky. The series focuses on five young men who founded a startup company in Silicon Valley."
    )
    ;

UPSERT INTO seasons (series_id, season_id, title, first_aired, last_aired)
VALUES
    (1, 1, "Season 1", Date("2006-02-03"), Date("2006-03-03")),
    (1, 2, "Season 2", Date("2007-08-24"), Date("2007-09-28")),
    (2, 1, "Season 1", Date("2014-04-06"), Date("2014-06-01")),
    (2, 2, "Season 2", Date("2015-04-12"), Date("2015-06-14"))
;

UPSERT INTO episodes (series_id, season_id, episode_id, title, air_date)
VALUES
    (1, 1, 1, "Yesterday's Jam", Date("2006-02-03")),
    (1, 1, 2, "Calamity Jen", Date("2006-02-03")),
    (2, 1, 1, "Minimum Viable Product", Date("2014-04-06")),
    (2, 1, 2, "The Cap Table", Date("2014-04-13"))
;
```

To execute the script via the {{ ydb-short-name }} CLI, follow the instructions given under [Executing YQL queries in the {{ ydb-short-name }} CLI](#cli) in this article.

To learn more about commands for writing data, see the YQL reference:

* [INSERT](../../yql/reference/syntax/insert_into.md): Add logs.
* [REPLACE](../../yql/reference/syntax/replace_into.md): Add/update logs.
* [UPDATE](../../yql/reference/syntax/update.md): Update specified fields.
* [UPSERT](../../yql/reference/syntax/upsert_into.md): Add logs/update specified fields.

### SELECT : Data retrieval {#select}

Make a select of the data added in the previous step:

```sql
SELECT
    series_id,
    title AS series_title,
    release_date
FROM series;
```

or

```sql
SELECT * FROM episodes;
```

If there are several `SELECT` statements in the YQL script, its execution will return several samples, each of which can be accessed separately. Run the above `SELECT` statements as a single script.

To execute the script via the {{ ydb-short-name }} CLI, follow the instructions given under [Executing YQL queries in the {{ ydb-short-name }} CLI](#cli) in this article.

To learn more about the commands for selecting data, see the YQL reference:

* [SELECT](../../yql/reference/syntax/select.md): Select data.
* [SELECT ... JOIN](../../yql/reference/syntax/join.md): Join tables when selecting data.
* [SELECT ... GROUP BY](../../yql/reference/syntax/group_by.md): Group data when selecting it.

### Parameterized queries {#param}

Transactional applications working with a database are characterized by the execution of multiple similar queries that only differ in parameters. Like most databases, {{ ydb-short-name }} will work more efficiently if you define variable parameters and their types and then initiate the execution of a query by passing the parameter values separately from its text.

To define parameters in the text of a YQL query, use the [DECLARE](../../yql/reference/syntax/declare.md).

A description of the execution methods for the parametrized {{ ydb-short-name }} SDK queries is available in the [Test example](../../reference/ydb-sdk/example/index.md) section under Parametrized queries for the desired programming language.

When debugging a parameterized query in the {{ ydb-short-name }} SDK, you can test it by calling the {{ ydb-short-name }} CLI, copying the full text of the query without any edits, and setting parameter values.

Save the parameterized query script in a text file named `script.yql`:

```sql
DECLARE $seriesId AS Uint64;
DECLARE $seasonId AS Uint64;

SELECT sa.title AS season_title, sr.title AS series_title
FROM   seasons AS sa
INNER JOIN series AS sr ON sa.series_id = sr.series_id
WHERE sa.series_id = $seriesId AND sa.season_id = $seasonId;
```

To make a parameterized select query, make sure the prerequisites of the [Executing YQL scripts in the {{ ydb-short-name }} CLI](#cli) section of this article are met, then run:

```bash
{{ ydb-cli }} --profile db1 yql -f script.yql -p '$seriesId=1' -p '$seasonId=1'
```

For a full description of the ways to pass parameters, see the [{{ ydb-short-name }} CLI reference](../../reference/ydb-cli/index.md).

## YQL tutorial {#tutorial}

You can learn more about YQL use cases by completing tasks from the [YQL tutorial](../../yql/tutorial/index.md).

## Next step {#next}

Go to [YDB SDK - Getting started](../sdk.md).
