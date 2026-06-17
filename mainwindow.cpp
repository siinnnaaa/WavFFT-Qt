#include "mainwindow.h"
#include "qpen.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QVector>
#include <cmath>

// Human auditory range (in Hz)
static const int AUDIBLE_START = 20;
static const int AUDIBLE_END = 20000;

// Constructor: sets up the UI, plots, and signal-slot connections
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
    ui(new Ui::MainWindow) { ui->setupUi(this);

    setWindowTitle("Show WAV و FFT");

    // Waveform plot configuration
    ui->wavePlot->xAxis->setLabel("Time (s)");
    ui->wavePlot->yAxis->setLabel("Amplitude");
    ui->wavePlot->addGraph()->setPen(QPen(Qt::black));
    ui->wavePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom); // Enable drag and zoom

    // FFT plot configuration
    ui->fftPlot->xAxis->setLabel("Frequency (Hz)");
    ui->fftPlot->yAxis->setLabel("Magnitude");
    ui->fftPlot->addGraph()->setPen(QPen(Qt::darkBlue));
    ui->fftPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom); // Enable drag and zoom

    // Connect buttons to their corresponding handler slots
    connect(ui->openBtn,   &QPushButton::clicked, this,&MainWindow::openWavFile);
    connect(ui->btnDown,  &QPushButton::clicked, this, &MainWindow::downsample);
    connect(ui->btnUp,       &QPushButton::clicked, this, &MainWindow::upsample);
    connect(ui->btnSave,    &QPushButton::clicked, this, &MainWindow::saveSegment);
    connect(ui->pbFast,      &QPushButton::clicked, this, &::MainWindow::pbFast_Clicked);
    connect(ui->pbSlow,     &QPushButton::clicked, this, &::MainWindow::pbSlow_Clicked);
}

// Destructor: release FFT resources and UI
MainWindow::~MainWindow(){
    freeFft();
    delete ui;
}
///////////////////////////////////////////////////////////////////////////////////////////
// Opens a WAV file via dialog, loads it, and plots waveform + FFT
void MainWindow::openWavFile()
{
    // Show file open dialog filtered to WAV files
    QString path = QFileDialog::getOpenFileName(this,
                                                QString::fromUtf8("Open WAV File"),
                                                QString(),
                                                QStringLiteral("WAV Files (*.wav *.WAV)")
                                                );
    if (path.isEmpty())
        return; // User cancelled

    QString err;
    QVector<double> mono;
    int sr = 0;
    // Attempt to load WAV; on failure, clear state and show error
    if (!loadWav(path, mono, sr, err)) {

        ui->infoLabel->setText(QString::fromUtf8("Error reading WAV: ") + err);

        samples.clear();
        sampleRate = 0;

        // Clear both plots
        ui->wavePlot->graph(0)->data()->clear();
        ui->fftPlot->graph(0)->data()->clear();
        ui->wavePlot->replot();
        ui->fftPlot->replot();
        return;
    }

    // Store loaded data
    samples    = std::move(mono);
    sampleRate = sr;
    // Display file metadata
    ui->infoLabel->setText(QString::fromUtf8("File: %1 | Sampling: %2 Hz | Samples: %3")
                               .arg(QFileInfo(path).fileName())
                               .arg(sampleRate)
                               .arg(samples.size()));
    plotWaveform();
    computeAndPlotFft();
}
///////////////////////////////////////////////////////////////////////////////////////////
// Frees all FFTW-allocated resources (plan, input, output buffers)
void MainWindow::freeFft()
{
    if (fftPlan) {
        fftw_destroy_plan(fftPlan);
        fftPlan = nullptr;
    }
    if (fftIn) {
        fftw_free(fftIn);
        fftIn = nullptr;
    }
    if (fftOut) {
        fftw_free(fftOut);
        fftOut = nullptr;
    }
    fftN = 0;
}
///////////////////////////////////////////////////////////////////////////////////////////
// Loads a 16-bit PCM WAV file (mono or stereo) and converts it to a mono double vector
bool MainWindow::loadWav(const QString &path,
                         QVector<double> &outMono,
                         int &outSampleRate,
                         QString &err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        err = QString::fromUtf8("Unable to open the file");
        return false;
    }
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian); // WAV is little-endian

    // --- Read RIFF header ---
    char riff[4], wave[4];
    quint32 riffSize = 0;
    if (f.read(riff, 4) != 4 || QByteArray(riff,4) != "RIFF") {
        err = QString::fromUtf8("RIFF Header not found");
        return false;
    }
    ds >> riffSize;
    if (f.read(wave, 4) != 4 || QByteArray(wave,4) != "WAVE") {
        err = QString::fromUtf8("Wave Header not found");
        return false;
    }

    // --- Search for "fmt " and "data" chunks ---
    bool haveFmt = false, haveData = false;
    quint16 audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    quint32 sampleRateLocal = 0, byteRate = 0, dataSize = 0;
    quint16 blockAlign = 0;
    qint64 dataPos = -1;

    while (!f.atEnd()) {
        char id[4]; quint32 size=0;
        if (f.read(id,4)!=4) break;
        ds >> size;
        QByteArray chunkId(id,4);

        if (chunkId=="fmt ") {
            // Read format subchunk fields
            haveFmt = true;
            ds >> audioFormat >> numChannels >> sampleRateLocal>> byteRate
                >> blockAlign >> bitsPerSample;
            if (size>16) f.seek(f.pos() + (size-16)); // Skip any extra format bytes
        }
        else if (chunkId=="data") {
            // Record data chunk position and size, then skip its content
            haveData = true;
            dataSize = size;
            dataPos = f.pos();
            f.seek(f.pos()+size);
        }
        else {
            f.seek(f.pos()+size); // Skip unknown chunk
        }
        if (haveFmt && haveData) break;
    }

    if (!haveFmt || !haveData) {
        err = QString::fromUtf8("No fmt or data chunks found.");
        return false;
    }
    // Only support uncompressed 16-bit PCM, mono or stereo
    if (audioFormat!=1 || bitsPerSample!=16 ||
        !(numChannels==1||numChannels==2)) {
        err = QString::fromUtf8("Unsupported WAV format.");
        return false;
    }

    // --- Read raw sample data ---
    f.seek(dataPos);
    QByteArray raw = f.read(dataSize);
    int frameSize = (bitsPerSample/8)*numChannels; // Bytes per audio frame
    int frames   = raw.size()/frameSize;
    outMono.resize(frames);
    const char *p = raw.constData();

    // Convert each frame to a normalized mono sample [-1, 1]
    for (int i=0; i<frames; ++i) {
        qint16 sL = qint16(uchar(p[0]) | (uchar(p[1])<<8));            // Left channel
        qint16 sR = numChannels==2 ? qint16(uchar(p[2]) | (uchar(p[3])<<8))
                                     : sL;                                          // Right channel (or copy of left if mono)
        p += frameSize;
        outMono[i] = 0.5*(double(sL)/32768.0 + double(sR)/32768.0);   // Average to mono and normalize
    }
    outSampleRate = int(sampleRateLocal);
    return true;
}
///////////////////////////////////////////////////////////////////////////////////////////
// Applies a simple moving-average low-pass filter to the signal in place
void applyLowPassFilter(QVector<double>& signal, double cutoffRatio)
{
    int N = signal.size();

    QVector<double> filtered(N);

    // Determine window size from the cutoff ratio (smaller ratio -> larger window)
    int windowSize = static_cast<int>(1.0 / cutoffRatio);
    if (windowSize < 1) windowSize = 1;

    // Compute moving average centered on each sample
    for (int i = 0; i < N; ++i) {
        double sum = 0.0;
        int count = 0;
        for (int j = -windowSize/2; j <= windowSize/2; ++j) {
            if (i+j >= 0 && i+j < N) { // Stay within bounds
                sum += signal[i+j];
                count++;
            }
        }
        filtered[i] = sum / count;
    }

    signal = filtered;
}
/////////////////////////////////////////////////////////////////////////////////////////
// Speeds up playback by keeping every Nth sample (simple decimation, no anti-aliasing)
void MainWindow::pbFast_Clicked()     ///// Important
{
    bool ok;
    int factor = ui->leFactor->text().toInt(&ok);
    if (!ok || factor < 1) return; // Validate factor

    QVector<double> fast;
    fast.reserve(samples.size() / factor);
    for (int i = 0; i < samples.size(); i += factor)
        fast.append(samples[i]); // Take every Nth sample

    samples = fast;
    plotWaveform();
    computeAndPlotFft();
}
///////////////////////////////////////////////////////////////////////////////////////////
// Slows down playback by repeating each sample N times (sample-and-hold)
void MainWindow::pbSlow_Clicked()        ///// Important
{
    bool ok;
    int factor = ui->leFactor->text().toInt(&ok);
    if (!ok || factor < 1) return; // Validate factor

    QVector<double> slow;
    slow.reserve(samples.size() * factor);
    for (double v : samples)
        for (int k = 0; k < factor; ++k)
            slow.append(v); // Duplicate each sample 'factor' times

    samples = slow;
    plotWaveform();
    computeAndPlotFft();
}
///////////////////////////////////////////////////////////////////////////////////////////
// Downsamples a selected time segment by factor N, with an anti-aliasing low-pass filter
void MainWindow::downsample()
{
    bool ok1, ok2, ok3;
    double s = ui->leStart->text().toDouble(&ok1);
    double e = ui->leEnd->text().toDouble(&ok2);
    int    n = ui->leFactor->text().toInt(&ok3);

    if (!ok1 || !ok2 || !ok3 || n < 1) {
        QMessageBox::warning(this, "Input Error", "Enter the parameters correctly.");
        return;
    }

    // Convert start/end times (seconds) to sample indices
    int i0 = qMax(0,           int(s * sampleRate));
    int i1 = qMin(int(samples.size()), int(e * sampleRate));

    // Extract the selected time-domain segment
    QVector<double> segment;
    for (int i = i0; i < i1; ++i) {
        segment.append(samples[i]);
    }

    // Apply a low-pass filter before reducing the sample rate to avoid aliasing
    double newNyquist = sampleRate / (2.0 * n);       // New Nyquist frequency after downsampling
    double cutoffRatio = newNyquist / (sampleRate / 2.0); // Cutoff relative to current Nyquist
    applyLowPassFilter(segment, cutoffRatio);

    // Decimate: keep every Nth filtered sample
    QVector<double> downsampled;
    downsampled.reserve((i1 - i0 + n - 1) / n);
    for (int i = 0; i < segment.size(); i += n) {
        downsampled.append(segment[i]);
    }

    samples = downsampled;
    sampleRate = sampleRate / n; // Update the global sample rate for plotting/saving

    // Update the info label with the new signal properties
    ui->infoLabel->setText(QString("Downsampled ×%1 | Rate: %2 Hz | Samples: %3")
                               .arg(n)
                               .arg(sampleRate)
                               .arg(samples.size()));

    plotWaveform();
    computeAndPlotFft();
}
// ---------------------------------------------------------------------------
// upsample(): Increase the sample rate of a selected segment by an integer factor
// ---------------------------------------------------------------------------
void MainWindow::upsample()
{
    // Read user inputs: start time, end time, and the upsampling factor
    bool okS, okE, okN;
    double s = ui->leStart->text().toDouble(&okS);
    double e = ui->leEnd->text().toDouble(&okE);
    int    n = ui->leFactor->text().toInt(&okN);

    // Validate the inputs; the factor must be at least 1
    if (!okS || !okE || !okN || n < 1) {
        QMessageBox::warning(this, "Input Error", "Enter Parameters correctly.");
        return;
    }

    // Convert start/end times (seconds) to sample indices
    int i0 = qMax(0,                    int(s * sampleRate));
    int i1 = qMin(int(samples.size()),  int(e * sampleRate));

    // Extract the selected time-domain segment
    QVector<double> segment;
    for (int i = i0; i < i1; ++i) {
        segment.append(samples[i]);
    }

    // Create the upsampled buffer (n times larger) initialized with zeros
    QVector<double> upsampled(segment.size() * n, 0.0);

    // Zero-stuffing: place each original sample every Nth position.
    // Multiplying by n compensates for the energy lost during zero-stuffing.
    for (int i = 0; i < segment.size(); ++i) {
        upsampled[i * n] = segment[i] * n;
    }

    // Apply a low-pass filter to interpolate the inserted zeros
    applyLowPassFilter(upsampled, 1.0 / n);

    samples = upsampled;
    sampleRate = sampleRate * n; // Update the global sample rate

    // Update the info label with the new signal properties
    ui->infoLabel->setText(QString("Upsampled ×%1 | Rate: %2 Hz | Samples: %3")
                               .arg(n)
                               .arg(sampleRate)
                               .arg(samples.size()));

    plotWaveform();
    computeAndPlotFft();
}

// ---------------------------------------------------------------------------
// saveSegment(): Save a selected time interval of the signal to a WAV file
// ---------------------------------------------------------------------------
void MainWindow::saveSegment()
{
    // Make sure there is a valid signal to work with
    if (samples.isEmpty() || sampleRate <= 0) {
        QMessageBox::warning(this, "No Data", "Take action first..");
        return;
    }

    // Read the start and end times (in seconds)
    bool okS, okE;
    double s = ui->leStart->text().toDouble(&okS);
    double e = ui->leEnd->text().toDouble(&okE);

    // Validate: start must be non-negative and end must be after start
    if (!okS || !okE || s < 0 || e <= s) {
        QMessageBox::warning(this, "Input Error", "Enter the correct start/end time..");
        return;
    }

    // Convert times to bounded sample indices
    int i0 = qBound(0, int(s * sampleRate), samples.size());
    int i1 = qBound(0, int(e * sampleRate), samples.size());

    // Extract the segment to be saved
    QVector<double> segment = samples.mid(i0, i1 - i0);

    // Temporarily swap in the segment so the plots show only the saved interval
    auto oldSamples = samples;
    samples = segment;
    plotWaveform();
    computeAndPlotFft();
    samples = oldSamples; // Restore the full signal

    // Ask the user where to save the file
    QString fn = QFileDialog::getSaveFileName(this, "Save signal",
                                              "processed.wav",
                                              "WAV Files (*.wav)");
    if (fn.isEmpty()) {
        return; // User cancelled
    }

    // Write the segment to disk and report the result
    if (writeWav(fn, segment, sampleRate)) {
        QMessageBox::information(this, "Done", "Save Completed.");
    } else {
        QMessageBox::critical(this, "Error", "Error in saving WAV.");
    }
}

// ---------------------------------------------------------------------------
// writeWav(): Write mono 16-bit PCM samples to a standard RIFF/WAVE file
// ---------------------------------------------------------------------------
bool MainWindow::writeWav(const QString &path, const QVector<double> &data, int sr)
{
    // Open the target file for writing
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    // Use little-endian byte order as required by the WAV format
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // Compute size fields for the header
    quint32 dataBytes = quint32(data.size() * sizeof(qint16));
    quint32 riffSize  = 36 + dataBytes;

    // --- RIFF chunk descriptor ---
    out.writeRawData("RIFF", 4);
    out << riffSize;
    out.writeRawData("WAVE", 4);

    // --- "fmt " sub-chunk (PCM format description) ---
    out.writeRawData("fmt ", 4);
    out << quint32(16);          // Sub-chunk1 size for PCM
    out << quint16(1);           // Audio format: 1 = PCM
    out << quint16(1);           // Number of channels: mono
    out << quint32(sr);          // Sample rate
    out << quint32(sr * 2);      // Byte rate = sampleRate * blockAlign
    out << quint16(2);           // Block align = channels * bytesPerSample
    out << quint16(16);          // Bits per sample

    // --- "data" sub-chunk (the actual audio samples) ---
    out.writeRawData("data", 4);
    out << dataBytes;

    // Convert each normalized double sample to a clamped 16-bit integer
    for (double d : data) {
        double v = qBound(-1.0, d, 1.0);     // Clamp to valid range
        qint16 sVal = qint16(v * 32767);     // Scale to 16-bit
        out << sVal;
    }

    return true;
}

// ---------------------------------------------------------------------------
// resetFft(): (Re)allocate FFTW resources for a transform of length N
// ---------------------------------------------------------------------------
void MainWindow::resetFft(int N)
{
    // If everything is already set up for this N, there is nothing to do
    if (fftN == N && fftIn && fftOut && fftPlan) {
        return;
    }

    // Release any previously allocated FFTW resources
    freeFft();

    fftN = N;

    // Allocate input (real) and output (complex) buffers
    fftIn  = (double*)       fftw_malloc(sizeof(double)       * fftN);
    fftOut = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (fftN / 2 + 1));

    // Create a real-to-complex 1D FFT plan
    fftPlan = fftw_plan_dft_r2c_1d(fftN, fftIn, fftOut, FFTW_ESTIMATE);
}

// ---------------------------------------------------------------------------
// plotWaveform(): Draw the time-domain signal on the waveform plot
// ---------------------------------------------------------------------------
void MainWindow::plotWaveform()
{
    // Nothing to plot without a valid signal
    if (samples.isEmpty() || sampleRate <= 0) {
        return;
    }

    // Build the time axis: t[i] = i / sampleRate
    QVector<double> t(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        t[i] = double(i) / double(sampleRate);
    }

    // Feed the data to the plot
    ui->wavePlot->graph(0)->setData(t, samples);

    // Fit the axes to the signal
    ui->wavePlot->xAxis->setRange(0, t.last());
    ui->wavePlot->yAxis->setRange(-1, 1);

    ui->wavePlot->replot();
}

// ---------------------------------------------------------------------------
// computeAndPlotFft(): Compute the magnitude spectrum and plot it (audible range)
// ---------------------------------------------------------------------------
void MainWindow::computeAndPlotFft()
{
    // Nothing to compute without a valid signal
    if (samples.isEmpty() || sampleRate <= 0) {
        return;
    }

    // Analyze at most the first 2 seconds of the signal
    int twoSec = 2 * sampleRate;
    int N = qMin(twoSec, samples.size());

    // Skip the FFT for very short segments
    if (N < 128) {
        ui->fftPlot->graph(0)->data()->clear();
        ui->fftPlot->replot();
        return;
    }

    // Prepare FFTW for a transform of length N
    resetFft(N);

    // Copy the first N samples into the FFT input buffer
    for (int i = 0; i < N; ++i) {
        fftIn[i] = samples[i];
    }

    // Run the forward FFT; results are written into fftOut
    fftw_execute(fftPlan);

    // Limit the displayed bins to the audible frequency range [20 Hz, 20 kHz]
    int kMax   = N / 2;
    int kStart = qMax(1,    int(qCeil(double(AUDIBLE_START) * N / sampleRate)));
    int kEnd   = qMin(kMax, int(qFloor(double(AUDIBLE_END)  * N / sampleRate)));

    QVector<double> freqs;
    QVector<double> mags;

    // Compute the normalized magnitude for each bin in range
    for (int k = kStart; k <= kEnd; ++k) {
        double re = fftOut[k][0];
        double im = fftOut[k][1];
        double m  = std::sqrt(re * re + im * im) / N; // Normalized magnitude

        freqs.append(double(k) * sampleRate / N);     // Bin center frequency
        mags.append(m);
    }

    // Plot the spectrum
    ui->fftPlot->graph(0)->setData(freqs, mags);

    // Set the frequency (x) axis to the audible range, capped at the Nyquist limit
    ui->fftPlot->xAxis->setRange(AUDIBLE_START,
                                 qMin(double(AUDIBLE_END), sampleRate / 2.0));

    // Auto-scale the magnitude (y) axis with a small headroom
    double maxY = 0.0;
    if (!mags.isEmpty()) {
        maxY = *std::max_element(mags.begin(), mags.end());
    }
    ui->fftPlot->yAxis->setRange(0, maxY * 1.1);

    ui->fftPlot->replot();
}
