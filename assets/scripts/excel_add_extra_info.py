import json
import os
from pathlib import Path

import numpy as np
import pandas as pd
import pydantic
from base import ArgsBase, log_info_to_cpp, log_warning_to_cpp, log_error_to_cpp, remove_model_suffix, start_program
from pydantic import BaseModel

# IMGS_FOLDER_SUFFIX_PRIORITY = ["_no_bg_webp_crop", "_webp_crop", "_no_bg_webp", "_no_bg", ""]
IMG_EXTENSIONS = ['.png', '.jpg', '.jpeg', '.webp', '.gif', '.bmp']


class Args(ArgsBase):
    xlsx_path: str = pydantic.Field(description="Папка исходных файлов")
    save_path: str = pydantic.Field(description="Папка для сохранения")
    rules_path: str = pydantic.Field(description="Путь к файлу с правилами")
    per_page_img_folder: str = pydantic.Field(description="Папка с картинками по странице")
    per_page_rule_img_folder: str = pydantic.Field(description="Папка с картинками по правилам")
    no_condition_img_folder: str = pydantic.Field(description="Папка с картинками без условий")
    extra_parser_type: str = pydantic.Field(default="", description="Выбор дополнительного обработчика полей")


class GlobalAddListObject(BaseModel):
    name: str
    value: int | float | str | bool
    ignore_warns: bool = False
    pos: int | None = None


class GlobalCalcListObject(BaseModel):
    name: str
    value: str
    ignore_warns: bool = False
    pos: int | None = None


class SimpleAddListValueObject(BaseModel):
    index: list[int]
    value: int | float | str | bool


class SimpleAddListObject(BaseModel):
    name: str
    values: list[SimpleAddListValueObject]
    ignore_warns: bool = False
    pos: int | None = None


class SimpleRenameListObject(BaseModel):
    index: list[int]
    src_name: str
    dst_name: str


class PerPageCalcListValueObject(BaseModel):
    index: list[int]
    exec: str


class PerPageCalcListObject(BaseModel):
    name: str
    values: list[PerPageCalcListValueObject]


class PerPageSimpleRuleImgListValueItemObject(BaseModel):
    cmp: str
    cmp_value: str
    img_filename_hash: str


class PerPageSimpleRuleImgListValueObject(BaseModel):
    index: list[int]
    list: list[PerPageSimpleRuleImgListValueItemObject]


class PerPageSimpleRuleImgListObject(BaseModel):
    name: str
    values: list[PerPageSimpleRuleImgListValueObject]


class ExtraInfoRules(BaseModel):
    global_add_list: list[GlobalAddListObject] | None = None
    global_calc_list: list[GlobalCalcListObject] | None = None
    simple_add_list: list[SimpleAddListObject] | None = None
    simple_rename_list: list[SimpleRenameListObject] | None = None
    constr_rename_list: dict[str, str] | None = None                                                                    # ConstrRenameList
    per_page_calc_list: list[PerPageCalcListObject] | None = None
    per_page_simple_rule_img_list: list[PerPageSimpleRuleImgListObject] | None = None
    per_constr_calc_list: dict[str, str] | None = None                                                                  # PerConstrCalcList


def get_simple_add_list(index: int, simple_add_list_values: list[SimpleAddListValueObject]):
    for val_obj in simple_add_list_values:
        is_found = index == val_obj.index if isinstance(val_obj.index, int) else index in val_obj.index
        if is_found:
            return val_obj.value

    return None


def get_simple_rename_list(index: int, simple_rename_list: list[SimpleRenameListObject]):
    result: dict[str, str] = {}

    for rename_list_obj in simple_rename_list:
        if index in rename_list_obj.index:
            result[rename_list_obj.src_name] = rename_list_obj.dst_name

    return result


def get_per_page_calc_list_result(index: int, df: pd.DataFrame, per_page_calc_list_item: PerPageCalcListObject):
    result: list[None | int | float | str] = []
    for val_obj in per_page_calc_list_item.values:
        if index in val_obj.index:
            for i in df.index:
                exec(val_obj.exec, {"df": df, "i": i, "result": result, "pd": pd})                                      # pylint: disable=exec-used
            return result

    return None


def get_glob_calc_list_result(df: pd.DataFrame, glob_calc_list_item: GlobalCalcListObject):
    result: list[None | int | float | str] = []
    for i in df.index:
        exec(glob_calc_list_item.value, {"df": df, "i": i, "result": result, "pd": pd})                                 # pylint: disable=exec-used
    return result


def get_per_page_simple_rule_img_list_result(index: int, df: pd.DataFrame,
                                             per_page_simple_rule_img_list_item: PerPageSimpleRuleImgListObject):

    result: list[None | str] = []
    for val_obj in per_page_simple_rule_img_list_item.values:
        if index in val_obj.index:
            for i in df.index:
                res = None
                for rule in val_obj.list:
                    if df[per_page_simple_rule_img_list_item.name][i] == rule.cmp_value:
                        res = rule.img_filename_hash
                        break
                result.append(res)
            return result

    return None


# TODO: Create per field check (like check by serie)


def get_img_with_prefix(img_full_path: str, extra_parser_type: str):
    if extra_parser_type is None:
        return img_full_path.replace("\\", "/")

    if extra_parser_type == "yg1-shop":
        return os.path.join("http://194.113.153.157/nameduploads/",
                            Path(img_full_path).parent.parent.parent.parent.name,
                            Path(img_full_path).parent.name,
                            Path(img_full_path).name).replace("\\", "/")

    return img_full_path.replace("\\", "/")


def get_col_index(df: pd.DataFrame, col: str) -> int:
    loc = df.columns.get_loc(col)
    if isinstance(loc, (int, np.integer)):
        return int(loc)
    if isinstance(loc, slice):
        return int(loc.start)
    if isinstance(loc, (list, np.ndarray)):
        return int(loc[0])

    raise TypeError(f"Неизвестный тип результата: {type(loc)}")


def handle_glob_add_list(df: pd.DataFrame, page_id: int, global_add_list: list[GlobalAddListObject] | None):
    if global_add_list is None:
        return

    for glob_add_list_obj in global_add_list:
        to_add_value = glob_add_list_obj.value
        if to_add_value is None:
            log_warning_to_cpp(f"Для столбца '{glob_add_list_obj.name}' на странице '{page_id}' не найдено значение")
            continue

        pos: None | int = None
        if glob_add_list_obj.name in df.columns:
            # log_warning_to_cpp(f"Столбец '{glob_add_list_obj.name}' уже есть на странице '{page_id}'")
            pos = get_col_index(df, glob_add_list_obj.name)
            df.drop(glob_add_list_obj.name, axis=1, inplace=True)

        if pos is None:
            pos = glob_add_list_obj.pos if glob_add_list_obj.pos is not None else len(df.columns)
        df.insert(pos, glob_add_list_obj.name, to_add_value)


def handle_glob_calc_list(df: pd.DataFrame, page_id: int, global_calc_list: list[GlobalCalcListObject] | None):
    if global_calc_list is None:
        return

    for glob_calc_list_obj in global_calc_list:
        to_add_value = get_glob_calc_list_result(df, glob_calc_list_obj)
        if to_add_value is None or len(to_add_value) == 0:
            log_warning_to_cpp(f"Для столбца '{glob_calc_list_obj.name}' на странице '{page_id}' не найдено значение")
            continue

        pos: None | int = None
        if glob_calc_list_obj.name in df.columns:
            # log_warning_to_cpp(f"Столбец '{glob_calc_list_obj.name}' уже есть на странице '{page_id}'")
            pos = get_col_index(df, glob_calc_list_obj.name)
            df.drop(glob_calc_list_obj.name, axis=1, inplace=True)

        if pos is None:
            pos = glob_calc_list_obj.pos if glob_calc_list_obj.pos is not None else len(df.columns)
        df.insert(pos, glob_calc_list_obj.name, to_add_value)


def handle_simple_add_list(df: pd.DataFrame, page_id: int, simple_add_list: list[SimpleAddListObject] | None):
    if simple_add_list is None:
        return

    for simple_add_list_obj in simple_add_list:
        to_add_value = get_simple_add_list(page_id, simple_add_list_obj.values)
        if to_add_value is None or len(to_add_value) == 0:
            # if not simple_add_list_obj.ignore_warns:
            #     log_warning_to_cpp(f"Для столбца '{simple_add_list_obj.name}' на странице '{page_id}' не найдено значение")
            continue
        pos: None | int = None
        if simple_add_list_obj.name in df.columns:
            # log_warning_to_cpp(f"Столбец '{simple_add_list_obj.name}' уже есть на странице '{page_id}'")
            pos = get_col_index(df, simple_add_list_obj.name)
            df.drop(simple_add_list_obj.name, axis=1, inplace=True)

        if pos is None:
            pos = simple_add_list_obj.pos if simple_add_list_obj.pos is not None else len(df.columns)
        df.insert(pos, simple_add_list_obj.name, to_add_value)


def handle_per_page_calc_list(df: pd.DataFrame, page_id: int, per_page_calc_list: list[PerPageCalcListObject] | None):
    if per_page_calc_list is None:
        return

    for per_page_calc_list_obj in per_page_calc_list:
        calc_to_add_value = get_per_page_calc_list_result(page_id, df, per_page_calc_list_obj)

        if calc_to_add_value is not None:
            if not per_page_calc_list_obj.name in df.columns:
                # log_warning_to_cpp(f"Столбец '{per_page_calc_list_obj.name}' уже есть на странице '{page_id}'")
                # df.drop(per_page_calc_list_obj.name, axis=1, inplace=True)
                df.insert(len(df.columns), per_page_calc_list_obj.name, None)
            for i in df.index:
                if calc_to_add_value[i] is not None:
                    df.at[i, per_page_calc_list_obj.name] = calc_to_add_value[i]


def find_img_file_with_any_extension(base_path: str, base_name: str) -> str | None:
    for ext in IMG_EXTENSIONS:
        candidate = os.path.join(base_path, f"{base_name}{ext}")
        if os.path.exists(candidate):
            return candidate
    return None


def handle_no_condition_img_list_row(row: str, no_condition_img_folder: str, extra_parser_type: str):
    if pd.isna(row) or row is None or row == "":
        return None

    img_full_path = os.path.join(no_condition_img_folder, row)
    if os.path.isfile(img_full_path):
        return get_img_with_prefix(img_full_path, extra_parser_type)

    log_warning_to_cpp(f"Изображение не найдено в папке без условий: '{img_full_path}'")
    return None


def handle_no_condition_img_list(df: pd.DataFrame, no_condition_img_folder: str, extra_parser_type: str):
    for col in ["img_pic", "img_drw"]:
        if col not in df.columns:
            continue
        df[col] = df[col].map(
            lambda row: handle_no_condition_img_list_row(row, no_condition_img_folder, extra_parser_type))


def handle_per_page_img_folder(df: pd.DataFrame, xlsx_path_name: str, per_page_img_folder: str, extra_parser_type: str):
    img_pic = find_img_file_with_any_extension(per_page_img_folder, f"{Path(xlsx_path_name).stem}_pic")
    img_drw = find_img_file_with_any_extension(per_page_img_folder, f"{Path(xlsx_path_name).stem}_drw")

    new_img_pic = get_img_with_prefix(img_pic, extra_parser_type) if img_pic is not None else None
    if new_img_pic is not None:
        df["img_pic"] = new_img_pic

    new_img_drw = get_img_with_prefix(img_drw, extra_parser_type) if img_drw is not None else None
    if new_img_drw is not None:
        df["img_drw"] = new_img_drw


def handle_per_page_simple_rule_img_list(df: pd.DataFrame, page_id: int, per_page_rule_img_folder: str,
                                         extra_parser_type: str,
                                         per_page_simple_rule_img_list: list[PerPageSimpleRuleImgListObject] | None):
    if per_page_simple_rule_img_list is None:
        return

    for per_page_simple_rule_img_list_object in per_page_simple_rule_img_list:
        calc_to_add_value = get_per_page_simple_rule_img_list_result(page_id, df, per_page_simple_rule_img_list_object)

        if calc_to_add_value is not None:
            for i in df.index:
                if calc_to_add_value[i] is not None:
                    img_pic = find_img_file_with_any_extension(per_page_rule_img_folder, f"{calc_to_add_value[i]}_pic")
                    img_drw = find_img_file_with_any_extension(per_page_rule_img_folder, f"{calc_to_add_value[i]}_drw")
                    if img_pic is not None:
                        df.at[i, "img_pic"] = get_img_with_prefix(img_pic, extra_parser_type)
                    if img_drw is not None:
                        df.at[i, "img_drw"] = get_img_with_prefix(img_drw, extra_parser_type)


def normalize_and_validate_app_fields(df: pd.DataFrame, page_id: int):
    allowed_order = "PMKNSHO"
    allowed_set = set(allowed_order)
    allowed_pos = {ch: i for i, ch in enumerate(allowed_order)}

    for col in ["app1", "app2"]:
        if col not in df.columns:
            continue

        for row_idx in df.index:
            value = df.at[row_idx, col]
            if pd.isna(value) or value is None:
                continue

            value_str = str(value).strip()
            if value_str == "":
                df.at[row_idx, col] = ""
                continue

            upper_value = value_str.upper()
            invalid_chars = sorted(set(ch for ch in upper_value if ch not in allowed_set))
            if invalid_chars:
                raise ValueError(f"Недопустимые символы в поле '{col}' на странице '{page_id}', строка '{row_idx}': "
                                 f"'{value_str}'. Допустимы только символы из '{allowed_order}'.")

            normalized_value = "".join(sorted(upper_value, key=lambda ch: allowed_pos[ch]))
            df.at[row_idx, col] = normalized_value

    if "app1" in df.columns and "app2" in df.columns:
        for row_idx in df.index:
            app1_value = df.at[row_idx, "app1"]
            app2_value = df.at[row_idx, "app2"]

            if not pd.isna(app1_value) and not pd.isna(app2_value):
                app1_set = set(str(app1_value))
                app2_set = set(str(app2_value))
                if app1_set & app2_set:
                    raise ValueError(
                        f"Пересечение символов между 'app1' и 'app2' в строке '{row_idx}' на странице '{page_id}'. "
                        f"Значения: 'app1'='{app1_value}', 'app2'='{app2_value}'.")


def normalize_string_cells_symbols(df: pd.DataFrame):
    for col in df.columns:
        if not (pd.api.types.is_string_dtype(df[col]) or df[col].dtype == object):
            continue

        def replace_symbols(value: object):
            if isinstance(value, str):
                return value.replace("…", "...").replace("–", "-")
            return value

        df[col] = df[col].map(replace_symbols)


def run_post_transform_checks(df: pd.DataFrame, page_id: int):
    normalize_string_cells_symbols(df)
    normalize_and_validate_app_fields(df, page_id)


# def select_img_folder_with_suffix(base_folder: str) -> str:
#     for suffix in IMGS_FOLDER_SUFFIX_PRIORITY:
#         candidate_folder = (base_folder.removesuffix("/") if base_folder.endswith("/") else base_folder) + suffix
#         if os.path.exists(candidate_folder) and os.path.isdir(candidate_folder):
#             return candidate_folder

#     return base_folder


def df_remove_model_suffix(df: pd.DataFrame):
    if "model" in df.columns:
        df["model"] = df["model"].map(lambda x: remove_model_suffix(x) if pd.notna(x) else x)


# NOTE: rows_count = len(df.index)
# NOTE: cols_count = len(df.columns)
# NOTE: cols_names = list(df.columns)
def add_extra_info_single(xlsx_path: Path, page_id: int, save_path: str, extra_info_rules: ExtraInfoRules,
                          per_page_img_folder: str, per_page_rule_img_folder: str, no_condition_img_folder: str,
                          extra_parser_type: str):
    dtype_dict: dict[str, str | type] = {
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

    df = pd.read_excel(xlsx_path, index_col=None, engine="openpyxl", dtype=dtype_dict)

    df_remove_model_suffix(df)
    handle_glob_add_list(df, page_id, extra_info_rules.global_add_list)
    df_remove_model_suffix(df)
    handle_glob_calc_list(df, page_id, extra_info_rules.global_calc_list)

    df_remove_model_suffix(df)
    handle_simple_add_list(df, page_id, extra_info_rules.simple_add_list)

    # NOTE: Simple rename list
    # df.rename(columns=get_simple_rename_list(page_id, extra_info_rules.simple_rename_list), inplace=True)

    df_remove_model_suffix(df)
    handle_per_page_calc_list(df, page_id, extra_info_rules.per_page_calc_list)

    # ? TODO: Handle per_constr_calc_list

    df_remove_model_suffix(df)
    handle_no_condition_img_list(df, no_condition_img_folder, extra_parser_type)
    handle_per_page_img_folder(df, xlsx_path.name, per_page_img_folder, extra_parser_type)
    handle_per_page_simple_rule_img_list(df, page_id, per_page_rule_img_folder, extra_parser_type,
                                         extra_info_rules.per_page_simple_rule_img_list)

    run_post_transform_checks(df, page_id)

    # ! TODO: Add remove list
    # NOTE: Tmp remove list here:
    for col in ["Раздел 1", "Раздел 2", "Раздел 3"]:
        if col in df.columns:
            df.drop(col, axis=1, inplace=True)

    writer = pd.ExcelWriter(os.path.join(save_path, xlsx_path.name), engine="xlsxwriter")                               # pylint: disable=abstract-class-instantiated
    df.to_excel(writer, sheet_name="sm", freeze_panes=(1, 0), index=False)
    worksheet = writer.sheets["sm"]
    worksheet.autofit()
    writer.close()


def add_extra_info(args: Args):
    extra_info_rules: ExtraInfoRules | None = None
    log_info_to_cpp(f"Открытие файла с правилами: '{args.rules_path}'")
    with open(args.rules_path, encoding="utf-8") as conf_file:
        json_data = json.load(conf_file)
        if json_data is not None:
            extra_info_rules = ExtraInfoRules(**json_data)
        else:
            extra_info_rules = ExtraInfoRules()

    if extra_info_rules is None:
        log_error_to_cpp(f"Не удалось открыть файл с правилами: '{args.rules_path}'")
        return

    os.makedirs(args.save_path, exist_ok=True)

    xlsx_path = Path(args.xlsx_path)
    filenames = sorted(filter(lambda f: f.is_file() and not f.name.startswith("~$"), list(xlsx_path.rglob("*.xlsx"))))

    for i, filename in enumerate(filenames):
        log_info_to_cpp(f"Обработка файла: {filename.name}")
        add_extra_info_single(filename, i, args.save_path, extra_info_rules, args.per_page_img_folder,
                              args.per_page_rule_img_folder, args.no_condition_img_folder, args.extra_parser_type)
        log_info_to_cpp("Файл обработан успешно!")

    log_info_to_cpp("Все файлы обработаны успешно!")


if __name__ == "__main__":
    start_program(add_extra_info, Args)

# * python add_extra_info_drill.py C:/Coding/works/wbi/amati_drill/xlsx_fix_values/ C:/Coding/works/wbi/amati_drill/xlsx_add_info/

# python excel_add_extra_info.py W:\Work\WBI\ToolinformProjects\WBI_Stock_2\data\excel\startup `
# W:\Work\WBI\ToolinformProjects\WBI_Stock_2\data\excel\xlsx_add_info `
# W:\Work\WBI\ToolinformProjects\WBI_Stock_2\data\excel\extra_info.json `
# W:\Work\WBI\ToolinformProjects\WBI_Stock_2\data\excel\img_raw `
# W:\Work\WBI\ToolinformProjects\WBI_Stock_2\data\excel\img_simple
