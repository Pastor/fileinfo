# MEMO — fileinfo

Утилита для просмотра и редактирования метаданных файлов Windows (Win32 GUI, C/C++).

## Назначение

Позволяет открыть любой файл или папку и изучить/изменить:
- **Основные** — атрибуты файла (readonly, hidden, system, …), временны́е метки (creation, access, write, change), владелец файла
- **Размеры** — размер на диске, размер файла, кол-во жёстких ссылок, флаги directory/delete-pending, степень сжатия NTFS
- **Потоки** — NTFS alternate data streams (список, просмотр hex/текст, сохранение в файл, создание нового потока, запись файла в поток с прогресс-диалогом)
- **Идентификаторы** — volume serial number, 128-bit file unique ID (FileIdInfo, требует Windows 8+)
- **EXIF** — EXIF/IPTC/XMP через Exiv2 (при наличии библиотеки); без Exiv2 — заглушка-сообщение

## Архитектура

```
WinMain()
 └─ DialogBoxParam → MainDialog (main.c)
      ├─ TabCtrl с 5 вкладками → CreateDialogParam для каждой
      │    ├─ FILE_BASIC_INFO    → fbi_WindowHandler   (file_basic_info.c)
      │    ├─ FILE_STANDART_INFO → fsi_WindowHandler   (file_standart_info.c)
      │    ├─ FILE_STREAM_INFO   → fssi_WindowHandler  (file_stream_info.c)
      │    ├─ FILE_ID_INFO       → fii_WindowHandler   (file_id_info.c)  [Win8+]
      │    └─ FILE_EXIF_INFO     → fxi_WindowHandler   (file_exif_info.c) [заглушка]
      └─ Кнопки: открыть файл/папку, restart as admin
```

Связь главного окна с дочерними диалогами — через кастомные WM_USER сообщения:
- `WM_SETFILE_HANDLE` (WM_USER+1020) — передать HANDLE файла
- `WM_RESETFILE_HANDLE` (WM_USER+1021) — сбросить HANDLE
- `WM_SETFILE_NAME` (WM_USER+1022) — передать путь к файлу

## Ключевые модули

| Файл | Описание |
|------|----------|
| `main.c` | WinMain, MainDialog, открытие файла, drag-and-drop, tab switching, F5 refresh, recent files, compare |
| `common.c/h` | CreateSecurityAttributes (ACL для Administrators), FormatMessage-ошибки |
| `file_basic_info.c/h` | Атрибуты (incl. ReparsePoint) + DateTimePicker для 4 временных меток + владелец; сохранение с подтверждением; DST-корректный timezone |
| `file_standart_info.c/h` | FILE_STANDARD_INFO: размеры (StrFormatByteSize64), ссылки, compression/storage/alignment/reparse в IDC_EXTRA_INFO |
| `file_stream_info.c/h` | ListView NTFS-потоков; просмотр (hex/text), сохранение, создание с валидацией имени; I/O в фоновом потоке с IProgressDialog |
| `file_id_info.c/h` | Volume serial + 128-bit file ID в hex |
| `file_exif_info.c/h` | EXIF/IPTC/XMP через Exiv2 (`#ifdef EXIV2_AVAILABLE`); двойной клик — редактирование; без библиотеки — заглушка |
| `resource.h` | Все IDC_* / IDM_* / IDS_* константы; IDC_FILE_OWNER=1092, IDC_EXTRA_INFO=1093 |

## Сборка

- VS2022 BuildTools, toolset **v145**
- Windows SDK **10.0.26100.0**
- Конфигурации: Debug|Win32 (C), Release|Win32 (C++), Debug|x64, **Release|x64** (основная)
- Debug компилируется как C (`/TC`), Release — как C++ (`/TP`)
- Exiv2 (x64): `3rdParty\exiv2\lib64\exiv2.lib` + `exiv2.dll` рядом с exe
- **Рабочая сборка**: `Release|x64` — 0 ошибок, 0 предупреждений

### Команда сборки (MSBuild)

```
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    fileinfo.sln
    /p:Configuration=Release /p:Platform=x64
    /p:WindowsTargetPlatformVersion=10.0.26100.0
    /p:PlatformToolset=v145
    /noautoresponse
```

## Ключевые технические детали

- **Кодировка**: весь Кириллический текст в `.c` и `.rc` — CP1251 байты; Edit tool запрещён для таких файлов — только Python `rb`/`wb`
- **Окончания строк**: `file_basic_info.c` / `file_standart_info.c` / `main.c` — CRLF; `file_stream_info.c` / `fileinfo.rc` — LF
- **COM в C vs C++**: Debug (C) — `pObj->lpVtbl->Method(pObj, args)` + `&CLSID_*`; Release (C++) — `pObj->Method(args)` + `CLSID_*` без `&`
- **Timezone**: `SystemTimeToTzSpecificLocalTime` / `TzSpecificLocalTimeToSystemTime` (DST-корректно, вместо `FileTimeToLocalFileTime`)
- **Размеры файла**: `StrFormatByteSize64` из shlwapi.lib — только сокращённый вид («1,15 ГБ»)
- **Владелец файла**: `GetSecurityInfo` + `LookupAccountSid` → `IDC_FILE_OWNER` (1092) в FILE_BASIC_INFO
- **Фоновый I/O потоков**: `private_StreamCopyThread` + `private_RunStreamCopy` с `IProgressDialog` (CLSCTX_INPROC_SERVER)
- **Валидация имени ADS**: запрещены `: / \ * ? " < > |` и имена с `$`
- **F5 refresh**: WM_KEYDOWN в MainDialog → `private_SetFileHandle(hDlg, hTabCtrl, hFile, lpstrFileName)`
- **STRINGTABLE**: все UI-строки в ресурсах (IDS_*), загрузка через `ResStr()` — ротирующий буфер из 8 слотов

## Известные проблемы

- Нет обработки ERROR_MORE_DATA в file_stream_info.c при большом числе потоков
- README.md устаревший
- Нет лицензии
- `common_CreateSecurityAttributes` / `common_FreeSecurityAttributes` — глобальное состояние (не thread-safe)
