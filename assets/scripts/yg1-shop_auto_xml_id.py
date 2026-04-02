from playwright.sync_api import sync_playwright, TimeoutError as PLTimeoutError

from base import (ArgsBase, log_info_to_cpp, log_warning_to_cpp, log_error_to_cpp, start_program)


def Args(ArgsBase):
    pass


BOOL_FIELDS = [
    "DPC", "Production", "Actuality", "BBD", "HANDY", "STRAGGED", "IEP", "IncludeInPackage", "AllowSwitchInsert",
    "BRAZED", "RIBBONCON", "CHESSEDGE", "FULLROUND", "ZipAttention", "DIFDIR", "CBP", "ROUNDDIE", "KEYDRW", "COMBILINE",
    "SEALED", "SCRW", "INSTOCK", "WEP", "OIL_GROOVES", "RIGCYC", "VAROM", "CPDF", "CCC", "HPCOOL", "SWMA", "TMINS",
    "WasChanged", "REVERPROP", "PILOT_NOSE", "CENHOLE", "ADDITIONAL", "DIF_TOOH_DIR", "BRACE_EX", "THR_HOLES_EX",
    "ANVIL_EX", "IS_HM"
]

DESCR_FIELDS = [
    "BMC", "CXSC", "CEATC", "ClampDirection", "HAND", "MachiningType", "THRTYP", "SurfacePropose", "PSIRDIR", "PFS",
    "FORM_ID", "BLANK_TYPE", "SCRSTROKE", "LCS", "SC", "ADJTYPE", "PROF_ID", "BURSHARP", "ShankType", "MGRO", "CST",
    "CENDRFORM", "ArborTypeWS", "HOLETYPE", "HOLDER_SIDE", "HOLDER_TYPE", "METHOD", "INSTYPE", "THFT", "TPT", "TTP",
    "InsertType", "FLUTEHAND", "CLFAM", "IFS", "AXGSUP", "CNSC", "SUB_TYP", "GTYP", "CONSTR_TYP", "TAPER_TYP",
    "TAPER_CSIZE", "TAPER_TOL", "MODIFIC", "PRISM_TYP", "LEVER_TYP", "ANG_TYP", "PITCH_TYP", "CUTSIDES", "DEVICE_SEC",
    "DEVICE_TYP", "ARB_TYP", "LAYERS_COUNT", "JAWTYP", "MECHAN", "MICROMETER_TYP", "NOTCHING_TYP", "NOTCHING_CLASS",
    "FILE_FORM", "SHJAW_TYP", "MANAGE_TYP", "CLIMATIC_STYLE", "BLOCK_TYP"
]

ALL_FIELDS = BOOL_FIELDS + DESCR_FIELDS

SELECTOR_INPUT_SELECT = 'div.bx-core-adm-dialog div.bx-core-adm-dialog-content tr:has(td:text("Поле свойства для загрузки:")) select'
SELECTOR_SAVE_BUTTON = 'div.bx-core-adm-dialog div.bx-core-adm-dialog-buttons #savebtn'


class NoFieldException(Exception):
    pass


class NoInputException(Exception):
    pass


def handle_fields():
    with sync_playwright() as p:
        browser = p.chromium.connect_over_cdp("http://localhost:9222")

        context = browser.contexts[0]
        page = context.pages[0]

        handled_count = 0

        for fieldname in ALL_FIELDS:
            try:
                try:
                    selector_field = f'td div.selected:has(span.fieldval[title*="[{fieldname}]"]) a.field_settings'
                    if page.locator(selector_field).is_visible():
                        page.click(selector_field)
                    else:
                        raise NoFieldException()
                except PLTimeoutError as exc:
                    raise NoFieldException() from exc

                try:
                    page.select_option(SELECTOR_INPUT_SELECT, label="XML_ID", timeout=2000)
                except PLTimeoutError as exc:
                    page.click(SELECTOR_SAVE_BUTTON)
                    raise NoInputException() from exc

                page.click(SELECTOR_SAVE_BUTTON)
                log_info_to_cpp(f"Обработано поле {fieldname}")
                handled_count += 1
            except NoFieldException:
                log_info_to_cpp(f"Не найдено поле: {fieldname}")
            except NoInputException:
                log_error_to_cpp(f"У поля, указанного в списке, нет XML_ID: {fieldname}")

        log_info_to_cpp(f"Всего обработано полей: {handled_count}")

        log_info_to_cpp("=" * 50)

        header_row = page.locator("table.list tbody tr.kda-ie-tbl-headers").first
        td_index_pic = header_row.locator("td").evaluate_all("""(cells) => {
                const idx = cells.findIndex((td) => td.textContent.trim() === "img_pic");
                return idx;
            }""")

        if td_index_pic != -1:
            log_info_to_cpp(f"Номер td с текстом img_pic: {td_index_pic}")

            target_cell = page.locator("table.list tbody tr").nth(0).locator("td").nth(td_index_pic)
            placeholder_span_first = target_cell.locator('span[placeholder="- выбрать поле -"]')
            if placeholder_span_first.count() == 0:
                placeholder_span_first = target_cell.locator('span:has-text("- выбрать поле -")')

            if placeholder_span_first.count() > 0:
                placeholder_span_first.first.scroll_into_view_if_needed()
                placeholder_span_first.first.click()
                page.click('div#kda_select_chosen ul.chosen-results li:has-text("Картинка для анонса (путь)")')
            else:
                log_error_to_cpp(f"В td с индексом {td_index_pic} не найден span '- выбрать поле -'")

            target_cell.locator(
                "div.kda-ie-field-select-btns div.kda-ie-field-select-btns-inner a.kda-ie-add-load-field").first.click(
                )
            placeholder_span_second = target_cell.locator('span[placeholder="- выбрать поле -"]')
            if placeholder_span_second.count() == 0:
                placeholder_span_second = target_cell.locator('span:has-text("- выбрать поле -")')

            if placeholder_span_second.count() > 0:
                placeholder_span_second.first.scroll_into_view_if_needed()
                placeholder_span_second.first.click()
                page.click('div#kda_select_chosen ul.chosen-results li:has-text("Детальная картинка (путь)")')
            else:
                log_error_to_cpp(f"В td с индексом {td_index_pic} не найден span '- выбрать поле -'")
        else:
            log_warning_to_cpp("td с текстом img_pic не найден")

        td_index_drw = header_row.locator("td").evaluate_all("""(cells) => {
                const idx = cells.findIndex((td) => td.textContent.trim() === "img_drw");
                return idx;
            }""")

        if td_index_drw != -1:
            log_info_to_cpp(f"Номер td с текстом img_drw: {td_index_drw}")
            target_cell = page.locator("table.list tbody tr").nth(0).locator("td").nth(td_index_drw)
            placeholder_span_first = target_cell.locator('span[placeholder="- выбрать поле -"]')
            if placeholder_span_first.count() == 0:
                placeholder_span_first = target_cell.locator('span:has-text("- выбрать поле -")')

            if placeholder_span_first.count() > 0:
                placeholder_span_first.first.click()
                page.click('div#kda_select_chosen ul.chosen-results li:has-text("Картинки [MORE_PHOTO]")')
            else:
                log_error_to_cpp(f"В td с индексом {td_index_drw} не найден span '- выбрать поле -'")
        else:
            log_warning_to_cpp("td с текстом img_drw не найден")


if __name__ == "__main__":
    start_program(handle_fields, Args)
