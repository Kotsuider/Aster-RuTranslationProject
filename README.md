# Проект перевода Aster на русский язык

## Общие сведения
Перевод делается gemini с последующей редактурой.

## Статус

**Progress:**
`[██████████░░░░░░░░░] 55%`

- [x] Полный перевод
- [ ] Перевод графики
- [ ] Редактура

В данный момент времени строки в самой вн могут выглядеть весьма специфично (перенос посреди предложения), ибо сами строки в оригинале так разбиты. Будет правиться с редактурой. 

[Таблица с переводом](https://docs.google.com/spreadsheets/d/1OwNxSBqtfSx_dSwn6DjITNLY31HOFFt1/edit?usp=sharing&ouid=103224880279791937700&rtpof=true&sd=true)

## Установка
1. Скачайте
2. Сделайте бэкап script.pak и data.pak (сейчас без data.pak)
3. Распакуйте архив в папку, установите шрифт
4. Профит

## Про G2
Движок подхватывает файлы из папки как из архива, если переназвать папку в нужное имя.

## Редактирование патча
Для распаковки используем [GARbro](https://github.com/morkt/GARbro/releases/tag/v1.5.44) и распаковываем допустим в папку ORIG.
Получаем папку со скриптами. 
Далее используем [G2CryptTool.exe] (https://github.com/julixian/MyVisualNovelTransTools/blob/main/G2CryptTool.exe) от julixian
```
G2CryptTool.exe dump ORIG ORIG_STR
```

Скачиваем таблицу с переводом и запускаем `create_inject.bat`

### Графика 
Для редактирования графики используется bgra_tool.exe.

### Dll
Для импорта DLL используется dinput.dll. 
Половина самого DLL содержит уже не использующиеся функции. Самое нужное и важно сейчас - работа с шрифтом.

```MSYS2
export PATH="/mingw32/bin:$PATH"

i686-w64-mingw32-gcc -O2 -shared -o translation.asi translation.c -lkernel32
```


