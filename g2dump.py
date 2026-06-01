#!/usr/bin/env python3
"""
g2dump.py — инструмент для работы с переводом .g2 скриптов движка G2

Режимы:

  dump-xlsx <папка_с_g2/> <out.xlsx>
      Дамп всех .g2 из папки в Excel таблицу.
      Каждый .g2 = отдельный лист.
      Колонки: Row | Original JP | TL (RU) | Max bytes
      Строки-пути к ресурсам (*.pak\\...) НЕ включаются в таблицу,
      строки \\x(31)... — включаются (токены имён персонажей).

  dump-xlsx <file.g2> <out.xlsx>
      То же, но для одного файла.

  from-xlsx <папка_с_g2/> <input.xlsx> <папка_вывода/>
      Конвертировать заполненный xlsx обратно в .g2 файлы.
      Берёт оригинальные .g2, подставляет переводы из xlsx по номеру строки (Row).
      Строки без перевода остаются оригинальными.
      Кириллица автоматически конвертируется в RU_F кодировку.
      Перенос строк выполняется автоматически.

  wrap <input.xlsx> <output.xlsx>
      Применить автоматический перенос строк к колонке TL (RU) в xlsx.
      Обрабатывает только ячейки с текстом (кириллицей).
      Разделитель строк: \\n  Перенос слова: -- (среднее тире)
      Лимит: 48 символов на строку.

Формат .g2:
  Обычный текстовый файл, строки разделены \\r\\n (CP932).
  Строки-пути к ресурсам (*.pak\\...) не переводятся.

RU_F кодировка (автоматически при from-xlsx):
  Кириллица в переводе -> латинские символы кастомного шрифта
  А->A, Б->B, В->C ... и т.д.
"""

import sys, re, os
from pathlib import Path

# RU_F таблица

_RU_F_UPPER = {
    'A': 'А', 'B': 'Б', 'C': 'В', 'D': 'Г', 'E': 'Д', 'F': 'Е',
    'G': 'Ж', 'H': 'З', 'I': 'И', 'J': 'Й', 'K': 'К', 'L': 'Л',
    'M': 'М', 'N': 'Н', 'O': 'О', 'P': 'П', 'Q': 'Р', 'R': 'С',
    'S': 'Т', 'T': 'У', 'U': 'Ф', 'V': 'Х', 'W': 'Ц', 'X': 'Ч',
    'Y': 'Ш', 'Z': 'Щ',
}
_RU_F_LOWER = {k.lower(): v.lower() for k, v in _RU_F_UPPER.items()}
_CYR_TO_LATIN = {}
for _l, _c in _RU_F_UPPER.items(): _CYR_TO_LATIN[_c] = _l
for _l, _c in _RU_F_LOWER.items(): _CYR_TO_LATIN[_c] = _l
_CYR_SPECIAL = {
    'Ъ': '[', 'Ь': ']', 'ё': '`', 'э': '{', 'ы': '|', 'я': '}',
    'Ы': '\xa1', 'ь': '&', 'ъ': '+', 'Ю': '-', 'ю': '$',
    '—': '#', 'Я': '>', 'Ё': '<', 'Э': '=', 'Й': 'J', 'й': 'j',
}
_FULL_TABLE = {}
_FULL_TABLE.update(_CYR_TO_LATIN)
_FULL_TABLE.update(_CYR_SPECIAL)


def apply_ru_f(text):
    if not text:
        return text
    return ''.join(_FULL_TABLE.get(ch, ch) for ch in text)


# Работа с .g2 файлами

_ANY_PAK = re.compile(r'^[a-zA-Z0-9_]+\.pak\\')


def read_g2_lines(path):
    data = path.read_bytes()
    lines = data.split(b'\r\n')
    if lines and lines[-1] == b'':
        lines = lines[:-1]
    return lines


def decode_line(raw):
    for enc in ('cp932', 'gbk', 'utf-8'):
        try:
            return raw.decode(enc)
        except Exception:
            pass
    return raw.decode('cp932', errors='replace')


def encode_line(text):
    return text.encode('cp932', errors='replace')


def is_skip_line(text):
    return bool(_ANY_PAK.match(text))


def get_translatable_rows(lines):
    result = []
    for i, raw in enumerate(lines):
        text = decode_line(raw)
        if is_skip_line(text):
            continue
        result.append({
            'row_index': i,
            'text': text,
            'byte_len': len(raw),
        })
    return result


# Перенос строк (wrap)

WRAP_LIMIT  = 48
WRAP_HYPHEN = '#'   
_HAS_CYRILLIC = re.compile(r'[\u0430-\u044f\u0451\u0410-\u042f\u0401]')

_hyph = None


def _get_hyph():
    global _hyph
    if _hyph is not None:
        return _hyph
    try:
        import pyphen
        _hyph = pyphen.Pyphen(lang='ru')
    except ImportError:
        print("ВНИМАНИЕ: pyphen не установлен, перенос по слогам недоступен.")
        print("  Установите: pip install pyphen")
        _hyph = False
    return _hyph


def _syllable_positions(word):
    h = _get_hyph()
    if h:
        return h.positions(word)
    return []


def _split_long_word(word, limit, out_lines):
    """
    Разбивает слово на части <= limit, добавляя каждую часть кроме последней в out_lines с тире.
    Возвращает последний остаток (без тире).
    """
    remaining = word
    while len(remaining) > limit:
        positions = _syllable_positions(remaining)
        best = None
        for p in positions:
            if p + 1 <= limit:   # нужно место под символ + тире
                best = p
        if best is None:
            best = limit - 1     # жёсткий разрыв
        out_lines.append(remaining[:best] + WRAP_HYPHEN)
        remaining = remaining[best:]
    return remaining


def wrap_text(text, limit=WRAP_LIMIT):
    """
    Переносит текст по словам и слогам.
    - Разделитель строк: \\n (без пробела между строками)
    - Перенос слова через слог: – (среднее тире)
    - Лимит: limit символов на строку
    - Текст без кириллицы не трогается
    - Уже имеющиеся \\n — каждый отрезок оборачивается независимо
    """
    if not _HAS_CYRILLIC.search(text):
        return text

    if '\\n' in text:
        return '\\n'.join(wrap_text(part, limit) for part in text.split('\\n'))

    words = text.split(' ')
    out_lines = []
    current = ''

    for word in words:
        if not word:
            continue

        sep = '' if current == '' else ' '
        candidate = current + sep + word

        if len(candidate) <= limit:
            current = candidate
            continue

        # Слово не влезает целиком на текущую строку.
        # Попробуем разбить слово по слогу прямо здесь, используя остаток строки.
        space_left = limit - len(current) - (1 if current else 0)
        positions  = _syllable_positions(word)

        best_here = None
        for p in positions:
            if p + 1 <= space_left and p >= 2:
                best_here = p

        if best_here:
            # Часть слова — на текущую строку с тире
            out_lines.append(current + sep + word[:best_here] + WRAP_HYPHEN)
            rest = word[best_here:]
            if len(rest) > limit:
                current = _split_long_word(rest, limit, out_lines)
            else:
                current = rest
        else:
            # Не получилось разбить — переносим слово на следующую строку
            if current:
                out_lines.append(current)
            if len(word) <= limit:
                current = word
            else:
                current = _split_long_word(word, limit, out_lines)

    if current:
        out_lines.append(current)

    return '\\n'.join(out_lines)


# dump-xlsx

def cmd_dump_xlsx(args):
    try:
        import openpyxl
        from openpyxl.styles import Font, PatternFill, Alignment
    except ImportError:
        print("Установите openpyxl:  pip install openpyxl")
        return

    if len(args) < 2:
        print("Использование: g2dump.py dump-xlsx <папка|file.g2> <output.xlsx>")
        return

    src = Path(args[0])
    out = Path(args[1])

    files = []
    if src.is_dir():
        for f in sorted(src.glob('*.g2')):
            files.append(f)
    elif src.suffix.lower() == '.g2':
        files.append(src)
    else:
        print(f"Неизвестный источник: {src}")
        return

    wb = openpyxl.Workbook()
    wb.remove(wb.active)

    hdr_font  = Font(bold=True, color='FFFFFF')
    hdr_fill  = PatternFill('solid', fgColor='2F4F8F')
    orig_fill = PatternFill('solid', fgColor='F0F0F0')
    center    = Alignment(horizontal='center', vertical='top')
    wrap_al   = Alignment(wrap_text=True, vertical='top')

    total = 0
    for g2_path in files:
        lines = read_g2_lines(g2_path)
        rows  = get_translatable_rows(lines)

        if not rows:
            print(f"  {g2_path.name}: нет строк для перевода, пропускаем")
            continue

        sheet_name = g2_path.stem[:31]
        ws = wb.create_sheet(title=sheet_name)

        headers = ['Row', 'Original JP', 'TL (RU)', 'Max bytes']
        for col, h in enumerate(headers, 1):
            cell = ws.cell(row=1, column=col, value=h)
            cell.font      = hdr_font
            cell.fill      = hdr_fill
            cell.alignment = center

        ws.column_dimensions['A'].width = 8
        ws.column_dimensions['B'].width = 50
        ws.column_dimensions['C'].width = 50
        ws.column_dimensions['D'].width = 10
        ws.row_dimensions[1].height = 20

        for xls_row, s in enumerate(rows, 2):
            ws.cell(row=xls_row, column=1, value=s['row_index']).alignment = center
            c_orig = ws.cell(row=xls_row, column=2, value=s['text'])
            c_orig.fill      = orig_fill
            c_orig.alignment = wrap_al
            ws.cell(row=xls_row, column=3, value='').alignment = wrap_al
            ws.cell(row=xls_row, column=4, value=s['byte_len']).alignment = center

        ws.auto_filter.ref = f"A1:D{len(rows)+1}"
        ws.freeze_panes    = 'A2'

        total += len(rows)
        print(f"  {g2_path.name}: {len(rows)} строк")

    wb.save(out)
    print(f"\nСохранено: {out}  ({total} строк, {len(files)} листов)")


# wrap (xlsx -> xlsx)

def cmd_wrap(args):
    try:
        import openpyxl
        from openpyxl.styles import Alignment
    except ImportError:
        print("Установите openpyxl:  pip install openpyxl")
        return

    if len(args) < 2:
        print("Использование: g2dump.py wrap <input.xlsx> <output.xlsx>")
        return

    in_path  = Path(args[0])
    out_path = Path(args[1])

    if not in_path.exists():
        print(f"Файл не найден: {in_path}")
        return

    _get_hyph()   # инициализировать сразу, чтобы предупреждение вышло до начала работы

    wb = openpyxl.load_workbook(in_path)
    wrap_al = Alignment(wrap_text=True, vertical='top')

    total_wrapped = 0
    total_cells   = 0

    for sheet_name in wb.sheetnames:
        ws = wb[sheet_name]
        sheet_wrapped = 0

        for row in ws.iter_rows(min_row=2):
            tl_cell = row[2]   # колонка C — TL (RU)
            val = tl_cell.value
            if not val or not str(val).strip():
                continue

            text = str(val).strip()
            total_cells += 1

            wrapped = wrap_text(text)
            if wrapped != text:
                tl_cell.value     = wrapped
                tl_cell.alignment = wrap_al
                sheet_wrapped += 1

        if sheet_wrapped:
            print(f"  {sheet_name}: перенесено {sheet_wrapped} ячеек")
        total_wrapped += sheet_wrapped

    wb.save(out_path)
    print(f"\nГотово: {total_wrapped} ячеек из {total_cells} переработаны -> {out_path}")


# from-xlsx

def cmd_from_xlsx(args):
    try:
        import openpyxl
    except ImportError:
        print("Установите openpyxl:  pip install openpyxl")
        return

    if len(args) < 3:
        print("Использование: g2dump.py from-xlsx <папка_с_g2/> <input.xlsx> <папка_вывода/>")
        return

    g2_dir    = Path(args[0])
    xlsx_path = Path(args[1])
    out_dir   = Path(args[2])

    if not g2_dir.is_dir():
        print(f"Папка с .g2 не найдена: {g2_dir}")
        return
    if not xlsx_path.exists():
        print(f"Файл xlsx не найден: {xlsx_path}")
        return

    out_dir.mkdir(parents=True, exist_ok=True)
    _get_hyph()   # инициализировать сразу

    wb = openpyxl.load_workbook(xlsx_path, data_only=True)

    total_files   = 0
    total_applied = 0

    for sheet_name in wb.sheetnames:
        ws = wb[sheet_name]

        g2_path = g2_dir / f"{sheet_name}.g2"
        if not g2_path.exists():
            print(f"  {sheet_name}.g2: файл не найден в {g2_dir}, пропускаем")
            continue

        lines = read_g2_lines(g2_path)

        translations = {}
        for xls_row in ws.iter_rows(min_row=2, values_only=True):
            if not xls_row or xls_row[0] is None:
                continue
            try:
                row_idx = int(xls_row[0])
            except (ValueError, TypeError):
                continue
            tl = xls_row[2]
            if tl and str(tl).strip():
                translations[row_idx] = str(tl).strip()

        applied   = 0
        new_lines = list(lines)
        for row_idx, tl_text in translations.items():
            if row_idx < 0 or row_idx >= len(new_lines):
                print(f"  {sheet_name}: строка {row_idx} вне диапазона, пропускаем")
                continue
            tl_wrapped = wrap_text(tl_text)
            tl_encoded = apply_ru_f(tl_wrapped)
            new_lines[row_idx] = encode_line(tl_encoded)
            applied += 1

        out_path = out_dir / f"{sheet_name}.g2"
        out_data = b'\r\n'.join(new_lines) + b'\r\n'
        out_path.write_bytes(out_data)

        total_files   += 1
        total_applied += applied
        print(f"  {sheet_name}.g2: применено {applied} переводов -> {out_path}")

    print(f"\nГотово: {total_files} файлов, {total_applied} строк переведено -> {out_dir}")


# main

def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(1)

    cmd = args[0].lower()

    if cmd == 'dump-xlsx':
        cmd_dump_xlsx(args[1:])
    elif cmd == 'wrap':
        cmd_wrap(args[1:])
    elif cmd == 'from-xlsx':
        cmd_from_xlsx(args[1:])
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == '__main__':
    main()