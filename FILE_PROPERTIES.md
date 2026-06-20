# Встраивание fileinfo в окно File Properties Windows

## Цель

Добавить вкладки «Основные», «Стандартные», «Потоки», «Идентификаторы» и «EXIF»
прямо в системное окно **Свойства файла** (правая кнопка → Свойства) в Explorer,
не открывая отдельное приложение.

---

## Механизм: Shell Property Sheet Extension

Windows позволяет добавлять вкладки в диалог Свойств через COM-интерфейс
**IShellPropSheetExt**. Расширение регистрируется в реестре — Explorer при открытии
Свойств загружает нужные DLL и вызывает `AddPages()`, которая передаёт страницы
в общий диалог.

Это **In-Process COM Server** — DLL, загружаемая прямо в адресное пространство Explorer.

---

## Часть 1 — Архитектурные изменения

### 1.1 EXE → DLL

Текущий проект собирает `.exe`. Для Shell Extension нужна `.dll`.

**Изменить в `fileinfo.vcxproj`:**

```xml
<!-- Было -->
<ConfigurationType>Application</ConfigurationType>

<!-- Надо -->
<ConfigurationType>DynamicLibrary</ConfigurationType>
```

Точка входа `WinMain` заменяется на `DllMain`.

### 1.2 Новая структура проекта

```
fileinfo/
├── shell_ext.cpp          ← НОВЫЙ: COM-объект (IShellExtInit + IShellPropSheetExt)
├── shell_ext.h            ← НОВЫЙ: объявление класса CFileInfoShellExt
├── class_factory.cpp      ← НОВЫЙ: IClassFactory, DllGetClassObject
├── class_factory.h
├── dll_exports.cpp        ← НОВЫЙ: DllMain, DllRegisterServer, DllUnregisterServer,
│                                   DllCanUnloadNow
├── fileinfo.def           ← НОВЫЙ: экспорт символов DLL
├── guid.h                 ← НОВЫЙ: CLSID_FileInfoShellExt (один сгенерированный GUID)
│
│   (существующие файлы — почти без изменений)
├── file_basic_info.c/h
├── file_standart_info.c/h
├── file_stream_info.c/h
├── file_id_info.c/h
├── file_exif_info.c/h
├── common.c/h
└── resource.h / fileinfo.rc
```

`main.c` и логика `WinMain`/`MainDialog` **удаляются** или переносятся в отдельный
`fileinfo_standalone.exe` (сохранить как отдельный проект при желании).

### 1.3 DEF-файл экспортов

```
; fileinfo.def
LIBRARY fileinfo
EXPORTS
    DllGetClassObject   PRIVATE
    DllCanUnloadNow     PRIVATE
    DllRegisterServer   PRIVATE
    DllUnregisterServer PRIVATE
```

---

## Часть 2 — COM-реализация

### 2.1 GUID

Сгенерировать один раз через `uuidgen` или Visual Studio:

```c
// guid.h
// {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} — сгенерировать реальный GUID
DEFINE_GUID(CLSID_FileInfoShellExt,
    0xXXXXXXXX, 0xXXXX, 0xXXXX,
    0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX);
```

### 2.2 COM-объект `CFileInfoShellExt`

```cpp
// shell_ext.h
class CFileInfoShellExt :
    public IShellExtInit,
    public IShellPropSheetExt
{
public:
    CFileInfoShellExt();
    virtual ~CFileInfoShellExt();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IShellExtInit
    STDMETHODIMP Initialize(LPCITEMIDLIST pidlFolder,
                            IDataObject   *pdtobj,
                            HKEY           hkeyProgID);

    // IShellPropSheetExt
    STDMETHODIMP AddPages(LPFNADDPROPSHEETPAGE lpfnAddPage, LPARAM lParam);
    STDMETHODIMP ReplacePage(UINT, LPFNADDPROPSHEETPAGE, LPARAM) { return E_NOTIMPL; }

private:
    LONG  m_refCount;
    WCHAR m_szFilePath[MAX_PATH];   // путь, извлечённый в Initialize()
    HANDLE m_hFile;                 // HANDLE, открытый в AddPages()
};
```

### 2.3 Извлечение пути файла в `Initialize()`

```cpp
STDMETHODIMP CFileInfoShellExt::Initialize(
    LPCITEMIDLIST pidlFolder, IDataObject *pdtobj, HKEY hkeyProgID)
{
    if (!pdtobj) return E_INVALIDARG;

    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = { 0 };

    HRESULT hr = pdtobj->GetData(&fmt, &stg);
    if (FAILED(hr)) return hr;

    HDROP hDrop = (HDROP)stg.hGlobal;
    UINT cFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

    hr = (cFiles > 0) ? S_OK : E_FAIL;
    if (SUCCEEDED(hr))
        DragQueryFileW(hDrop, 0, m_szFilePath, ARRAYSIZE(m_szFilePath));

    ReleaseStgMedium(&stg);
    return hr;
}
```

> **Важно:** `IDataObject` действителен только во время вызова `Initialize()`.
> Путь копируется в `m_szFilePath`, указатель не сохраняется.

### 2.4 Создание страниц в `AddPages()`

Каждая существующая вкладка становится одной страницей `PROPSHEETPAGE`.
Диалог-ресурс, `DLGPROC` и логика остаются прежними.

```cpp
STDMETHODIMP CFileInfoShellExt::AddPages(
    LPFNADDPROPSHEETPAGE lpfnAddPage, LPARAM lParam)
{
    // Открываем файл один раз для всех вкладок
    m_hFile = CreateFileW(m_szFilePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
        NULL);

    // Таблица вкладок — повторяет g_TabInfoCtrl из main.c
    static const struct {
        LPCWSTR  pszTitle;
        LPCWSTR  pszTemplate;   // имя ресурса диалога
        DLGPROC  pfnProc;
    } kPages[] = {
        { L"Основные",       L"FILE_BASIC_INFO",    fbi_WindowHandler  },
        { L"Стандартные",    L"FILE_STANDART_INFO", fsi_WindowHandler  },
        { L"Потоки",         L"FILE_STREAM_INFO",   fssi_WindowHandler },
        { L"Идентификаторы", L"FILE_ID_INFO",       fii_WindowHandler  },
        { L"EXIF",           L"FILE_EXIF_INFO",     fxi_WindowHandler  },
    };

    for (int i = 0; i < ARRAYSIZE(kPages); ++i) {
        // Контекст страницы — передаёт this и hFile в pfnDlgProc
        PageContext *pCtx = new PageContext { this, m_hFile, m_szFilePath };

        PROPSHEETPAGEW psp  = { 0 };
        psp.dwSize          = sizeof(psp);
        psp.dwFlags         = PSP_USETITLE | PSP_USEREFPARENT;
        psp.hInstance       = g_hDllInstance;
        psp.pszTemplate     = kPages[i].pszTemplate;
        psp.pszTitle        = kPages[i].pszTitle;
        psp.pfnDlgProc      = kPages[i].pfnProc;
        psp.lParam          = (LPARAM)pCtx;
        psp.pfnCallback     = PageCallback;   // освобождает pCtx при уничтожении
        psp.pcRefParent     = (UINT *)&m_refCount;  // продлевает жизнь объекта

        HPROPSHEETPAGE hPage = CreatePropertySheetPageW(&psp);
        if (hPage) {
            if (!lpfnAddPage(hPage, lParam))
                DestroyPropertySheetPage(hPage);
        }
    }
    return S_OK;
}
```

### 2.5 Адаптация диалог-процедур

Существующие `fbi_WindowHandler`, `fssi_WindowHandler` и остальные получают
`HANDLE` через `WM_SETFILE_HANDLE` — это не меняется.

Нужно добавить обработку `WM_INITDIALOG`, который теперь получает `lParam`
со структурой `PageContext`, а не `HINSTANCE`:

```cpp
// Добавить в начало каждого pfnDlgProc:
case WM_INITDIALOG: {
    PageContext *pCtx = (PageContext *)
        ((LPPROPSHEETPAGE)lParam)->lParam;   // вместо (HINSTANCE)lParam
    // Отправить WM_SETFILE_HANDLE / WM_SETFILE_NAME самому себе
    SendMessage(hDlg, WM_SETFILE_HANDLE, 0, (LPARAM)pCtx->hFile);
    SendMessage(hDlg, WM_SETFILE_NAME,   0, (LPARAM)pCtx->szFilePath);
    ...
}
```

Дополнительно: для вкладки «Основные» (редактирование атрибутов) нужно перехватить
уведомление `PSN_APPLY` (кнопка «Применить»):

```cpp
case WM_NOTIFY: {
    LPNMHDR pnm = (LPNMHDR)lParam;
    if (pnm->code == PSN_APPLY) {
        // Сохранить изменения (аналог IDC_FILEINFO_SAVE)
        // SetWindowLong(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
        return TRUE;
    }
}
```

### 2.6 `DllMain`

```cpp
HINSTANCE g_hDllInstance = NULL;
volatile LONG g_DllRefCount = 0;

BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hDllInstance = hInst;
        DisableThreadLibraryCalls(hInst);
    }
    return TRUE;
}
```

### 2.7 `DllRegisterServer` / `DllUnregisterServer`

```cpp
STDAPI DllRegisterServer()
{
    WCHAR szModule[MAX_PATH];
    GetModuleFileNameW(g_hDllInstance, szModule, ARRAYSIZE(szModule));

    // 1. Зарегистрировать CLSID → InprocServer32
    RegSet(HKEY_CLASSES_ROOT,
        L"CLSID\\{GUID}\\InprocServer32", NULL, szModule);
    RegSet(HKEY_CLASSES_ROOT,
        L"CLSID\\{GUID}\\InprocServer32", L"ThreadingModel", L"Apartment");

    // 2. Добавить в список PropertySheetHandlers
    RegSet(HKEY_CLASSES_ROOT,
        L"*\\shellex\\PropertySheetHandlers\\FileInfoExt", NULL, L"{GUID}");
    // Или для папок тоже:
    RegSet(HKEY_CLASSES_ROOT,
        L"AllFilesystemObjects\\shellex\\PropertySheetHandlers\\FileInfoExt",
        NULL, L"{GUID}");

    // 3. Уведомить Shell об изменении
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return S_OK;
}
```

---

## Часть 3 — Регистрация и установка

### 3.1 Ключи реестра (итого)

```
HKEY_CLASSES_ROOT
 └── CLSID
 │    └── {YOUR-GUID}
 │         ├── (default) = "FileInfo Shell Extension"
 │         └── InprocServer32
 │              ├── (default)      = "C:\...\fileinfo.dll"
 │              └── ThreadingModel = "Apartment"
 └── *
      └── shellex
           └── PropertySheetHandlers
                └── FileInfoExt = "{YOUR-GUID}"
```

### 3.2 Регистрация через `regsvr32`

```bat
:: Регистрация (с правами администратора)
regsvr32 fileinfo.dll

:: Удаление
regsvr32 /u fileinfo.dll
```

### 3.3 Одобрение расширения (Windows Vista+)

На Vista/7 Shell требует явного одобрения сторонних расширений:

```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved
    {YOUR-GUID} = "FileInfo Shell Extension"
```

Добавить в `DllRegisterServer()`.

---

## Часть 4 — 32-бит vs 64-бит

На 64-битных Windows Explorer работает как 64-битный процесс и **не загружает
32-битные DLL**. Нужны две сборки.

### 4.1 Схема с двумя GUID (рекомендуется)

| Сборка | GUID | Путь |
|--------|------|------|
| Win32  | `{GUID-32}` | `Program Files (x86)\fileinfo\fileinfo32.dll` |
| x64    | `{GUID-64}` | `Program Files\fileinfo\fileinfo64.dll` |

Оба регистрируются в реестре. 64-бит Explorer загружает x64, 32-бит — Win32.

### 4.2 Альтернатива: один GUID, регистрация по Wow6432Node

```
HKLM\SOFTWARE\Classes\CLSID\{GUID}\InprocServer32
    → 64-битная DLL

HKLM\SOFTWARE\Wow6432Node\Classes\CLSID\{GUID}\InprocServer32
    → 32-битная DLL
```

Проще в установке, но требует инсталлятора.

### 4.3 Изменения в `.vcxproj`

x64-конфигурации уже добавлены в предыдущих правках — достаточно сменить
`ConfigurationType` на `DynamicLibrary`.

---

## Часть 5 — UAC и права

### 5.1 Проблема

Shell Extension запускается в контексте Explorer **без прав администратора**.
Редактировать защищённые атрибуты (system-файлы и т.д.) не получится напрямую.

### 5.2 Решение: вспомогательный EXE-помощник

```
fileinfo.dll          ← Shell Extension (без прав)
fileinfo_helper.exe   ← Вспомогательный процесс с манифестом requireAdministrator
```

Схема взаимодействия:

```
Explorer (нет прав)
  └── fileinfo.dll
        ├── Читает метаданные — ОК (без прав)
        └── Нажата «Сохранить» →
              ShellExecuteExW(runas, fileinfo_helper.exe,
                             "/set-attr \"C:\file.txt\" 0x20")
              └── UAC-запрос
                  └── fileinfo_helper.exe применяет изменения
                      и завершается (exit code = 0 / 1)
```

Вкладки **Стандартные**, **Потоки**, **Идентификаторы**, **EXIF** — только чтение,
права не нужны.

Вкладка **Основные** (редактирование) — через helper при нажатии «Применить».

---

## Часть 6 — Изменения в системе сборки

### 6.1 Итоговые конфигурации `.vcxproj`

| Конфигурация | Тип | Выход |
|---|---|---|
| `Debug\|Win32`   | DynamicLibrary | `Debug\fileinfo.dll` |
| `Release\|Win32` | DynamicLibrary | `Release\fileinfo.dll` |
| `Debug\|x64`     | DynamicLibrary | `Debug_x64\fileinfo.dll` |
| `Release\|x64`   | DynamicLibrary | `Release_x64\fileinfo.dll` |

### 6.2 Дополнительные линкер-флаги

```xml
<Link>
  <ModuleDefinitionFile>fileinfo.def</ModuleDefinitionFile>
  <AdditionalDependencies>comctl32.lib;advapi32.lib;shell32.lib;%(AdditionalDependencies)</AdditionalDependencies>
</Link>
```

### 6.3 Обновление CI (msbuild.yml)

Добавить сборку x64 и шаг регистрации DLL для smoke-теста:

```yaml
- name: Build Win32
  run: msbuild /m /p:Configuration=Release /p:Platform=Win32

- name: Build x64
  run: msbuild /m /p:Configuration=Release /p:Platform=x64
```

---

## Часть 7 — Пошаговый план реализации

```
Шаг 1. Подготовка проекта
  1.1  Создать fileinfo_shell.vcxproj (DLL) рядом с fileinfo.vcxproj (EXE)
  1.2  Сгенерировать два GUID (Win32 и x64): uuidgen -s -c
  1.3  Создать guid.h с DEFINE_GUID

Шаг 2. COM-инфраструктура
  2.1  Написать dll_exports.cpp (DllMain, экспорты)
  2.2  Написать class_factory.cpp (IClassFactory)
  2.3  Написать shell_ext.cpp (IShellExtInit + IShellPropSheetExt)
  2.4  Написать fileinfo.def

Шаг 3. Адаптация диалог-процедур
  3.1  Изменить WM_INITDIALOG во всех *_WindowHandler: читать PageContext из lParam
  3.2  Добавить обработку PSN_APPLY в fbi_WindowHandler (сохранение атрибутов)
  3.3  Убрать зависимость от WM_SETFILE_HANDLE при инициализации (инициализировать
       сразу в WM_INITDIALOG из PageContext)

Шаг 4. Ресурсы
  4.1  Убедиться, что все диалоги (FILE_BASIC_INFO и т.д.) корректно работают
       как PropertySheet Page (стиль WS_CHILD обязателен — уже стоит)
  4.2  Удалить MAINDIALOG из .rc (или оставить для standalone EXE)

Шаг 5. DllRegisterServer / DllUnregisterServer
  5.1  Реализовать регистрацию CLSID и PropertySheetHandlers
  5.2  Добавить одобрение расширения (Approved key)
  5.3  Добавить SHChangeNotify

Шаг 6. Вспомогательный EXE для UAC
  6.1  Создать fileinfo_helper.exe (отдельный .vcxproj)
  6.2  Манифест: requestedExecutionLevel = requireAdministrator
  6.3  Парсинг аргументов командной строки: /set-attr /set-times и т.д.
  6.4  Вызов из fbi_WindowHandler вместо прямого SetFileInformationByHandle

Шаг 7. Тестирование
  7.1  regsvr32 fileinfo.dll
  7.2  Правая кнопка на любом файле → Свойства → проверить вкладки
  7.3  Тест редактирования атрибутов (UAC-запрос)
  7.4  Тест на файлах без EXIF, с большим числом потоков
  7.5  regsvr32 /u fileinfo.dll → убедиться, что вкладки исчезли

Шаг 8. Упаковка
  8.1  Installer (NSIS / WiX): копирует DLL, запускает regsvr32
  8.2  Опционально: подпись кода (EV certificate)
```

---

## Плюсы и минусы

### Плюсы

| # | Плюс | Описание |
|---|------|----------|
| 1 | **Нативная интеграция** | Вкладки появляются прямо в системном диалоге Properties — не нужно открывать отдельную утилиту |
| 2 | **Привычный UX** | Пользователь работает в знакомом интерфейсе Windows; кнопки «Применить» / «OK» / «Отмена» уже есть |
| 3 | **Переиспользование кода** | Все пять диалог-процедур остаются практически без изменений; логика чтения/записи метаданных не трогается |
| 4 | **Контекстный запуск** | Расширение получает путь к файлу автоматически — не нужно открывать браузер файлов |
| 5 | **Работает для папок** | `AllFilesystemObjects` позволяет показывать вкладки и для директорий |
| 6 | **Стандартный механизм** | IShellPropSheetExt — официальный задокументированный API, не хак |

### Минусы и проблемы

| # | Проблема | Серьёзность | Пояснение |
|---|----------|-------------|-----------|
| 1 | **Крах Explorer при ошибке в DLL** | 🔴 Критическая | In-process COM: необработанное исключение в `AddPages()` или диалог-процедуре роняет Explorer целиком. Требуется тщательная обработка всех ошибок и SEH (`__try/__except`) |
| 2 | **32-бит / 64-бит** | 🔴 Критическая | На 64-битных Windows нужны две отдельные DLL с раздельной регистрацией. Без x64-сборки расширение невидимо в стандартном Explorer |
| 3 | **UAC для редактирования** | 🟠 Высокая | Shell Extension работает без прав администратора. Изменение атрибутов и временны́х меток для защищённых файлов требует отдельного helper-процесса с UAC-запросом — заметно усложняет архитектуру |
| 4 | **Подпись кода** | 🟠 Высокая | На Windows 10/11 неподписанные Shell Extensions вызывают предупреждения SmartScreen или блокируются групповой политикой в корпоративной среде. EV-сертификат стоит дорого |
| 5 | **Производительность открытия Properties** | 🟡 Средняя | Каждый вызов Properties на любом файле инициализирует все зарегистрированные расширения. Если `Initialize()` или `AddPages()` медленные (напр., при медленном диске / сетевом пути), вся задержка ощущается пользователем |
| 6 | **Совместимость ThreadingModel** | 🟡 Средняя | Все диалог-процедуры должны работать в STA-апартаменте Explorer. Нельзя использовать фоновые потоки без правильного маршаллинга. Текущий код использует статические переменные в диалог-процедурах — это **сломается** при одновременно открытых нескольких окнах Свойств |
| 7 | **Статические переменные в диалог-процедурах** | 🟡 Средняя | Все `static HANDLE hFile`, `static FILE_BASIC_INFO fbi` и т.д. — глобальное состояние. При одновременном открытии Properties двух файлов вкладки будут показывать данные друг друга. Нужен рефакторинг: вынести состояние в `PageContext` |
| 8 | **Lifetime управление** | 🟡 Средняя | COM-объект должен жить дольше диалога. Неправильный `AddRef`/`Release` — либо утечка памяти, либо use-after-free. Нужна осторожная реализация `pcRefParent` в PROPSHEETPAGE |
| 9 | **Установка требует прав** | 🟡 Средняя | `regsvr32` и запись в `HKEY_CLASSES_ROOT` требуют прав администратора. Для пользователей без прав можно зарегистрировать в `HKCU` (per-user регистрация), но это сложнее |
| 10 | **Отладка затруднена** | 🟢 Низкая | Для отладки нужно подключаться к процессу `explorer.exe`. Перекомпиляция DLL требует остановки Explorer или перезагрузки. Рабочий обход: `taskkill /f /im explorer.exe && regsvr32 fileinfo.dll && explorer.exe` |
| 11 | **Сторонние файловые менеджеры** | 🟢 Низкая | Total Commander, Far Manager и другие менеджеры часто не вызывают стандартный диалог Properties и не покажут вкладки |
| 12 | **Windows Sandbox / AppContainer** | 🟢 Низкая | В Microsoft Store-приложениях и AppContainer-окружениях сторонние Shell Extensions не загружаются |

---

## Итоговое резюме

### Когда реализовывать стоит

- Утилита предназначена для **повседневного использования** опытными пользователями
  или системными администраторами, которые часто открывают Properties
- Целевая аудитория знакома с установкой Shell Extensions
- Есть возможность подписать код сертификатом

### Когда лучше оставить как EXE

- Приоритет — **простота распространения** (один `.exe`, без регистрации, без прав)
- Важна стабильность Explorer (любой баг в DLL роняет оболочку)
- Нет ресурсов на поддержку двух сборок (Win32 / x64)

### Рекомендуемый компромисс

Реализовать **оба варианта** в одном репозитории:

```
fileinfo.exe       ← standalone утилита (текущий проект)
fileinfo.dll       ← Shell Extension (новый проект в том же .sln)
fileinfo_helper.exe ← UAC helper для Shell Extension
```

Пользователь выбирает: запустить утилиту напрямую или установить расширение.
Общий код (`file_basic_info.c`, `file_stream_info.c` и т.д.) используется в обоих.
