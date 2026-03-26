import copy
import os
from pathlib import Path
from functools import partial
import sys

import pandas as pd
from base import DEFAULT_YG1_SHOP_CONSTRUCTION_LEVEL_CONFIG_PATH, ArgsBase, Yg1ShopConstructionLevelConfigItemDict, import_yg1_shop_construction_level_config, log_error_to_cpp, log_info_to_cpp, remove_model_suffix, start_program
import pydantic

SECTION_1_FIELD = "Название раздела {ISECT1_NAME}"
SECTION_2_FIELD = "Название раздела {ISECT2_NAME}"
SECTION_3_FIELD = "Название раздела {ISECT3_NAME}"

MANUF_FIELD = "Производитель [MANUFACTURER] {IP_PROP668}"
CODEM_FIELD = "Артикул [CML2_ARTICLE] {IP_PROP380}"
NAME_FIELD = "Наименование элемента {IE_NAME}"
FULLDESCRIPTION_FIELD = "Наименование [NAIMENOVANIE] {IP_PROP666}"

FIELDS_MAP = {
    "Раздел 1": SECTION_1_FIELD,
    "Раздел 2": SECTION_2_FIELD,
    "Раздел 3": SECTION_3_FIELD,
    "manuf": MANUF_FIELD,
    "codem": CODEM_FIELD,
    "model": NAME_FIELD,
    "fulldescription": FULLDESCRIPTION_FIELD,
}


class Args(ArgsBase):
    xlsx_path: str = pydantic.Field(description="Папка исходных файлов")
    save_path: str = pydantic.Field(description="Папка для сохранения")
    img_prefix: str = pydantic.Field(description="Префикс для добавления к картинкам", default="")


def get_img_with_prefix(img_prefix: str, img_full_path: str):
    return os.path.join(img_prefix,
                        Path(img_full_path).parent.parent.parent.parent.name,
                        Path(img_full_path).parent.name,
                        Path(img_full_path).name).replace("\\", "/")


def process_row(img_prefix: str, constr_to_lvl: dict[str, Yg1ShopConstructionLevelConfigItemDict], row):
    drw = str(row["img_drw"]).strip()
    pic = str(row["img_pic"]).strip()

    is_drw_not_empty = drw and drw.lower() != "nan" and drw.strip() != ""
    is_pic_empty = not pic or pic.lower() == "nan" or pic.strip() == ""
    if is_drw_not_empty and is_pic_empty:
        img_list = [p.strip() for p in drw.split(";") if p.strip()]
        if img_list:
            first_img = img_list.pop(0)
            row["img_pic"] = first_img

            row["img_drw"] = ";".join(img_list)

    drw = str(row["img_drw"]).strip()
    is_drw_not_empty = drw and drw.lower() != "nan" and drw.strip() != ""
    if is_drw_not_empty:
        img_list = [p.strip() for p in drw.split(";") if p.strip()]
        row["img_drw"] = ";".join([get_img_with_prefix(img_prefix, x) for x in img_list])

    pic = str(row["img_pic"]).strip()
    is_pic_not_empty = pic and pic.lower() != "nan" and pic.strip() != ""
    if is_pic_not_empty:
        img_list = [p.strip() for p in pic.split(";") if p.strip()]
        row["img_pic"] = ";".join([get_img_with_prefix(img_prefix, x) for x in img_list])

    row[FIELDS_MAP["model"]] = remove_model_suffix(row[FIELDS_MAP["model"]])

    lvl = constr_to_lvl.get(row["constr"])
    if lvl is None:
        log_error_to_cpp(f"Не найдена информация для конструкции {row['constr']}")
        sys.exit(-1)

    if row["constr"] == "ctd_jse_m":
        lvl = copy.deepcopy(lvl)
        match row["thft"]:
            case "M60":
                lvl["l2"] = "Метчики метрические"
            case "UNC60":
                lvl["l2"] = "Дюймовые метчики"
                lvl["l3"] = "Дюймовый UNC"
            case "UNF60":
                lvl["l2"] = "Дюймовые метчики"
                lvl["l3"] = "Дюймовый UNF"
            case "NPT60":
                lvl["l2"] = "Дюймовые метчики"
                lvl["l3"] = "Дюймовый NPT"
            case _:
                raise ValueError("Need propper handling for ctd_jse_m for thft=" + str(row["thft"]))

    row[SECTION_1_FIELD] = lvl.get("l1")
    row[SECTION_2_FIELD] = lvl.get("l2")
    row[SECTION_3_FIELD] = lvl.get("l3")
    row["DOP_NAIMENOVANIE"] = lvl.get("dop")
    return row


def add_extra_info(args: Args):
    os.makedirs(args.save_path, exist_ok=True)

    constr_to_lvl = import_yg1_shop_construction_level_config(DEFAULT_YG1_SHOP_CONSTRUCTION_LEVEL_CONFIG_PATH)

    bound_process_row = partial(process_row, args.img_prefix, constr_to_lvl)

    all_dfs = []
    for filename in os.scandir(args.xlsx_path):
        if filename.is_file():
            if filename.name.startswith("~$"):
                continue

            log_info_to_cpp(filename.name)

            df = pd.read_excel(filename,
                               index_col=None,
                               engine="openpyxl",
                               dtype={
                                   'codem': str,
                                   'model': str,
                                   'fulldescription': str,
                                   'manuf': str
                               })
            for key, value in FIELDS_MAP.items():
                if key in df.columns:
                    df.rename(columns={key: value}, inplace=True)
            if SECTION_3_FIELD not in df.columns:
                df.insert(0, SECTION_3_FIELD, None)
            if SECTION_2_FIELD not in df.columns:
                df.insert(0, SECTION_2_FIELD, None)
            if SECTION_1_FIELD not in df.columns:
                df.insert(0, SECTION_1_FIELD, None)
            if "DOP_NAIMENOVANIE" not in df.columns:
                df.insert(3, "DOP_NAIMENOVANIE", None)
            df = df.apply(bound_process_row, axis=1)
            all_dfs.append(df)

    combined_df = pd.concat(all_dfs, ignore_index=True)
    combined_df.dropna(axis=1, how='all', inplace=True)

    writer = pd.ExcelWriter(os.path.join(args.save_path, "ALL.xlsx"), engine="xlsxwriter")                              # pylint: disable=abstract-class-instantiated
    combined_df.to_excel(writer, sheet_name="sm", freeze_panes=(1, 0), index=False)
    worksheet = writer.sheets["sm"]
    worksheet.autofit()
    writer.close()

    log_info_to_cpp("Все файлы обработаны успешно!")


if __name__ == "__main__":
    start_program(add_extra_info, Args)

# python excel_add_extra_info.py `
# --xlsx_path W:\Work\WBI\ToolinformProjects\WBI_Stock_2\data\excel\xlsx_add_info `
# --save_path W:\Work\WBI\ToolinformProjects\WBI_Stock_2\data\excel\xlsx_add_info-shop `

# python .\excel_add_extra_info-yg1-shop.py `
# --xlsx_path "W:\Work\WBI\yg1-shop_ru\from-Alena\2025-10-20\Excel" `
# --save_path "W:\Work\WBI\yg1-shop_ru\from-Alena\2025-10-20\ExcelForImport" `
# --img_prefix "http://194.113.153.157/nameduploads/YG1/AlenaImg/"
