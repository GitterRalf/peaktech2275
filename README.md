# PeakTech 2275

Windows-Anwendung zur seriellen Steuerung der programmierbaren elektronischen Last PeakTech 2275. Sie zeigt Spannung, Strom und Leistung an und bietet allgemeine Laststeuerung, Batterie- und Listentests.

Entwickelt und gepflegt von Ralf Tralow mit Unterstützung von OpenAI Codex.

## Bildschirmaufnahmen

| Hell | Dunkel |
| --- | --- |
| <a href="docs/screenshots/peaktech2275-hell.png"><img src="docs/screenshots/peaktech2275-hell.png" alt="PeakTech 2275 in der hellen Ansicht" width="400"></a> | <a href="docs/screenshots/peaktech2275-dunkel.png"><img src="docs/screenshots/peaktech2275-dunkel.png" alt="PeakTech 2275 in der dunklen Ansicht" width="400"></a> |

## Installation unter Windows

Das [GitHub-Release](https://github.com/GitterRalf/peaktech2275/releases/latest) enthält die Setups für x86 und x64 mit Qt- und MinGW-Laufzeitdateien. Der Standardordner für x64 ist `C:\Programme\PeakTech 2275`. Zum Download ist ein GitHub-Konto mit Zugriff auf das private Repository erforderlich. Falls ein USB-Seriell-Adapter einen Treiber benötigt, diesen separat installieren.

## Bedienung

- Unter **Einstellungen → Serielle Verbindung** den COM-Port, die Baudrate und die Geräteadresse passend zum Gerät einstellen. Voreinstellung sind **COM17**, **38.400 Baud** und Adresse **1**. Die Schnittstelle arbeitet mit 8 Datenbits, keiner Parität, einem Stoppbit und ohne Flusssteuerung.
- **Verbinden** liest Gerätekennung, Messwerte und Geräteeinstellungen. Unter **Allgemeine Bedienung** stehen die Konstant-Modi CC, CV, CP und CR sowie Sollwert und Lastgrenzen zur Wahl.
- Unter **Batterietest** werden der Entladevorgang und sein Messverlauf angezeigt. **Listentest** dient zum Bearbeiten, Übertragen und Ausführen von Prüfschritten.
- Das Sprachsymbol bietet Deutsch, English und Français. Unter **Einstellungen → Darstellung** stehen Hell und Dunkel zur Wahl. Fensterposition und -größe werden beim nächsten Start wiederhergestellt.
- **Über** zeigt die Version und das animierte OsciViewer-Logo. **Über → Nach Updates suchen** startet die manuelle Prüfung. Für das private Repository muss die GitHub CLI mit einem berechtigten Konto angemeldet sein (`gh auth login`).

## Bauen

Benötigt werden ein Windows-Kit mit Qt 6 für x64 oder Qt 5.15 für x86 einschließlich Widgets, Network, Serial Port, Charts und Linguist Tools, CMake ab 3.24 und Ninja.

~~~powershell
.\build.ps1                    # x64
.\build.ps1 -Architecture x86  # x86
~~~

Das Skript baut `build\PeakTech2275.exe` und stellt die Laufzeit-DLLs bereit. Danach `PeakTech2275.iss` mit Inno Setup kompilieren. Es übernimmt die Version aus `CMakeLists.txt` und das Icon aus `assets/app.rc` und legt die Setupdatei unter `installer` ab.


### Windows-Architekturen

Die x86-Ausgabe verwendet Qt 5.15.19 mit MinGW 8.1 und hat einen eigenen
Installationsordner **PeakTech 2275 (32 Bit)**. Auf 64-Bit-Windows können beide
Ausgaben parallel installiert werden. Die x64-Ausgabe verwendet weiterhin
Qt 6 und ist beim Build der Standard.

Der Build für x86 nutzt `build-x86`, der Build für x64 den Ordner `build`.
Die Installer entstehen im Projektordner mit folgenden Aufrufen.

```powershell
ISCC.exe PeakTech2275.iss
ISCC.exe /DBuildArch=x86 PeakTech2275.iss
```

Für den x86-Build wird Qt Charts aus dem Qt-5.15.19-Kit oder einem separat
mit MinGW x86 gebauten Modul benötigt. Für einen separaten Build erwartet
CMake die öffentlichen und generierten Header unter `.deps/qtcharts-x86/include`,
die Importbibliothek unter `.deps/qtcharts-x86/lib` und die DLL unter
`.deps/qtcharts-x86/bin` oder `.deps/qtcharts-x86/lib`. Der Pfad lässt sich
bei direkter CMake-Konfiguration über `-DQT5_CHARTS_ROOT=…` anpassen.

## Lizenz

Der Projektquellcode und das animierte OsciViewer-Logo stehen unter der [MIT-Lizenz](LICENSE). Für die ausgelieferte Anwendung mit Qt Charts gilt GPLv3. Die [Drittanbieter-Hinweise](THIRD-PARTY-NOTICES.txt) nennen die jeweiligen Lizenztexte und die passenden Quellarchive.
