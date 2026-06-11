# exclude_test_data 0.png,1.png,102.png,2.png,3.png,4.png,5.png
import json
import os
import pathlib
import sys
import time
from concurrent.futures import Future, ThreadPoolExecutor

from base import (ArgsBase, file_format_img, parse_args, print_to_cpp, read_cv_file)
from img2table.document import Image                                                                                    # type: ignore
from img2table.ocr import EasyOCR                                                                                       # type: ignore
from psutil import cpu_count                                                                                            # type: ignore


class Args(ArgsBase):
    in_img_path: str
    config_filename: str
    save_path: str
    min_confidence: int


default_dir = pathlib.Path(__file__).parent.resolve()

print_to_cpp(f"Текущая директория: {default_dir}")

threads_to_use = max(1, cpu_count() // 4 - 1)
# ocr = TesseractOCR(
#     n_threads=threads_to_use,
#     lang="eng+rus",
#     psm=6,
#     #    tessdata_dir=os.path.join(default_dir, "tessdata"),
# )

ocr = EasyOCR()

# ocr = PaddleOCR()


def extract_from_img(img_path: os.DirEntry[str], save_path: str, min_confidence: int):
    print_to_cpp(f"Обрабатывается файл: {img_path.name}")

    img = Image(src=img_path.path)

    try:
        img.to_xlsx(os.path.join(save_path, f"{img_path.name.split('.')[0]}.xlsx"),
                    ocr=ocr,
                    borderless_tables=True,
                    min_confidence=min_confidence)
    except Exception as e:
        print_to_cpp("Error: " + str(e))
        return False

    return True


def extract_tables_to_xlsx(args: Args):
    result: list[str] = []

    print_to_cpp(f"Изображения: {args.in_img_path}")
    print_to_cpp(f"Конфигурация: {args.config_filename}")
    print_to_cpp(f"Сохранение: {args.save_path}")
    print_to_cpp(f"Минимальная уверенность: {args.min_confidence}")

    with open(args.config_filename, encoding="utf-8") as conf_file:
        config = json.load(conf_file)

    generated_catalog_exclude_pages = config.get("GeneratedCatalogExcludePages", [])

    exclude_files: list[str] = [file_format_img(x) for x in generated_catalog_exclude_pages]

    os.makedirs(args.save_path, exist_ok=True)

    # for filename in os.scandir(args.in_img_path):
    #     if filename.is_file():
    #         if filename.name in exclude_files:
    #             print_to_cpp(f"Пропускается файл: {filename.name}")
    #             continue
    #         if (not extract_from_img(filename, args.save_path, args.min_confidence)):
    #             print_to_cpp(f"Не удалось обработать: {filename.name}")
    #             result.append(filename.name.split(".")[0])

    # print_to_cpp("Не удалось обработать: " + " ".join(result))

    parsed_filenames: list[str] = []
    parsed_futures: list[Future] = []
    with ThreadPoolExecutor(max_workers=threads_to_use) as e:
        for filename in os.scandir(args.in_img_path):
            if filename.is_file():
                if filename.name in exclude_files:
                    print_to_cpp(f"Пропускается файл: {filename.name}")
                    continue
                parsed_filenames.append(filename.name)
                parsed_futures.append(e.submit(extract_from_img, filename, args.save_path, args.min_confidence))
                # if (not extract_from_img(filename, args.save_path, args.min_confidence)):
                #     print_to_cpp(f"Не удалось обработать: {filename.name}")
                #     result.append(filename.name.split(".")[0])

    for i, (res_future, res_filename) in enumerate(zip(parsed_futures, parsed_filenames)):
        if not res_future.result():
            result.append(res_filename.split(".")[0])

    print_to_cpp("Не удалось обработать: " + " ".join(result))


# python .\\extract_tables_to_xlsx.py C:/Coding/works/wbi/insearch_wbi/assets/projects/2023_08_15__18_31_58/cut_img/ C:/Coding/works/wbi/insearch_wbi/assets/projects/2023_08_15__18_31_58/xlsx_raw/ 6 0.png,1.png,102.png,2.png,3.png,4.png,5.png
extract_tables_to_xlsx(parse_args(Args))
   # ! python .\\extract_tables_to_xlsx.py C:\\Coding\\works\\wbi\\ProjectsISC\\Amati_2023_10_30\\data\\catalog\\cut_img C:\\Coding\\works\\wbi\\ProjectsISC\\Amati_2023_10_30\\data\\catalog\\first_xlsx 6 none

   # ! python -O -OO .\\extract_tables_to_xlsx.py "C:\\Coding\\works\\wbi\\ProjectsISC\\data/catalog/cut_by_pattern_img/" "C:\\Coding\\works\\wbi\\ProjectsISC\\project.lmproj" "C:\\Coding\\works\\wbi\\ProjectsISC\\data/catalog/raw_xlsx/" "6"
