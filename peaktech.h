#pragma once

#include <QDeadlineTimer>
#include <QObject>
#include <QString>
#include <QList>

#include <functional>

class QSerialPort;
class QTimer;

struct SerialSettings {
    QString port = QStringLiteral("COM17");
    int baud = 38400;
    int address = 1;
};

struct LoadSetup {
    double maxVoltage = 0;
    double maxCurrent = 0;
    double maxPower = 0;
    double onVoltage = 0;
    double offVoltage = 0;
    int autoOff = 0;
};

bool isLoadActive(int status);

struct ListStep {
    QString mode = QStringLiteral("CC");
    double value = 0.2;
    double seconds = 5;
    QString compare = QStringLiteral("OFF");
    double low = 0;
    double high = 0;
};

struct ListProgram {
    QList<ListStep> steps;
    bool triggered = false;
    bool repeat = false;
};

struct Reading {
    QString mode;
    int load = 0;
    double volts = 0;
    double amps = 0;
    double ampHours = 0;
    int seconds = 0;
    bool measured = false;
    bool currentKnown = false;
    bool batteryData = false;
    int listStep = -1;
    int listOverall = -1;
    QList<int> listResults;
    bool listIdle = false;
};

enum class TestEnd { Cutoff, Stopped, DeviceStopped, Fault, Disconnected };

class PeakTech : public QObject
{
    Q_OBJECT

public:
    explicit PeakTech(QObject *parent = nullptr);
    void perform(const std::function<void(PeakTech &)> &operation);
    void open(const SerialSettings &settings);
    void closePort();
    void configure(const QString &mode, double value, double cutoff = 0);
    void configureLoadSetup(const LoadSetup &requested);
    void readList();
    void uploadList(const ListProgram &program);
    void startList();
    void triggerList();
    void start(const QString &mode, int period = 0);
    void stop();

signals:
    void connectionChanged(const QString &identity);
    void readingChanged(const Reading &reading);
    void basicRead(const QString &mode, double value);
    void loadSetupRead(const LoadSetup &setup);
    void settingsApplied(const QString &mode);
    void settingsInvalidated();
    void listRead(const ListProgram &program);
    void listUploaded();
    void listStarted();
    void listFinished(TestEnd cause, int loadState);
    void testStarted();
    void sampleRecorded(const Reading &reading);
    void testFinished(TestEnd cause, int loadState);
    void operationFinished(const QString &error, bool offUnconfirmed);
    void communicationLost(const QString &error, bool offUnconfirmed);

private:
    void poll();
    void fail(const QString &error, bool operation);
    void updateTest(const Reading &reading);
    void finishTest(TestEnd cause, int state);
    void recordBatteryReading(const Reading &reading, bool final = false);
    void readBasicSettings(const QString &mode);
    Reading readInstrument();
    void updateList(const Reading &reading);
    void finishList(TestEnd cause, int state);
    ListProgram readListProgram();
    void verifyList(const ListProgram &expected, const ListProgram &actual);
    void validateList(const ListProgram &program, const LoadSetup &limits);
    LoadSetup readLoadSetup();
    int readLoad();
    void turnOff();
    void verifySettings();
    void writeLine(const QByteArray &command, bool addressed = true);
    QByteArray query(const QByteArray &command, bool addressed = true);
    double readNumber(const QByteArray &command, const QByteArray &unit);

    QSerialPort *m_port;
    QTimer *m_pollTimer;
    QDeadlineTimer m_nextCommand;
    SerialSettings m_settings;
    QString m_configuredMode;
    double m_value = 0;
    double m_cutoff = 0;
    bool m_mayBeOn = false;
    bool m_test = false;
    int m_periodSeconds = 0;
    int m_lastSecond = -1;
    ListProgram m_listProgram;
    bool m_listUploaded = false;
    bool m_listRunning = false;
};
