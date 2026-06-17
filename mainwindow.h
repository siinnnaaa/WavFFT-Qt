#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <fftw3.h>
#include "ui_mainwindow.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void resetPlotsView();

private slots:
    void openWavFile();
    void downsample();
    void upsample();
    void saveSegment();
    void pbFast_Clicked();
    void pbSlow_Clicked();

private:
    Ui::MainWindow *ui;

    QVector<double> samples;
    int             sampleRate = 0;

    // FFT
    int                        fftN    = 0;
    double              *fftIn   = nullptr;
    fftw_complex  *fftOut  = nullptr;
    fftw_plan             fftPlan = nullptr;

    void resetFft(int N);
    void freeFft();

    bool loadWav(const QString &path,QVector<double> &outMono,int &outSampleRate,QString &err);
    void plotWaveform();
    void computeAndPlotFft();
    bool writeWav(const QString &path,const QVector<double> &data, int sr);
};

#endif // MAINWINDOW_H
