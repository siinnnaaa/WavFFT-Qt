# WavFFT-Qt

**A Qt-based desktop application for visualizing WAV audio files in time and frequency domains using FFTW**

![Qt](https://img.shields.io/badge/Qt-Widgets-green) ![C++](https://img.shields.io/badge/C%2B%2B-11-blue) ![FFTW](https://img.shields.io/badge/FFTW-3.3.5-orange) ![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

---

## 📋 Overview

WavFFT-Qt is a desktop audio analyzer that allows you to open WAV files and visualize both the **time-domain waveform** and **frequency-domain FFT spectrum**. The application supports interactive zooming and panning, along with audio processing features like upsampling, downsampling, and segment saving.

### Features

- ✅ Open and load **16-bit PCM WAV files** (mono or stereo)
- ✅ Automatic stereo-to-mono conversion
- ✅ Real-time **waveform visualization** (time domain)
- ✅ Real-time **FFT magnitude spectrum** (frequency domain)
- ✅ Interactive plot controls (zoom, pan, drag)
- ✅ **Downsample** and **Upsample** audio
- ✅ Save audio segments
- ✅ Playback speed control (Fast / Slow)

---

## 🛠️ Tech Stack

- **Language**: C++
- **GUI Framework**: Qt 5/6 (Widgets)
- **Plotting**: QCustomPlot
- **FFT Library**: FFTW 3.3.5 (64-bit)

---

## 📦 Prerequisites

Before building the project, ensure you have the following installed:

- **Qt SDK** (5.x or 6.x) with Qt Creator
- **FFTW 3.3.5 DLL (64-bit)** – [Download here](http://www.fftw.org/install/windows.html)
- **QCustomPlot** library (usually included or as a separate `.h`/`.cpp`)
- **MinGW-w64** or **MSVC** compiler (depending on your Qt setup)

---

## 🚀 Setup & Build

### 1. Clone the repository
```bash
git clone https://github.com/[your-username]/WavFFT-Qt.git
cd WavFFT-Qt
```

### 2. Download and install FFTW
- Download **FFTW 3.3.5 precompiled DLL (64-bit)** from the [official site](http://www.fftw.org/install/windows.html)
- Extract and place `fftw3.dll` in your project directory or system PATH
- Update the `.pro` file to link against `libfftw3-3.dll` or `fftw3.lib`

### 3. Open the project in Qt Creator
```bash
open FFTW_PRJ.pro
```
or simply double-click `FFTW_PRJ.pro`

### 4. Build and run
- Click **Build** → **Build Project**
- Click **Run** to launch the application

---

## 📂 Project Structure

WavFFT-Qt/
├── src/
│   ├── main.cpp              # Entry point
│   ├── mainwindow.cpp        # Main logic and signal processing
│   ├── mainwindow.h          # Header file
│   └── mainwindow.ui         # Qt Designer UI file
├── FFTW_PRJ.pro              # Qt project file
├── docs/
│   └── screenshots/          # Application screenshots
├── README.md
├── LICENSE
└── .gitignore


---

## 🎨 UI Components

### Main Window: `Show WAV و FFT`

| Widget | Description |
|--------|-------------|
| **wavePlot** | Time-domain waveform plot<br>X: Time (s), Y: Amplitude |
| **fftPlot** | Frequency-domain FFT magnitude plot<br>X: Frequency (Hz), Y: Magnitude |
| **openBtn** | Open WAV file dialog |
| **btnDown** | Downsample audio |
| **btnUp** | Upsample audio |
| **btnSave** | Save selected segment |
| **pbFast** | Increase playback speed |
| **pbSlow** | Decrease playback speed |
| **infoLabel** | Displays file name, sample rate, and sample count |

---

## 🔧 How It Works

1. **Open WAV File**: User selects a 16-bit PCM WAV file
2. **Load & Convert**: File is loaded, stereo is converted to mono
3. **Plot Waveform**: Time-domain signal is plotted on `wavePlot`
4. **Compute FFT**: FFTW computes the FFT of the audio signal
5. **Plot Spectrum**: Magnitude spectrum is plotted on `fftPlot`
6. **Interactive Controls**: User can zoom, pan, resample, or save segments

---

## 📸 Screenshots

> *Add screenshots of your application here*

---

## 📄 License

This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- [FFTW](http://www.fftw.org/) – Fast Fourier Transform library
- [QCustomPlot](https://www.qcustomplot.com/) – Qt plotting library
- Qt Project

---

## 📧 Contact

For questions or suggestions, feel free to open an issue or reach out!

---

**Made with ❤️ using Qt + FFTW + C++**
