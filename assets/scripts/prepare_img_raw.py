import os
from concurrent.futures import Future, ThreadPoolExecutor

from base import ArgsBase, file_format_img, parse_args, print_to_cpp
from pdf2image import convert_from_bytes, pdfinfo_from_bytes
from PIL import Image
from psutil import cpu_count                                                                                            # type: ignore

PREV_MAX_SIZE = 2560, 1440


class Args(ArgsBase):
    catalog_path: str
    save_path: str
    prev_save_path: str
    multi: int
    need_split: bool


# if (len(sys.argv) < 5):
#     print_to_cpp("Скрипт не выполнен (не хватает агрументов)")
#     exit(-1)

# try:
#     CATALOG_PATH = str(sys.argv[1])
#     SAVE_PATH = str(sys.argv[2])
#     MULTI = int(sys.argv[3])
#     NEED_SPLIT = bool(int(sys.argv[4]))
# except Exception:
#     print_to_cpp("Аргументы скрипта имеют ошибки!")
#     exit(-1)


def save_images(img: Image.Image, page_id: int, save_path: str, prev_save_path: str, need_split: bool):
    print_to_cpp(f"Сохраняется страница ({page_id})")
    if need_split:
        width, height = img.size
        split = width // 2

        box1 = (0, 0, split, height)
        img1 = img.crop(box1)
        img1.save(os.path.join(save_path, file_format_img(page_id * 2 + 0)), "PNG")

        img1.thumbnail(PREV_MAX_SIZE, Image.Resampling.LANCZOS)
        img1.save(os.path.join(prev_save_path, file_format_img(page_id * 2 + 0)), "PNG")

        box2 = (split, 0, width, height)
        img2 = img.crop(box2)
        img2.save(os.path.join(save_path, file_format_img(page_id * 2 + 1)), "PNG")

        img2.thumbnail(PREV_MAX_SIZE, Image.Resampling.LANCZOS)
        img2.save(os.path.join(prev_save_path, file_format_img(page_id * 2 + 1)), "PNG")

    else:
        img.save(os.path.join(save_path, file_format_img(page_id)), "PNG")

        img.thumbnail(PREV_MAX_SIZE, Image.Resampling.LANCZOS)
        img.save(os.path.join(prev_save_path, file_format_img(page_id)), "PNG")


def process_pdf_img(pdf_bytes: bytes, start_id: int, page_id: int, multi: int, save_path: str, prev_save_path: str,
                    need_split: bool):
    print_to_cpp(f"Конвертируется страница ({page_id})")
    img = convert_from_bytes(pdf_bytes,
                             100 * multi,
                             first_page=start_id + page_id,
                             last_page=start_id + page_id + 1,
                             poppler_path="W:\\Programs\\poppler-23.07.0\\Library\\bin")[0]

    save_images(img, page_id, save_path, prev_save_path, need_split)

    return True


def prepare_img_raw(args: Args):
    threads_to_use = max(1, cpu_count() - 1)
    print_to_cpp("PDF Преобразуется (это может занять несколько минут)")

    with open(args.catalog_path, "rb") as pdf_file:
        pdf_bytes = pdf_file.read()

    pdf_info = pdfinfo_from_bytes(pdf_bytes, poppler_path="W:\\Programs\\poppler-23.07.0\\Library\\bin")
    print_to_cpp("PDF информация: " + str(pdf_info))
    # * print("pdfinfo_from_bytes: ", pdfinfo_from_bytes(pdf_bytes))
    start_page = 1                                                                                                      # * pdfinfo_from_bytes["Page_rot"] ???

    futures: list[Future] = []
    with ThreadPoolExecutor(max_workers=threads_to_use) as e:
        for i in range(pdf_info["Pages"]):
            futures.append(
                e.submit(process_pdf_img, pdf_bytes, start_page, i, args.multi, args.save_path, args.prev_save_path,
                         args.need_split))

    for i, res_future in enumerate(futures):
        if not res_future.result():
            print_to_cpp(f"Преобразование завершено с ошибкой для страницы {i}")

    print_to_cpp("Преобразование завершено")


prepare_img_raw(parse_args(Args))
