from concurrent.futures import Future, ThreadPoolExecutor
import os
import json
from typing import Any, TypedDict
from psutil import cpu_count                                                                                            # type: ignore

import cv2
from cv2.typing import Point
from PIL import Image

from base import ArgsBase, parse_args, print_to_cpp, file_format_img, read_cv_file

SHAPE_X = 1
SHAPE_Y = 0

LOC_X = 0
LOC_Y = 1

PREV_MAX_SIZE = 2560, 1440


class ConfigPoint(TypedDict):
    x: float
    y: float


class Args(ArgsBase):
    in_img_path: str
    config_filename: str
    tmp_path: str
    save_path: str
    prev_save_path: str
    treshold: float                                                                                                     # = 0.999


def find_point(img, pattern) -> tuple[float, Point]:
    result = cv2.matchTemplate(img, pattern, cv2.TM_CCORR_NORMED)
    (_, max_val, _, max_loc) = cv2.minMaxLoc(result)
    return (max_val, max_loc)


def clamp(value, min_value, max_value):
    return max(min_value, min(value, max_value))


def split_img(img_path: os.DirEntry[str], pattern_1, pattern_2, pattern_offsets: list[list[int]], save_path: str,
              prev_save_path: str, treshold: float):
    print_to_cpp(f"Обрезается файл: {img_path.name}")
    imgcv = read_cv_file(img_path.path)

    max_val_1, max_loc_1 = find_point(imgcv, pattern_1)
    max_val_2, max_loc_2 = find_point(imgcv, pattern_2)

    max_locs = [max_loc_1, max_loc_2]

    min_x_id = 0 if max_loc_1[LOC_X] < max_loc_2[LOC_X] else 1
    max_x_id = (min_x_id + 1) % 2

    min_y_id = 0 if max_loc_1[LOC_Y] < max_loc_2[LOC_Y] else 1
    max_y_id = (min_y_id + 1) % 2

    min_x = clamp(max_locs[min_x_id][LOC_X] + pattern_offsets[min_x_id][LOC_X], 0, imgcv.shape[SHAPE_X])
    max_x = clamp(max_locs[max_x_id][LOC_X] + pattern_offsets[max_x_id][LOC_X], 0, imgcv.shape[SHAPE_X])

    min_y = clamp(max_locs[min_y_id][LOC_Y] + pattern_offsets[min_y_id][LOC_Y], 0, imgcv.shape[SHAPE_Y])
    max_y = clamp(max_locs[max_y_id][LOC_Y] + pattern_offsets[max_y_id][LOC_Y], 0, imgcv.shape[SHAPE_Y])

    # max_x = min(max(max_loc_1[LOC_X] + pattern_1.shape[SHAPE_X], max_loc_2[LOC_X] + pattern_2.shape[SHAPE_X]),
    #             imgcv.shape[SHAPE_X])
    # max_y = min(max(max_loc_1[LOC_Y] + pattern_1.shape[SHAPE_Y], max_loc_2[LOC_Y] + pattern_2.shape[SHAPE_Y]),
    #             imgcv.shape[SHAPE_Y])

    res_img = imgcv[min_y:max_y, min_x:max_x]

    if not res_img.size:
        return False

    # res_img = cv2.cvtColor(res_img, cv2.COLOR_BGR2GRAY)
    # res_img = cv2.GaussianBlur(res_img, (3, 3), 0)
    # res_img = cv2.threshold(res_img, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)[1]

    # kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
    # res_img = cv2.morphologyEx(res_img, cv2.MORPH_OPEN, kernel, iterations=1)
    # res_img = 255 - res_img

    img_save_path = os.path.join(save_path, img_path.name)
    cv2.imencode(".png", res_img)[1].tofile(img_save_path)
    with Image.open(img_save_path) as img:
        img.thumbnail(PREV_MAX_SIZE, Image.Resampling.LANCZOS)
        img.save(os.path.join(prev_save_path, img_path.name), "PNG")

    if max_val_1 < treshold or max_val_2 < treshold:
        return False

    return True


def create_pattern_and_get_pixel_offset(img_path: str, save_filename: str, perc_min: ConfigPoint, perc_max: ConfigPoint,
                                        perc_offset: ConfigPoint) -> list[int]:
    print_to_cpp("Создается паттерн из: " + img_path)
    imgcv = read_cv_file(img_path)

    min_x = int(imgcv.shape[SHAPE_X] * perc_min["x"] / 100.0)
    min_y = int(imgcv.shape[SHAPE_Y] * perc_min["y"] / 100.0)

    max_x = int(imgcv.shape[SHAPE_X] * perc_max["x"] / 100.0)
    max_y = int(imgcv.shape[SHAPE_Y] * perc_max["y"] / 100.0)

    res_img = imgcv[min_y:max_y, min_x:max_x]

    cv2.imencode(".png", res_img)[1].tofile(save_filename)

    return [int((max_x - min_x) * perc_offset["x"] / 100.0), int((max_y - min_y) * perc_offset["y"] / 100.0)]


def split_by_pattern(args: Args):
    args.treshold = 0.99
    print_to_cpp(f"Args: {args}")
    # TODO: tmp
    result: list[str] = []
    threads_to_use = max(1, cpu_count() - 1)

    os.makedirs(args.tmp_path, exist_ok=True)
    os.makedirs(args.save_path, exist_ok=True)

    with open(args.config_filename, encoding="utf-8") as conf_file:
        config = json.load(conf_file)

    pattern_paths: list[str] = []
    pattern_offsets: list[list[int]] = []
    for i, pattern_name in enumerate(["TopLeftCutPattern", "BotRightCutPattern"]):
        pattern_config = config["Catalog"][pattern_name]
        pattern_page = os.path.join(args.in_img_path, file_format_img(pattern_config["PageId"]))
        pattern_paths.append(os.path.join(args.tmp_path, f"pattern_{i}.png"))
        pattern_offsets.append(
            create_pattern_and_get_pixel_offset(pattern_page, pattern_paths[i], pattern_config["PointMin"],
                                                pattern_config["PointMax"], pattern_config["CenterPoint"]))

    pattern_1 = read_cv_file(pattern_paths[0])
    pattern_2 = read_cv_file(pattern_paths[1])
    cut_filenames: list[str] = []
    cut_futures: list[Future] = []
    with ThreadPoolExecutor(max_workers=threads_to_use) as e:
        for filename in os.scandir(args.in_img_path):
            if filename.is_file():
                cut_filenames.append(filename.name)
                cut_futures.append(
                    e.submit(split_img, filename, pattern_1, pattern_2, pattern_offsets, args.save_path,
                             args.prev_save_path, args.treshold))
            # if not split_img(filename, pattern_1, pattern_2, pattern_offsets, args.save_path, args.prev_save_path,
            #                  args.treshold):
            # print_to_cpp(f"Возможна неточность в: {filename.name}")
            # result.append(filename.name.split(".")[0])

    for i, (res_future, res_filename) in enumerate(zip(cut_futures, cut_filenames)):
        if not res_future.result():
            result.append(res_filename.split(".")[0])

    print_to_cpp("Возможны неточности в: " + " ".join(result))


#  python .\cut_by_pattern.py C:/Coding/works/wbi/insearch_wbi/assets/projects/2023_08_15__18_31_58/raw_img/ C:/Coding/works/wbi/insearch_wbi/assets/projects/2023_08_15__18_31_58/patterns/cut_1.png C:/Coding/works/wbi/insearch_wbi/assets/projects/2023_08_15__18_31_58/patterns/cut_2.png C:/Coding/works/wbi/insearch_wbi/assets/projects/2023_08_15__18_31_58/cut_img/ 0.999
split_by_pattern(parse_args(Args))

# !  python .\cut_by_pattern.py C:\Coding\works\wbi\ProjectsISC\Amati_2023_10_30\data\catalog\raw_img C:\Coding\works\wbi\ProjectsISC\Amati_2023_10_30\project.lmproj C:\Coding\works\wbi\ProjectsISC\Amati_2023_10_30\tmp C:\Coding\works\wbi\ProjectsISC\Amati_2023_10_30\data\catalog\cut_img 0.999
