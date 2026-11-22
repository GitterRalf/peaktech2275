#include "mainwindow.h"
#include "listtestpage.h"

#include <QActionGroup>
#include <QApplication>
#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineSeries>
#include <QMenuBar>
#include <QMessageBox>
#include <QMovie>
#include <QPalette>
#include <QPainter>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QValueAxis>

#include <algorithm>
#include <cmath>

namespace {
const QStringList modes = {QStringLiteral("CC"), QStringLiteral("CV"),
                           QStringLiteral("CP"), QStringLiteral("CR")};
const QString dash = QStringLiteral("—");

struct LoadSetupField {
    const char *name;
    const char *label;
    const char *suffix;
    double maximum;
    double LoadSetup::*value;
};

constexpr LoadSetupField loadSetupFields[] = {
    {"maxVoltage", QT_TRANSLATE_NOOP("MainWindow", "Max. Spannung"), " V", 360, &LoadSetup::maxVoltage},
    {"maxCurrent", QT_TRANSLATE_NOOP("MainWindow", "Max. Strom"), " A", 30, &LoadSetup::maxCurrent},
    {"maxPower", QT_TRANSLATE_NOOP("MainWindow", "Max. Leistung"), " W", 150, &LoadSetup::maxPower},
    {"onVoltage", QT_TRANSLATE_NOOP("MainWindow", "Einschaltspannung"), " V", 360, &LoadSetup::onVoltage},
    {"offVoltage", QT_TRANSLATE_NOOP("MainWindow", "Ausschaltspannung"), " V", 360, &LoadSetup::offVoltage},
};

QIcon globeIcon(bool dark)
{
    QPixmap icon(24, 24);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(dark ? "#d4e3f5" : "#27425a"), 1.35));
    painter.drawEllipse(QRectF(3, 3, 18, 18));
    painter.drawEllipse(QRectF(8, 3, 8, 18));
    painter.drawLine(QPointF(3, 12), QPointF(21, 12));
    painter.drawLine(QPointF(5, 7.5), QPointF(19, 7.5));
    painter.drawLine(QPointF(5, 16.5), QPointF(19, 16.5));
    return QIcon(icon);
}

void addMetric(QGridLayout *grid, int column, QLabel *label, QLabel *value)
{
    label->setProperty("metric", "label");
    value->setProperty("metric", "value");
    grid->addWidget(label, 0, column);
    grid->addWidget(value, 1, column);
    grid->setColumnStretch(column, 1);
}

void labelFor(QLabel *label, QWidget *control)
{
    label->setBuddy(control);
}
}

MainWindow::MainWindow()
    : m_device(new PeakTech)
{
    m_serial.port = m_preferences.value(QStringLiteral("serial/port"), m_serial.port).toString();
    m_serial.baud = m_preferences.value(QStringLiteral("serial/baud"), m_serial.baud).toInt();
    m_serial.address = m_preferences.value(QStringLiteral("serial/address"), m_serial.address).toInt();

    setWindowTitle(QStringLiteral("PeakTech 2275"));
    resize(1140, 730);
    setMinimumSize(920, 630);

    auto *root = new QWidget;
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);
    auto *banner = new QWidget;
    banner->setObjectName(QStringLiteral("banner"));
    auto *bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(18, 13, 18, 13);
    auto *device = new QVBoxLayout;
    device->setSpacing(2);
    auto *hardwareTitle = new QLabel(QStringLiteral("PeakTech 2275"));
    hardwareTitle->setObjectName(QStringLiteral("hardwareTitle"));
    m_serialStatus = new QLabel;
    device->addWidget(hardwareTitle);
    device->addWidget(m_serialStatus);
    bannerLayout->addLayout(device);
    bannerLayout->addStretch();
    m_connectionStatus = new QLabel;
    m_connectionStatus->setObjectName(QStringLiteral("connectionStatus"));
    bannerLayout->addWidget(m_connectionStatus);
    bannerLayout->addSpacing(12);
    m_connectionButton = new QPushButton;
    m_connectionButton->setObjectName(QStringLiteral("primary"));
    bannerLayout->addWidget(m_connectionButton);
    layout->addWidget(banner);

    m_notice = new QLabel;
    m_notice->setObjectName(QStringLiteral("notice"));
    m_notice->setWordWrap(true);
    m_notice->hide();
    layout->addWidget(m_notice);

    m_pages = new QTabWidget;
    m_pages->addTab(createGeneralPage(), QString());
    m_pages->addTab(createBatteryPage(), QString());
    m_listPage = new ListTestPage;
    m_pages->addTab(m_listPage, QString());
    layout->addWidget(m_pages, 1);
    setCentralWidget(root);
    createMenus();
    setDarkTheme(m_preferences.value(QStringLiteral("appearance/dark"), true).toBool());

    qRegisterMetaType<Reading>();
    qRegisterMetaType<LoadSetup>();
    qRegisterMetaType<TestEnd>();
    qRegisterMetaType<ListProgram>();
    m_device->moveToThread(&m_deviceThread);
    connect(&m_deviceThread, &QThread::finished, m_device, &QObject::deleteLater);
    connect(m_device, &PeakTech::connectionChanged, this, &MainWindow::onConnectionChanged);
    connect(m_device, &PeakTech::readingChanged, this, &MainWindow::updateReading);
    connect(m_device, &PeakTech::loadSetupRead, this, &MainWindow::showLoadSetup);
    connect(m_device, &PeakTech::basicRead, this, [this](const QString &mode, double value) {
        m_mode->setCurrentIndex(modes.indexOf(mode));
        m_setpoint->setValue(value);
        m_basicApplied = std::abs(m_setpoint->value() - value) < 0.00051;
    });
    connect(m_device, &PeakTech::settingsApplied, this, [this](const QString &mode) {
        m_batteryApplied = mode == QStringLiteral("BAT");
        m_basicApplied = !m_batteryApplied;
        m_listPage->invalidate();
    });
    connect(m_device, &PeakTech::settingsInvalidated, this, [this] {
        m_basicApplied = m_batteryApplied = false;
    });
    connect(m_device, &PeakTech::testStarted, this, &MainWindow::resetBattery);
    connect(m_device, &PeakTech::sampleRecorded, this, &MainWindow::recordSample);
    connect(m_device, &PeakTech::testFinished, this, [this](TestEnd cause, int state) {
        switch (cause) {
        case TestEnd::Cutoff: m_batteryStatus = BatteryStatus::Cutoff; break;
        case TestEnd::Stopped: m_batteryStatus = BatteryStatus::Stopped; break;
        case TestEnd::DeviceStopped: m_batteryStatus = BatteryStatus::DeviceStopped; break;
        case TestEnd::Fault: m_batteryStatus = BatteryStatus::Fault; break;
        case TestEnd::Disconnected: m_batteryStatus = BatteryStatus::Disconnected; break;
        }
        m_testFault = state;
        updateBatteryStatus();
        updateControls();
    });
    connect(m_device, &PeakTech::operationFinished, this, [this](const QString &error, bool offUnconfirmed) {
        m_busy = false;
        if (!error.isEmpty()) {
            showError(error, offUnconfirmed);
            m_closing = false;
        }
        updateControls();
        if (m_closing)
            close();
    });
    connect(m_device, &PeakTech::communicationLost, this, &MainWindow::showError);
    connect(m_device, &PeakTech::listRead, m_listPage, &ListTestPage::setProgram);
    connect(m_device, &PeakTech::listUploaded, this, [this] {
        m_basicApplied = m_batteryApplied = false;
        m_listPage->markUploaded();
    });
    connect(m_device, &PeakTech::listStarted, this, [this] {
        m_listPage->beginTest();
        updateControls();
    });
    connect(m_device, &PeakTech::listFinished, this, [this](TestEnd cause, int state) {
        m_listPage->finishTest(cause, state);
        updateControls();
    });
    connect(m_listPage, &ListTestPage::readRequested, this, [this] {
        dispatch([](PeakTech &device) { device.readList(); });
    });
    connect(m_listPage, &ListTestPage::uploadRequested, this, [this](const ListProgram &program) {
        dispatch([program](PeakTech &device) { device.uploadList(program); });
    });
    connect(m_listPage, &ListTestPage::startRequested, this, [this] {
        dispatch([](PeakTech &device) { device.startList(); });
    });
    connect(m_listPage, &ListTestPage::stopRequested, this, [this] {
        dispatch([](PeakTech &device) { device.stop(); });
    });
    connect(m_listPage, &ListTestPage::triggerRequested, this, [this] {
        dispatch([](PeakTech &device) { device.triggerList(); });
    });
    connect(m_connectionButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);

    m_deviceThread.start();
    updateLanguage(m_preferences.value(QStringLiteral("language"), QStringLiteral("de")).toString());
}

MainWindow::~MainWindow()
{
    m_deviceThread.quit();
    m_deviceThread.wait();
    qApp->removeTranslator(&m_translator);
    qApp->removeTranslator(&m_qtTranslator);
}

QWidget *MainWindow::createGeneralPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(16);
    m_liveGroup = new QGroupBox;
    auto *live = new QGridLayout(m_liveGroup);
    live->setContentsMargins(20, 17, 20, 20);
    live->setHorizontalSpacing(22);
    addMetric(live, 0, m_voltageLabel = new QLabel, m_liveVoltage = new QLabel);
    addMetric(live, 1, m_currentLabel = new QLabel, m_liveCurrent = new QLabel);
    addMetric(live, 2, m_powerLabel = new QLabel, m_livePower = new QLabel);
    addMetric(live, 3, m_loadLabel = new QLabel, m_loadStatus = new QLabel);
    m_loadStatus->setStyleSheet(QStringLiteral("font-size: 16px;"));
    layout->addWidget(m_liveGroup);

    m_generalGroup = new QGroupBox;
    auto *settings = new QGridLayout(m_generalGroup);
    settings->setContentsMargins(20, 15, 20, 20);
    settings->setHorizontalSpacing(12);
    settings->setVerticalSpacing(8);
    m_mode = new QComboBox;
    m_mode->setMinimumWidth(210);
    for (const auto &mode : modes)
        m_mode->addItem(mode);
    m_setpoint = new QDoubleSpinBox;
    m_setpoint->setObjectName(QStringLiteral("setpoint"));
    m_setpoint->setDecimals(3);
    m_setpoint->setMinimumWidth(112);
    m_setpoint->setKeyboardTracking(false);
    m_modeLabel = new QLabel;
    m_setpointLabel = new QLabel;
    labelFor(m_modeLabel, m_mode);
    labelFor(m_setpointLabel, m_setpoint);
    m_basicApply = new QPushButton;
    m_basicApply->setObjectName(QStringLiteral("applySetpoint"));
    m_loadOn = new QPushButton;
    m_loadOn->setObjectName(QStringLiteral("primary"));
    m_loadOff = new QPushButton;
    m_loadOff->setObjectName(QStringLiteral("stop"));
    settings->addWidget(m_modeLabel, 0, 0);
    settings->addWidget(m_setpointLabel, 0, 1);
    settings->addWidget(m_mode, 1, 0);
    settings->addWidget(m_setpoint, 1, 1);
    settings->addWidget(m_basicApply, 1, 2);
    settings->setColumnStretch(3, 1);
    settings->addWidget(m_loadOff, 1, 4);
    settings->addWidget(m_loadOn, 1, 5);
    layout->addWidget(m_generalGroup);
    layout->addWidget(createLoadSetup());
    layout->addStretch();

    updateSetpointRange();
    m_setpoint->setValue(0.2);
    connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        updateSetpointRange();
        m_basicApplied = false;
        updateControls();
    });
    connect(m_setpoint, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] {
        m_basicApplied = false;
        updateControls();
    });
    connect(m_basicApply, &QPushButton::clicked, this, [this] {
        const auto mode = modes.at(m_mode->currentIndex());
        const double value = m_setpoint->value();
        m_basicApplied = m_batteryApplied = false;
        dispatch([mode, value](PeakTech &device) { device.configure(mode, value); });
    });
    connect(m_loadOn, &QPushButton::clicked, this, [this] {
        const auto mode = modes.at(m_mode->currentIndex());
        dispatch([mode](PeakTech &device) { device.start(mode); });
    });
    connect(m_loadOff, &QPushButton::clicked, this, [this] {
        dispatch([](PeakTech &device) { device.stop(); });
    });
    return page;
}

QGroupBox *MainWindow::createLoadSetup()
{
    m_loadSetup = new QGroupBox;
    auto *layout = new QGridLayout(m_loadSetup);
    layout->setContentsMargins(20, 15, 20, 20);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(8);
    layout->setColumnMinimumWidth(2, 24);
    layout->setColumnStretch(5, 1);

    for (std::size_t i = 0; i < m_loadSetupValues.size(); ++i) {
        const auto &field = loadSetupFields[i];
        auto *input = new QDoubleSpinBox;
        input->setObjectName(QString::fromLatin1(field.name));
        input->setRange(0, field.maximum);
        input->setDecimals(3);
        input->setSingleStep(0.1);
        input->setSuffix(QString::fromLatin1(field.suffix));
        input->setMinimumWidth(135);
        input->setKeyboardTracking(false);
        auto *label = new QLabel;
        label->setBuddy(input);
        const int row = static_cast<int>(i % 3);
        const int column = i < 3 ? 0 : 3;
        layout->addWidget(label, row, column);
        layout->addWidget(input, row, column + 1);
        m_loadSetupLabels[i] = label;
        m_loadSetupValues[i] = input;
    }

    m_autoOff = new QCheckBox;
    m_autoOff->setObjectName(QStringLiteral("autoOffEnabled"));
    m_autoOffDelay = new QSpinBox;
    m_autoOffDelay->setObjectName(QStringLiteral("autoOffSeconds"));
    m_autoOffDelay->setRange(0, 60000);
    m_autoOffDelay->setSuffix(QStringLiteral(" s"));
    m_autoOffDelay->setKeyboardTracking(false);
    m_loadSetupApply = new QPushButton;
    m_loadSetupApply->setObjectName(QStringLiteral("applyLoadSetup"));
    layout->addWidget(m_autoOff, 2, 3);
    layout->addWidget(m_autoOffDelay, 2, 4);
    layout->addWidget(m_loadSetupApply, 2, 6);
    connect(m_autoOff, &QCheckBox::toggled, this, [this](bool checked) {
        m_autoOffDelay->setMinimum(checked ? 1 : 0);
        if (!checked)
            m_autoOffDelay->setValue(0);
        updateControls();
    });
    connect(m_loadSetupApply, &QPushButton::clicked, this, [this] {
        LoadSetup setup;
        for (std::size_t i = 0; i < m_loadSetupValues.size(); ++i)
            setup.*loadSetupFields[i].value = m_loadSetupValues[i]->value();
        if (m_autoOff->isChecked())
            setup.autoOff = m_autoOffDelay->value();
        dispatch([setup](PeakTech &device) { device.configureLoadSetup(setup); });
    });
    return m_loadSetup;
}

QWidget *MainWindow::createBatteryPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *summary = new QWidget;
    summary->setObjectName(QStringLiteral("testSummary"));
    auto *summaryLayout = new QGridLayout(summary);
    summaryLayout->setContentsMargins(15, 11, 15, 11);
    summaryLayout->setHorizontalSpacing(15);
    addMetric(summaryLayout, 0, m_batteryStatusLabel = new QLabel, m_batteryState = new QLabel);
    addMetric(summaryLayout, 1, m_capacityLabel = new QLabel, m_capacity = new QLabel(dash));
    addMetric(summaryLayout, 2, m_durationLabel = new QLabel, m_duration = new QLabel(dash));
    m_batteryState->setStyleSheet(QStringLiteral("font-size: 16px;"));
    m_batteryState->setWordWrap(true);
    summaryLayout->setColumnStretch(0, 3);
    m_batteryStart = new QPushButton;
    m_batteryStart->setObjectName(QStringLiteral("primary"));
    m_batteryStop = new QPushButton;
    m_batteryStop->setObjectName(QStringLiteral("stop"));
    summaryLayout->addWidget(m_batteryStart, 0, 3, 2, 1);
    summaryLayout->addWidget(m_batteryStop, 0, 4, 2, 1);
    layout->addWidget(summary);

    auto *results = new QSplitter(Qt::Horizontal);
    m_curveGroup = new QGroupBox;
    auto *chartLayout = new QVBoxLayout(m_curveGroup);
    chartLayout->setContentsMargins(6, 4, 8, 5);
    m_chart = new QChart;
    m_chart->setMargins(QMargins(4, 1, 4, 0));
    m_chart->setBackgroundRoundness(0);
    m_chart->legend()->hide();
    m_series = new QLineSeries(m_chart);
    m_series->setPointsVisible(true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_series->setMarkerSize(4);
#endif
    m_chart->addSeries(m_series);
    m_xAxis = new QValueAxis(m_chart);
    m_xAxis->setRange(0, 40);
    m_xAxis->setTickCount(9);
    m_xAxis->setLabelFormat(QStringLiteral("%.0f"));
    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_yAxis = new QValueAxis(m_chart);
    m_yAxis->setRange(0, 1);
    m_yAxis->setTickCount(6);
    m_yAxis->setLabelFormat(QStringLiteral("%.2f"));
    m_chart->addAxis(m_yAxis, Qt::AlignLeft);
    for (auto *axis : {m_xAxis, m_yAxis})
        m_series->attachAxis(axis);
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setFrameShape(QFrame::NoFrame);
    chartLayout->addWidget(m_chartView);
    results->addWidget(m_curveGroup);

    m_tableGroup = new QGroupBox;
    auto *tableLayout = new QVBoxLayout(m_tableGroup);
    tableLayout->setContentsMargins(10, 8, 10, 10);
    m_table = new QTableWidget(0, 5);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableLayout->addWidget(m_table);
    results->addWidget(m_tableGroup);
    results->setChildrenCollapsible(false);
    results->setStretchFactor(0, 1);
    results->setStretchFactor(1, 1);
    results->setSizes({500, 500});
    layout->addWidget(results, 1);

    m_parameterGroup = new QGroupBox;
    auto *parameters = new QGridLayout(m_parameterGroup);
    parameters->setContentsMargins(14, 9, 14, 12);
    parameters->setHorizontalSpacing(14);
    m_dischargeLabel = new QLabel;
    m_discharge = new QDoubleSpinBox;
    m_discharge->setDecimals(2);
    m_discharge->setRange(0.01, 30);
    m_discharge->setSingleStep(0.01);
    m_discharge->setValue(m_preferences.value(QStringLiteral("battery/current"), 0.3).toDouble());
    m_discharge->setSuffix(QStringLiteral(" A"));
    m_cutoffLabel = new QLabel;
    m_cutoff = new QDoubleSpinBox;
    m_cutoff->setDecimals(3);
    m_cutoff->setRange(0.8, 36);
    m_cutoff->setSingleStep(0.1);
    m_cutoff->setValue(m_preferences.value(QStringLiteral("battery/cutoff"), 8).toDouble());
    m_cutoff->setSuffix(QStringLiteral(" V"));
    m_periodLabel = new QLabel;
    m_period = new QSpinBox;
    m_period->setRange(1, 60000);
    m_period->setValue(m_preferences.value(QStringLiteral("battery/period"), 2).toInt());
    m_period->setSuffix(QStringLiteral(" s"));
    m_batteryApply = new QPushButton;
    labelFor(m_dischargeLabel, m_discharge);
    labelFor(m_cutoffLabel, m_cutoff);
    labelFor(m_periodLabel, m_period);
    parameters->addWidget(m_dischargeLabel, 0, 0);
    parameters->addWidget(m_discharge, 1, 0);
    parameters->addWidget(m_cutoffLabel, 0, 1);
    parameters->addWidget(m_cutoff, 1, 1);
    parameters->addWidget(m_periodLabel, 0, 2);
    parameters->addWidget(m_period, 1, 2);
    parameters->setColumnStretch(3, 1);
    parameters->addWidget(m_batteryApply, 1, 4);
    layout->addWidget(m_parameterGroup);

    auto edited = [this] {
        m_batteryApplied = false;
        updateControls();
    };
    connect(m_discharge, qOverload<double>(&QDoubleSpinBox::valueChanged), this, edited);
    connect(m_cutoff, qOverload<double>(&QDoubleSpinBox::valueChanged), this, edited);
    connect(m_period, qOverload<int>(&QSpinBox::valueChanged), this, edited);
    connect(m_batteryApply, &QPushButton::clicked, this, [this] {
        const double current = m_discharge->value();
        const double cutoff = m_cutoff->value();
        m_preferences.setValue(QStringLiteral("battery/current"), current);
        m_preferences.setValue(QStringLiteral("battery/cutoff"), cutoff);
        m_preferences.setValue(QStringLiteral("battery/period"), m_period->value());
        m_batteryApplied = m_basicApplied = false;
        dispatch([current, cutoff](PeakTech &device) { device.configure(QStringLiteral("BAT"), current, cutoff); });
    });
    connect(m_batteryStart, &QPushButton::clicked, this, [this] {
        const int interval = m_period->value();
        dispatch([interval](PeakTech &device) { device.start(QStringLiteral("BAT"), interval); });
    });
    connect(m_batteryStop, &QPushButton::clicked, this, [this] {
        dispatch([](PeakTech &device) { device.stop(); });
    });
    return page;
}

void MainWindow::createMenus()
{
    auto *bar = menuBar();
    bar->setNativeMenuBar(false);
    m_settingsMenu = bar->addMenu(QString());
    m_connectionAction = m_settingsMenu->addAction(QString(), this, &MainWindow::editConnection);
    m_appearanceMenu = m_settingsMenu->addMenu(QString());
    auto *themes = new QActionGroup(m_appearanceMenu);
    m_lightThemeAction = m_appearanceMenu->addAction(QString(), this, [this] { setDarkTheme(false); });
    m_darkThemeAction = m_appearanceMenu->addAction(QString(), this, [this] { setDarkTheme(true); });
    m_lightThemeAction->setObjectName(QStringLiteral("lightTheme"));
    m_darkThemeAction->setObjectName(QStringLiteral("darkTheme"));
    for (auto *action : {m_lightThemeAction, m_darkThemeAction}) {
        action->setCheckable(true);
        themes->addAction(action);
    }

    auto *right = new QMenuBar(bar);
    right->setNativeMenuBar(false);
    m_languageMenu = right->addMenu(QString());
    m_languageMenu->setObjectName(QStringLiteral("languageMenu"));
    auto *languages = new QActionGroup(m_languageMenu);
    const auto saved = m_preferences.value(QStringLiteral("language"), QStringLiteral("de")).toString();
    const QStringList languageNames{QStringLiteral("Deutsch"), QStringLiteral("English"), QStringLiteral("Français")};
    const QStringList languageCodes{QStringLiteral("de"), QStringLiteral("en"), QStringLiteral("fr")};
    for (int i = 0; i < languageNames.size(); ++i) {
        auto *action = m_languageMenu->addAction(languageNames[i]);
        action->setData(languageCodes[i]);
        languages->addAction(action);
        action->setCheckable(true);
        action->setChecked(languageCodes[i] == saved);
        connect(action, &QAction::triggered, this, [this, language = languageCodes[i]] { updateLanguage(language); });
    }
    m_aboutMenu = right->addMenu(QString());
    m_aboutMenu->setObjectName(QStringLiteral("aboutMenu"));
    m_aboutQtAction = m_aboutMenu->addAction(QString(), this, [this] {
        QMessageBox::aboutQt(this, tr("Über Qt"));
    });
    m_aboutApplicationAction = m_aboutMenu->addAction(QString(), this, &MainWindow::showAbout);
    m_aboutMenu->addSeparator();
    m_updateAction = m_aboutMenu->addAction(QString(), this, &MainWindow::checkForUpdates);
    m_updateAction->setObjectName(QStringLiteral("checkForUpdates"));
    bar->setCornerWidget(right, Qt::TopRightCorner);
}

void MainWindow::showAbout()
{
    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("Über"));
    dialog.setTextFormat(Qt::RichText);
    dialog.setStandardButtons(QMessageBox::Ok);
    dialog.setText(QStringLiteral("<b>PeakTech 2275</b><br>%1<br><br><b>Ralf Tralow</b>")
                       .arg(tr("Version %1").arg(qApp->applicationVersion())));
    QString details = tr("Bedienung der elektronischen Gleichstromlast über RS-232.").toHtmlEscaped();
    if (!m_identity.isEmpty())
        details += QStringLiteral("<br>") + tr("Gerät: %1").arg(m_identity).toHtmlEscaped();
    dialog.setInformativeText(details);

    QMovie logo(QStringLiteral(":/branding/logo_g.gif"));
    if (logo.isValid()) {
        connect(&logo, &QMovie::frameChanged, &dialog, [&] {
            dialog.setIconPixmap(logo.currentPixmap());
        });
        logo.start();
    } else {
        dialog.setIconPixmap(windowIcon().pixmap(120));
    }
    dialog.exec();
}

void MainWindow::setDarkTheme(bool dark)
{
    QPalette palette;
    const auto color = [dark](const char *darkColor, const char *lightColor) {
        return QColor(dark ? darkColor : lightColor);
    };
    palette.setColor(QPalette::Window, color("#10151e", "#edf3f8"));
    palette.setColor(QPalette::WindowText, color("#e8f0fb", "#1c3044"));
    palette.setColor(QPalette::Base, color("#171f2b", "#ffffff"));
    palette.setColor(QPalette::AlternateBase, color("#192331", "#e7eef5"));
    palette.setColor(QPalette::Text, palette.color(QPalette::WindowText));
    palette.setColor(QPalette::Button, color("#222e3e", "#e7eef5"));
    palette.setColor(QPalette::ButtonText, palette.color(QPalette::WindowText));
    palette.setColor(QPalette::Highlight, color("#33536d", "#cbe3f7"));
    palette.setColor(QPalette::HighlightedText, color("#e8f0fb", "#17334e"));
    for (const auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText})
        palette.setColor(QPalette::Disabled, role, color("#8896a9", "#586d80"));
    qApp->setPalette(palette);

    QFile base(QStringLiteral(":/theme.qss"));
    QString style;
    if (base.open(QIODevice::ReadOnly))
        style = QString::fromUtf8(base.readAll());
    if (!dark) {
        QFile light(QStringLiteral(":/theme-light.qss"));
        if (light.open(QIODevice::ReadOnly))
            style += QLatin1Char('\n') + QString::fromUtf8(light.readAll());
    }
    qApp->setStyleSheet(style);

    m_languageMenu->setIcon(globeIcon(dark));
    (dark ? m_darkThemeAction : m_lightThemeAction)->setChecked(true);
    m_preferences.setValue(QStringLiteral("appearance/dark"), dark);

    const QColor surface = color("#171f2b", "#f9fbfd");
    m_chartView->setBackgroundBrush(surface);
    m_chart->setBackgroundBrush(surface);
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->setPlotAreaBackgroundBrush(color("#141b26", "#ffffff"));
    m_series->setPen(QPen(color("#77ccff", "#236b9f"), 2));
    for (auto *axis : {m_xAxis, m_yAxis}) {
        axis->setGridLineColor(color("#303e51", "#d8e4ef"));
        axis->setLinePenColor(color("#536a82", "#a7bacb"));
        axis->setLabelsColor(color("#b7c9dd", "#496177"));
        axis->setTitleBrush(palette.color(QPalette::WindowText));
    }
}

void MainWindow::updateLanguage(const QString &language)
{
    qApp->removeTranslator(&m_translator);
    qApp->removeTranslator(&m_qtTranslator);
    if (language != QStringLiteral("de") && m_translator.load(QStringLiteral(":/i18n/peaktech_") + language))
        qApp->installTranslator(&m_translator);
    if (language != QStringLiteral("en") && m_qtTranslator.load(QStringLiteral(":/qt-i18n/qtbase_") + language))
        qApp->installTranslator(&m_qtTranslator);
    m_locale = QLocale(language);
    m_locale.setNumberOptions(QLocale::OmitGroupSeparator);
    m_setpoint->setLocale(m_locale);
    for (auto *input : m_loadSetupValues)
        input->setLocale(m_locale);
    m_autoOffDelay->setLocale(m_locale);
    m_discharge->setLocale(m_locale);
    m_cutoff->setLocale(m_locale);
    m_period->setLocale(m_locale);
    m_listPage->setNumberLocale(m_locale);
    m_chart->setLocale(m_locale);
    m_chart->setLocalizeNumbers(true);
    m_preferences.setValue(QStringLiteral("language"), language);
    retranslate();
}

void MainWindow::retranslate()
{
    m_settingsMenu->setTitle(tr("&Einstellungen"));
    m_connectionAction->setText(tr("&Serielle Verbindung…"));
    m_appearanceMenu->setTitle(tr("Darstellung"));
    m_lightThemeAction->setText(tr("Hell"));
    m_darkThemeAction->setText(tr("Dunkel"));
    m_languageMenu->setTitle(tr("Sprache wählen"));
    m_languageMenu->menuAction()->setText({});
    m_languageMenu->menuAction()->setToolTip(tr("Sprache wählen"));
    m_aboutMenu->setTitle(tr("Über"));
    m_aboutApplicationAction->setText(tr("Über"));
    m_aboutQtAction->setText(tr("Über Qt"));
    m_updateAction->setText(tr("Nach Updates suchen"));

    auto *bar = menuBar();
    auto *right = bar->cornerWidget(Qt::TopRightCorner);
    bar->setCornerWidget(nullptr, Qt::TopRightCorner);
    bar->setCornerWidget(right, Qt::TopRightCorner);
    m_pages->setTabText(0, tr("Allgemeine Bedienung"));
    m_pages->setTabText(1, tr("Batterietest"));
    m_pages->setTabText(2, tr("Listentest"));
    m_listPage->retranslate();
    m_liveGroup->setTitle(tr("Live-Messwerte"));
    m_voltageLabel->setText(tr("Eingangsspannung"));
    m_currentLabel->setText(tr("Eingangsstrom"));
    m_powerLabel->setText(tr("Eingangsleistung"));
    m_loadLabel->setText(tr("Laststatus"));
    m_generalGroup->setTitle(tr("Laststeuerung"));
    m_modeLabel->setText(tr("&Betriebsart"));
    m_setpointLabel->setText(tr("&Sollwert"));
    m_mode->setItemText(0, tr("CC · Konstantstrom"));
    m_mode->setItemText(1, tr("CV · Konstantspannung"));
    m_mode->setItemText(2, tr("CP · Konstantleistung"));
    m_mode->setItemText(3, tr("CR · Konstantwiderstand"));
    m_basicApply->setText(tr("Übernehmen"));
    m_loadOn->setText(tr("Last einschalten"));
    m_loadOff->setText(tr("Last ausschalten"));
    m_loadSetup->setTitle(tr("Lasteinstellungen"));
    for (std::size_t i = 0; i < m_loadSetupLabels.size(); ++i)
        m_loadSetupLabels[i]->setText(tr(loadSetupFields[i].label));
    m_autoOff->setText(tr("Auto-Abschaltung"));
    m_loadSetupApply->setText(tr("Übernehmen"));
    m_curveGroup->setTitle(tr("Spannungsverlauf"));
    m_tableGroup->setTitle(tr("Messprotokoll"));
    m_parameterGroup->setTitle(tr("Testparameter"));
    m_batteryStatusLabel->setText(tr("Teststatus"));
    m_capacityLabel->setText(tr("Kapazität"));
    m_durationLabel->setText(tr("Entladezeit"));
    m_dischargeLabel->setText(tr("Entladestrom"));
    m_cutoffLabel->setText(tr("Abschaltspannung"));
    m_periodLabel->setText(tr("Messintervall"));
    m_batteryApply->setText(tr("Übernehmen"));
    m_batteryStart->setText(tr("Test starten"));
    m_batteryStop->setText(tr("Test stoppen"));
    m_xAxis->setTitleText(tr("Entladezeit (s)"));
    m_yAxis->setTitleText(tr("Spannung (V)"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("#"), tr("Spannung (V)"), tr("Strom (A)"),
                                        tr("Kapazität (Ah)"), tr("Zeit (s)")});
    for (int row = 0; row < m_table->rowCount(); ++row) {
        for (int column = 1; column <= 3; ++column) {
            auto *cell = m_table->item(row, column);
            cell->setText(spinNumber(cell->data(Qt::UserRole).toDouble(), 3));
        }
    }
    if (m_table->rowCount())
        m_capacity->setText(m_table->item(m_table->rowCount() - 1, 3)->text() + QStringLiteral(" Ah"));
    updateReading(m_reading);
    updateBatteryStatus();
}

void MainWindow::updateSetpointRange()
{
    const int index = m_mode->currentIndex();
    const double low[] = {0, 1.5, 0, 0.1};
    const double high[] = {30, 360, 150, 4000};
    const QStringList units{QStringLiteral(" A"), QStringLiteral(" V"), QStringLiteral(" W"), QStringLiteral(" Ω")};
    if (index >= 0) {
        m_setpoint->setRange(low[index], high[index]);
        m_setpoint->setSuffix(units[index]);
        m_setpoint->setSingleStep(0.1);
    }
}

void MainWindow::editConnection()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Serielle Verbindung"));
    auto *form = new QFormLayout(&dialog);
    QComboBox ports;
    ports.setEditable(true);
    for (const auto &port : QSerialPortInfo::availablePorts())
        ports.addItem(port.portName());
    ports.setCurrentText(m_serial.port);
    QComboBox baud;
    for (int rate : {4800, 9600, 11520, 12800, 14400, 19200, 28800, 38400})
        baud.addItem(QString::number(rate));
    baud.setCurrentText(QString::number(m_serial.baud));
    QSpinBox address;
    address.setRange(1, 127);
    address.setValue(m_serial.address);
    QLabel framing(tr("8 Datenbits · keine Parität · 1 Stoppbit"));
    form->addRow(tr("COM-Port"), &ports);
    form->addRow(tr("Baudrate"), &baud);
    form->addRow(tr("Geräteadresse"), &address);
    form->addRow(tr("Datenformat"), &framing);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted && !ports.currentText().trimmed().isEmpty()) {
        m_serial = {ports.currentText().trimmed(), baud.currentText().toInt(), address.value()};
        m_preferences.setValue(QStringLiteral("serial/port"), m_serial.port);
        m_preferences.setValue(QStringLiteral("serial/baud"), m_serial.baud);
        m_preferences.setValue(QStringLiteral("serial/address"), m_serial.address);
        updateControls();
    }
}

void MainWindow::toggleConnection()
{
    if (m_identity.isEmpty()) {
        const auto serial = m_serial;
        dispatch([serial](PeakTech &device) { device.open(serial); });
    } else {
        dispatch([](PeakTech &device) { device.closePort(); });
    }
}

void MainWindow::dispatch(std::function<void(PeakTech &)> operation)
{
    if (m_busy)
        return;
    m_busy = true;
    m_notice->hide();
    updateControls();
    QMetaObject::invokeMethod(m_device, [device = m_device, operation = std::move(operation)] {
        device->perform(operation);
    }, Qt::QueuedConnection);
}

void MainWindow::onConnectionChanged(const QString &identity)
{
    m_identity = identity;
    m_basicApplied = m_batteryApplied = false;
    m_loadSetupReady = false;
    if (identity.isEmpty())
        updateReading({});
    updateControls();
}

void MainWindow::updateReading(const Reading &reading)
{
    m_reading = reading;
    const bool connected = !m_identity.isEmpty();
    const auto measurement = [this, &reading](double value, const QString &unit) {
        return reading.measured ? spinNumber(value, 3) + unit : dash;
    };
    m_liveVoltage->setText(measurement(reading.volts, QStringLiteral(" V")));
    m_liveCurrent->setText(reading.currentKnown ? measurement(reading.amps, QStringLiteral(" A")) : dash);
    m_livePower->setText(reading.currentKnown ? measurement(reading.volts * reading.amps, QStringLiteral(" W")) : dash);
    m_loadStatus->setText(connected ? loadStatus(reading.load) : dash);
    if (m_basicApplied && !reading.mode.isEmpty() && reading.mode != modes.value(m_mode->currentIndex()))
        m_basicApplied = false;
    if (m_batteryApplied && !reading.mode.isEmpty() && reading.mode != QStringLiteral("BAT"))
        m_batteryApplied = false;
    if (reading.mode != QStringLiteral("LIST"))
        m_listPage->invalidate();
    m_listPage->showReading(reading);
    updateControls();
}

void MainWindow::updateControls()
{
    const bool connected = !m_identity.isEmpty();
    const bool available = connected && !m_busy;
    const bool testing = m_batteryStatus == BatteryStatus::Running || m_listPage->running();
    const bool idle = available && m_reading.load == 0 && !testing;
    const bool liveSetpoint = available && !testing && isLoadActive(m_reading.load) &&
                              m_reading.mode == modes.value(m_mode->currentIndex());
    m_serialStatus->setText(tr("%1 · %2 Baud · 8N1 · Adresse %3").arg(m_serial.port).arg(m_serial.baud).arg(m_serial.address));
    m_connectionStatus->setText(connected ? tr("Verbunden") : tr("Getrennt"));
    m_connectionStatus->setToolTip(m_identity);
    m_connectionButton->setText(connected ? tr("Trennen") : tr("Verbinden"));
    m_connectionButton->setEnabled(!m_busy);
    m_connectionAction->setEnabled(!connected && !m_busy);
    m_mode->setEnabled(!m_busy && (!connected || idle));
    m_setpoint->setEnabled(m_mode->isEnabled() || liveSetpoint);
    m_basicApply->setEnabled(idle || liveSetpoint);
    m_loadOn->setEnabled(idle && m_basicApplied);
    m_loadOff->setEnabled(available && (m_reading.load != 0 || testing));
    m_loadSetup->setEnabled(idle && m_loadSetupReady);
    m_autoOffDelay->setEnabled(m_autoOff->isChecked());
    m_discharge->setEnabled(!m_busy && (!connected || idle));
    m_cutoff->setEnabled(m_discharge->isEnabled());
    m_period->setEnabled(m_discharge->isEnabled());
    m_batteryApply->setEnabled(idle);
    m_batteryStart->setEnabled(idle && m_batteryApplied);
    m_batteryStop->setEnabled(available && m_batteryStatus == BatteryStatus::Running);
    m_listPage->setDeviceState(connected, m_busy, idle);
}

void MainWindow::showLoadSetup(const LoadSetup &setup)
{
    for (std::size_t i = 0; i < m_loadSetupValues.size(); ++i)
        m_loadSetupValues[i]->setValue(setup.*loadSetupFields[i].value);
    m_autoOff->setChecked(setup.autoOff > 0);
    m_autoOffDelay->setValue(setup.autoOff);
    m_loadSetupReady = true;
    updateControls();
}

void MainWindow::showError(const QString &error, bool offUnconfirmed)
{
    m_notice->setText(offUnconfirmed ?
        error + QLatin1Char(' ') + tr("Last AUS unbestätigt. Prüfen Sie den Lasteingang am Gerät.") : error);
    m_notice->show();
}

void MainWindow::resetBattery()
{
    m_batteryStatus = BatteryStatus::Running;
    m_series->clear();
    m_table->setRowCount(0);
    m_xAxis->setRange(0, 40);
    m_yAxis->setRange(0, 1);
    m_capacity->setText(dash);
    m_duration->setText(dash);
    updateBatteryStatus();
    updateControls();
}

void MainWindow::recordSample(const Reading &reading)
{
    if (m_series->count() == 0)
        m_lowVoltage = m_highVoltage = reading.volts;
    m_lowVoltage = std::min(m_lowVoltage, reading.volts);
    m_highVoltage = std::max(m_highVoltage, reading.volts);
    const double padding = std::max(0.15, (m_highVoltage - m_lowVoltage) * 0.12);
    m_yAxis->setRange(std::max(0.0, m_lowVoltage - padding), std::max(0.3, m_highVoltage + padding));
    if (reading.seconds > m_xAxis->max()) {
        const double extent = reading.seconds * 1.25;
        const double tick = std::pow(10.0, std::floor(std::log10(extent))) / 2;
        m_xAxis->setMax(std::ceil(extent / (8 * tick)) * 8 * tick);
    }
    int row = m_table->rowCount();
    if (row && m_table->item(row - 1, 4)->data(Qt::UserRole).toInt() == reading.seconds) {
        --row;
        m_series->replace(row, reading.seconds, reading.volts);
    } else {
        m_series->append(reading.seconds, reading.volts);
        m_table->insertRow(row);
    }
    const double data[] = {static_cast<double>(row + 1), reading.volts, reading.amps,
                           reading.ampHours, static_cast<double>(reading.seconds)};
    for (int col = 0; col < 5; ++col) {
        auto *item = new QTableWidgetItem(spinNumber(data[col], col == 0 || col == 4 ? 0 : 3));
        item->setData(Qt::UserRole, data[col]);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, col, item);
    }
    m_table->scrollToBottom();
    m_capacity->setText(spinNumber(reading.ampHours, 3) + QStringLiteral(" Ah"));
    m_duration->setText(QString::number(reading.seconds) + QStringLiteral(" s"));
}

void MainWindow::updateBatteryStatus()
{
    switch (m_batteryStatus) {
    case BatteryStatus::Ready: m_batteryState->setText(tr("Bereit")); break;
    case BatteryStatus::Running: m_batteryState->setText(tr("Entladung läuft")); break;
    case BatteryStatus::Cutoff: m_batteryState->setText(tr("Abschaltspannung erreicht")); break;
    case BatteryStatus::Stopped: m_batteryState->setText(tr("Manuell gestoppt")); break;
    case BatteryStatus::DeviceStopped: m_batteryState->setText(tr("Vom Gerät beendet")); break;
    case BatteryStatus::Fault:
        m_batteryState->setText(tr("Abgebrochen (%1)").arg(loadStatus(m_testFault)));
        break;
    case BatteryStatus::Disconnected: m_batteryState->setText(tr("Verbindung unterbrochen")); break;
    }
}

QString MainWindow::loadStatus(int state) const
{
    switch (state) {
    case 0: return tr("Aus");
    case 1: return QStringLiteral("CC");
    case 2: return QStringLiteral("CV");
    case 3: return QStringLiteral("CP");
    case 4: return QStringLiteral("CR");
    case 5: return tr("Überspannung");
    case 6: return tr("Überlast");
    case 7: return tr("Überstrom");
    case 8: return tr("Übertemperatur");
    case 9: return tr("Verpolung");
    case 10: return tr("Aktiv");
    case 11: return tr("Außer Regelung");
    case 12: return tr("Wartet");
    case 13: return tr("Gerätefehler");
    default: return tr("Unbekannt");
    }
}

QString MainWindow::spinNumber(double value, int precision) const
{
    return m_locale.toString(value, 'f', precision);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_busy) {
        m_closing = true;
        event->ignore();
    } else if (!m_identity.isEmpty()) {
        m_closing = true;
        event->ignore();
        toggleConnection();
    } else {
        event->accept();
    }
}
