import copy
import os
from pathlib import Path
from functools import partial
import sys

import pandas as pd
from base import ArgsBase, log_error_to_cpp, log_info_to_cpp, remove_model_suffix, start_program
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

constr_to_lvl = {
    "ctd_jse_m": {
        "dop": "Метчик цельный машинный",
        "l1": "Метчики",
    },
    "ctd_jfe_m": {
        "dop": "Раскатник резьбовой",
        "l1": "Метчики",
        "l2": "Раскатники",
    },
    "ctd_arbm_pullstud": {
        "dop": "Штревель",
        "l1": "Оснастка",
        "l2": "Штревели",
    },
    "ctd_collet_spring": {
        "dop": "Цанга с конической образующей для цилиндрических хвостовиков",
        "l1": "Оснастка",
        "l2": "Цанги",
    },
    "ctd_arb_extension": {
        "dop": "Удлинитель-переходник",
        "l1": "Оснастка",
        "l2": "Цанговые удлинители",
    },
    "ctd_arbm_hydroholdchuck": {
        "dop": "Патрон фрезерный гидропластовый",
        "l1": "Оснастка",
        "l2": "Фрезерные патроны и оправки",
    },
    "ctd_arbm_powerchuck": {
        "dop": "Патрон фрезерный силовой",
        "l1": "Оснастка",
        "l2": "Фрезерные патроны и оправки",
    },
    "ctd_arbm_weldonchuck": {
        "dop": "Патрон фрезерный с прямым закреплением винтом",
        "l1": "Оснастка",
        "l2": "Фрезерные патроны и оправки",
    },
    "ctd_arbm_colletchuck": {
        "dop": "Патрон фрезерный цанговый",
        "l1": "Оснастка",
        "l2": "Фрезерные патроны и оправки",
    },
    "ctd_arbm_shellchuck": {
        "dop": "Оправка фрезерная для насадных фрез",
        "l1": "Оснастка",
        "l2": "Фрезерные патроны и оправки",
    },
    "ctd_arbm_shrinkchuck": {
        "dop": "Термопатрон фрезерный",
        "l1": "Оснастка",
        "l2": "Фрезерные патроны и оправки",
    },
    "ctd_collet_round": {
        "dop": "Цанга цилиндрическая",
        "l1": "Оснастка",
        "l2": "Фрезерные патроны и оправки",
    },
    "ctd_sha1": {
        "dop": "Хвостовик для ввинчиваемого инструмента",
        "l1": "Оснастка",
        "l2": "Хвостовики для модульного инструмента",
    },
    "ctd_tavs_arbor": {
        "dop": "Антивибрационная оправка",
        "l1": "Оснастка",
        "l2": "Токарные оправки",
    },
    "ctd_me1": {
        "dop": "Фреза цельная с плоским торцом",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Фрезы с плоским торцом",
    },
    "ctd_me2": {
        "dop": "Фреза цельная с плоским торцом и с обнижением",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Фрезы с плоским торцом",
    },
    "ctd_mr1": {
        "dop": "Фреза цельная с радиусом",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Фрезы с плоским торцом",
    },
    "ctd_mr2": {
        "dop": "Фреза цельная с радиусом и обнижением",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Фрезы с плоским торцом",
    },
    "ctd_mb1": {
        "dop": "Фреза цельная полусферическая",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Монолитные полусферические фрезы",
    },
    "ctd_mb2": {
        "dop": "Фреза цельная полусферическая с обнижением",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Монолитные полусферические фрезы",
    },
    "ctd_met": {
        "dop": "Фреза цельная для обработки канавок",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Фрезы для профильной обработки",
    },
    "ctd_ma1": {
        "dop": "Фреза цельная для обработки фасок",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Фрезы для обработки фасок",
    },
    "ctd_ma3": {
        "dop": "Фреза цельная для обработки фасок и галтелью",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Фрезы для обработки фасок",
    },
    "ctd_me5": {
        "dop": "Фреза насадная цельная",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Фрезы насадные",
    },
    "ctd_mt1": {
        "dop": "Фреза цельная резьбовая",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Резьбофрезы монолитные",
    },
    "ctd_mt3": {
        "dop": "Фреза цельная резьбовая \"орбитальная\"",
        "l1": "Фрезы",
        "l2": "Фрезы монолитные",
        "l3": "Резьбофрезы монолитные",
    },
    "ctd_mh1": {
        "dop": "Головка фрезерная с плоским торцом",
        "l1": "Фрезы",
        "l2": "Фрезерные головки",
        "l3": "Головки фрезерные с плоским торцом",
    },
    "ctd_mhr": {
        "dop": "Головка фрезерная с радиусом",
        "l1": "Фрезы",
        "l2": "Фрезерные головки",
        "l3": "Головки фрезерные с радиусом",
    },
    "ctd_mhb": {
        "dop": "Головка фрезерная полусферическая",
        "l1": "Фрезы",
        "l2": "Фрезерные головки",
        "l3": "Головки фрезерные полусферические",
    },
    "ctd_mhc1": {
        "dop": "Головка фрезерная с рифлями и фаской",
        "l1": "Фрезы",
        "l2": "Фрезерные головки",
        "l3": "Головки фрезерные с плоским торцом",
    },
    "ctd_mib_hf": {
        "dop": "Фреза для высоких подач насадная",
        "l1": "Фрезы",
        "l2": "Фрезы сборные",
        "l3": "Фрезы сборные высокоподачные",
    },
    "ctd_mie_hf": {
        "dop": "Фреза для высоких подач концевая",
        "l1": "Фрезы",
        "l2": "Фрезы сборные",
        "l3": "Фрезы сборные высокоподачные",
    },
    "ctd_mih_90": {
        "dop": "Фреза для обработки уступов ввинчиваемая",
        "l1": "Фрезы",
        "l2": "Фрезы сборные",
        "l3": "Фрезерные головки с пластинами",
    },
    "ctd_mih_hf": {
        "dop": "Фреза сборная для высоких подач ввинчиваемая",
        "l1": "Фрезы",
        "l2": "Фрезы сборные",
        "l3": "Фрезерные головки с пластинами",
    },
    "ctd_ds1": {
        "dop": "Сверло цельное спиральное",
        "l1": "Сверла",
        "l2": "Сверла монолитные",
    },
    "ctd_ds4": {
        "dop": "Сверло цельное спиральное с коническим хвостовиком",
        "l1": "Сверла",
        "l2": "Сверла монолитные",
    },
    "ctd_ds5": {
        "dop": "Сверло центровочное",
        "l1": "Сверла",
        "l2": "Сверла монолитные",
    },
    "ctd_die_2": {
        "dop": "Сверло сборное с двумя пластинами",
        "l1": "Сверла",
        "l2": "Сверла сборные",
        "l3": "Корпуса сверл с СМП",
    },
    "ctd_inserts_turning": {
        "dop": "Токарная пластина",
        "l1": "Пластины",
        "l2": "Токарные пластины",
    },
    "ctd_inserts_milling": {
        "dop": "Фрезерная пластина",
        "l1": "Пластины",
        "l2": "Фрезерные пластины",
    },
    "ctd_inserts_threading": {
        "dop": "Резьбовая пластина",
        "l1": "Пластины",
        "l2": "Резьбовые пластины",
    },
    "ctd_inserts_grooving": {
        "dop": "Пластина канавочная",
        "l1": "Пластины",
        "l2": "Токарные пластины канавочные",
    },
    "ctd_the": {
        "dop": "Державка наружная токарная для пластин ISO",
        "l1": "Резцы",
        "l2": "Резцы сборные",
        "l3": "Токарные державки с СМП",
    },
    "ctd_tsmb_turn": {
        "dop": "Вставка цельная для растачивания",
        "l1": "Резцы",
        "l2": "Микрорасточные цельные вставки",
        "l3": "Вставки расточные",
    },
    "ctd_rm1": {
        "dop": "Развертка цельная машинная концевая",
        "l1": "Развертки",
    },
    "ctd_arb_bush": {
        "dop": "Втулка переходная",
        "l1": "Оснастка",
        "l2": "Втулка переходная"
    },
    "ctd_bs_carti_indexable": {
        "dop": "Нерегулируемый расточной сборный резец",
        "l1": "Резцы",
        "l2": "Резцы сборные",
        "l3": "Расточные системы",
    },
    "ctd_bs_head": {
        "dop": "Расточная головка",
        "l1": "Резцы",
        "l2": "Резцы сборные",
        "l3": "Регулируемые головки",
    },
    "ctd_tkoi_holder": {
        "dop": "Державка расточная",
        "l1": "Резцы",
        "l2": "Резцы сборные",
        "l3": "Державки для насадок-пластин",
    },
    "ctd_dli_mb2" : {
        "dop": "Фреза сферическая",
        "l1": "Инструменты для зуботехнических лабораторий",
        "l2": "Сферические фрезы",
    },
    "ctd_dli_me2": {
        "dop": "Фреза c плоским торцом",
        "l1": "Инструменты для зуботехнических лабораторий",
        "l2": "Фрезы с плоским торцом",
    },
    "ctd_dli_mr2": {
        "dop": "Фреза с радиусом",
        "l1": "Инструменты для зуботехнических лабораторий",
        "l2": "Фрезы с радиусом",
    },
    "ctd_dli_mt3": {
        "dop": "Резьбофреза",
        "l1": "Инструменты для зуботехнических лабораторий",
        "l2": "Резьбофрезы",
    },
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


def process_row(img_prefix: str, row):
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
        log_error_to_cpp(f"Не найдена информация для конструкции {row["constr"]}")
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

    bound_process_row = partial(process_row, args.img_prefix)

    all_dfs = []
    for filename in os.scandir(args.xlsx_path):
        if filename.is_file():
            if (filename.name.startswith("~$")):
                continue

            log_info_to_cpp(filename.name)

            df = pd.read_excel(filename, index_col=None, engine="openpyxl", dtype={'codem': str, 'model': str, 'fulldescription': str, 'manuf': str})
            for key, value in FIELDS_MAP.items():
                if key in df.columns:
                    df.rename(columns={key: value}, inplace=True)
            if SECTION_3_FIELD not in df.columns: df.insert(0, SECTION_3_FIELD, None)
            if SECTION_2_FIELD not in df.columns: df.insert(0, SECTION_2_FIELD, None)
            if SECTION_1_FIELD not in df.columns: df.insert(0, SECTION_1_FIELD, None)
            if "DOP_NAIMENOVANIE" not in df.columns: df.insert(3, "DOP_NAIMENOVANIE", None)
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
