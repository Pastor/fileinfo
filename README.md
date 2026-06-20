# fileinfo

Windows-утилита для просмотра и редактирования расширенных метаданных файлов и папок.

## Возможности

| Вкладка | Описание |
|---------|----------|
| **Основные** | Чтение и редактирование атрибутов файла (Archive, Hidden, ReadOnly, System, Compressed, Encrypted, Sparse…) и четырёх временны́х меток (Creation, LastAccess, LastWrite, Change) |
| **Стандартные** | Размер файла и занятое место на диске, количество жёстких ссылок, флаги Directory / Delete pending |
| **Потоки** | Список NTFS Alternate Data Streams с именами, размерами и выделенным пространством |
| **Идентификаторы** | Серийный номер тома и уникальный 128-битный FileID (требует Windows 8+) |
| **EXIF** | EXIF / IPTC / XMP метаданные через библиотеку [Exiv2](https://github.com/Exiv2/exiv2) *(требует сборки с `EXIV2_AVAILABLE`)* |

Дополнительно:
- Открытие файлов через диалог «Обзор» или **перетаскивание** (drag-and-drop)
- Кнопка **«Перезапуск от администратора»** для редактирования защищённых файлов

## Требования

- Windows 7 и выше (некоторые функции требуют Windows 8+)
- Visual Studio 2019 / 2022 (toolset v142)
- Windows SDK 10.0.18362.0 или новее

## Сборка

### Быстрый старт

```bat
git clone https://github.com/Pastor/fileinfo.git
cd fileinfo
msbuild fileinfo.sln /p:Configuration=Release /p:Platform=x64
```

Исполняемый файл окажется в `Release_x64\fileinfo.exe`.

### Конфигурации

| Конфигурация | Платформа | Описание |
|---|---|---|
| `Debug` | `Win32` | Отладочная сборка, 32-бит |
| `Release` | `Win32` | Оптимизированная, 32-бит |
| `Debug` | `x64` | Отладочная сборка, 64-бит |
| `Release` | `x64` | Оптимизированная, 64-бит |

### Сборка через Visual Studio

Откройте `fileinfo.sln` в Visual Studio 2019/2022, выберите конфигурацию и нажмите **Build → Build Solution**.

## Подключение EXIF-поддержки (Exiv2)

По умолчанию вкладка **EXIF** показывает заглушку. Для полной поддержки метаданных:

1. Клонируйте и соберите [Exiv2](https://github.com/Exiv2/exiv2):

   ```bat
   git clone https://github.com/Exiv2/exiv2.git
   cd exiv2
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -A x64
   cmake --build build --config Release
   ```

2. Скопируйте результат сборки:
   - Заголовки → `3rdParty\exiv2\include\`
   - Импорт-библиотека Win32 → `3rdParty\exiv2\lib\exiv2.lib`
   - Импорт-библиотека x64  → `3rdParty\exiv2\lib64\exiv2.lib`
   - DLL → рядом с `fileinfo.exe`

3. В `fileinfo\fileinfo.vcxproj` в нужной конфигурации:
   - Добавьте `EXIV2_AVAILABLE` в `PreprocessorDefinitions`
   - Раскомментируйте строку с `exiv2.lib` в `AdditionalDependencies`

4. Пересоберите проект.

## Структура проекта

```
fileinfo/
├── .github/workflows/msbuild.yml   — CI (GitHub Actions)
├── fileinfo.sln                    — Solution файл Visual Studio
├── MEMO.md                         — Технические заметки для разработчика
├── LICENSE                         — MIT лицензия
└── fileinfo/
    ├── main.c                      — WinMain, главный диалог, drag-and-drop
    ├── common.c/h                  — Утилиты (security attributes, ошибки)
    ├── file_basic_info.c/h         — Атрибуты и временны́е метки
    ├── file_standart_info.c/h      — Стандартная информация о файле
    ├── file_stream_info.c/h        — NTFS потоки
    ├── file_id_info.c/h            — Идентификаторы файла
    ├── file_exif_info.c/h          — EXIF/IPTC/XMP (Exiv2)
    ├── resource.h                  — Идентификаторы ресурсов
    ├── fileinfo.rc                 — Ресурсный файл (диалоги, иконки)
    └── fileinfo.vcxproj            — Проект Visual Studio
```

## TODO

- [ ] Реализовать создание NTFS-потоков (кнопка «Создать» на вкладке «Потоки»)

## Лицензия

[MIT](LICENSE) © Андрей Хлебников
