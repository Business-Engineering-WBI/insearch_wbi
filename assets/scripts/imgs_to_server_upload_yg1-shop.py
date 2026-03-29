import ftplib
import os

import pydantic
from base import (DEFAULT_CONNECTION_CONFIG_PATH, ArgsBase, import_connection_config, log_info_to_cpp, start_program)


class Args(ArgsBase):
    project_name: str = pydantic.Field(description="Название проекта для папки на сервере")
    imgs_path: str = pydantic.Field(description="Папка исходных файлов")
    connection_config_path: str = pydantic.Field(description="Путь к файлу конфигурации подключения по ftp",
                                                 default=DEFAULT_CONNECTION_CONFIG_PATH)


def ftp_makedirs(ftp: ftplib.FTP, remote_path: str):
    parts = remote_path.replace("\\", "/").split("/")
    current = ""
    for part in parts:
        if not part:
            current += "/"
            continue
        current = current.rstrip("/") + "/" + part
        try:
            ftp.mkd(current)
        except ftplib.error_perm:
            pass                                                                                                        # already exists


def process_files(args: Args):
    connection_config = import_connection_config(args.connection_config_path)

    ftp = ftplib.FTP()
    ftp.connect(connection_config.shop_ftp_host, connection_config.shop_ftp_port)
    ftp.login(connection_config.shop_ftp_user, connection_config.shop_ftp_password)

    local_base = os.path.abspath(args.imgs_path)
    local_folder_name = os.path.basename(os.path.normpath(local_base))

    remote_base = (connection_config.shop_ftp_imgs_path.rstrip("/") + "/" + args.project_name + "/" + local_folder_name)
    ftp_makedirs(ftp, remote_base)

    for dirpath, _dirnames, filenames in os.walk(local_base):
        rel = os.path.relpath(dirpath, local_base)
        if rel == ".":
            remote_dir = remote_base
        else:
            remote_dir = remote_base + "/" + rel.replace("\\", "/")

        ftp_makedirs(ftp, remote_dir)

        for filename in filenames:
            local_file = os.path.join(dirpath, filename)
            remote_file = remote_dir + "/" + filename
            with open(local_file, "rb") as f:
                ftp.storbinary(f"STOR {remote_file}", f)
            log_info_to_cpp(f"Загружен: {remote_file}")

    ftp.quit()
    log_info_to_cpp("Загрузка завершена")


if __name__ == "__main__":
    start_program(process_files, Args)
