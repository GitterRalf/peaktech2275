#pragma once

#include "peaktech.h"

#include <QLocale>
#include <QMainWindow>
#include <QSettings>
#include <QThread>
#include <QTranslator>

#include <array>

class QAction;
class QChart;
class QChartView;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineSeries;
class QMenu;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QValueAxis;
class ListTestPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class BatteryStatus { Ready, Running, Cutoff, Stopped, DeviceStopped, Fault, Disconnected };

    QWidget *createGeneralPage();
    QWidget *createBatteryPage();
    QGroupBox *createLoadSetup();
    void createMenus();
    void showAbout();
    void checkForUpdates();
    void checkPrivateRelease();
    void finishUpdateCheck(const QByteArray &data, const QString &error = {});
    void setDarkTheme(bool dark);
    void updateLanguage(const QString &language);
    void retranslate();
    void editConnection();
    void toggleConnection();
    void dispatch(std::function<void(PeakTech &)> operation);
    void onConnectionChanged(const QString &identity);
    void updateReading(const Reading &reading);
    void showLoadSetup(const LoadSetup &setup);
    void updateControls();
    void updateSetpointRange();
    void showError(const QString &error, bool offUnconfirmed);
    void resetBattery();
    void recordSample(const Reading &reading);
    void updateBatteryStatus();
    QString loadStatus(int state) const;
    QString spinNumber(double value, int precision) const;

    QSettings m_preferences;
    SerialSettings m_serial;
    QLocale m_locale;
    QTranslator m_translator;
    QTranslator m_qtTranslator;
    QThread m_deviceThread;
    PeakTech *m_device;
    QString m_identity;
    Reading m_reading;
    bool m_busy = false;
    bool m_closing = false;
    bool m_basicApplied = false;
    bool m_batteryApplied = false;
    bool m_loadSetupReady = false;
    BatteryStatus m_batteryStatus = BatteryStatus::Ready;
    int m_testFault = 0;
    double m_lowVoltage = 0;
    double m_highVoltage = 0;

    QMenu *m_settingsMenu;
    QMenu *m_languageMenu;
    QMenu *m_aboutMenu;
    QAction *m_aboutApplicationAction;
    QAction *m_aboutQtAction;
    QAction *m_updateAction;
    QMenu *m_appearanceMenu;
    QAction *m_connectionAction;
    QAction *m_lightThemeAction;
    QAction *m_darkThemeAction;
    QLabel *m_connectionStatus;
    QLabel *m_serialStatus;
    QLabel *m_notice;
    QPushButton *m_connectionButton;
    QTabWidget *m_pages;

    QGroupBox *m_liveGroup;
    QGroupBox *m_generalGroup;
    QLabel *m_voltageLabel;
    QLabel *m_currentLabel;
    QLabel *m_powerLabel;
    QLabel *m_liveVoltage;
    QLabel *m_liveCurrent;
    QLabel *m_livePower;
    QLabel *m_loadLabel;
    QLabel *m_loadStatus;
    QLabel *m_modeLabel;
    QLabel *m_setpointLabel;
    QComboBox *m_mode;
    QDoubleSpinBox *m_setpoint;
    QPushButton *m_basicApply;
    QPushButton *m_loadOn;
    QPushButton *m_loadOff;

    QGroupBox *m_loadSetup;
    std::array<QLabel *, 5> m_loadSetupLabels;
    std::array<QDoubleSpinBox *, 5> m_loadSetupValues;
    QCheckBox *m_autoOff;
    QSpinBox *m_autoOffDelay;
    QPushButton *m_loadSetupApply;

    QGroupBox *m_curveGroup;
    QGroupBox *m_tableGroup;
    QGroupBox *m_parameterGroup;
    QLabel *m_batteryStatusLabel;
    QLabel *m_batteryState;
    QLabel *m_capacityLabel;
    QLabel *m_capacity;
    QLabel *m_durationLabel;
    QLabel *m_duration;
    QLabel *m_dischargeLabel;
    QLabel *m_cutoffLabel;
    QLabel *m_periodLabel;
    QDoubleSpinBox *m_discharge;
    QDoubleSpinBox *m_cutoff;
    QSpinBox *m_period;
    QPushButton *m_batteryApply;
    QPushButton *m_batteryStart;
    QPushButton *m_batteryStop;
    QChart *m_chart;
    QChartView *m_chartView;
    QLineSeries *m_series;
    QValueAxis *m_xAxis;
    QValueAxis *m_yAxis;
    QTableWidget *m_table;
    ListTestPage *m_listPage;
};
