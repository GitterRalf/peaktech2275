#pragma once

#include "peaktech.h"

#include <QElapsedTimer>
#include <QLocale>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

class ListTestPage : public QWidget
{
    Q_OBJECT

public:
    explicit ListTestPage(QWidget *parent = nullptr);
    void setDeviceState(bool connected, bool busy, bool idle);
    bool running() const { return m_running; }
    void setProgram(const ListProgram &program);
    void markUploaded();
    void beginTest();
    void finishTest(TestEnd cause, int loadState);
    void showReading(const Reading &reading);
    void invalidate();
    void retranslate();
    void setNumberLocale(const QLocale &locale);

signals:
    void readRequested();
    void uploadRequested(const ListProgram &program);
    void startRequested();
    void stopRequested();
    void triggerRequested();

private:
    enum class Pending { None, Read, Upload, Start };
    ListProgram program() const;
    void addStep(const ListStep &step);
    void rebuild(const ListProgram &program, int selected);
    void updateRow(int row);
    void edited();
    void updateControls();
    void updateSummary();
    void updateStatus();
    void completeCycle(const Reading &reading);
    QString resultText(int code) const;
    QString modeText(const QString &mode) const;

    QGroupBox *m_controlGroup;
    QGroupBox *m_stepGroup;
    QGroupBox *m_resultGroup;
    QLabel *m_stateLabel;
    QLabel *m_state;
    QLabel *m_stepLabel;
    QLabel *m_activeStep;
    QLabel *m_elapsedLabel;
    QLabel *m_elapsed;
    QLabel *m_cycleLabel;
    QLabel *m_cycle;
    QLabel *m_advanceLabel;
    QLabel *m_summary;
    QComboBox *m_advance;
    QCheckBox *m_repeat;
    QTableWidget *m_steps;
    QTableWidget *m_results;
    QPushButton *m_add;
    QPushButton *m_duplicate;
    QPushButton *m_remove;
    QPushButton *m_up;
    QPushButton *m_down;
    QPushButton *m_read;
    QPushButton *m_upload;
    QPushButton *m_start;
    QPushButton *m_stop;
    QPushButton *m_trigger;
    QTimer *m_clock;
    QElapsedTimer m_started;
    QLocale m_numberLocale;
    ListProgram m_executing;
    QVector<Reading> m_samples;
    QVector<double> m_sampleTimes;
    bool m_connected = false;
    bool m_busy = false;
    bool m_idle = false;
    bool m_uploaded = false;
    bool m_running = false;
    bool m_cycleFinished = false;
    bool m_rebuilding = false;
    int m_cycleNumber = 0;
    int m_overall = -1;
    int m_currentStep = -1;
    TestEnd m_end = TestEnd::Stopped;
    bool m_hasRun = false;
    Pending m_pending = Pending::None;
};
