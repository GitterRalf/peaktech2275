#include "peaktech.h"

#include <QElapsedTimer>
#include <QSerialPort>
#include <QThread>
#include <QTimer>

#include <cmath>
#include <utility>

namespace {
constexpr int timeoutMs = 1000;
constexpr int commandGapMs = 70;
constexpr int setterGapMs = 250;
constexpr int normalPeriodMs = 1000;
constexpr int listPeriodMs = 250;
// This firmware uses 100 ms units for LIST:STEPn:LEV, despite its "s" suffix.
constexpr double listTicksPerSecond = 10;

struct Failure {
    QString message;
    bool disconnect = true;
};

struct BasicMode {
    const char *mode;
    const char *command;
    const char *unit;
    const char *limit;
};

struct SetupParameter {
    const char *command;
    const char *unit;
    double LoadSetup::*value;
};

constexpr SetupParameter setupParameters[] = {
    {"MSET:VMAX", "V", &LoadSetup::maxVoltage},
    {"MSET:IMAX", "A", &LoadSetup::maxCurrent},
    {"MSET:PMAX", "W", &LoadSetup::maxPower},
    {"MSET:VON", "V", &LoadSetup::onVoltage},
    {"MSET:VOFF", "V", &LoadSetup::offVoltage},
};

const BasicMode basicModes[] = {
    {"CC", "LOAD:CURR", "A", "MSET:IMAX?"},
    {"CV", "LOAD:VOLT", "V", "MSET:VMAX?"},
    {"CP", "LOAD:POW", "W", "MSET:PMAX?"},
    {"CR", "LOAD:RES", "ohm", nullptr},
};

const BasicMode *basicMode(const QString &mode)
{
    for (const auto &entry : basicModes) {
        if (mode == QLatin1String(entry.mode))
            return &entry;
    }
    return nullptr;
}

double number(const QByteArray &reply, const QByteArray &unit)
{
    QByteArray value = reply.trimmed();
    if (value.toLower().endsWith(unit.toLower()))
        value.chop(unit.size());
    bool ok = false;
    const double result = value.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(result))
        throw Failure{PeakTech::tr("Ungültiger Mess- oder Einstellwert: %1").arg(QString::fromLatin1(reply))};
    return result;
}

bool fault(int state)
{
    return (state >= 5 && state <= 9) || state == 13;
}

QString listMode(const QByteArray &reply)
{
    const auto mode = reply.trimmed().toUpper();
    if (mode == "CONSTCURR" || mode == "CC") return QStringLiteral("CC");
    if (mode == "CONSTVOLT" || mode == "CV") return QStringLiteral("CV");
    if (mode == "CONSTPOWER" || mode == "CP") return QStringLiteral("CP");
    if (mode.startsWith("CONSTRES") || mode == "CR") return QStringLiteral("CR");
    if (mode == "SHORT" || mode == "SH") return QStringLiteral("SH");
    if (mode == "OPEN" || mode == "OP") return QStringLiteral("OP");
    throw Failure{PeakTech::tr("Unbekannte Listen-Betriebsart: %1").arg(QString::fromLatin1(reply))};
}

int integer(const QByteArray &reply, int maximum)
{
    bool ok = false;
    const int value = reply.trimmed().toInt(&ok);
    if (!ok || value < 0 || value > maximum)
        throw Failure{PeakTech::tr("Ungültiger Listenwert: %1").arg(QString::fromLatin1(reply))};
    return value;
}

}

bool isLoadActive(int status)
{
    return (status >= 1 && status <= 4) || (status >= 10 && status <= 12);
}

PeakTech::PeakTech(QObject *parent)
    : QObject(parent), m_port(new QSerialPort(this)), m_pollTimer(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &PeakTech::poll);
}

void PeakTech::perform(const std::function<void(PeakTech &)> &operation)
{
    try {
        operation(*this);
    } catch (const Failure &failure) {
        if (failure.disconnect)
            fail(failure.message, true);
        else
            emit operationFinished(failure.message, false);
        return;
    }
    emit operationFinished({}, false);
}

void PeakTech::open(const SerialSettings &settings)
{
    const bool sameDevice = settings.port == m_settings.port && settings.address == m_settings.address &&
                            settings.baud == m_settings.baud;
    m_mayBeOn = m_mayBeOn && sameDevice;
    m_settings = settings;
    m_port->setPortName(settings.port);
    if (!m_port->setBaudRate(settings.baud) ||
        !m_port->setDataBits(QSerialPort::Data8) ||
        !m_port->setParity(QSerialPort::NoParity) ||
        !m_port->setStopBits(QSerialPort::OneStop) ||
        !m_port->setFlowControl(QSerialPort::NoFlowControl) ||
        !m_port->open(QIODevice::ReadWrite)) {
        throw Failure{tr("%1 konnte nicht geöffnet werden: %2").arg(settings.port, m_port->errorString())};
    }
    m_port->clear();
    m_listUploaded = false;
    m_nextCommand.setRemainingTime(commandGapMs, Qt::PreciseTimer);
    const QByteArray reply = query("*IDN?", false);
    const QByteArray model = reply.split(',').value(0).simplified().toUpper();
    if (!model.contains("PEAKTECH") || !(model.contains("150W") || model.contains("2275")))
        throw Failure{tr("Unerwartete Gerätekennung: %1").arg(QString::fromLatin1(reply))};

    const Reading reading = readInstrument();
    if (reading.load == 0)
        m_mayBeOn = false;
    emit connectionChanged(QString::fromLatin1(reply).simplified());
    emit readingChanged(reading);
    readBasicSettings(reading.mode);
    emit loadSetupRead(readLoadSetup());
    m_pollTimer->start(normalPeriodMs);
}

void PeakTech::readBasicSettings(const QString &mode)
{
    if (const auto *basic = basicMode(mode)) {
        m_value = readNumber(QByteArray(basic->command) + '?', basic->unit);
        m_configuredMode = mode;
        emit basicRead(mode, m_value);
    }
}

void PeakTech::closePort()
{
    if (m_mayBeOn)
        stop();
    m_pollTimer->stop();
    m_port->close();
    m_configuredMode.clear();
    m_listUploaded = false;
    emit connectionChanged({});
}

void PeakTech::configure(const QString &mode, double value, double cutoff)
{
    if (m_listRunning)
        throw Failure{tr("Zuerst den Listentest stoppen."), false};
    const int load = readLoad();
    const auto *basic = basicMode(mode);
    if (load != 0 && (!isLoadActive(load) || !basic || query("LOAD:MODE?").toUpper() != mode.toLatin1()))
        throw Failure{tr("Zum Übernehmen der Einstellungen muss die Last aus sein."), false};
    if (mode == QStringLiteral("BAT")) {
        if (value > readNumber("MSET:IMAX?", "A"))
            throw Failure{tr("Der Entladestrom liegt über dem eingestellten Stromlimit."), false};
        m_configuredMode.clear();
        writeLine("BAT:CURR " + QByteArray::number(value, 'f', 3));
        writeLine("BAT:VMIN " + QByteArray::number(cutoff, 'f', 3));
    } else {
        if (!basic)
            throw Failure{tr("Unbekannte Betriebsart."), false};
        if (basic->limit && value > readNumber(basic->limit, basic->unit))
            throw Failure{tr("Der Sollwert liegt über dem eingestellten Gerätelimit."), false};
        m_configuredMode.clear();
        writeLine(QByteArray(basic->command) + ' ' + QByteArray::number(value, 'f', 3));
    }
    // The instrument rejects mode changes while the input is on.
    if (load == 0)
        writeLine("LOAD:MODE " + mode.toLatin1());
    m_configuredMode = mode;
    m_listUploaded = false;
    m_value = value;
    m_cutoff = cutoff;
    verifySettings();
    emit readingChanged(readInstrument());
    emit settingsApplied(mode);
}

LoadSetup PeakTech::readLoadSetup()
{
    LoadSetup setup;
    for (const auto &parameter : setupParameters)
        setup.*parameter.value = readNumber(QByteArray(parameter.command) + '?', parameter.unit);
    const double seconds = readNumber("MSET:AOFF?", "s");
    if (seconds < 0 || seconds > 60000 || seconds != std::floor(seconds))
        throw Failure{tr("Ungültige Abschaltzeit: %1 s").arg(seconds)};
    setup.autoOff = static_cast<int>(seconds);
    return setup;
}

void PeakTech::configureLoadSetup(const LoadSetup &requested)
{
    if (m_listRunning || readLoad() != 0)
        throw Failure{tr("Zum Übernehmen der Einstellungen muss die Last aus sein."), false};

    const auto current = readLoadSetup();
    for (const auto &parameter : setupParameters) {
        const double value = requested.*parameter.value;
        if (std::abs(current.*parameter.value - value) >= 0.0005)
            writeLine(QByteArray(parameter.command) + ' ' + QByteArray::number(value, 'f', 3));
    }
    if (current.autoOff != requested.autoOff)
        writeLine("MSET:AOFF " + QByteArray::number(requested.autoOff));

    const auto actual = readLoadSetup();
    emit loadSetupRead(actual);
    for (const auto &parameter : setupParameters) {
        if (std::abs(actual.*parameter.value - requested.*parameter.value) >= 0.0005)
            throw Failure{tr("Das Gerät hat den Parameter %1 nicht bestätigt.").arg(QString::fromLatin1(parameter.command)), false};
    }
    if (actual.autoOff != requested.autoOff)
        throw Failure{tr("Das Gerät hat die automatische Abschaltzeit nicht bestätigt."), false};
}

ListProgram PeakTech::readListProgram()
{
    ListProgram program;
    const int last = integer(query("LIST:SNUM?"), 14);
    const auto stepMode = query("LIST:SMOD?").toUpper();
    const auto repeat = query("LIST:REP?").toUpper();
    if ((stepMode != "AUTO" && stepMode != "TRIG") || (repeat != "ON" && repeat != "OFF"))
        throw Failure{tr("Ungültige Listen-Ablaufeinstellungen.")};
    program.triggered = stepMode == "TRIG";
    program.repeat = repeat == "ON";
    for (int i = 0; i <= last; ++i) {
        const auto prefix = "LIST:STEP" + QByteArray::number(i);
        ListStep step;
        step.mode = listMode(query(prefix + ":LMOD?"));
        const auto level = query(prefix + ":LEV?").split(',');
        const auto compare = query(prefix + ":COMP?").split(',');
        if (level.size() != 2 || compare.size() != 3)
            throw Failure{tr("Ungültige Daten für Listenschritt %1.").arg(i + 1)};
        const auto *basic = basicMode(step.mode);
        step.value = number(level[0], basic ? basic->unit : "");
        step.seconds = number(level[1], "s") / listTicksPerSecond;
        step.compare = QString::fromLatin1(compare[0].trimmed().toUpper());
        if (!QStringList{QStringLiteral("OFF"), QStringLiteral("VOLT"), QStringLiteral("CURR"),
                         QStringLiteral("POW")}.contains(step.compare))
            throw Failure{tr("Ungültige Daten für Listenschritt %1.").arg(i + 1)};
        step.low = number(compare[1], "");
        step.high = number(compare[2], "");
        program.steps.append(step);
    }
    return program;
}

void PeakTech::readList()
{
    if (m_listRunning || readLoad() != 0)
        throw Failure{tr("Zuerst die Last ausschalten."), false};
    const auto program = readListProgram();
    emit listRead(program);
}

void PeakTech::validateList(const ListProgram &program, const LoadSetup &limits)
{
    if (program.steps.isEmpty() || program.steps.size() > 15)
        throw Failure{tr("Eine Liste benötigt 1 bis 15 Schritte."), false};
    for (int i = 0; i < program.steps.size(); ++i) {
        const auto &step = program.steps[i];
        const double ticks = step.seconds * listTicksPerSecond;
        const bool timed = std::isfinite(ticks) && ticks >= 1 && ticks <= 60000 &&
                           std::abs(ticks - std::round(ticks)) < 0.0001;
        const auto *basic = basicMode(step.mode);
        bool valid = timed && std::isfinite(step.value) && step.value >= 0;
        if (step.mode == QStringLiteral("CC"))
            valid &= step.value <= limits.maxCurrent;
        else if (step.mode == QStringLiteral("CV"))
            valid &= step.value >= 1.5 && step.value <= limits.maxVoltage;
        else if (step.mode == QStringLiteral("CP"))
            valid &= step.value <= limits.maxPower;
        else if (step.mode == QStringLiteral("CR"))
            valid &= step.value >= 0.1 && step.value <= 4000;
        else
            valid &= step.mode == QStringLiteral("OP") || step.mode == QStringLiteral("SH");
        const bool compared = step.compare != QStringLiteral("OFF");
        valid &= QStringList{QStringLiteral("OFF"), QStringLiteral("VOLT"), QStringLiteral("CURR"),
                             QStringLiteral("POW")}.contains(step.compare);
        if (compared)
            valid &= std::isfinite(step.low) && std::isfinite(step.high) && step.low >= 0 && step.high >= step.low;
        if (!valid || (!basic && step.mode != QStringLiteral("OP") && step.mode != QStringLiteral("SH")))
            throw Failure{tr("Schritt %1 enthält einen ungültigen Wert oder überschreitet ein Gerätelimit.").arg(i + 1), false};
    }
}

void PeakTech::verifyList(const ListProgram &expected, const ListProgram &actual)
{
    bool matches = expected.steps.size() == actual.steps.size() && expected.triggered == actual.triggered &&
                   expected.repeat == actual.repeat;
    for (int i = 0; matches && i < expected.steps.size(); ++i) {
        const auto &wanted = expected.steps[i];
        const auto &got = actual.steps[i];
        const double resolution = wanted.value < 10 ? 0.001 : wanted.value < 100 ? 0.01 :
                                  wanted.value < 1000 ? 0.1 : 1;
        matches = wanted.mode == got.mode && wanted.compare == got.compare &&
                  std::abs(wanted.seconds - got.seconds) < 0.001;
        if (basicMode(wanted.mode))
            matches &= std::abs(wanted.value - got.value) <= resolution * 0.51;
        if (wanted.compare != QStringLiteral("OFF"))
            matches &= std::abs(wanted.low - got.low) < 0.00051 && std::abs(wanted.high - got.high) < 0.00051;
    }
    if (!matches)
        throw Failure{tr("Das Gerät hat die Liste nicht vollständig bestätigt. Liste erneut übertragen."), false};
}

void PeakTech::uploadList(const ListProgram &program)
{
    m_listUploaded = false;
    if (m_test || m_listRunning || readLoad() != 0)
        throw Failure{tr("Zuerst die Last ausschalten."), false};
    validateList(program, readLoadSetup());
    for (int i = 0; i < program.steps.size(); ++i) {
        const auto &step = program.steps[i];
        const auto prefix = "LIST:STEP" + QByteArray::number(i);
        writeLine(prefix + ":LMOD " + step.mode.toLatin1());
        writeLine(prefix + ":LEV " + QByteArray::number(step.value, 'f', 3) + ',' +
                  QByteArray::number(qRound64(step.seconds * listTicksPerSecond)));
        writeLine(prefix + ":COMP " + step.compare.toLatin1() + ',' +
                  QByteArray::number(step.low, 'f', 3) + ',' + QByteArray::number(step.high, 'f', 3));
    }
    writeLine("LIST:SNUM " + QByteArray::number(program.steps.size() - 1));
    writeLine(program.triggered ? "LIST:SMOD TRIG" : "LIST:SMOD AUTO");
    writeLine(program.repeat ? "LIST:REP ON" : "LIST:REP OFF");
    verifyList(program, readListProgram());
    writeLine("LOAD:MODE LIST");
    if (query("LOAD:MODE?").toUpper() != "LIST")
        throw Failure{tr("Das Gerät hat den Listenmodus nicht bestätigt."), false};
    m_configuredMode.clear();
    m_listProgram = program;
    m_listUploaded = true;
    emit readingChanged(readInstrument());
    emit listUploaded();
}

void PeakTech::startList()
{
    if (!m_listUploaded || m_test || m_listRunning || readLoad() != 0)
        throw Failure{tr("Zuerst die Liste übertragen und die Last ausschalten."), false};
    validateList(m_listProgram, readLoadSetup());
    verifyList(m_listProgram, readListProgram());
    if (query("LOAD:MODE?").toUpper() != "LIST")
        throw Failure{tr("Das Gerät hat den Listenmodus nicht bestätigt."), false};
    if (m_listProgram.triggered) {
        writeLine("TRIG:SOUR BUS");
        if (query("TRIG:SOUR?").toUpper() != "BUS")
            throw Failure{tr("Der Software-Trigger wurde nicht bestätigt."), false};
    }
    m_mayBeOn = true;
    m_listRunning = true;
    emit listStarted();
    writeLine("LOAD ON");
    m_pollTimer->start(listPeriodMs);
    const auto reading = readInstrument();
    emit readingChanged(reading);
    updateList(reading);
}

void PeakTech::triggerList()
{
    if (!m_listRunning || !m_listProgram.triggered)
        throw Failure{tr("Kein Listentest mit Software-Trigger aktiv."), false};
    writeLine("TRIG");
}

void PeakTech::verifySettings()
{
    const QByteArray mode = query("LOAD:MODE?").toUpper();
    bool matches = mode == m_configuredMode.toLatin1();
    if (m_configuredMode == QStringLiteral("BAT")) {
        const double current = readNumber("BAT:CURR?", "A");
        const double cutoff = readNumber("BAT:VMIN?", "V");
        matches &= std::abs(current - m_value) < 0.0051 && std::abs(cutoff - m_cutoff) < 0.0051;
    } else if (const auto *basic = basicMode(m_configuredMode)) {
        const double value = readNumber(QByteArray(basic->command) + '?', basic->unit);
        // The instrument can round the requested value according to its selected range.
        const double resolution = m_value < 10 ? 0.001 : m_value < 100 ? 0.01 : m_value < 1000 ? 0.1 : 1;
        matches &= std::abs(value - m_value) <= resolution * 0.51;
    }
    if (!matches) {
        m_configuredMode.clear();
        emit settingsInvalidated();
        throw Failure{tr("Das Gerät hat die Werte nicht bestätigt. Einstellungen erneut übernehmen."), false};
    }
}

void PeakTech::start(const QString &mode, int period)
{
    if (readLoad() != 0)
        throw Failure{tr("Die Last ist bereits aktiv oder meldet einen Fehler."), false};
    if (mode != m_configuredMode)
        throw Failure{tr("Zuerst die Einstellungen übernehmen."), false};
    verifySettings();

    if (mode == QStringLiteral("BAT")) {
        const Reading before = readInstrument();
        if (!before.measured || before.volts < 0.8 || before.volts > 120 || before.volts <= m_cutoff)
            throw Failure{tr("Die Eingangsspannung muss im Batterietestbereich (0,8–120 V) "
                             "und über der Abschaltspannung liegen."), false};
        if (before.volts > readNumber("MSET:VMAX?", "V") ||
            before.volts * m_value > readNumber("MSET:PMAX?", "W"))
            throw Failure{tr("Der Batterietest würde das Spannungs- oder Leistungslimit überschreiten."), false};
        if (m_value > readNumber("MSET:IMAX?", "A"))
            throw Failure{tr("Der Entladestrom liegt über dem eingestellten Stromlimit."), false};
    } else if (const auto *basic = basicMode(mode)) {
        if (basic->limit && m_value > readNumber(basic->limit, basic->unit))
            throw Failure{tr("Der Sollwert liegt über dem eingestellten Gerätelimit."), false};
    }
    m_mayBeOn = true;
    writeLine("LOAD ON");
    m_test = mode == QStringLiteral("BAT");
    if (m_test) {
        m_periodSeconds = period;
        m_lastSecond = -1;
        emit testStarted();
    }
    const auto reading = readInstrument();
    emit readingChanged(reading);
    if (reading.load == 0)
        m_mayBeOn = false;
    if (m_test) {
        updateTest(reading);
    } else if (reading.load == 0 || fault(reading.load)) {
        if (m_mayBeOn)
            turnOff();
        throw Failure{tr("Die Last konnte nicht eingeschaltet werden."), false};
    }
}

void PeakTech::stop()
{
    if (!m_port->isOpen())
        throw Failure{tr("Keine Verbindung zum Gerät."), false};
    m_mayBeOn = true;
    turnOff();
    if (m_listRunning)
        finishList(TestEnd::Stopped, 0);
    if (m_test) {
        const Reading last = readInstrument();
        recordBatteryReading(last, true);
        finishTest(TestEnd::Stopped, last.load);
        emit readingChanged(last);
    } else {
        emit readingChanged(readInstrument());
    }
}

void PeakTech::turnOff()
{
    writeLine("LOAD OFF");
    // A protection state can precede the physical shutdown. Require OFF.
    if (readLoad() != 0)
        throw Failure{tr("Das Gerät hat das Abschalten der Last nicht bestätigt.")};
    m_mayBeOn = false;
}

Reading PeakTech::readInstrument()
{
    Reading reading;
    // While running, poll FETCH only to preserve the one-shot completion record.
    reading.mode = m_listRunning ? QStringLiteral("LIST") :
                                  QString::fromLatin1(query("LOAD:MODE?").toUpper());
    const auto reply = query("FETCH?");
    reading.load = m_listRunning && reply.startsWith("L2:") ? 10 : readLoad();
    if (reading.mode == QStringLiteral("LIST")) {
        const auto data = reply.mid(3).split(',');
        if (reply.startsWith("L1:") && data.size() == 1) {
            reading.volts = number(data[0], "V");
            reading.measured = true;
            reading.listIdle = true;
        } else if (reply.startsWith("L2:") && data.size() == 4) {
            reading.listStep = integer(data[0], 14);
            reading.volts = number(data[1], "V");
            reading.amps = number(data[2], "A");
            reading.measured = reading.currentKnown = true;
        } else if (reply.startsWith("L3:") && data.size() >= 2) {
            reading.listOverall = integer(data[0], 2);
            for (int i = 1; i < data.size(); ++i)
                reading.listResults.append(integer(data[i], 2));
            if (m_listRunning && reading.listResults.size() != m_listProgram.steps.size())
                throw Failure{tr("Unerwartete Listendaten: %1").arg(QString::fromLatin1(reply))};
        } else {
            throw Failure{tr("Unerwartete Listendaten: %1").arg(QString::fromLatin1(reply))};
        }
        return reading;
    }
    if (reply.toUpper() == "N/A")
        return reading;
    const auto fields = reply.split(',');
    if (fields.size() != 2 && fields.size() != 4)
        throw Failure{tr("Unerwartete Messdaten: %1").arg(QString::fromLatin1(reply))};
    reading.volts = number(fields[0], "V");
    reading.amps = number(fields[1], "A");
    reading.measured = reading.currentKnown = true;
    if (fields.size() == 4 && reading.mode == QStringLiteral("BAT")) {
        auto capacity = fields[2].trimmed();
        auto dischargeTime = fields[3].trimmed();
        // Some firmware reports the time before the capacity, with unit suffixes.
        if (capacity.toLower().endsWith('s') || dischargeTime.toLower().endsWith("ah"))
            std::swap(capacity, dischargeTime);
        reading.ampHours = number(capacity, "Ah");
        const double seconds = number(dischargeTime, "s");
        if (seconds < 0 || seconds > 2147483647 || seconds != std::floor(seconds))
            throw Failure{tr("Ungültige Entladezeit: %1").arg(QString::fromLatin1(dischargeTime))};
        reading.seconds = static_cast<int>(seconds);
        reading.batteryData = true;
    }
    return reading;
}

int PeakTech::readLoad()
{
    const QByteArray reply = query("LOAD?");
    bool ok = false;
    const int value = reply.toInt(&ok);
    if (!ok || value < 0 || value > 13)
        throw Failure{tr("Unbekannter Laststatus: %1").arg(QString::fromLatin1(reply))};
    return value;
}

void PeakTech::poll()
{
    try {
        const Reading reading = readInstrument();
        emit readingChanged(reading);
        if (reading.load == 0 && !m_listRunning)
            m_mayBeOn = false;
        if (m_listRunning)
            updateList(reading);
        else if (m_test)
            updateTest(reading);
        else if (isLoadActive(reading.load) && reading.mode != m_configuredMode)
            readBasicSettings(reading.mode);
    } catch (const Failure &failure) {
        fail(failure.message, false);
    }
}

void PeakTech::updateList(const Reading &reading)
{
    if (reading.mode != QStringLiteral("LIST") || fault(reading.load) || reading.load == 11) {
        turnOff();
        finishList(TestEnd::Fault, reading.load);
    } else if (reading.listOverall >= 0 && !m_listProgram.repeat) {
        turnOff();
        finishList(TestEnd::DeviceStopped, reading.load);
    } else if (reading.listIdle && reading.load == 0) {
        turnOff();
        finishList(TestEnd::DeviceStopped, reading.load);
    }
}

void PeakTech::finishList(TestEnd cause, int state)
{
    m_listRunning = false;
    m_pollTimer->start(normalPeriodMs);
    emit listFinished(cause, state);
}

void PeakTech::updateTest(const Reading &reading)
{
    if (reading.mode != QStringLiteral("BAT") || fault(reading.load) || reading.load == 11) {
        if (m_mayBeOn)
            turnOff();
        finishTest(TestEnd::Fault, reading.load);
        return;
    }
    const bool cutoff = reading.measured && reading.volts <= m_cutoff;
    recordBatteryReading(reading, reading.load == 0 || cutoff);
    if (reading.load == 0) {
        finishTest(cutoff ? TestEnd::Cutoff : TestEnd::DeviceStopped, reading.load);
    } else if (cutoff) {
        turnOff();
        const Reading last = readInstrument();
        recordBatteryReading(last, true);
        emit readingChanged(last);
        finishTest(TestEnd::Cutoff, last.load);
    }
}

void PeakTech::recordBatteryReading(const Reading &reading, bool final)
{
    const qint64 elapsed = static_cast<qint64>(reading.seconds) - m_lastSecond;
    if (reading.batteryData && (m_lastSecond < 0 || elapsed >= m_periodSeconds || (final && elapsed >= 0))) {
        emit sampleRecorded(reading);
        m_lastSecond = reading.seconds;
    }
}

void PeakTech::finishTest(TestEnd cause, int state)
{
    m_test = false;
    emit testFinished(cause, state);
}

void PeakTech::fail(const QString &error, bool operation)
{
    if (m_mayBeOn && m_port->isOpen()) {
        for (int attempt = 0; attempt < 2 && m_mayBeOn; ++attempt) {
            try {
                m_port->clear(QSerialPort::Input);
                turnOff();
            } catch (const Failure &) {
            }
        }
    }
    const bool unconfirmed = m_mayBeOn;
    if (m_test)
        finishTest(TestEnd::Disconnected, 0);
    if (m_listRunning)
        finishList(TestEnd::Disconnected, 0);
    m_pollTimer->stop();
    m_port->close();
    m_configuredMode.clear();
    m_listUploaded = false;
    emit connectionChanged({});
    if (operation)
        emit operationFinished(error, unconfirmed);
    else
        emit communicationLost(error, unconfirmed);
}

void PeakTech::writeLine(const QByteArray &command, bool addressed)
{
    const qint64 remainingGap = m_nextCommand.remainingTime();
    if (remainingGap > 0)
        QThread::msleep(static_cast<unsigned long>(remainingGap));

    QByteArray line = command;
    if (addressed)
        line.prepend("*ADR " + QByteArray::number(m_settings.address) + ';');
    line += "\r\n";
    if (m_port->write(line) != line.size())
        throw Failure{tr("Senden fehlgeschlagen (%1): %2").arg(QString::fromLatin1(command), m_port->errorString())};
    QElapsedTimer elapsed;
    elapsed.start();
    while (m_port->bytesToWrite()) {
        const int remaining = timeoutMs - static_cast<int>(elapsed.elapsed());
        if (remaining <= 0 || !m_port->waitForBytesWritten(remaining))
            throw Failure{tr("Senden fehlgeschlagen (%1): %2").arg(QString::fromLatin1(command), m_port->errorString())};
    }
    // Setters have no reply and can keep the firmware busy longer than queries.
    const int gap = command.endsWith('?') ? commandGapMs : setterGapMs;
    m_nextCommand.setRemainingTime(gap, Qt::PreciseTimer);
}

QByteArray PeakTech::query(const QByteArray &command, bool addressed)
{
    writeLine(command, addressed);
    QElapsedTimer elapsed;
    elapsed.start();
    while (!m_port->canReadLine()) {
        const int remaining = timeoutMs - static_cast<int>(elapsed.elapsed());
        if (remaining <= 0 || m_port->bytesAvailable() > 1024 || !m_port->waitForReadyRead(remaining))
            throw Failure{tr("Keine vollständige Antwort auf %1 (%2).")
                              .arg(QString::fromLatin1(command), m_port->errorString())};
    }
    const auto line = m_port->readLine().trimmed();
    m_nextCommand.setRemainingTime(commandGapMs, Qt::PreciseTimer);
    if (line.isEmpty() || line.toUpper().startsWith("ERR"))
        throw Failure{tr("Gerätefehler bei %1: %2").arg(QString::fromLatin1(command), QString::fromLatin1(line))};
    return line;
}

double PeakTech::readNumber(const QByteArray &command, const QByteArray &unit)
{
    return number(query(command), unit);
}


