#!/usr/bin/env bash
#
# Устанавливает лаунчер (.desktop) и иконки приложения для текущего пользователя
# (без sudo):
#
#   ~/.local/share/applications/insearch-creator.desktop
#   ~/.local/share/icons/hicolor/<size>x<size>/apps/insearch-creator.png
#
# На Linux, в отличие от Windows .exe, иконку нельзя вшить в бинарник: меню
# приложений и файловый менеджер берут её из .desktop-файла и темы иконок
# hicolor, а таскбар связывает запущенное окно с лаунчером по WM_CLASS
# (res_class=InSearchCreator задаётся в GLFWWindow.cpp, StartupWMClass — здесь).
# Иконку самого окна/таскбара приложение ставит через glfwSetWindowIcon.
#
# Использование:
#   scripts/linux-desktop/install.sh                # для build/linux-ninja-gcc-x64/App/App
#   scripts/linux-desktop/install.sh <бинарник>     # для указанного бинарника
#   scripts/linux-desktop/install.sh --uninstall    # удалить лаунчер и иконки

set -euo pipefail

APP_ID="insearch-creator"

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
desktop_template="${repo_root}/App/res/${APP_ID}.desktop.in"
icons_src_dir="${repo_root}/assets/textures/app_icon"
apps_dir="${HOME}/.local/share/applications"
icons_dir="${HOME}/.local/share/icons/hicolor"

refresh_caches() {
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${apps_dir}" >/dev/null 2>&1 || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q "${icons_dir}" >/dev/null 2>&1 || true
    fi
}

if [[ "${1:-}" == "--uninstall" ]]; then
    rm -f "${apps_dir}/${APP_ID}.desktop"
    find "${icons_dir}" -path "*/apps/${APP_ID}.png" -delete
    refresh_caches
    echo "Лаунчер и иконки ${APP_ID} удалены."
    exit 0
fi

binary="${1:-${repo_root}/build/linux-ninja-gcc-x64/App/App}"
if [[ ! -x "${binary}" ]]; then
    echo "Бинарник не найден: ${binary}" >&2
    echo "Сначала соберите приложение:" >&2
    echo "  cmake --preset linux-ninja-gcc-x64 && cmake --build --preset linux-ninja-gcc-x64-debug" >&2
    exit 1
fi
binary="$(realpath -- "${binary}")"

shopt -s nullglob
installed_icons=0
for icon in "${icons_src_dir}"/app_icon_*.png; do
    size="$(basename -- "${icon}" .png)"
    size="${size#app_icon_}"
    install -D -m 644 "${icon}" "${icons_dir}/${size}x${size}/apps/${APP_ID}.png"
    installed_icons=$((installed_icons + 1))
done
if [[ ${installed_icons} -eq 0 ]]; then
    echo "Не найдены PNG-иконки в ${icons_src_dir} (извлекаются из App/res/app.ico" >&2
    echo "скриптом scripts/linux-desktop/ico-to-png.py)." >&2
    exit 1
fi

install -d "${apps_dir}"
sed -e "s|@APP_EXEC@|${binary}|g" \
    -e "s|@APP_WORKDIR@|${repo_root}|g" \
    -e "s|@APP_ICON@|${APP_ID}|g" \
    "${desktop_template}" > "${apps_dir}/${APP_ID}.desktop"
chmod 644 "${apps_dir}/${APP_ID}.desktop"

refresh_caches

echo "Готово: установлено иконок ${installed_icons}, лаунчер ${apps_dir}/${APP_ID}.desktop -> ${binary}"
echo "Приложение доступно в меню приложений как «InSearch Creator»."
