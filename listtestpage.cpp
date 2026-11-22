#include "listtestpage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QStringList listModes{QStringLiteral("CC"), QStringLiteral("CV"), QStringLiteral("CP"),
                           QStringLiteral("CR"), QStringLiteral("OP"), QStringLiteral("SH")};
const QStringList comparisons{QStringLiteral("OFF"), QStringLiteral("VOLT"),
                             QStringLiteral("CURR"), QStringLiteral("POW")};
const QStringList modeUnits{QStringLiteral(" A"), QStringLiteral(" V"), QStringLiteral(" W"),
                           QStringLiteral(" Ω"), QString(), QString()};
const QStringList compareUnits{QString(), QStringLiteral(" V"), QStringLiteral(" A"), QStringLiteral(" W")};
constexpr int modeColumn = 0;
constexpr int valueColumn = 1;
constexpr int durationColumn = 2;
constexpr int compareColumn = 3;
constexpr int lowColumn = 4;
constexpr int highColumn = 5;

QDoubleSpinBox *numericCell(QTableWidget *table, int row, int column)
{
    return qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, column));
}

QComboBox *choiceCell(QTableWidget *table, int row, int column)
{
    return qobject_cast<QComboBox *>(table->cellWidget(row, column));
}
}

ListTestPage::ListTestPage(QWidget *parent)
    : QWidget(parent), m_numberLocale(QLocale::German)
{
    setObjectName(QStringLiteral("listTestPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(8);

    auto *summary = new QWidget;
    summary->setObjectName(QStringLiteral("testSummary"));
    auto *status = new QGridLayout(summary);
    status->setContentsMargins(14, 9, 14, 10);
    const auto addMetric = [status](int column, QLabel *label, QLabel *value) {
        label->setProperty("metric", "label");
        value->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 600;"));
        status->addWidget(label, 0, column);
        status->addWidget(value, 1, column);
        status->setColumnStretch(column, column == 0 ? 3 : 1);
    };
    addMetric(0, m_stateLabel = new QLabel, m_state = new QLabel);
    addMetric(1, m_stepLabel = new QLabel, m_activeStep = new QLabel);
    addMetric(2, m_elapsedLabel = new QLabel, m_elapsed = new QLabel(QStringLiteral("—")));
    addMetric(3, m_cycleLabel = new QLabel, m_cycle = new QLabel(QStringLiteral("—")));
    m_start = new QPushButton;
    m_start->setObjectName(QStringLiteral("startList"));
    m_start->setProperty("primary", true);
    m_stop = new QPushButton;
    m_stop->setObjectName(QStringLiteral("stop"));
    m_trigger = new QPushButton;
    m_trigger->setObjectName(QStringLiteral("triggerList"));
    status->addWidget(m_start, 0, 4, 2, 1);
    status->addWidget(m_trigger, 0, 5, 2, 1);
    status->addWidget(m_stop, 0, 6, 2, 1);
    layout->addWidget(summary);

    m_controlGroup = new QGroupBox;
    auto *controls = new QHBoxLayout(m_controlGroup);
    controls->setContentsMargins(14, 8, 14, 12);
    m_advanceLabel = new QLabel;
    m_advance = new QComboBox;
    m_advance->setObjectName(QStringLiteral("listAdvance"));
    m_advance->addItems({QString(), QString()});
    m_advanceLabel->setBuddy(m_advance);
    m_repeat = new QCheckBox;
    m_repeat->setObjectName(QStringLiteral("listRepeat"));
    m_read = new QPushButton;
    m_read->setObjectName(QStringLiteral("readList"));
    m_upload = new QPushButton;
    m_upload->setObjectName(QStringLiteral("uploadList"));
    controls->addWidget(m_advanceLabel);
    controls->addWidget(m_advance);
    controls->addSpacing(14);
    controls->addWidget(m_repeat);
    controls->addStretch();
    controls->addWidget(m_read);
    controls->addWidget(m_upload);
    layout->addWidget(m_controlGroup);

    auto *split = new QSplitter(Qt::Vertical);
    split->setChildrenCollapsible(false);
    m_stepGroup = new QGroupBox;
    auto *stepLayout = new QVBoxLayout(m_stepGroup);
    stepLayout->setContentsMargins(10, 8, 10, 10);
    auto *toolbar = new QHBoxLayout;
    m_add = new QPushButton;
    m_duplicate = new QPushButton;
    m_remove = new QPushButton;
    m_up = new QPushButton(QStringLiteral("↑"));
    m_down = new QPushButton(QStringLiteral("↓"));
    m_up->setFixedWidth(38);
    m_down->setFixedWidth(38);
    m_summary = new QLabel;
    for (auto *button : {m_add, m_duplicate, m_remove, m_up, m_down})
        toolbar->addWidget(button);
    toolbar->addStretch();
    toolbar->addWidget(m_summary);
    stepLayout->addLayout(toolbar);
    m_steps = new QTableWidget(0, 6);
    m_steps->setObjectName(QStringLiteral("listSteps"));
    m_steps->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_steps->setSelectionMode(QAbstractItemView::SingleSelection);
    m_steps->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_steps->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_steps->verticalHeader()->setDefaultSectionSize(44);
    m_steps->setMinimumHeight(130);
    stepLayout->addWidget(m_steps);
    split->addWidget(m_stepGroup);

    m_resultGroup = new QGroupBox;
    auto *results = new QVBoxLayout(m_resultGroup);
    results->setContentsMargins(10, 8, 10, 10);
    m_results = new QTableWidget(0, 8);
    m_results->setObjectName(QStringLiteral("listResults"));
    m_results->setAlternatingRowColors(true);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->verticalHeader()->hide();
    m_results->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_results->setMinimumHeight(110);
    results->addWidget(m_results);
    split->addWidget(m_resultGroup);
    split->setSizes({245, 180});
    layout->addWidget(split, 1);

    connect(m_add, &QPushButton::clicked, this, [this] { addStep({}); edited(); });
    connect(m_duplicate, &QPushButton::clicked, this, [this] {
        const int selected = m_steps->currentRow();
        auto value = program();
        value.steps.insert(selected + 1, value.steps.at(selected));
        rebuild(value, selected + 1);
        edited();
    });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        const int row = m_steps->currentRow();
        m_steps->removeRow(row);
        m_steps->setCurrentCell(qMin(row, m_steps->rowCount() - 1), 0);
        edited();
    });
    const auto move = [this](int delta) {
        const int row = m_steps->currentRow();
        auto value = program();
        value.steps.swapItemsAt(row, row + delta);
        rebuild(value, row + delta);
        edited();
    };
    connect(m_up, &QPushButton::clicked, this, [move] { move(-1); });
    connect(m_down, &QPushButton::clicked, this, [move] { move(1); });
    connect(m_steps, &QTableWidget::itemSelectionChanged, this, &ListTestPage::updateControls);
    connect(m_advance, qOverload<int>(&QComboBox::currentIndexChanged), this, &ListTestPage::edited);
    connect(m_repeat, &QCheckBox::toggled, this, &ListTestPage::edited);
    connect(m_read, &QPushButton::clicked, this, [this] {
        m_pending = Pending::Read;
        emit readRequested();
    });
    connect(m_upload, &QPushButton::clicked, this, [this] {
        m_uploaded = false;
        m_pending = Pending::Upload;
        emit uploadRequested(program());
    });
    connect(m_start, &QPushButton::clicked, this, [this] {
        m_pending = Pending::Start;
        emit startRequested();
    });
    connect(m_stop, &QPushButton::clicked, this, &ListTestPage::stopRequested);
    connect(m_trigger, &QPushButton::clicked, this, &ListTestPage::triggerRequested);
    m_clock = new QTimer(this);
    connect(m_clock, &QTimer::timeout, this, [this] {
        m_elapsed->setText(m_numberLocale.toString(m_started.elapsed() / 1000.0, 'f', 1) + QStringLiteral(" s"));
    });
    setProgram({{ListStep{}, ListStep{}}, false, false});
}

ListProgram ListTestPage::program() const
{
    ListProgram value;
    value.triggered = m_advance->currentIndex() == 1;
    value.repeat = m_repeat->isChecked();
    for (int row = 0; row < m_steps->rowCount(); ++row) {
        ListStep step;
        step.mode = listModes.at(choiceCell(m_steps, row, modeColumn)->currentIndex());
        step.compare = comparisons.at(choiceCell(m_steps, row, compareColumn)->currentIndex());
        step.value = numericCell(m_steps, row, valueColumn)->value();
        step.seconds = numericCell(m_steps, row, durationColumn)->value();
        step.low = numericCell(m_steps, row, lowColumn)->value();
        step.high = numericCell(m_steps, row, highColumn)->value();
        value.steps.append(step);
    }
    return value;
}

void ListTestPage::addStep(const ListStep &step)
{
    const int row = m_steps->rowCount();
    if (row >= 15)
        return;
    m_steps->insertRow(row);
    for (int column : {modeColumn, compareColumn}) {
        auto *combo = new QComboBox;
        const auto &choices = column == modeColumn ? listModes : comparisons;
        combo->addItems(choices);
        combo->setCurrentIndex(qMax(0, choices.indexOf(column == modeColumn ? step.mode : step.compare)));
        m_steps->setCellWidget(row, column, combo);
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, combo] {
            for (int i = 0; i < m_steps->rowCount(); ++i) {
                if (m_steps->cellWidget(i, modeColumn) == combo || m_steps->cellWidget(i, compareColumn) == combo) {
                    updateRow(i);
                    break;
                }
            }
            edited();
        });
    }
    const double values[] = {0, step.value, step.seconds, 0, step.low, step.high};
    for (int column : {valueColumn, durationColumn, lowColumn, highColumn}) {
        auto *input = new QDoubleSpinBox;
        input->setLocale(m_numberLocale);
        input->setRange(0, column == durationColumn ? 6000 : 4000);
        input->setDecimals(column == durationColumn ? 1 : 3);
        input->setSingleStep(column == durationColumn ? 0.1 : 0.01);
        input->setKeyboardTracking(false);
        input->setValue(values[column]);
        m_steps->setCellWidget(row, column, input);
        connect(input, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ListTestPage::edited);
    }
    numericCell(m_steps, row, durationColumn)->setSuffix(QStringLiteral(" s"));
    updateRow(row);
    m_steps->setCurrentCell(row, 0);
    retranslate();
}

void ListTestPage::updateRow(int row)
{
    const int mode = choiceCell(m_steps, row, modeColumn)->currentIndex();
    const int compare = choiceCell(m_steps, row, compareColumn)->currentIndex();
    auto *value = numericCell(m_steps, row, valueColumn);
    const double maximum[] = {30, 360, 150, 4000, 0, 0};
    value->setMaximum(maximum[mode]);
    value->setEnabled(mode < 4);
    value->setSuffix(modeUnits[mode]);
    for (int column : {lowColumn, highColumn}) {
        auto *limit = numericCell(m_steps, row, column);
        limit->setEnabled(compare != 0);
        limit->setSuffix(compareUnits[compare]);
    }
}

void ListTestPage::setProgram(const ListProgram &program)
{
    rebuild(program, 0);
    edited();
}

void ListTestPage::rebuild(const ListProgram &value, int selected)
{
    m_rebuilding = true;
    m_steps->setRowCount(0);
    m_advance->setCurrentIndex(value.triggered ? 1 : 0);
    m_repeat->setChecked(value.repeat);
    for (const auto &step : value.steps)
        addStep(step);
    m_steps->setCurrentCell(selected, 0);
    m_rebuilding = false;
    retranslate();
}

void ListTestPage::edited()
{
    if (m_rebuilding)
        return;
    m_uploaded = false;
    m_hasRun = false;
    retranslate();
}

void ListTestPage::setDeviceState(bool connected, bool busy, bool idle)
{
    m_connected = connected;
    m_busy = busy;
    m_idle = idle;
    if (!busy)
        m_pending = Pending::None;
    if (!connected)
        m_uploaded = false;
    updateControls();
    updateStatus();
}

void ListTestPage::invalidate()
{
    m_uploaded = false;
    updateControls();
    updateStatus();
}

void ListTestPage::markUploaded()
{
    m_uploaded = true;
    m_hasRun = false;
    updateControls();
    updateStatus();
}

void ListTestPage::updateControls()
{
    const bool editable = !m_busy && !m_running;
    m_steps->setEnabled(editable);
    m_advance->setEnabled(editable);
    m_repeat->setEnabled(editable);
    const int selected = m_steps->currentRow();
    m_add->setEnabled(editable && m_steps->rowCount() < 15);
    m_duplicate->setEnabled(editable && selected >= 0 && m_steps->rowCount() < 15);
    m_remove->setEnabled(editable && selected >= 0 && m_steps->rowCount() > 1);
    m_up->setEnabled(editable && selected > 0);
    m_down->setEnabled(editable && selected >= 0 && selected + 1 < m_steps->rowCount());
    m_read->setEnabled(m_connected && !m_busy && m_idle);
    m_upload->setEnabled(m_read->isEnabled() && m_steps->rowCount() > 0);
    m_start->setEnabled(m_uploaded && m_upload->isEnabled());
    m_stop->setEnabled(m_connected && !m_busy && m_running);
    m_trigger->setEnabled(m_stop->isEnabled() && m_executing.triggered);
}

void ListTestPage::updateSummary()
{
    double total = 0;
    for (const auto &step : program().steps)
        total += step.seconds;
    m_summary->setText(tr("%1 Schritte · %2 s je Durchlauf").arg(m_steps->rowCount())
                           .arg(m_numberLocale.toString(total, 'f', 1)));
}

void ListTestPage::beginTest()
{
    m_executing = program();
    m_running = m_hasRun = true;
    m_overall = -1;
    m_currentStep = -1;
    m_cycleNumber = 1;
    m_cycleFinished = false;
    m_samples = QVector<Reading>(m_executing.steps.size());
    m_sampleTimes = QVector<double>(m_executing.steps.size(), -1);
    m_results->setRowCount(0);
    m_started.start();
    m_elapsed->setText(m_numberLocale.toString(0.0, 'f', 1) + QStringLiteral(" s"));
    m_clock->start(100);
    updateStatus();
    updateControls();
}

void ListTestPage::finishTest(TestEnd cause, int)
{
    m_running = false;
    m_end = cause;
    m_clock->stop();
    updateStatus();
    updateControls();
}

void ListTestPage::showReading(const Reading &reading)
{
    if (!m_running || reading.mode != QStringLiteral("LIST"))
        return;
    if (reading.listStep >= 0 && reading.listStep < m_samples.size()) {
        if (m_cycleFinished) {
            ++m_cycleNumber;
            m_cycleFinished = false;
            m_overall = -1;
            m_samples.fill(Reading{});
            m_sampleTimes.fill(-1);
        }
        m_currentStep = reading.listStep;
        m_samples[reading.listStep] = reading;
        m_sampleTimes[reading.listStep] = m_started.elapsed() / 1000.0;
        m_steps->selectRow(reading.listStep);
    }
    if (reading.listOverall >= 0)
        completeCycle(reading);
    updateStatus();
}

void ListTestPage::completeCycle(const Reading &reading)
{
    if (m_cycleFinished) {
        ++m_cycleNumber;
        m_samples.fill(Reading{});
        m_sampleTimes.fill(-1);
    }
    m_overall = reading.listOverall;
    m_cycleFinished = true;
    m_currentStep = m_executing.steps.size() - 1;
    for (int stepIndex = 0; stepIndex < m_executing.steps.size(); ++stepIndex) {
        const auto &step = m_executing.steps[stepIndex];
        const int row = m_results->rowCount();
        m_results->insertRow(row);
        const auto &sample = m_samples[stepIndex];
        const int code = reading.listResults.value(stepIndex, -1);
        const QStringList cells{
            QString::number(m_cycleNumber), QString::number(stepIndex + 1), modeText(step.mode),
            sample.measured ? m_numberLocale.toString(sample.volts, 'f', 3) : QStringLiteral("—"),
            sample.currentKnown ? m_numberLocale.toString(sample.amps, 'f', 3) : QStringLiteral("—"),
            m_numberLocale.toString(step.seconds, 'f', 1),
            m_sampleTimes[stepIndex] >= 0 ? m_numberLocale.toString(m_sampleTimes[stepIndex], 'f', 1) : QStringLiteral("—"),
            resultText(code)};
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells[column]);
            item->setTextAlignment(column == 2 || column == 7 ? Qt::AlignLeft | Qt::AlignVCenter :
                                   Qt::AlignRight | Qt::AlignVCenter);
            if (column == 7)
                item->setData(Qt::UserRole, code);
            if (column == 2)
                item->setData(Qt::UserRole, step.mode);
            if (column >= 3 && column <= 6) {
                const double values[] = {sample.volts, sample.amps, step.seconds, m_sampleTimes[stepIndex]};
                if (cells[column] != QStringLiteral("—"))
                    item->setData(Qt::UserRole, values[column - 3]);
            }
            m_results->setItem(row, column, item);
        }
    }
    m_results->scrollToBottom();
}

QString ListTestPage::resultText(int code) const
{
    switch (code) {
    case 0: return tr("Ohne Prüfung");
    case 1: return tr("Bestanden");
    case 2: return tr("Fehlgeschlagen");
    default: return tr("Kein Ergebnis");
    }
}

QString ListTestPage::modeText(const QString &mode) const
{
    if (mode == QStringLiteral("OP")) return tr("Offen");
    if (mode == QStringLiteral("SH")) return tr("Kurzschluss");
    return mode;
}

void ListTestPage::updateStatus()
{
    m_activeStep->setText(m_currentStep >= 0 ? tr("%1 von %2").arg(m_currentStep + 1).arg(m_executing.steps.size()) :
                                               QStringLiteral("—"));
    m_cycle->setText(m_cycleNumber ? QString::number(m_cycleNumber) : QStringLiteral("—"));
    if (m_running) {
        m_state->setText(m_executing.triggered ? tr("Läuft · Triggerbetrieb") : tr("Läuft"));
    } else if (m_pending == Pending::Read) {
        m_state->setText(tr("Lese Liste vom Gerät…"));
    } else if (m_pending == Pending::Upload) {
        m_state->setText(tr("Übertrage und prüfe Liste…"));
    } else if (m_pending == Pending::Start) {
        m_state->setText(tr("Prüfe und starte Test…"));
    } else if (m_hasRun) {
        if (m_end == TestEnd::Fault) m_state->setText(tr("Gerätefehler · abgebrochen"));
        else if (m_end == TestEnd::Disconnected) m_state->setText(tr("Verbindung unterbrochen"));
        else if (m_end == TestEnd::Stopped) m_state->setText(tr("Manuell gestoppt"));
        else if (m_overall >= 0) m_state->setText(tr("Beendet · %1").arg(resultText(m_overall)));
        else m_state->setText(tr("Vom Gerät beendet"));
    } else {
        m_state->setText(m_uploaded ? tr("Übertragen · startbereit") : tr("Liste bearbeiten und übertragen"));
    }
}

void ListTestPage::setNumberLocale(const QLocale &locale)
{
    m_numberLocale = locale;
    for (auto *input : m_steps->findChildren<QDoubleSpinBox *>())
        input->setLocale(locale);
    retranslate();
}

void ListTestPage::retranslate()
{
    m_stateLabel->setText(tr("Teststatus"));
    m_stepLabel->setText(tr("Aktueller Schritt"));
    m_elapsedLabel->setText(tr("Laufzeit"));
    m_cycleLabel->setText(tr("Durchlauf"));
    m_controlGroup->setTitle(tr("Ablauf"));
    m_stepGroup->setTitle(tr("Schrittliste"));
    m_resultGroup->setTitle(tr("Messprotokoll"));
    m_advanceLabel->setText(tr("Fortschalten"));
    m_advance->setItemText(0, tr("Automatisch"));
    m_advance->setItemText(1, tr("Software-Trigger"));
    m_repeat->setText(tr("Wiederholen"));
    m_start->setText(tr("Test starten"));
    m_stop->setText(tr("Stoppen"));
    m_trigger->setText(tr("Trigger"));
    m_read->setText(tr("Vom Gerät lesen"));
    m_upload->setText(tr("Liste übertragen"));
    m_add->setText(tr("Hinzufügen"));
    m_duplicate->setText(tr("Duplizieren"));
    m_remove->setText(tr("Entfernen"));
    m_up->setToolTip(tr("Schritt nach oben"));
    m_down->setToolTip(tr("Schritt nach unten"));
    m_steps->setHorizontalHeaderLabels({tr("Betriebsart"), tr("Sollwert"), tr("Dauer"), tr("Prüfgröße"),
                                        tr("Untergrenze"), tr("Obergrenze")});
    m_results->setHorizontalHeaderLabels({tr("Durchlauf"), tr("Schritt"), tr("Betriebsart"),
                                          tr("Spannung (V)"), tr("Strom (A)"), tr("Dauer (s)"),
                                          tr("Messzeit (s)"), tr("Ergebnis")});
    for (int row = 0; row < m_steps->rowCount(); ++row) {
        auto *mode = choiceCell(m_steps, row, modeColumn);
        mode->setItemText(4, tr("Offen"));
        mode->setItemText(5, tr("Kurzschluss"));
        auto *compare = choiceCell(m_steps, row, compareColumn);
        compare->setItemText(0, tr("Keine"));
        compare->setItemText(1, tr("Spannung"));
        compare->setItemText(2, tr("Strom"));
        compare->setItemText(3, tr("Leistung"));
        m_steps->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(row + 1)));
    }
    for (int row = 0; row < m_results->rowCount(); ++row) {
        auto *result = m_results->item(row, 7);
        result->setText(resultText(result->data(Qt::UserRole).toInt()));
        auto *mode = m_results->item(row, 2);
        mode->setText(modeText(mode->data(Qt::UserRole).toString()));
        for (int column = 3; column <= 6; ++column) {
            auto *item = m_results->item(row, column);
            if (item->data(Qt::UserRole).isValid())
                item->setText(m_numberLocale.toString(item->data(Qt::UserRole).toDouble(), 'f', column < 5 ? 3 : 1));
        }
    }
    updateSummary();
    updateStatus();
    updateControls();
}
