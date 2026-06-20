# MEMO — fileinfo

Утилита для просмотра и редактирования метаданных файлов Windows (Win32 GUI, C/C++).

## Назначение

Позволяет открыть любой файл или папку и изучить/изменить:
- **Основные** — атрибуты файла (readonly, hidden, system, …), временны́е метки (creation, access, write, change)
- **Стандартные** — размер на диске, размер файла, кол-во жёстких ссылок, флаги directory/delete-pending
- **Потоки** — NTFS alternate data streams (список, просмотр)
- **Идентификаторы** — volume serial number, 128-bit file unique ID (FileIdInfo, требует Windows 8+)
- **EXIF** — заглушка, планируется интеграция с Exiv2

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
| `main.c` | WinMain, MainDialog, открытие файла, drag-and-drop, tab switching |
| `common.c/h` | CreateSecurityAttributes (ACL для Administrators), FormatMessage-ошибки |
| `file_basic_info.c/h` | Атрибуты + DateTimePicker для 4 временных меток, сохранение через SetFileInformationByHandle |
| `file_standart_info.c/h` | Отображение FILE_STANDARD_INFO (read-only) |
| `file_stream_info.c/h` | ListView NTFS-потоков, контекстное меню (просмотр / создать) |
| `file_id_info.c/h` | Volume serial + 128-bit file ID в hex |
| `file_exif_info.c/h` | Заглушка под будущую Exiv2-интеграцию |
| `resource.h` | Все IDC_* / IDM_* константы |

## Сборка

- Visual Studio 2019+ (toolset v142), Win32 только
- Windows SDK 10.0.18362.0
- Конфигурации: Debug|Win32, Release|Win32
- Зависимости: comctl32.lib, advapi32.lib
- Путь к Exiv2 прописан в .vcxproj (3rdParty\exiv2\), библиотека не подключена
- **CI**: GitHub Actions → `.github/workflows/msbuild.yml` (Release|Win32)

## Известные проблемы и TODO

### Нереализовано
- [x] EXIF-вкладка — реализована через Exiv2 (см. «Подключение Exiv2» ниже)
- [ ] Создание нового NTFS-потока (IDM_CREATE_STREAM — обработчик пустой)
- [ ] x64 конфигурация сборки отсутствует

### Подключение Exiv2 (EXIF-поддержка)

1. Склонируйте и соберите [Exiv2](https://github.com/Exiv2/exiv2) для Win32:
   ```
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -A Win32
   cmake --build build --config Release
   ```
2. Скопируйте результат:
   - Заголовки → `3rdParty\exiv2\include\`
   - Импорт-библиотека → `3rdParty\exiv2\lib\exiv2.lib`
   - DLL → рядом с `fileinfo.exe`
3. В `fileinfo.vcxproj` в секции Release `PreprocessorDefinitions` добавьте `EXIV2_AVAILABLE`.
4. Там же раскомментируйте `<AdditionalDependencies>exiv2.lib;...</AdditionalDependencies>`.
5. Пересоберите: `msbuild /p:Configuration=Release`

Без Exiv2 вкладка показывает сообщение-заглушку — CI и Debug-сборки работают без изменений.

### Технический долг
- Компиляция Debug=C, Release=C++ (несогласованность в .vcxproj)
- `common_CreateSecurityAttributes` / `common_FreeSecurityAttributes` — глобальное состояние (не thread-safe, задокументировано)
- Нет обработки ERROR_MORE_DATA в file_stream_info.c при большом числе потоков
- README.md пустой (только заголовок + TODO про EXIF)
- Нет лицензии

### История
- 2017: основная разработка (все вкладки, drag-and-drop, admin elevation)
- 2023: настройка GitHub Actions CI

## Улучшения (предложения)

Все улучшения реализованы:

1. ~~**Exiv2**~~ — реализовано, см. «Подключение Exiv2» выше
2. ~~**x64**~~ — добавлены конфигурации `Debug|x64` и `Release|x64` (выход: `Release_x64\`, `Debug_x64\`)
3. ~~**ERROR_MORE_DATA**~~ — `fssi_WindowHandler` теперь удваивает буфер и повторяет вызов
4. ~~**Документация**~~ — написан полноценный `README.md`
5. ~~**Лицензия**~~ — добавлен `LICENSE` (MIT)
