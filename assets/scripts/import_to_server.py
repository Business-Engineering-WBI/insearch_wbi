import json
import os

import pandas as pd
import pydantic
from base import (DEFAULT_CONNECTION_CONFIG_PATH, ArgsBase, import_connection_config, interp_model, log_info_to_cpp,
                  log_space_to_cpp, log_trace_to_cpp, log_warning_to_cpp, start_program)
from pg_shared import create_sqlalchemy_engine
from sqlalchemy import text
from sqlalchemy.engine import Connection, Engine

GEN_TOOLS_INSERT_FIELDS = ["model", "codem", "manuf", "fulldescription", "lcs", "moq", "interpmodel"]


class Args(ArgsBase):
    xlsx_path: str = pydantic.Field(description="Папка исходных файлов")
    # TODO: Add options to fail on existing records
    # TODO: Add options to remove previous images
    remove_previous_images: bool = pydantic.Field(description="Удалять предыдущие изображения", default=False)
    skip_files: str = pydantic.Field(description="Файлы для пропуска; Можно использовать ';' для нескольких файлов",
                                     default="")
    connection_config_path: str = pydantic.Field(description="Путь к файлу конфигурации подключения к БД",
                                                 default=DEFAULT_CONNECTION_CONFIG_PATH)


def get_boolean_columns(sqlalchemy_engine: Engine, table_name: str) -> set[str]:
    """Получает список boolean столбцов из таблицы"""
    with sqlalchemy_engine.begin() as connection:
        query = text("""
            SELECT column_name
            FROM information_schema.columns
            WHERE table_schema = 'tools'
            AND table_name = :table_name
            AND data_type = 'boolean'
        """)
        result = connection.execute(query, {'table_name': table_name})
        return {row[0] for row in result}


def convert_to_boolean(value):
    """Преобразует значение в boolean для PostgreSQL"""
    if pd.isna(value):
        return None
    if isinstance(value, (bool, type(None))):
        return value
    if isinstance(value, (int, float)):
        return bool(int(value))
    if isinstance(value, str):
        return value.lower() in ('true', '1', 'yes', 't', 'y')
    return bool(value)


def update_displayfields_map(constr_set: set[str], sqlalchemy_engine: Engine, displayfields_map: dict[str, list[str]]):
    for constr in constr_set:
        if displayfields_map.get(constr) is None:
            log_trace_to_cpp(f"Загружаются отображаемые поля для конструкции: {constr}")
            displayfields_map[constr] = []
            with sqlalchemy_engine.begin() as connection:
                query = text("SELECT displayfields FROM tools.gen_constructions WHERE constr = :constr")
                displayfields_db = connection.execute(query, {'constr': constr}).first()

                if displayfields_db is None:
                    raise ValueError(f"Не найдены отображаемые поля для конструкции: {constr}")
                log_trace_to_cpp(f"Полученные отображаемые поля: {displayfields_db[0]}")
                displayfields_map[constr] = list(displayfields_db[0])


def insert_into_gen_tools(sqlalchemy_engine: Engine, row: pd.Series) -> int:
    with sqlalchemy_engine.begin() as connection:
        columns = ', '.join(i for i in row.index if i in GEN_TOOLS_INSERT_FIELDS)
        values_placeholders = ', '.join(f":{i}" for i in row.index if i in GEN_TOOLS_INSERT_FIELDS)

        query = text(f"""
            INSERT INTO tools.gen_tools ({columns})
            VALUES ({values_placeholders})
            ON CONFLICT (model, manuf) DO UPDATE SET
                {', '.join(f"{col} = EXCLUDED.{col}" for col in row.index if col in GEN_TOOLS_INSERT_FIELDS)}
            RETURNING id;
        """)
        params = {col: row[col] for col in row.index if col in GEN_TOOLS_INSERT_FIELDS}
        params = {k: (None if pd.isna(v) else v) for k, v in params.items()}

        raw_tool_id = connection.execute(query, params).first()
        if raw_tool_id is None:
            raise ValueError(
                f"Не удалось вставить или обновить запись для модели: {row['model']}, производитель: {row['manuf']}")

        return raw_tool_id[0]


def insert_into_ctd_table(sqlalchemy_connection: Connection, constr: str, row: pd.Series, tool_id: int, item_id: int,
                          boolean_columns: set[str]):
    columns = ', '.join(row.index)
    values_placeholders = ', '.join(f":{col}" for col in row.index)
    query = text(f"""
        INSERT INTO tools.{constr} (tool_id, item_id, {columns})
        VALUES (:tool_id, :item_id, {values_placeholders})
        ON CONFLICT (tool_id) DO UPDATE SET
            {', '.join(f"{col} = EXCLUDED.{col}" for col in row.index)};
    """)
    params = {}
    for col in row.index:
        if col in boolean_columns:
            params[col] = convert_to_boolean(row[col])
        else:
            params[col] = row[col]
    params['tool_id'] = tool_id
    params['item_id'] = item_id
    params = {k: (None if pd.isna(v) else v) for k, v in params.items()}
    sqlalchemy_connection.execute(query, params)


def insert_into_gen_items(sqlalchemy_connection: Connection, row: pd.Series, tool_id: int,
                          json_attributes: dict) -> int:
    query = text("""
        INSERT INTO tools.gen_items (manuf, model, constr, json_attributes, tool_id)
        VALUES (:manuf, :model, :constr, :json_attributes, :tool_id)
        ON CONFLICT (manuf, model) DO UPDATE SET
            constr = EXCLUDED.constr,
            json_attributes = EXCLUDED.json_attributes,
            tool_id = EXCLUDED.tool_id
        RETURNING id;
    """)
    raw_item_id = sqlalchemy_connection.execute(
        query, {
            'manuf': row["manuf"],
            'model': row["model"],
            'constr': row["constr"],
            'json_attributes': json.dumps(json_attributes),
            'tool_id': tool_id,
        }).first()
    if raw_item_id is None:
        raise ValueError(
            f"Не удалось вставить или обновить запись для модели: {row['model']}, производитель: {row['manuf']}")
    return raw_item_id[0]


def insert_into_pic_tools(sqlalchemy_engine: Engine, row: pd.Series, tool_id: int):
    imgs_pic = str(row["img_pic"]).strip().split(";") if pd.notna(row["img_pic"]) else None
    imgs_drw = str(row["img_drw"]).strip().split(";") if pd.notna(row["img_drw"]) else None

    for img_list, img_type in [(imgs_pic, "picture"), (imgs_drw, "drawing")]:
        if img_list is None:
            continue
        for img_path in img_list:
            img_path = img_path.strip()
            if not img_path:
                continue

            with sqlalchemy_engine.begin() as connection:
                query_check_img = text("""
                    SELECT 1 FROM tools.pic_tools
                    WHERE tool_id = :tool_id AND typ = :typ AND link_to_image = :link_to_image;
                """)
                result = connection.execute(query_check_img, {
                    'tool_id': tool_id,
                    'typ': img_type,
                    'link_to_image': img_path,
                }).first()
                if result is not None:
                    log_warning_to_cpp(f"Изображение уже существует в базе: {img_path}, пропуск...")
                    continue

                query_insert_img = text("""
                    INSERT INTO tools.pic_tools (tool_id, typ, link_to_image, ext, model, manuf)
                    VALUES (:tool_id, :typ, :link_to_image, :ext, :model, :manuf)
                    RETURNING id;
                """)
                raw_img_id = connection.execute(
                    query_insert_img, {
                        'tool_id': tool_id,
                        'typ': img_type,
                        'img_path': img_path,
                        'link_to_image': img_path,
                        'ext': os.path.splitext(img_path)[1].lstrip('.'),
                        'model': row["model"],
                        'manuf': row["manuf"],
                    })

                if raw_img_id is None:
                    raise ValueError(
                        f"Не удалось вставить изображение для модели: {row['model']}, производитель: {row['manuf']}, путь: {img_path}"
                    )


def process_files(args: Args):
    log_info_to_cpp("Начало импорта на сервер")

    log_info_to_cpp("Подключение к базе данных")
    connection_config = import_connection_config(args.connection_config_path)
    sqlalchemy_engine = create_sqlalchemy_engine(connection_config.db_user, connection_config.db_password,
                                                 connection_config.db_host, connection_config.db_port)

    skip_files = args.skip_files.split(';') if args.skip_files else []
    log_info_to_cpp(f"Файлы для пропуска: {skip_files}")

    displayfields_map: dict[str, list[str]] = {}
    boolean_columns_map: dict[str, set[str]] = {}

    dtype_dict = {
        "model": str,
        "codem": str,
        "manuf": str,
        "constr": str,
        "fulldescription": str,
        "lcs": "Int64",
        "moq": float,
        "img_pic": str,
        "img_drw": str,
        "serie": str,
        "cle": str,
        "cs1": str,
    }

    for filename in os.scandir(args.xlsx_path):
        if filename.is_file() and not filename.name.startswith("~$") and filename.name not in skip_files:
            log_info_to_cpp(f"Импортируется файл: {filename.name}")

            df = pd.read_excel(filename.path, dtype=dtype_dict)
            df["interpmodel"] = df["model"].map(interp_model).fillna(df["model"])
            for req_col in ["img_pic", "img_drw", "constr"]:
                if req_col not in df.columns:
                    df[req_col] = pd.NA
            df = df.where(pd.notna(df), None)                                                                           # type: ignore[call-overload]

            constr_set = set(df["constr"].replace('', pd.NA).dropna().astype(str))
            constr_set = set(df["constr"].replace('', pd.NA).dropna().astype(str))
            log_info_to_cpp(f"Найденные конструкции в файле {filename.name}: {constr_set}")

            update_displayfields_map(constr_set, sqlalchemy_engine, displayfields_map)

            for constr in constr_set:
                if constr not in boolean_columns_map:
                    boolean_columns_map[constr] = get_boolean_columns(sqlalchemy_engine, constr)

            for _, row in df.iterrows():
                log_info_to_cpp(f"Обрабатывается строка: {row['manuf']} - {row['model']}")
                constr = row["constr"]
                displayfields = displayfields_map.get(constr)
                if (displayfields is None or len(displayfields) == 0) and constr is not None:
                    raise ValueError(f"Отсутствуют отображаемые поля для конструкции {constr}, файл {filename.name}")

                log_trace_to_cpp("Вставка/обновление в gen_tools")
                tool_id = insert_into_gen_tools(sqlalchemy_engine, row)

                if args.remove_previous_images:
                    log_trace_to_cpp("Удаление предыдущих изображений для инструмента")
                    with sqlalchemy_engine.begin() as connection:
                        query_delete_imgs = text("""
                            DELETE FROM tools.pic_tools WHERE tool_id = :tool_id;
                        """)
                        connection.execute(query_delete_imgs, {'tool_id': tool_id})

                log_trace_to_cpp("Вставка/обновление ссылок на изображения")
                insert_into_pic_tools(sqlalchemy_engine, row, tool_id)

                item_id = None
                if displayfields is None or len(displayfields) == 0 or constr is None:
                    log_warning_to_cpp(
                        "Пропуск вставки в таблицу конструкции и в таблицу gen_items из-за отсутствия конструкции")
                else:
                    with sqlalchemy_engine.begin() as connection:
                        log_trace_to_cpp("Вставка/обновление в gen_items")
                        json_attributes = {
                            field: row[field]
                            for field in displayfields
                            if field in row and pd.notna(row[field])
                        }

                        item_id = insert_into_gen_items(connection,
                                                        row,
                                                        tool_id=tool_id,
                                                        json_attributes=json_attributes)

                        log_trace_to_cpp(f"Вставка/обновление в {constr} таблицу")
                        ctd_row = row.drop(
                            ["codem", "moq", "fulldescription", "interpmodel", "img_pic", "img_drw", "constr"])
                        boolean_columns = boolean_columns_map.get(constr, set())
                        insert_into_ctd_table(connection, constr, ctd_row, tool_id, item_id, boolean_columns)

                log_info_to_cpp(f"Импорт на сервер завершен успешно tool_id: {tool_id}, item_id: {item_id }")
                log_space_to_cpp()


if __name__ == "__main__":
    start_program(process_files, Args)
