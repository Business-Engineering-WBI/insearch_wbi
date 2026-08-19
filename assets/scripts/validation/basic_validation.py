import re

import pandas as pd

validate_metric_threadsh = re.compile(r"^M(\d+(?:[.]\d+)?)[x](\d+(?:[.]\d+)?)$")
validate_unc_like_threadsh = re.compile(
    r"^(#?\d+|\d+(?:[ ]\d+\/\d+|\/\d+)?\"?) - (\d+(?:[.]\d+)?) (UNC|UNF|UNJC|UNJF|NPT|NPTF)$")
validate_pipe_threadsh = re.compile(r"^G (#?\d+|\d+(?:[ ]\d+\/\d+|\/\d+)?\"?)$")

validate_metric_regexes = [validate_metric_threadsh, validate_unc_like_threadsh, validate_pipe_threadsh]

ALLOWED_JSE_BSG_VALUES = [
    "DIN 2174",
    "DIN 2181",
    "DIN 2181-1",
    "DIN 2182",
    "DIN 2183",
    "DIN 2184-1",
    "DIN 352",
    "DIN 357",
    "DIN 371",
    "DIN 374",
    "DIN 375",
    "DIN 376",
    "DIN 40432",
    "DIN 40435",
    "DIN 5156",
    "DIN 5157",
    "DIN-ANSI",
    "ISO 529",
    "JIS",
    "NORM HELION",
    "NORM ILIX",
    "NORM VERGNANO",
    "ГОСТ 3266-81",
]


def normalize_and_validate_threadsh_fields(df: pd.DataFrame, page_id: int):
    if "threadsh" not in df.columns:
        return

    for row_idx in df.index:
        is_validation_required = df.at[row_idx,
                                       "constr"] in ["ctd_jse_m", "ctd_jfe_m", "ctd_jse_hand_set", "ctd_jse_hand_each"]

        threadsh = df.at[row_idx, "threadsh"]
        if pd.isna(threadsh) or threadsh is None:
            if is_validation_required:
                raise ValueError(f"Поле 'threadsh' на странице '{page_id}', строка '{row_idx}' не может быть пустым.")
            continue

        df.at[row_idx, "threadsh"] = str(threadsh).strip().replace(",", ".")
        threadsh = df.at[row_idx, "threadsh"]

        is_one_of_the_valid_formats = False
        for regex in validate_metric_regexes:
            if regex.match(str(threadsh)):
                is_one_of_the_valid_formats = True
                break

        if not is_one_of_the_valid_formats:
            raise ValueError(
                f"Недопустимый формат значения в поле 'threadsh' на странице '{page_id}', строка '{row_idx}': '{threadsh}'. Допустимые форматы:\n"
                "    M<nominal_shown>x<tp>\n"
                "    <nominal_shown> - <tpi> UNC/UNF/UNJC/UNJF/NPT/NPTF\n"
                "    G <nominal_shown>\n")


def validate_jse_bsg(df: pd.DataFrame, page_id: int):
    for row_idx in df.index:
        bsg = df.at[row_idx, "bsg"]
        if pd.isna(bsg) or bsg is None or df.at[row_idx, "constr"] != "ctd_jse_m":
            continue

        if bsg not in ALLOWED_JSE_BSG_VALUES:
            raise ValueError(
                f"Недопустимое значение в поле 'bsg' на странице '{page_id}', строка '{row_idx}': '{bsg}'. Допустимые значения:\n"
                f"    {'\n    '.join(ALLOWED_JSE_BSG_VALUES)}")


def validate_bsg(df: pd.DataFrame, page_id: int):
    for func in [validate_jse_bsg]:
        func(df, page_id)


def validate_basic_lengths(df: pd.DataFrame, page_id: int):
    for row_idx in df.index:
        oal = df.at[row_idx, "oal"] if "oal" in df.columns else None
        lu = df.at[row_idx, "lu"] if "lu" in df.columns else None
        thl = df.at[row_idx, "thl"] if "thl" in df.columns else None
        lcf = df.at[row_idx, "lcf"] if "lcf" in df.columns else None
        lf = df.at[row_idx, "lf"] if "lf" in df.columns else None

        for field_name, field_value in [("lu", lu), ("thl", thl), ("lcf", lcf), ("lf", lf)]:
            if pd.notna(oal) and oal is not None and pd.notna(field_value) and field_value is not None:
                if float(oal) < float(field_value):
                    raise ValueError(
                        f"Недопустимое значение в поле 'oal' или '{field_name}' на странице '{page_id}', строка '{row_idx}'. Значение 'oal' ({oal}) не может быть меньше значения '{field_name}' ({field_value})."
                    )
