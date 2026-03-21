// AudioAnalyzer.cpp
#include "Audio.hpp"
#include <cmath>
#include <algorithm>
#include <complex>

namespace nk {

// ====================================================================
// FFT IMPLEMENTATION (Cooley-Tukey Algorithm)
// ====================================================================

class FFT {
public:
    // FFT récursive (pour comprendre l'algorithme)
    static void compute(std::vector<std::complex<float>>& data) {
        int n = data.size();
        
        if (n <= 1) return;
        
        // Diviser en pairs et impairs
        std::vector<std::complex<float>> even(n / 2);
        std::vector<std::complex<float>> odd(n / 2);
        
        for (int i = 0; i < n / 2; i++) {
            even[i] = data[i * 2];
            odd[i] = data[i * 2 + 1];
        }
        
        // Récursion
        compute(even);
        compute(odd);
        
        // Combiner
        for (int k = 0; k < n / 2; k++) {
            std::complex<float> t = std::polar(1.0f, -2.0f * M_PI * k / n) * odd[k];
            data[k] = even[k] + t;
            data[k + n / 2] = even[k] - t;
        }
    }
    
    // FFT itérative (plus rapide)
    static void computeIterative(std::vector<std::complex<float>>& data) {
        int n = data.size();
        
        // Vérifier que n est une puissance de 2
        if ((n & (n - 1)) != 0) {
            // Pad to next power of 2
            int nextPow2 = 1;
            while (nextPow2 < n) nextPow2 <<= 1;
            data.resize(nextPow2, 0.0f);
            n = nextPow2;
        }
        
        // Bit-reversal permutation
        int j = 0;
        for (int i = 0; i < n; i++) {
            if (i < j) {
                std::swap(data[i], data[j]);
            }
            
            int m = n / 2;
            while (m >= 1 && j >= m) {
                j -= m;
                m /= 2;
            }
            j += m;
        }
        
        // Butterfly operations
        for (int s = 1; s <= std::log2(n); s++) {
            int m = 1 << s;
            std::complex<float> wm = std::polar(1.0f, -2.0f * M_PI / m);
            
            for (int k = 0; k < n; k += m) {
                std::complex<float> w = 1.0f;
                
                for (int j = 0; j < m / 2; j++) {
                    std::complex<float> t = w * data[k + j + m / 2];
                    std::complex<float> u = data[k + j];
                    
                    data[k + j] = u + t;
                    data[k + j + m / 2] = u - t;
                    
                    w *= wm;
                }
            }
        }
    }
    
    // Inverse FFT
    static void inverseCompute(std::vector<std::complex<float>>& data) {
        int n = data.size();
        
        // Conjugate
        for (auto& val : data) {
            val = std::conj(val);
        }
        
        // Forward FFT
        computeIterative(data);
        
        // Conjugate and scale
        for (auto& val : data) {
            val = std::conj(val) / static_cast<float>(n);
        }
    }
};

// ====================================================================
// WINDOW FUNCTIONS (pour réduire les fuites spectrales)
// ====================================================================

class WindowFunction {
public:
    enum Type {
        Rectangular,
        Hanning,
        Hamming,
        Blackman,
        Kaiser
    };
    
    static std::vector<float> generate(Type type, int size) {
        std::vector<float> window(size);
        
        switch (type) {
            case Rectangular:
                std::fill(window.begin(), window.end(), 1.0f);
                break;
                
            case Hanning:
                for (int i = 0; i < size; i++) {
                    window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
                }
                break;
                
            case Hamming:
                for (int i = 0; i < size; i++) {
                    window[i] = 0.54f - 0.46f * std::cos(2.0f * M_PI * i / (size - 1));
                }
                break;
                
            case Blackman:
                for (int i = 0; i < size; i++) {
                    float t = static_cast<float>(i) / (size - 1);
                    window[i] = 0.42f - 0.5f * std::cos(2.0f * M_PI * t) + 
                               0.08f * std::cos(4.0f * M_PI * t);
                }
                break;
                
            case Kaiser:
                // Kaiser window (simplified, beta=8.6)
                for (int i = 0; i < size; i++) {
                    float x = 2.0f * i / (size - 1) - 1.0f;
                    window[i] = besselI0(8.6f * std::sqrt(1.0f - x * x)) / besselI0(8.6f);
                }
                break;
        }
        
        return window;
    }
    
private:
    // Bessel function I0 (pour Kaiser window)
    static float besselI0(float x) {
        float sum = 1.0f;
        float term = 1.0f;
        float m = 1.0f;
        
        while (term > 1e-12f) {
            term *= (x * x) / (4.0f * m * m);
            sum += term;
            m += 1.0f;
        }
        
        return sum;
    }
};

// ====================================================================
// AUDIO ANALYZER
// ====================================================================

float AudioAnalyzer::calculateRMS(const AudioSample& sample) {
    if (sample.data.empty()) return 0.0f;
    
    double sum = 0.0;
    
    if (sample.format == SampleFormat::Int16) {
        const int16_t* data = reinterpret_cast<const int16_t*>(sample.data.data());
        size_t sampleCount = sample.data.size() / 2;
        
        for (size_t i = 0; i < sampleCount; i++) {
            float value = data[i] / 32768.0f;
            sum += value * value;
        }
        
        return std::sqrt(sum / sampleCount);
        
    } else if (sample.format == SampleFormat::Float32) {
        const float* data = reinterpret_cast<const float*>(sample.data.data());
        size_t sampleCount = sample.data.size() / 4;
        
        for (size_t i = 0; i < sampleCount; i++) {
            sum += data[i] * data[i];
        }
        
        return std::sqrt(sum / sampleCount);
    }
    
    return 0.0f;
}

float AudioAnalyzer::calculatePeak(const AudioSample& sample) {
    if (sample.data.empty()) return 0.0f;
    
    float peak = 0.0f;
    
    if (sample.format == SampleFormat::Int16) {
        const int16_t* data = reinterpret_cast<const int16_t*>(sample.data.data());
        size_t sampleCount = sample.data.size() / 2;
        
        for (size_t i = 0; i < sampleCount; i++) {
            float value = std::abs(data[i] / 32768.0f);
            peak = std::max(peak, value);
        }
        
    } else if (sample.format == SampleFormat::Float32) {
        const float* data = reinterpret_cast<const float*>(sample.data.data());
        size_t sampleCount = sample.data.size() / 4;
        
        for (size_t i = 0; i < sampleCount; i++) {
            peak = std::max(peak, std::abs(data[i]));
        }
    }
    
    return peak;
}

std::vector<float> AudioAnalyzer::computeFFT(const AudioSample& sample, int fftSize) {
    // Convertir en float si nécessaire
    std::vector<float> floatData;
    size_t frameCount = sample.getFrameCount();
    
    if (sample.format == SampleFormat::Int16) {
        const int16_t* data = reinterpret_cast<const int16_t*>(sample.data.data());
        
        for (size_t i = 0; i < frameCount; i++) {
            // Mono mix si stereo
            float value = 0.0f;
            for (int ch = 0; ch < sample.channels; ch++) {
                value += data[i * sample.channels + ch] / 32768.0f;
            }
            floatData.push_back(value / sample.channels);
        }
        
    } else if (sample.format == SampleFormat::Float32) {
        const float* data = reinterpret_cast<const float*>(sample.data.data());
        
        for (size_t i = 0; i < frameCount; i++) {
            float value = 0.0f;
            for (int ch = 0; ch < sample.channels; ch++) {
                value += data[i * sample.channels + ch];
            }
            floatData.push_back(value / sample.channels);
        }
    }
    
    // Limiter à fftSize
    if (floatData.size() > static_cast<size_t>(fftSize)) {
        floatData.resize(fftSize);
    } else {
        floatData.resize(fftSize, 0.0f); // Pad avec des zéros
    }
    
    // Appliquer une fenêtre
    auto window = WindowFunction::generate(WindowFunction::Hanning, fftSize);
    
    for (int i = 0; i < fftSize; i++) {
        floatData[i] *= window[i];
    }
    
    // Convertir en complexe
    std::vector<std::complex<float>> complexData(fftSize);
    for (int i = 0; i < fftSize; i++) {
        complexData[i] = std::complex<float>(floatData[i], 0.0f);
    }
    
    // FFT
    FFT::computeIterative(complexData);
    
    // Calculer les magnitudes (seulement la moitié positive du spectre)
    std::vector<float> magnitudes(fftSize / 2);
    
    for (int i = 0; i < fftSize / 2; i++) {
        float magnitude = std::abs(complexData[i]);
        
        // Convertir en dB
        magnitudes[i] = 20.0f * std::log10(magnitude + 1e-10f);
    }
    
    return magnitudes;
}

std::vector<std::vector<float>> AudioAnalyzer::computeSpectrogram(const AudioSample& sample,
                                                                   int windowSize,
                                                                   int hopSize) {
    std::vector<std::vector<float>> spectrogram;
    
    // Convertir en mono float
    std::vector<float> monoData;
    size_t frameCount = sample.getFrameCount();
    
    if (sample.format == SampleFormat::Int16) {
        const int16_t* data = reinterpret_cast<const int16_t*>(sample.data.data());
        
        for (size_t i = 0; i < frameCount; i++) {
            float value = 0.0f;
            for (int ch = 0; ch < sample.channels; ch++) {
                value += data[i * sample.channels + ch] / 32768.0f;
            }
            monoData.push_back(value / sample.channels);
        }
    } else if (sample.format == SampleFormat::Float32) {
        const float* data = reinterpret_cast<const float*>(sample.data.data());
        
        for (size_t i = 0; i < frameCount; i++) {
            float value = 0.0f;
            for (int ch = 0; ch < sample.channels; ch++) {
                value += data[i * sample.channels + ch];
            }
            monoData.push_back(value / sample.channels);
        }
    }
    
    // Fenêtre
    auto window = WindowFunction::generate(WindowFunction::Hanning, windowSize);
    
    // Calculer FFT pour chaque fenêtre
    for (size_t start = 0; start + windowSize <= monoData.size(); start += hopSize) {
        // Extraire la fenêtre
        std::vector<float> windowedData(windowSize);
        
        for (int i = 0; i < windowSize; i++) {
            windowedData[i] = monoData[start + i] * window[i];
        }
        
        // Convertir en complexe
        std::vector<std::complex<float>> complexData(windowSize);
        for (int i = 0; i < windowSize; i++) {
            complexData[i] = std::complex<float>(windowedData[i], 0.0f);
        }
        
        // FFT
        FFT::computeIterative(complexData);
        
        // Magnitudes
        std::vector<float> magnitudes(windowSize / 2);
        
        for (int i = 0; i < windowSize / 2; i++) {
            float magnitude = std::abs(complexData[i]);
            magnitudes[i] = 20.0f * std::log10(magnitude + 1e-10f);
        }
        
        spectrogram.push_back(magnitudes);
    }
    
    return spectrogram;
}

// ====================================================================
// PITCH DETECTION (Autocorrelation)
// ====================================================================

float AudioAnalyzer::detectPitch(const AudioSample& sample) {
    // Convertir en mono float
    std::vector<float> monoData;
    size_t frameCount = sample.getFrameCount();
    
    if (sample.format == SampleFormat::Int16) {
        const int16_t* data = reinterpret_cast<const int16_t*>(sample.data.data());
        
        for (size_t i = 0; i < frameCount; i++) {
            float value = 0.0f;
            for (int ch = 0; ch < sample.channels; ch++) {
                value += data[i * sample.channels + ch] / 32768.0f;
            }
            monoData.push_back(value / sample.channels);
        }
    }
    
    if (monoData.size() < 2048) {
        return 0.0f; // Pas assez de données
    }
    
    // Autocorrélation
    int maxLag = std::min(static_cast<int>(monoData.size() / 2), 
                         sample.sampleRate / 50); // Min 50 Hz
    
    std::vector<float> autocorr(maxLag);
    
    for (int lag = 0; lag < maxLag; lag++) {
        float sum = 0.0f;
        
        for (size_t i = 0; i + lag < monoData.size(); i++) {
            sum += monoData[i] * monoData[i + lag];
        }
        
        autocorr[lag] = sum;
    }
    
    // Trouver le premier pic après le zéro
    int minLag = sample.sampleRate / 1000; // Max 1000 Hz
    
    float maxCorr = 0.0f;
    int peakLag = 0;
    
    for (int lag = minLag; lag < maxLag; lag++) {
        if (autocorr[lag] > maxCorr) {
            maxCorr = autocorr[lag];
            peakLag = lag;
        }
    }
    
    if (peakLag == 0) {
        return 0.0f; // Pas de pitch détecté
    }
    
    // Convertir le lag en fréquence
    return static_cast<float>(sample.sampleRate) / peakLag;
}

// ====================================================================
// TEMPO DETECTION (Beat Detection simplifié)
// ====================================================================

float AudioAnalyzer::detectTempo(const AudioSample& sample) {
    // Calculer l'énergie par fenêtre
    int hopSize = sample.sampleRate / 100; // 10ms hop
    int windowSize = hopSize * 2;
    
    std::vector<float> energy;
    
    if (sample.format == SampleFormat::Int16) {
        const int16_t* data = reinterpret_cast<const int16_t*>(sample.data.data());
        size_t frameCount = sample.getFrameCount();
        
        for (size_t start = 0; start + windowSize < frameCount; start += hopSize) {
            float sum = 0.0f;
            
            for (int i = 0; i < windowSize; i++) {
                for (int ch = 0; ch < sample.channels; ch++) {
                    float value = data[(start + i) * sample.channels + ch] / 32768.0f;
                    sum += value * value;
                }
            }
            
            energy.push_back(sum / (windowSize * sample.channels));
        }
    }
    
    // Détecter les pics d'énergie
    std::vector<int> peaks;
    float threshold = 0.0f;
    
    // Calculer le threshold (moyenne)
    for (float e : energy) {
        threshold += e;
    }
    threshold /= energy.size();
    threshold *= 1.5f; // 1.5x la moyenne
    
    for (size_t i = 1; i < energy.size() - 1; i++) {
        if (energy[i] > energy[i - 1] && 
            energy[i] > energy[i + 1] && 
            energy[i] > threshold) {
            peaks.push_back(i);
        }
    }
    
    if (peaks.size() < 2) {
        return 0.0f; // Pas assez de beats
    }
    
    // Calculer l'intervalle moyen entre les beats
    float avgInterval = 0.0f;
    
    for (size_t i = 1; i < peaks.size(); i++) {
        avgInterval += peaks[i] - peaks[i - 1];
    }
    avgInterval /= (peaks.size() - 1);
    
    // Convertir en BPM
    float secondsPerBeat = (avgInterval * hopSize) / sample.sampleRate;
    float bpm = 60.0f / secondsPerBeat;
    
    return bpm;
}

} // namespace nk