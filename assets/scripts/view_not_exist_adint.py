import os

import pandas as pd
import pydantic
from base import (DEFAULT_CONNECTION_CONFIG_PATH, ArgsBase, import_connection_config, log_info_to_cpp, log_space_to_cpp,
                  resolve_project_path, start_program)
from pg_shared import create_sqlalchemy_engine
from sqlalchemy import text


class Args(ArgsBase):
    project_path: str = pydantic.Field(description="Папка проекта")
    xlsx_path: str = pydantic.Field(description="Папка исходных файлов")
    # save_path: str = pydantic.Field(description="Путь для сохранения результата")
    connection_config_path: str = pydantic.Field(description="Путь к файлу конфигурации подключения к БД",
                                                 default=DEFAULT_CONNECTION_CONFIG_PATH)


def process_files(args: Args):
    args.project_path = os.path.abspath(args.project_path)
    args.xlsx_path = resolve_project_path(args.project_path, args.xlsx_path)

    log_info_to_cpp("Начало импорта несуществующих ADINT моделей")

    log_info_to_cpp("Подключение к базе данных")
    connection_config = import_connection_config(args.connection_config_path)
    sqlalchemy_engine = create_sqlalchemy_engine(connection_config.db_user, connection_config.db_password,
                                                 connection_config.db_host, connection_config.db_port)

    all_adint_set = set()

    for filename in os.scandir(args.xlsx_path):
        if filename.is_file() and not filename.name.startswith("~$"):
            log_info_to_cpp(f"Обработка файла {filename.name}")
            df = pd.read_excel(filename.path, dtype=str)

            for _, row in df.iterrows():
                for adint_col in ["adintms", "adintws"]:
                    adint = row.get(adint_col)
                    if pd.notna(adint):
                        all_adint_set.add(adint)

    log_info_to_cpp(f"Всего ADINT моделей в файлах: {len(all_adint_set)}")

    not_exist_adint_list = []
    with sqlalchemy_engine.connect() as connection:
        for adint in all_adint_set:
            query = text("""
                SELECT 1 from tools.gen_adint WHERE adint = :adint
            """)
            result = connection.execute(query, {'adint': adint})
            if result.first() is None:
                not_exist_adint_list.append(adint)
    log_info_to_cpp(f"Не существующих ADINT моделей: {len(not_exist_adint_list)}")
    log_space_to_cpp()
    for adint in not_exist_adint_list:
        log_info_to_cpp(f"  {adint}")
    log_space_to_cpp()
    # log_info_to_cpp("Сохранение результата")
    # os.makedirs(args.save_path, exist_ok=True)
    # with open(os.path.join(args.save_path, "not_exist_adint.txt"), "w", encoding="utf-8") as f:
    #     f.write("\n".join(not_exist_adint_list))


if __name__ == "__main__":
    start_program(process_files, Args)
