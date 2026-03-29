import argparse
import json
import sys
import traceback
from typing import Callable, TypeVar, TypedDict

import cv2
import numpy as np
from pydantic import BaseModel as ArgsBase

DEFAULT_CONNECTION_CONFIG_PATH = "./assets/configs/shared_connection_config.json"
DEFAULT_YG1_SHOP_CONSTRUCTION_LEVEL_CONFIG_PATH = "./assets/configs/yg1-shop_construction_level.json"

utf8stdout = open(1, 'w', encoding='utf-8', closefd=False)


def log_space_to_cpp():
    print_to_cpp("")


def log_trace_to_cpp(string: str):
    print_to_cpp("[DEBUG]: " + str(string))


def log_info_to_cpp(string: str):
    print_to_cpp("[INFO]: " + str(string))


def log_warning_to_cpp(string: str):
    print_to_cpp("[WARN]: " + str(string))


def log_error_to_cpp(string: str):
    print_to_cpp("[ERROR]: " + str(string))


def log_error_trace_to_cpp(string: str):
    print_to_cpp("[ERROR TRACE]: " + str(string))


def print_to_cpp(string: str):
    print(string, file=utf8stdout, flush=True)


TArgs = TypeVar("TArgs", bound=ArgsBase)


def parse_args(args_type: type[TArgs]) -> TArgs:
    if (len(sys.argv) - 1) < len(args_type.model_fields.keys()):
        log_error_to_cpp("Скрипт не выполнен (не хватает агрументов)")
        sys.exit(-1)

    try:
        args_input = {}
        for i, key in enumerate(args_type.model_fields):
            args_input[key] = str(sys.argv[i + 1])
        return args_type(**args_input)
    except (ValueError, TypeError) as e:
        log_error_to_cpp(f"Аргументы скрипта имеют ошибки: {e}")
        sys.exit(-1)


def parse_args_add_model(parser, model: type[TArgs]):
    "Add Pydantic model to an ArgumentParser"
    fields = model.model_fields
    for name, field in fields.items():
        parser.add_argument(
            f"--{name}",
            dest=name,
            type=field.annotation,
            default=field.default,
            help=field.description,
        )


def parse_args_new(args_type: type[TArgs]) -> TArgs:
    try:
        parser = argparse.ArgumentParser()
        parse_args_add_model(parser, args_type)
        args = parser.parse_args()
        item = args_type(**vars(args))
        return item
    except (ValueError, TypeError, AttributeError, SystemExit) as e:
        # exc_type, exc_value, exc_traceback = sys.exc_info()
        formatted_traceback = traceback.format_exc()
        log_error_to_cpp(f"Аргументы скрипта имеют ошибки: {e}")
        log_error_trace_to_cpp(f"{formatted_traceback}")
        sys.exit(-1)


def start_program(program_function: Callable[[TArgs], None], args_type: type[TArgs]):
    try:
        program_function(parse_args_new(args_type))
    except KeyboardInterrupt:
        pass
    except Exception as e:                                                                                              # pylint: disable=broad-exception-caught
        formatted_traceback = traceback.format_exc()                                                                    # pylint: disable=invalid-name
        log_error_to_cpp(f"Ошибка во время выполнения скрипта: {e}")
        log_error_trace_to_cpp(f"{formatted_traceback}")
        sys.exit(-1)


def file_format_id(index: int) -> str:
    return f"{index:0>4}"


def file_format_img(index: int) -> str:
    return file_format_id(index) + ".png"


def read_cv_file(path: str):
    with open(path, "rb") as f:
        chunk = f.read()
        chunk_arr = np.frombuffer(chunk, dtype=np.uint8)
        return cv2.imdecode(chunk_arr, cv2.IMREAD_COLOR)


def remove_suffix(input_string, suffix):
    if suffix and input_string.endswith(suffix):
        return input_string[:-len(suffix)]
    return input_string


def remove_model_suffix(val) -> str:
    result = val
    for suf in ["_AMATI", "_ASKUP", "_DEREK", "_HT", "_PL", "_LIK", "_ТИЗ"]:
        result = remove_suffix(result, suf)

    return result


def interp_model(val) -> str:
    return (str(val).replace(" ", "").replace(" ", "").replace("-", "").replace(".", ",").replace("*", "").replace(
        "^",
        "").replace("\\", "").replace("/", "").upper().replace("К", "K").replace("Е", "E").replace("Н", "H").replace(
            "Х", "X").replace("В",
                              "B").replace("А",
                                           "A").replace("Р",
                                                        "P").replace("О",
                                                                     "O").replace("С",
                                                                                  "C").replace("М",
                                                                                               "M").replace("Т", "T"))


class ConnectionConfigItem(ArgsBase):
    name: str

    ssh_host: str
    ssh_port: int = 22
    ssh_user: str
    ssh_password: str

    db_host: str
    db_port: int = 5432
    db_user: str
    db_password: str

    server_imgs_path: str

    shop_ftp_host: str
    shop_ftp_port: int = 21
    shop_ftp_user: str
    shop_ftp_password: str
    shop_ftp_imgs_path: str


class ConnectionConfig(ArgsBase):
    configs: list[ConnectionConfigItem]
    current_config_name: str


def import_connection_config(config_path: str) -> ConnectionConfigItem:
    with open(config_path, "r", encoding="utf-8") as f:
        json_data = json.load(f)
        configs = ConnectionConfig(**json_data)
        for config in configs.configs:
            if config.name == configs.current_config_name:
                return config

    raise RuntimeError("Не найдена конфигурация подключения с именем " + configs.current_config_name)


class Yg1ShopConstructionLevelConfigItem(ArgsBase):
    dop: str
    l1: str
    l2: str | None = None
    l3: str | None = None


class Yg1ShopConstructionLevelConfigItemDict(TypedDict):
    dop: str
    l1: str
    l2: str | None
    l3: str | None


def import_yg1_shop_construction_level_config(config_path: str) -> dict[str, Yg1ShopConstructionLevelConfigItemDict]:
    with open(config_path, "r", encoding="utf-8") as f:
        json_data = json.load(f)
        result: dict[str, Yg1ShopConstructionLevelConfigItemDict] = {}
        for key, value in json_data.items():
            result[key] = Yg1ShopConstructionLevelConfigItem(**value).model_dump()                                      # type: ignore
        return result
